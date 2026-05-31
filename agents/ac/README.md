# AC Agent - HVAC Controller

The AC agent manages HVAC equipment — heat-pump/compressor units, air-handler fans, and water-circulation pumps — for the SolarDirector system. It drives a thermal-storage (water tank) heat-pump system: it charges the tank with solar-powered heating/cooling, conditions the building from the tank, and can run a unit's loop **directly** through an air handler when the tank can't serve the demand. It integrates with the Power Agent (PA) for power allocation.

**Agent role:** `SOLARD_ROLE_CONTROLLER` &nbsp;•&nbsp; **Description:** `HVAC Controller` &nbsp;•&nbsp; **Author:** Stephen P. Shoecraft

## Table of Contents

- [Overview](#overview)
- [Components](#components)
- [State Machines](#state-machines)
- [Mode Selection](#mode-selection)
- [Thermal Storage Charging](#thermal-storage-charging)
- [Direct Mode](#direct-mode)
- [Power Integration (PA)](#power-integration-pa)
- [Configuration](#configuration)
- [Registered Functions](#registered-functions)
- [Communication](#communication)
- [Development](#development)

---

## Overview

The agent manages four kinds of objects, each with its own state machine:

1. **Units** — heat-pump/compressor units controlled via GPIO (cool/heat contactors + reversing valve). A unit moves heat between its refrigerant loop and a water loop driven by a pump.
2. **Fans** — air handlers controlled remotely over MQTT (the agent subscribes to a fan controller topic and publishes the desired state). A fan may have an associated water pump.
3. **Pumps** — water-circulation pumps controlled via GPIO, with optional inlet/outlet temperature sensors, flow sensing, and a primer pump for air purging. Pumps are reference-counted so several units/fans can share one.
4. **Direct groups** — a binding of one fan + one unit + two valve pins that lets a unit's water loop run straight through an air handler's coil, bypassing the storage tank.

On top of these sit several control loops, all in JavaScript and dispatched each cycle from `run.js`:

- **mode.js** — automatic heat/cool mode selection from outside-temperature history.
- **charge.js** — thermal-storage charging (drives units to heat/cool the tank to target).
- **direct.js** — direct-mode state machine (tank-bypass).
- **sample.js** — periodic pump/temperature sampling.
- **cycle.js** — periodic circulation/cycling.

```
                         ┌──────────── run.js (main loop) ────────────┐
                         │  mode → charge → direct → sample → cycle    │
                         └──┬───────────┬───────────┬──────────────────┘
              ┌─────────────┘           │           └──────────────┐
        ┌─────▼─────┐             ┌──────▼─────┐             ┌──────▼─────┐
        │   Units   │             │    Fans    │             │   Pumps    │
        │  (GPIO)   │             │   (MQTT)   │             │  (GPIO)    │
        └─────┬─────┘             └──────┬─────┘             └──────┬─────┘
              │                          │                          │
              └──────────── PA (reserve/release/repri/revoke) ──────┘
                          │                              │
                    ┌─────▼─────┐                 ┌──────▼──────┐
                    │  CAN bus  │                 │ Temp sensors│
                    │ 0x450-45F │                 │ (via CAN)   │
                    └───────────┘                 └─────────────┘
```

### File Structure

```
ac/
├── main.c          # Entry point; registers can_target/can_topts/interval; -i flag
├── driver.c        # Driver struct (New/Free/Read/Config), agent info
├── can.c           # CAN bus: recv thread, frame cache (IDs 0x450-0x45F)
├── jsfuncs.c       # C functions exposed to JS: can_read, can_write, signal
├── ac.h            # Session struct, CAN state bits
│
├── init.js         # Init + load order: mode,errors,pump,fan,unit,direct,cycle,sample,charge
├── common.js       # Shared component helpers
├── utils.js        # Utilities incl. can_get_sensor (CAN temp decode)
├── run.js          # Main per-cycle state-machine dispatch
├── read.js         # Read sensors / water_temp
├── write.js        # Drive outputs
├── pub.js          # Publish state to MQTT/InfluxDB
├── start.js        # Startup
├── mode.js         # Auto mode selection (InfluxDB-weighted outside temp)
├── charge.js       # Thermal-storage charging
├── direct.js       # Direct-mode (tank-bypass) state machine
├── sample.js       # Periodic temperature sampling
├── cycle.js        # Periodic circulation
├── unit.js         # Unit component
├── fan.js          # Fan component
├── pump.js         # Pump component
└── errors.js       # Error tracking (clear_error)
```

Additional docs in this directory: `direct_mode.md`, `state.md`, `influxdb_hvac.md`, `CLAUDE.md`. A JavaScript test suite lives in `test/`.

---

## Components

### Units

Heat-pump/compressor units controlled via GPIO contactors.

**Properties** (`unit_props`, defaults shown):

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `enabled` | bool | true | Enable/disable the unit |
| `pump` | string | null | Associated pump name (required for heat exchange) |
| `pump_timeout` | int | 60 | Seconds to wait for the pump to start |
| `min_runtime` | int | 300 | Minimum runtime hint (seconds) — see note below |
| `coolpin` | int | -1 | GPIO pin for cooling contactor |
| `heatpin` | int | -1 | GPIO pin for heating contactor |
| `rvpin` | int | -1 | GPIO pin for reversing valve |
| `rvcool` | bool | true | Reversing-valve polarity (energized for cooling) |
| `rvevery` | bool | false | Energize RV every cycle vs. only on mode change |
| `liquid_temp_sensor` | string | "" | Optional liquid-line temp sensor |
| `vapor_temp_sensor` | string | "" | Optional vapor-line temp sensor (freeze protection) |
| `direct` | bool | false | Unit currently running in direct mode (also a runtime flag) |
| `reserve` | int | 0 | PA power reservation (watts); 0 = no reservation |
| `priority` | int | 0 | PA priority (1 = highest) |

> **Note on `min_runtime`:** the AC agent no longer enforces minimum runtime via a dedicated state. The old `MIN_RUNTIME_WAIT` state was **removed** — the comment in `unit_cooldown()`/`fan_cooldown()` reads *"MIN_RUNTIME_WAIT removed - let thermostat handle minimum runtime."* The property is still registered but stops now go straight through the cooldown path to off.

### Fans

Air handlers controlled over MQTT. The agent subscribes to the fan's `topic` for state and publishes the desired state.

**Properties** (`fan_props`):

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `enabled` | bool | true | Enable/disable the fan |
| `topic` | string | null | MQTT topic for fan data |
| `pump` | string | null | Associated pump name (optional) |
| `start_timeout` | int | 60 | Seconds to wait for the fan to report started |
| `pump_timeout` | int | 40 | Seconds to wait for the pump to start |
| `min_runtime` | int | 0 | Minimum runtime hint (seconds) — see unit note |
| `cooldown` | int | 30 | Thermal cooldown period after stopping (seconds) |
| `cooldown_threshold` | double | 25.0 | Temperature threshold for cooldown |
| `reserve` | int | 0 | PA power reservation (watts) |
| `priority` | int | 100 | PA priority |
| `stop_wt` | bool | true | Stop if water temp goes out of usable range |
| `wt_thresh` | int | 3 | Water-temp threshold offset (°) |

Fan modes: `FAN_MODE_NONE` (0), `FAN_MODE_COOL` (1), `FAN_MODE_HEAT` (2).

### Pumps

Water-circulation pumps with temperature/flow monitoring and reference counting.

**Properties** (`pump_props`):

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `enabled` | bool | true | Enable/disable the pump |
| `pin` | int | -1 | GPIO pin for pump control |
| `primer` | string | "" | Primer pump name (air purging) |
| `primer_timeout` | int | 30 | Seconds to wait for primer |
| `flow_sensor` | string | "" | Flow sensor name (optional) |
| `min_flow` | double | 0 | Minimum flow rate |
| `wait_time` | int | 10 | Initial wait after starting (seconds) |
| `flow_wait_time` | int | 5 | Wait for stable flow (seconds) |
| `flow_timeout` | int | 20 | Timeout waiting for flow (seconds) |
| `temp_in_sensor` | string | "" | Inlet temperature sensor |
| `temp_out_sensor` | string | "" | Outlet temperature sensor |
| `warmup` | int | 30 | Warmup period before "running" (seconds) |
| `warmup_threshold` | double | 10.0 | Temperature-change threshold for warmup |
| `cooldown` | int | 0 | Cooldown period after stopping (seconds) |
| `cooldown_threshold` | double | 10.0 | Temperature threshold for cooldown |
| `reserve` | int | 0 | PA power reservation (watts) |
| `priority` | int | 100 | PA priority |

### Direct Groups

A direct group binds a `fan` and a `unit` plus two valve pins. See [Direct Mode](#direct-mode).

**Properties** (`direct_props`):

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `enabled` | bool | true | Enable/disable the group |
| `pin1` | int | -1 | Stage-1 valve GPIO (AC → air handler) |
| `pin2` | int | -1 | Stage-2 valve GPIO (loop closure) |
| `fan` | string | "" | Air-handler fan name |
| `unit` | string | "" | Heat-pump unit name |
| `primer_time` | int | 30 | Primer run time (seconds) |
| `temp_threshold` | double | 2.0 | Temperature delta threshold |
| `unit_timeout` | int | 60 | Wait for unit to start (seconds) |
| `water_temp_timeout` | int | 600 | Wait for usable water temp (seconds) |
| `flow_confirm_threshold` | double | 2.0 | Flow-confirmation thermal delta |
| `flow_confirm_timeout` | int | 180 | Flow-confirmation timeout (seconds) |

---

## State Machines

Components use reference counting (`refs`): `refs > 0` ⇒ run, `refs == 0` ⇒ stop. Multiple consumers can hold refs on a shared pump.

**Unit states:** `STOPPED → START_PUMP → WAIT_PUMP → RESERVE → START → RUNNING → RELEASE → STOPPED` (plus `ERROR`).

**Fan states:** `STOPPED → RESERVE → START_PUMP → WAIT_PUMP → START → WAIT_START → RUNNING → RELEASE → STOPPED` (plus `ERROR`).

**Pump states:** `STOPPED → RESERVE → START_PRIMER → WAIT_PRIMER → START → WAIT_PUMP → FLOW → WARMUP → RUNNING → COOLDOWN → RELEASE → STOPPED` (plus `ERROR`).

**Direct states:** `STOPPED → STOP_FAN_PUMP → WAIT_FAN_PUMP → STOP_UNIT → WAIT_UNIT_STOP → PREPARE_UNIT → START_UNIT → WAIT_UNIT → WAIT_WATER_TEMP → RUNNING_OPEN → WAIT_TEMP → ACTIVATE_PIN2 → START_PRIMER → WAIT_PRIMER → RUNNING` (plus `ERROR`, `CONFIRM_FLOW`, `ACTIVATE_VALVE`).

**Charge states:** `STOPPED → START → WAIT_START → RUNNING → STOP`.

State strings are available via `unit_statestr()`, `fan_statestr()`, `pump_statestr()`, `direct_statestr()`.

> There is **no** `MIN_RUNTIME_WAIT` state in any component. Some stale comments in `run.js` still mention it, but no such state or logic exists.

---

## Mode Selection

`mode.js` selects heat vs. cool automatically from outside-temperature history (not from instantaneous water temp):

- `mode_auto()` calls `get_wt()` and sets `AC_MODE_HEAT` if the result `<= mode_threshold` (default 60), otherwise `AC_MODE_COOL`. An invalid reading defaults to COOL.
- `get_wt()` computes a day/night weighted outside-temperature average over the **previous 3 days**, entirely from InfluxDB:
  - Daytime segment (`day_start`→`night_start`): `mean(f)` from `outside_temp`, weighted by `day_weight` (default 40).
  - Nighttime segment (`night_start`→next `day_start`): `mean(temp)` from `ac`, weighted by `night_weight = 100 - day_weight` (default 60).
- `day_start` / `night_start` are timespec strings resolved via `get_date(location, spec)` and support `sunrise`/`sunset` offsets. When `location` is set, the still-default `"06:00"`/`"20:00"` are auto-switched to `"sunrise+30"`/`"sunset+120"` (and reverted when location is cleared).
- The mode is changed only if it differs from the current `ac.mode`. The registered entry point is `set_mode("auto"|"cool"|"heat")`, which also persists config.

---

## Thermal Storage Charging

`charge.js` drives units to charge the water tank toward a target temperature while solar power is available. Start/stop is on `water_temp` vs. the mode's thresholds (`charge_threshold`, default 3, sets the start band):

- **Cool mode:** start charging when `water_temp >= cool_low_temp + charge_threshold`; stop (charged) when `water_temp <= cool_low_temp`. The tank is cooled *down* toward `cool_low_temp` (35).
- **Heat mode:** start charging when `water_temp <= heat_high_temp - charge_threshold`; stop (charged) when `water_temp >= heat_high_temp`. The tank is heated *up* toward `heat_high_temp` (125).
- `cool_high_temp` (60) and `heat_low_temp` (100) are not start/stop points — they are the far endpoints of the **priority ramp**: cool priority scales with `(cool_high_temp - water_temp) / (cool_high_temp - cool_low_temp)`, heat with `(water_temp - heat_low_temp) / (heat_high_temp - heat_low_temp)`.
- `vapor_low_temp` (25.0) provides vapor-line freeze protection — a unit is stopped if its vapor temp drops below this.
- **Dynamic priority:** while charging, the unit's PA priority is re-evaluated as the tank temperature changes by `repri_interval` (default **1.0**°). If `pa_client_repri()` fails (PA/MQTT unreachable), the unit stops immediately — it must not run without PA awareness.

A unit currently `in_direct` is skipped by charge.js.

---

## Direct Mode

Direct mode (`direct.js`) bypasses the storage tank: it runs a heat-pump unit's water loop directly through an air handler's coil when the tank is depleted or in the wrong mode.

- A direct group binds one `fan` and one `unit` (deriving `fan_pump` from the fan and `ac_pump`/`primer` from the unit) plus two valve pins: `pin1` (stage 1, AC → air handler) and `pin2` (stage 2, loop closure).
- `direct_on(group [, mode])` saves the current `water_temp`, flags the unit `in_direct_pending`, and starts the state machine (driven each loop by `direct_main()`).
- **Critical start ordering:** the unit is first brought to RUNNING on its normal storage flow, a thermal delta is confirmed on the AC pump (`CONFIRM_FLOW`) *before* opening `pin1` (`ACTIVATE_VALVE`), then the loop is optionally closed (`pin2` + primer) and water temp is allowed to come up before the fan starts. This ordering prevents the AC pump from under-flowing the heat exchanger and tripping vapor-temp freeze protection.
- `direct_off(group [, stop_fan])` deactivates the valves, restores the saved `water_temp`, clears the `in_direct` flags, and hands the unit back to charge.js. With `stop_fan = false` the fan is transitioned back to storage mode by restarting its pump.
- Fans request direct mode (via `run.js`/`fan_set_mode`) when their mode mismatches `ac.mode` or the tank is depleted. `unit_force_stop`/`unit_revoke` call `direct_off` first so the fan doesn't error on pump loss. While a unit is `in_direct`, run.js skips its 300s flow check.

See `direct_mode.md` for the full design narrative.

---

## Power Integration (PA)

Components integrate with the [PA agent](../pa/README.md) via the `pa_client.js` library.

| Call | Purpose |
|------|---------|
| `pa_client_reserve(ac, module, item, amount, priority)` | Request power; component stays in RESERVE until approved |
| `pa_client_release(ac, module, item, amount)` | Release power when stopping |
| `pa_client_repri(ac, module, item, amount, new_priority)` | Change priority while running (used by charging) — **fail ⇒ stop** |

When PA needs power back it invokes the agent's revoke handlers (`unit_revoke`, `fan_revoke`) over MQTT:

- `immediate = false/0` — normal revoke (graceful stop).
- `immediate = true/1` — emergency revoke; `force_stop_*` stops immediately and (for units) calls `direct_off` first.

`module` is `"unit"`, `"fan"`, or `"pump"`; `item` is the component name.

---

## Configuration

### Global `ac` properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `interval` | int | 10 | Main loop interval (seconds) |
| `location` | string | "" | "lat,lon" for sunrise/sunset (triggers day/night defaults) |
| `standard` | int | 0 | 0 = US/Imperial, 1 = Metric |
| `mode` | int | 1 (COOL) | Current mode (`AC_MODE_*`) |
| `mode_threshold` | int | 60 | Heat below this weighted outside temp, else cool |
| `day_start` | string | "06:00" | Day segment start (timespec; sunrise/sunset offsets ok) |
| `night_start` | string | "20:00" | Night segment start (timespec) |
| `day_weight` | int | 40 | Daytime weight for `get_wt()` (night = 100 − this) |
| `water_temp` | double | -200.0 | Current tank water temp (private/runtime) |
| `cool_high_temp` | int | 60 | Start cooling above this (°) |
| `cool_low_temp` | int | 35 | Stop cooling below this (°) |
| `heat_high_temp` | int | 125 | Stop heating above this (°) |
| `heat_low_temp` | int | 100 | Start heating below this (°) |
| `charge_threshold` | int | 3 | Charge target band (°) |
| `vapor_low_temp` | double | 25.0 | Vapor-line freeze-protection limit (°) |
| `repri_interval` | float | 1.0 | Temp change that triggers PA reprioritize (°) |
| `sample_interval` | int | 0 | Temperature sampling interval (seconds; 0 = off) |
| `sample_duration` | int | 180 | Sampling run duration (seconds) |
| `temp_sensor` | string | "" | Cycle temperature sensor |
| `cycle_start` | float | 32.0 | Cycle start temperature |
| `cycle_interval` | int | 1800 | Cycle interval (seconds) |
| `cycle_duration` | int | 180 | Cycle run duration (seconds) |
| `can_target` | string | "can0" | CAN interface |
| `can_topts` | string | "500000" | CAN options (bit rate) |

> The previously documented `waterTempWeight`, `charge_cool_high_temp`, `charge_heat_low_temp`, and `pump_failsafe_enable` properties **do not exist** in the code.

### Configuration commands

`sdconfig <agent> <verb> [args...]` either reads/writes a property (`get`/`set`) or invokes one of the agent's registered functions (see [Registered Functions](#registered-functions)). Function arguments are passed as strings; the `opts` argument of the `add_*`/`set_*` functions is a colon-delimited list of `key=value` pairs (parsed by `jconfig_set_opts`), **not** JSON.

```bash
# List / get / set properties
sdconfig -n ac -L                      # list properties (use -F to list functions)
sdconfig ac get cool_high_temp
sdconfig ac set cool_high_temp 65

# Add components: add_<type> <name> "key=value:key=value..."
sdconfig ac add_pump ac1 "pin=17:temp_in_sensor=ac1_in"
sdconfig ac add_unit ac1 "pump=ac1:coolpin=22:reserve=6500"
sdconfig ac add_fan  fan1 "topic=solar/hvac/fan1/data:pump=ac1"

# Control components: <verb> <name>
sdconfig ac start_unit ac1
sdconfig ac stop_unit ac1
sdconfig ac enable_unit ac1
sdconfig ac set_mode auto
```

> There is **no** way to run arbitrary JavaScript through sdconfig or MQTT — the remote `exec` config function was intentionally removed (commented out in `lib/sd/agent.c`) as a security hole. To run ad-hoc JS you need shell access on the host: use the `sdjs` REPL, or the agent binary's local-only `-e '<js>'` / `-X <script.js>` flags.

Test configs: `test.json`, `actest_units.json`, `actest_fans.json`, `actest_pumps.json`.

---

## Registered Functions

All registered via `config.add_funcs(ac, ...)`; callable as `sdconfig ac <func> [args...]`.

**Units:** `add_unit`, `del_unit`, `set_unit`, `get_unit`, `get_unit_config`, `set_unit_config`, `start_unit`, `stop_unit`, `force_stop_unit`, `disable_unit`, `enable_unit`

**Fans:** `add_fan`, `del_fan`, `set_fan`, `get_fan`, `get_fan_config`, `set_fan_config`, `start_fan`, `stop_fan`, `force_stop_fan`, `disable_fan`, `enable_fan`

**Pumps:** `add_pump`, `del_pump`, `get_pump`, `set_pump`, `get_pump_config`, `set_pump_config`, `start_pump`, `stop_pump`, `force_stop_pump`, `disable_pump`, `enable_pump` (`del_pump` refuses if the pump is in use)

**Mode:** `set_mode` (`"auto"`/`"cool"`/`"heat"`)

**Direct:** `direct_set`, `direct_get_config`, `direct_set_config`, `direct_on`, `direct_off`, `direct_enable`, `direct_disable`, `direct_get_pump`, `direct_is_pending`, `direct_is_active`, `direct_is_error`, `dg_valve1_on`, `dg_valve1_off`, `dg_valve2_on`, `dg_valve2_off`, `dg_valves_off`

**Errors:** `clear_error` (name or `"all"`)

**C functions** (`jsfuncs.c`, on the `ac` object): `can_read(id [, wait])`, `can_write(id, data[])`, `signal(module, action)`

---

## Communication

### MQTT

- **Publishes:** agent data on the standard `solar/agents/ac/...` topics (state, temperatures, mode); per-component data via InfluxDB.
- **Subscribes:** each fan's configured `topic` for air-handler state, plus PA command topics via sdconfig.

### CAN bus

- ID range `0x450`–`0x45F` (`CAN_START`/`CAN_END` in `can.c`), masked with `0x1FFFFFFF`.
- A background receive thread caches the latest frame per ID with a timestamp; reads treat data older than **5 seconds** as stale.
- Interface via `can_target` (default `can0`), bit rate via `can_topts` (default `500000`).
- `utils.js can_get_sensor()` decodes a 2-byte big-endian value from a CAN frame and divides by 10.

### InfluxDB

Measurements include `ac` (temps/mode/state), `fans`, `pumps`, `units`, and `errors`. See `influxdb_hvac.md`.

---

## Development

### Building

```bash
cd agents/ac
make
make install        # installs to PREFIX (/opt/sd or ~/)
```

### Testing

```bash
# Virtual CAN
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# Run with a test config (use -i to ignore wpi_init errors off-hardware)
./ac -c test.json -v -d -i

# JS test suite
sdjs -f test/harness.js        # see test/ for per-module tests

# Monitor
mosquitto_sub -t 'solar/agents/ac/#' -v
candump vcan0
```

### Debug levels

`-d 0` errors only · `-d 1` +warnings · `-d 2` +info · `-d 3` +debug · `-d 4` +verbose.

### Service / logs

The AC agent runs on the `ac` host. Its log is `/opt/sd/log/ac.log` (not journalctl). To clear errors without restarting the service:

```bash
sdconfig ac clear_error all
```

### Inspecting state

`get_unit`/`get_pump`/`get_fan` return the component object (including its numeric `state`):

```bash
sdconfig ac get_unit ac1
sdconfig ac get_pump ac1
sdconfig ac get_fan  fan1
```

To map a numeric state to its name (`unit_statestr` etc.) or iterate all components, run JS on the host with the `sdjs` REPL (or the agent's local-only `-e`/`-X` flags) — remote JS execution is intentionally disabled.

---

## See Also

- `direct_mode.md` — full direct-mode design and flow ordering
- `state.md` — state-machine reference
- `influxdb_hvac.md` — InfluxDB schema and queries
- [`../pa/README.md`](../pa/README.md) — Power Agent it reserves against
- `../../lib/sd/pa_client.js` — PA client library
- `../../CLAUDE.md` — overall system architecture
