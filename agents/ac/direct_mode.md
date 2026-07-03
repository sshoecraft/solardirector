# Direct Mode Implementation

## Overview

Direct mode plumbs an AC unit **straight to the air handler in a closed loop that bypasses
the thermal-storage tank**, so the house stays conditioned when the tank can't serve a
fan's request. It engages when:
1. The fan requests a mode that doesn't match storage mode (mode mismatch), or
2. The storage tank's water temperature is exhausted for the requested mode (depleted).

**Operating concept:** by day the solar system runs the AC unit to heat/cool (charge) the
storage tank; after dark the air handler (AH) pulls that stored thermal energy from the
tank to condition the house; when the tank is exhausted, the valves switch to the **direct
closed loop** between the AC and the AH — real-time conditioning with the tank bypassed.

## Architecture

### Hardware

- AC unit → **brazed-plate heat exchanger (BPHX)**; a **½ HP AC pump** pushes water through it.
- Air handler (AH), water-to-air heat exchanger in the house; a **¾ HP AH pump** pushes water through it.

### Water Flow

**Storage (valves at rest) — two independent loops through the tank:**
```
AC:  tank → AC-pump-in → AC pump → BPHX → BPHX-out → tank    (AC charges the tank)
AH:  tank → AH-pump-in → AH pump → AH coil → AH-out → tank   (AH conditions the house)
```

**Direct (both valve sets flipped, tank fully bypassed) — one closed loop, BOTH pumps in series:**
```
AC pump → BPHX → [valve1: BPHX-out → AH-in] → AH pump → AH coil → [valve2: AH-out → AC-in] → AC pump
```
Both the ½ HP AC pump and the ¾ HP AH pump run **at the same time**, in series around the
closed loop. An idle AH pump would choke the AC pump's discharge → BPHX under-flow →
vapor-temp freeze trip, so both pumps are always RUNNING before any valve flips. The two
valve sets fire in two stages (supply, then return — see State Machine) so only two valves
travel at a time.

### Physical Valve Setup

**Stage 1 valve set (valve1_power / valve1_select):**
- Valve 1: AC pump output - switches from "to tank" → "to AH input"
- Valve 2: AH input - switches from "from tank" → "from AC output"

**Stage 2 valve set (valve2_power / valve2_select):**
- Valve 3: AH output - switches from "to tank" → "to AC input"
- Valve 4: AC input - switches from "from tank" → "from AH output"

Both valves in a stage are wired together and driven by **two relays off a 24V bus**:
- **pwr relay (relay1)**: COMM=+24V, NO feeds the dir relay's COMM. Gates power.
- **dir relay (relay2)**: COMM=+24V (from relay1), NC=storage, NO=direct. Selects direction.

To move a stage: set the dir relay first, then energize the pwr relay; power is held
`valve_time` seconds (default 10) while the valve travels, then cut. Rest state = both
relays de-energized (storage selected); the valve physically holds its last position when
unpowered. On startup/error/off the valves are driven to storage.

### Key Constraints

- 3-way valves connect AC circuit directly to fan circuit
- Both the AC pump and the AH (fan) pump run **at the same time**, in series around the closed loop — the AH pump is NOT bypassed
- The AH pump must be RUNNING before the supply valve (valve1) flips, or it chokes the AC discharge (BPHX under-flow → freeze trip)
- The AH pump is marked `pump.direct = true` while under direct control so cycle.js/sample.js leave it alone (it is running, not stopped)
- Unit is marked with `unit.in_direct = true` to prevent charge.js from controlling it
- `unit.in_direct_pending = true` blocks charge.js during state machine transition
- Note: `unit.direct` (config property) vs `unit.in_direct` (runtime flag) - these are different!
- Only ONE fan can use direct mode at a time per AC unit
- When fan stops, unit is handed off to charge.js for continued storage charging
- Fan circuit temp_in = AC pump temp_out (sensor remapping)

### Water Temperature Triggering

Direct mode activates when water temperature reaches storage limits:
- **Cooling**: Activates when `water_temp >= cool_high_temp`
- **Heating**: Activates when `water_temp <= heat_low_temp`

**Important**: Direct mode uses exact thresholds (no wt_thresh offset). The `stop_wt`
property is ignored for fans with a direct_group configured - direct mode handles water
temperature limits instead of stopping the fan.

### Mode Mismatch Triggering

Direct mode also activates when:
- Storage is in HEAT mode but fan requests COOL (or vice versa)
- Unit will be switched to the requested mode in direct mode
- This allows cooling during winter or heating during summer without changing storage mode

## State Machine

### States

| State | Value | Description |
|-------|-------|-------------|
| Stopped | 0 | Not active |
| Start fan pump | 1 | Start the AH (fan) pump — BOTH pumps run in direct |
| Wait fan pump | 2 | Wait for the AH pump to reach RUNNING before any valve flips |
| Stop unit | 3 | Stop a RUNNING unit that's in the wrong mode (mode mismatch) |
| Wait unit stop | 4 | Wait for the unit to reach STOPPED |
| Prepare unit | 5 | Set unit mode + in_direct flags; dispatch by unit state |
| Start unit | 6 | Start the AC unit |
| Wait unit | 7 | Wait for the unit to reach RUNNING (Reserve-timeout retry) |
| Wait water temp | 8 | Gate the blower on supply water reaching target (fan pump already running) |
| Running open | 9 | Stage 1 (supply) active — monitor AH temp change before closing |
| Wait temp | 10 | (reserved / unused) |
| Activate pin2 | 11 | Flip valve2 (return) to close the loop |
| Start primer | 12 | Start primer to bleed air from the closed loop |
| Wait primer | 13 | Wait for primer to run (primer_time seconds) |
| Running | 14 | Fully closed loop active, blower on |
| Error | 15 | Error occurred |
| Confirm flow | 16 | Verify ac_pump temp_in/out delta (BPHX has flow) before starting the AH pump |
| Activate valve | 17 | Flip valve1 (supply); reached only after unit RUNNING + flow confirmed + AH pump RUNNING |

State values are runtime enums (not persisted). CONFIRM_FLOW (16) and ACTIVATE_VALVE (17)
were appended after the original 0–15 set, so they read out of numeric sequence but run in
the order shown below.

### Two-Stage Activation Sequence

Both pumps run throughout. The AC unit comes up and flow is confirmed, the AH pump is
started, THEN the valves flip — supply first, then return — so only two valves travel at a
time and both pumps are always moving water before any valve moves.

1. **Stage 1 — valve1 (supply)**: connect `BPHX out → AH pump in`
   - valve2 still at storage, so the return goes AH → tank (open series loop through the tank)
   - Monitor AH pump temp_out for a change ≥ `temp_threshold` (confirms conditioned water reached the AH)

2. **Stage 2 — valve2 (return)**: connect `AH out → AC pump in`, closing the loop (tank bypassed)
   - Run primer for `primer_time` seconds to bleed trapped air from the now-sealed loop
   - Air-relief valve at the loop high point vents air

### State Transitions

```
STOPPED --(direct_on)--> PREPARE_UNIT
                           |- unit RUNNING wrong mode --> STOP_UNIT --> WAIT_UNIT_STOP --,
                           |                                                             |
                           |<------------------------------------------------------------'
                           |- unit STOPPED --> START_UNIT --> WAIT_UNIT --,
                           |- unit already RUNNING (correct mode) --------|
                           |                                              v
                           '----------------------------------->  CONFIRM_FLOW   (BPHX flow, storage loop)
                                                                       v
                                     START_FAN_PUMP --> WAIT_FAN_PUMP   (AH pump RUNNING -- both pumps now run)
                                                                       v
                                     ACTIVATE_VALVE (valve1/supply)     (open series loop through tank)
                                                                       v
                                     RUNNING_OPEN   (monitor AH temp change)
                                                                       v
                                     ACTIVATE_PIN2 (valve2/return) --> START_PRIMER --> WAIT_PRIMER   (close loop, bleed air)
                                                                       v
                                     WAIT_WATER_TEMP   (gate blower on supply temp; skipped if blower already on)
                                                                       v
                                     (water at temp) --> start blower --> RUNNING

RUNNING --(direct_off)--> STOPPED
   - releases DIRECT's AH-pump ref (force-stop) only if DIRECT owns it; else the fan's own
     stop path releases it. Never re-starts the (already running) AH pump.
   - mode mismatch: stop unit, restore storage mode, hand off stopped to charge.js
   - mode matches:  hand off running unit to charge.js with repri
   - RUNNING monitors the unit AND both pumps -- loss of any tears down.
```

## Configuration

File: `/opt/sd/etc/ac_directs.json`

```json
{
    "dg1": {
        "enabled": true,
        "valve1_power": 2,
        "valve1_select": 3,
        "valve2_power": 28,
        "valve2_select": 29,
        "valve_time": 10,
        "fan": "ah1",
        "unit": "ac1",
        "primer_time": 30,
        "temp_threshold": 2.0
    }
}
```

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| enabled | bool | true | Enable/disable this direct group |
| valve1_power | int | -1 | GPIO pin for stage 1 power relay (relay1, +24V gate) |
| valve1_select | int | -1 | GPIO pin for stage 1 direction relay (relay2, NC=storage/NO=direct) |
| valve2_power | int | -1 | GPIO pin for stage 2 power relay (-1 = no stage 2) |
| valve2_select | int | -1 | GPIO pin for stage 2 direction relay |
| valve_time | int | 10 | Seconds power is applied per valve move |
| fan | string | "" | Name of fan this group controls |
| unit | string | "" | Name of AC unit for this group |
| primer_time | int | 30 | Seconds to run primer after activating pin2 |
| temp_threshold | double | 2.0 | Temperature change (°F) to trigger stage 2 |
| unit_timeout | int | 60 | Seconds to wait for unit to reach RUNNING before aborting |
| water_temp_timeout | int | 600 | Seconds to wait for water to reach temp threshold before starting fan anyway |

**Derived at init** (from fan/unit config):
- `dg.fan_pump`: Fan's pump name
- `dg.ac_pump`: Unit's pump name
- `dg.primer`: AC pump's primer (from pumps[ac_pump].primer)

**Runtime properties** (set during init):
- `fan.direct_group`: Links fan to its direct group
- `unit.in_direct_group`: Links unit to its direct group

**Runtime state** (managed by state machine):
- `dg.state`: Current state (0-17)
- `dg.active`: true when direct mode is active (stage 1 or 2)
- `dg.pending_fan`: Fan waiting for direct mode to start
- `dg.active_fan`: Fan currently using direct mode
- `dg.target_mode`: Requested mode (FAN_MODE_COOL/HEAT)
- `dg.mode`: Current active mode
- `dg.initial_temp`: Fan pump temp_out when entering RUNNING_OPEN
- `dg.saved_water_temp`: ac.water_temp saved at entry (restored on teardown so charge.js sees the real tank temp)
- `dg.primer_start_time`: Time when primer was started
- `dg.unit_start_time`: Time when unit_start was called (for timeout)
- `dg.fan_pump_start_time`: Time the AH-pump start began (WAIT_FAN_PUMP reserve timeout)
- `dg.confirm_flow_start`: Time CONFIRM_FLOW was entered (for timeout)
- `dg.water_wait_time`: Time when WAIT_WATER_TEMP state was entered (for timeout)
- `dg.direct_fan_pump_ref`: true when DIRECT owns the AH-pump ref (scenario 1 fresh start); false when the fan owns it
- `dg.in_teardown`: re-entry guard for direct_off → fan_stop → fan_off → direct_off

**Note**: For fans in a direct group, `stop_wt` is ignored. Direct mode handles water
temperature limits using exact thresholds (cool_high_temp / heat_low_temp) instead of
stopping the fan.

## Manual Control

Direct mode can be manually controlled via sdconfig:

```bash
# Create / delete a direct group (opts are colon-separated key=value pairs)
sdconfig ac add_direct dg1 "valve1_power=2:valve1_select=3:valve2_power=28:valve2_select=29:valve_time=10:fan=ah1:unit=ac1:primer_time=30:temp_threshold=2.0"
sdconfig ac del_direct dg1

# Modify an existing group
sdconfig ac direct_set dg1 "temp_threshold=2.5:primer_time=45"
sdconfig ac direct_get_config dg1      # "all" dumps every group

# Turn on/off direct mode (starts/stops state machine)
sdconfig ac direct_on dg1              # uses current storage mode
sdconfig ac direct_on dg1 heat         # force HEAT mode
sdconfig ac direct_on dg1 cool         # force COOL mode
sdconfig ac direct_off dg1

# Enable/disable direct group (allows/prevents it from being turned on)
sdconfig ac direct_enable dg1          # allows direct mode to be used
sdconfig ac direct_disable dg1         # prevents direct mode (turns off if active)
```

### Valve Control (low-level)
```bash
sdconfig ac dg_valve1_on dg1           # move stage 1 valve set to DIRECT
sdconfig ac dg_valve1_off dg1          # move stage 1 valve set to STORAGE
sdconfig ac dg_valve2_on dg1           # move stage 2 valve set to DIRECT
sdconfig ac dg_valve2_off dg1          # move stage 2 valve set to STORAGE
sdconfig ac dg_valves_off dg1          # drive both valve sets to STORAGE
```
Each command starts a move: the direction relay is set, the power relay is energized,
and power is cut automatically ~`valve_time` seconds later by the 1s `direct_main` loop.

## Files Modified

### direct.js
- State machine implementation with two-stage activation
- `direct_on()`, `direct_off()`: Turn direct mode on/off (starts/stops state machine)
- `direct_enable()`, `direct_disable()`: Enable/disable direct group
- `direct_main()`: Process all direct groups each loop
- `direct_is_pending()`, `direct_is_active()`, `direct_is_error()`: State query helpers
- `direct_get_pump()`: Get AC pump name for a direct group
- `dg_valve1_on()`, `dg_valve1_off()`: Stage 1 valve set (direct/storage)
- `dg_valve2_on()`, `dg_valve2_off()`: Stage 2 valve set (direct/storage)
- `dg_valve_move()`: Two-relay move helper (set dir relay, energize pwr relay, stamp time)
- `dg_valve_power_check()`: Cut valve power `valve_time` secs after a move (called from direct_main, ~1s)
- Sensor remapping, priority inheritance, valve control, primer management

### fan.js
- `fan_set_mode()`: Triggers `direct_on()` when mode mismatch or water depleted
- `fan_off()`: Calls `direct_off()` when fan stops

### unit.js
- `unit_force_stop()`: Calls `direct_off()` if unit is in direct mode
- `unit_mode()`: Prevents mode change while `unit.in_direct = true`
- `unit_revoke()`: Turns off direct mode before stopping unit

### run.js
- `FAN_STATE_STOPPED`: Checks direct mode BEFORE water temp check, skips pump startup if in direct mode
- `FAN_STATE_RESERVE`: Checks direct mode, skips pump startup if active
- `FAN_STATE_RUNNING`: Monitors AC pump instead of fan pump when in direct mode; triggers direct mode if water temp exceeds limits
- `UNIT_STATE_RUNNING`: Stops unit when storage reaches target temperature; if in direct mode, calls `direct_off(group, false)` to transition fan to storage mode
- Calls `direct_main()` at the end of `run_main` (~1s cadence) to step the direct state machine and drive the timed valve power-cut

### read.js
- No longer calls `direct_main()` (moved to run.js). Still runs cycle.js, sample.js, charge.js at the read cadence (`interval`, ~10s)

### pub.js
- Shows "Running" instead of "Temp" when fan is in direct mode
- Publishes direct group state to InfluxDB (`directs` measurement)

### start.js
- Resets all direct groups on startup (all valves OFF, states cleared)

### cycle.js
- Skips pumps with `pump.direct = true` (freeze protection cycling)

### sample.js
- Skips pumps with `pump.direct = true` (periodic temperature sampling)

### charge.js
- Skips units with `unit.in_direct = true` or `unit.in_direct_pending = true`

## Key Behaviors

### Power Management
- Unit inherits fan's priority during direct mode (`unit.direct_priority = fan.priority`)
- On fan stop, unit priority restored and handed to charge.js with repri
- If mode mismatch on handoff, unit is stopped first, then restarted by charge.js

### Unit Control Flags
- `unit.in_direct = true`: Prevents charge.js from controlling unit (set after unit_mode succeeds)
- `unit.in_direct_pending = true`: Blocks charge.js during transition (set in direct_on, cleared on success/error)
- `unit.in_direct_priority`: Unit's priority during direct mode (inherited from requesting fan)
- `unit.in_direct_group`: Links unit to its direct group (runtime only)
- Flags cleared on `direct_off()`

**Important**: The config property `unit.direct` (from ac_units.json) indicates the unit supports direct mode. The runtime flag `unit.in_direct` indicates the unit is currently IN direct mode. These are different properties!

### Pump Control Flags
- `pump.direct = true`: Prevents cycle.js and sample.js from starting pump
- Set by `dg_valve1_on()`, cleared by `dg_valve1_off()`

### Storage Temperature Monitoring
- **Unit shutdown on target reached**: When unit is RUNNING and water_temp reaches target (cool_low_temp - 0.5 for cooling, heat_high_temp + 0.5 for heating), unit stops automatically. If in direct mode, fan transitions to storage mode (keeps running).
- **Fan direct mode trigger**: When fan is RUNNING and water_temp exceeds limits (cool_high_temp for cooling, heat_low_temp for heating), direct mode activates if available

### Two-Stage Temperature Monitoring
- In RUNNING_OPEN state, monitors `fan_pump.temp_out` for change
- When temp changes by >= `temp_threshold` from initial reading, triggers stage 2
- Stage 2 closes the loop and runs primer to bleed air

### Startup Reset
- All valve relays driven LOW, then both valve sets pulsed to STORAGE (known position)
- All direct group states reset to STOPPED
- All flags (`dg.active`, `dg.pending_fan`, `dg.active_fan`) cleared

## Exposed Functions

Functions available via sdconfig:

| Function | Args | Description |
|----------|------|-------------|
| `add_direct` | name, opts | Create a new direct group (opts: colon-separated key=value). Runs `_direct_init` and saves to ac_directs.json. Fails if name exists or referenced fan/unit don't exist. |
| `del_direct` | name | Delete a direct group (turns off direct mode/valves and clears fan/unit links first), saves to ac_directs.json |
| `direct_set` | name, opts | Modify an EXISTING direct group (opts: colon-separated key=value). Will not create one. |
| `direct_get_config` | name | Dump a group's config ("all" for every group) |
| `direct_on` | group_name, [mode] | Turn on direct mode (mode: "heat" or "cool", default: current ac.mode) |
| `direct_off` | group_name, [stop_fan] | Turn off direct mode (stop_fan: true=stop fan (default), false=keep fan running on storage) |
| `direct_enable` | group_name | Enable direct group (allow it to be turned on) |
| `direct_disable` | group_name | Disable direct group (turn off if active, prevent future activation) |
| `dg_valve1_on` | group_name | Move stage 1 valve set to DIRECT |
| `dg_valve1_off` | group_name | Move stage 1 valve set to STORAGE |
| `dg_valve2_on` | group_name | Move stage 2 valve set to DIRECT |
| `dg_valve2_off` | group_name | Move stage 2 valve set to STORAGE |
| `dg_valves_off` | group_name | Drive both valve sets to STORAGE |

Internal helper functions (not exposed via sdconfig):

| Function | Args | Description |
|----------|------|-------------|
| `direct_get_pump` | group_name | Returns AC pump name for the group |
| `direct_is_pending` | group_name, fan_name | Check if direct mode is pending for fan |
| `direct_is_active` | group_name, fan_name | Check if direct mode is active for fan |
| `direct_is_error` | group_name | Check if direct mode is in error state |
| `direct_main` | none | Process all direct groups + valve power-cut (called from run.js, ~1s) |

## InfluxDB

Measurement: `directs`

| Field | Type | Description |
|-------|------|-------------|
| name | string | Direct group name |
| enabled | int | 1 if enabled, 0 if disabled |
| state | string | Current state name |
| active | int | 1 if direct mode is active |
| valve1 | string | Supply valve set position: `Direct` / `Storage` / `None` (derived from `dg.state`, not the relay pins) |
| valve2 | string | Return valve set position: `Direct` / `Storage` / `None` (derived from `dg.state`) |
| fan | string | Active or pending fan name |
| unit | string | Associated unit name |
| mode | string | Current mode (None/Cool/Heat) |

## Bug Fixes (2026-02-19)

### Property Naming Conflict
**Problem**: `unit.direct` was used as both a config property (from ac_units.json, meaning "unit supports direct mode") and a runtime flag (meaning "unit is currently in direct mode"). This caused `unit_mode()` to reject mode changes because it saw the config property as `true`.

**Fix**: Renamed runtime flags to avoid conflict:
- `unit.direct` → `unit.in_direct`
- `unit.direct_pending` → `unit.in_direct_pending`
- `unit.direct_priority` → `unit.in_direct_priority`
- `unit.direct_group` → `unit.in_direct_group`

### Priority Inheritance
**Problem**: In ACTIVATE_VALVE state, `fans[dg.fan]` was used to get the fan's priority, but `dg.fan` is the config default fan, not the requesting fan. This caused the unit to reserve with wrong priority.

**Fix**: Changed to `fans[dg.pending_fan]` to get the actual requesting fan's priority.

### Undefined fan_pump in RUNNING_OPEN
**Problem**: In `_direct_process()` RUNNING_OPEN state, `fan_pump` was referenced but not defined in that scope, causing TypeError.

**Fix**: Added `let fan_pump = pumps[dg.fan_pump];` before using it.

### Reserve Value Cleared on Handoff
**Problem**: When direct mode ended, `unit.reserve = 0` was set after releasing the PA reservation. This cleared the config value (e.g., 6500W), causing all future unit starts to skip PA reservation entirely.

**Fix**: Removed `unit.reserve = 0` line. The `pa_client_release()` call releases the reservation; the config value should remain intact.

## Bug Fixes (2026-02-23)

### Unit Start Timeout in WAIT_UNIT
**Problem**: When direct mode tried to start a unit but PA denied the power reservation (e.g., on battery with 6500W request exceeding battery limit), the direct state machine hung indefinitely in DIRECT_STATE_WAIT_UNIT. Valves were open, fan pump was stopped, and no water was flowing through the air handler.

**Fix**: Added configurable `unit_timeout` (default 60 seconds) to direct group config. In WAIT_UNIT state, if the unit hasn't reached RUNNING within the timeout, direct mode errors out: stops the unit, turns off valves, clears flags, and sets ERROR state. The fan then detects the error and stops.

### Unit Not Stopped During ACTIVATE_VALVE
**Problem**: When direct mode entered ACTIVATE_VALVE with the unit not stopped (running in wrong mode, or in a transition state like Reserve from charge.js restarting), `unit_mode()` rejected the mode change or `unit_start()` conflicted with the in-progress startup, causing timeouts. For pre-RUNNING states (Reserve, Start pump, etc.), `unit_stop` via `common_stop` did nothing when refs was already 0 (e.g., after hot-reload), leaving the unit stuck.

**Fix**: ACTIVATE_VALVE now handles three cases:
- **RUNNING in correct mode**: proceed (existing behavior)
- **RUNNING in wrong mode**: STOP_UNIT → WAIT_UNIT_STOP (proper cooldown, compressor is on)
- **Pre-RUNNING state** (Reserve, Start pump, Wait pump, Start): abort startup directly - release PA reservation, stop pump, set refs=0, set state=STOPPED, then proceed with valve activation. Safe because compressor isn't on.

### unit.direct vs unit.in_direct Confusion
**Problem**: Several places checked `unit.direct` (config property meaning "supports direct mode") instead of `unit.in_direct` (runtime flag meaning "currently in direct mode"):
- `unit_revoke()`: caused unnecessary `direct_off()` calls on every revoke
- `unit_force_stop()`: same issue on force stop
- `run.js` UNIT_STATE_RUNNING storage-reached check: called `direct_off()` on every storage temp shutdown

**Fix**: Changed all three to check `unit.in_direct` instead of `unit.direct`.

### unit_mode Fails During WAIT_UNIT Timeout Cleanup
**Problem**: When the WAIT_UNIT timeout fired (PA wouldn't grant power), `unit_stop` was called while `unit.in_direct` was still true. `unit_stop` → `unit_off` → `unit_mode(name,unit,ac.mode)` → rejected because `unit.in_direct` check prevents mode changes during direct mode.

**Fix**: Clear `unit.in_direct`, `unit.in_direct_pending`, and `unit.in_direct_priority` BEFORE calling `unit_stop` in the timeout path, so `unit_off` → `unit_mode` can set the mode back to storage mode.

### False Charge After Mode-Mismatch Direct Mode
**Problem**: When direct mode ran in Cool (storage in Heat), the AC pump circulated cold water. When direct mode ended and mode switched back to Heat, charge.js saw the residual cold water temp (e.g., 46°F) and thought the tank was way below target. It started the unit at priority 1, ran the pump, got the real tank reading (~109°F), realized the tank was fine, and stopped. This wasteful cycle happened every time direct mode ended with a mode mismatch.

**Fix**: Save `ac.water_temp` when direct mode activates (`dg.saved_water_temp`). On mode-mismatch shutdown, restore the saved water temp so charge.js sees the actual pre-direct tank temperature. Also reset `pump.settled = false` on the AC pump so charge.js waits for real readings before making priority decisions. `charge_get_pri()` uses the restored temp for the initial priority calculation.

### Stale Property References from Rename
**Problem**: The 2026-02-19 rename of `unit.direct_group` → `unit.in_direct_group` and `unit.direct_pending` → `unit.in_direct_pending` missed several references:
- `unit.js` `unit_force_stop()`: used `unit.direct_group` (never matched, direct_off never called on force stop)
- `unit.js` `unit_revoke()`: used `unit.direct_group` (never matched, direct_off never called on revoke)
- `unit.js` `_unit_init()`: initialized `unit.direct_pending` instead of `unit.in_direct_pending`
- `run.js` UNIT_STATE_RUNNING: used `unit.direct_group` (never matched, storage-reached handoff never triggered)
- `charge.js` `charge_main()`: checked `unit.direct_pending` instead of `unit.in_direct_pending` (charge.js never blocked during direct transitions)

**Fix**: Updated all references to use the correct renamed property names.

## Bug Fixes (2026-02-28)

### Reversing Valve Left in Wrong Position After Aborted Direct Mode
**Problem**: When direct mode was requested (e.g., HEAT for house heating) but aborted before reaching RUNNING (e.g., fan stopped within seconds), the reversing valve was left in the direct mode's position (HEAT). The `direct_off` function only restored the unit mode when `dg.active == true`, but `dg.active` is set in WAIT_UNIT when the unit reaches RUNNING. Since `unit_mode()` is called in the ACTIVATE_VALVE state (before RUNNING), there was a window where the mode/rvpin was changed but `dg.active` was still false. When charge.js later started the unit for storage cooling, `unit_on` set the coolpin but didn't touch the rvpin (only done when `rvevery=true`), so the compressor ran with the reversing valve still in HEAT position, heating the storage instead of cooling it.

**Timeline**:
1. 06:53 - Direct mode requested in HEAT, unit_mode set rvpin to HEAT in ACTIVATE_VALVE
2. 06:53 (+10s) - Direct mode aborted (fan stopped), dg.active was false, mode not restored
3. 10:12 - Charge started unit in COOL mode, coolpin activated but rvpin still in HEAT
4. 10:12-10:48 - Storage heated from 78.4°F to 82.2°F (wrong direction)

**Fix (direct.js)**: Added `else` clause to the `if (dg.active)` block in `direct_off`. When not fully active but target_mode was set and unit mode doesn't match storage mode, restore the unit mode to storage mode (calling `unit_mode()` to set rvpin correctly).

**Fix (unit.js)**: Added defense-in-depth to `unit_on` - always set rvpin to match the current mode before activating the compressor, regardless of `rvevery` setting. This ensures the reversing valve is correct even if something else left it in the wrong position.

### PA Reservation Timeout During Direct Mode ACTIVATE_VALVE
**Problem**: When direct mode triggered and found the unit in a pre-RUNNING state (e.g., Reserve or Wait pump from charge.js), ACTIVATE_VALVE released the PA reservation, stopped the pump, set state to STOPPED, then tried to start the unit fresh via START_UNIT. But PA has a 180-second settle time between grants. The new `pa_client_reserve` had to wait 180 seconds, while `unit_timeout` is only 60 seconds. Direct mode always timed out, went to Error, and the fan went to Error/disabled. Repeating the attempt hit the same 180s window, failing again.

**Timeline**:
1. 16:28:22 - PA granted 6500W for unit (charge.js starting it)
2. 16:28:44 - ACTIVATE_VALVE released PA reservation (abort pre-RUNNING startup)
3. 16:28:57 - unit_start re-requested 6500W from PA
4. 16:29:59 - Timeout after 62s (PA settle timer: 180s). Direct mode Error.
5. 16:30:16 - Retry, same result. Fan disabled, no AC.

**Fix (direct.js ACTIVATE_VALVE)**: When unit is in pre-RUNNING state, hijack the in-progress startup instead of aborting it. Keep the existing PA reservation, just take ownership from charge.js (`unit.charging = false`). Skip START_UNIT and go directly to WAIT_UNIT since the unit is already progressing through its startup state machine. This avoids the release-then-re-reserve cycle that triggers PA's 180s settle timer.

**Fix (unit.js unit_on)**: When `unit.in_direct` is true, don't override `unit.mode` with `ac.mode`. Direct mode's `unit_mode()` call already set the correct mode and rvpin during ACTIVATE_VALVE. Without this fix, `unit_on` would reset the mode back to storage mode when the compressor starts, defeating the direct mode's mode switch (e.g., unit would heat instead of cool in a mode-mismatch direct scenario).

## Bug Fixes (2026-03-12)

### Flow Check Evaluated Wrong Mode (run.js)
**Problem**: The flow check at run.js:676 used `ac.mode` (storage mode) instead of `unit.mode` (the unit's actual current mode) to determine delta direction. When direct mode ran Heat on a Cool storage, the flow check compared delta as if cooling (temp_in - temp_out) when it should have compared as heating (temp_out - temp_in). This produced negative deltas that always failed the threshold, triggering false "no flow" errors.

**Fix**: Changed `ac.mode` → `unit.mode` in the flow check delta calculation (run.js lines 676, 678, 680).

### Flow Check Dynamic Threshold Failures (run.js)
**Problem**: The flow check threshold formula (`Math.abs(temp_in) * 0.06`) produced thresholds that were either too high or too low depending on the operating range. Multiple dynamic approaches were tried and abandoned:
- `abs(temp_in) * 0.06` — threshold 6.5 at 107°F, too high for heat mode
- Distance from cool_high/heat_low * 0.06 — too low (heat range only 10°F)
- Normalized distance with various factors (0.15-0.25) — edge case failures
- Normalized with floor 2.5 / cap 5.0 — failed at 102.9°F (threshold 5.0, delta 4.7)

**Fix**: Replaced with static `threshold = 2.8`. Observed deltas ranged 4.7-10.7°F across all operating conditions, so 2.8 provides reliable detection without false positives.

### Water Temperature Gate Before Fan Start (direct.js)
**Problem**: In direct mode with a closed loop (pin2), the fan started immediately after the primer finished. But the water in the closed loop hadn't yet reached operating temperature, so the air handler blew lukewarm air until the water caught up. Additionally, if placed before stage 2 (initially attempted), the water couldn't reach temp in open loop because the storage tank diluted it.

**Fix**: Added `DIRECT_STATE_WAIT_WATER_TEMP` (state 8) after WAIT_PRIMER, before RUNNING. All subsequent states were renumbered (RUNNING_OPEN=9 through ERROR=15). Behavior:
- **Heat mode**: Waits until AC pump temp_out >= `heat_low_temp` (105°F)
- **Cool mode**: Waits until AC pump temp_out <= `cool_high_temp` (55°F)
- **Timeout**: Configurable `water_temp_timeout` (default 600s), starts fan anyway with warning
- **No-pin2 configs**: Skips water temp gate entirely (open loop can't reach threshold)
- **Logging**: Target logged on entry, progress every ~60 seconds
- Resets `unit.run_started = false` on entry so the flow check gets a fresh 300s window

### Error State Restart Loop (direct.js direct_off)
**Problem**: When a flow error occurred during direct mode, `direct_off` restored the saved `water_temp` and handed the unit off to `charge.js`. Charge.js saw the stale (pre-direct) water temp, calculated a priority, and restarted the unit immediately. The residual direct-loop water was still at the wrong temperature, so the flow check failed again, triggering another `direct_off` → `charge.js` restart → infinite loop.

**Fix**: Added check in `direct_off`: if `dg.active && unit.state == UNIT_STATE_ERROR`, stop everything (unit_stop, PA release) and do NOT restore water_temp or hand off to charge.js. The unit stays stopped until charge.js naturally re-evaluates on its next cycle with real pump readings.

### unit.in_direct Flags Cleared Too Late (direct.js direct_off)
**Problem**: `unit.in_direct` and `unit.in_direct_pending` were cleared at the end of `direct_off`, after the handoff logic. During the handoff, `unit_stop` → `unit_off` → `unit_mode(name, unit, ac.mode)` was called to restore the unit to storage mode. But `unit_mode()` checks `unit.in_direct` and rejects mode changes with "cannot be changed while in direct mode". This meant the reversing valve was never switched back to storage mode, and when charge.js restarted the unit, it ran in the wrong mode (e.g., Heat on Cool storage).

**Timeline** (from 21:51 logs):
1. Direct mode ended → `direct_off` called
2. `unit_mode` failed twice: "cannot be changed while in direct mode"
3. Reversing valve left in Heat position
4. Charge.js restarted unit → ran Heat on Cool storage → chaos

**Fix**: Moved `unit.in_direct = false`, `unit.in_direct_pending = false`, and `unit.in_direct_priority = 0` to BEFORE the handoff logic block (direct.js lines 808-810). Also resets `unit.run_started = false` to give the flow check a fresh window after the transition.

## Bug Fixes (2026-03-14)

### WAIT_UNIT Timeout Disables Fan Permanently (direct.js)
**Problem**: When PA denied a power reservation (e.g., battery < 35%, or settle timer from a prior release), the WAIT_UNIT timeout handler set `dg.state = DIRECT_STATE_ERROR`. The fan's run loop detected the error and set `FAN_STATE_ERROR`, which called `fan_force_stop`, set `fan.enabled = false` and `fan.error = true`. The fan was permanently disabled until a service restart. With the thermostat still calling for heat, the house got no heating.

**Timeline** (from 09:44 logs):
1. Direct mode stopped a RUNNING unit for mode change (Cool → Heat)
2. `unit_stop` released PA reservation (6500W now available)
3. `unit_start` re-requested PA — but PA had a settle timer, didn't grant within 129s
4. Timeout → ERROR → fan disabled → no more heating attempts

**Fix**: Changed WAIT_UNIT timeout from ERROR to retry:
1. Clean up: stop unit, turn off valves, clear `unit.in_direct` flags
2. Reset `fan.heat_state`/`fan.cool_state` to "off" and `fan.mode` to `FAN_MODE_NONE`
3. Go to `DIRECT_STATE_STOPPED` (not ERROR) with full state reset
4. The thermostat's next MQTT data message sees `fan.heat_state != data.heat_state`, re-triggers `fan_set_mode`, which calls `direct_on` again
5. Natural retry interval is the fan controller's data interval (~10s)
6. Once PA has power available (battery recovers, settle timer expires), direct mode starts normally

**Note**: PA was also patched separately to remove the settle timer — if enough power is available, it grants immediately regardless of prior release timing. But the retry logic is still needed for the battery < 35% case where PA legitimately cannot grant power.

## Bug Fixes (2026-05-19)

### Valve Activated Before AC Unit Running — HX Freeze (direct.js)
**Problem**: The old `ACTIVATE_VALVE` state opened pin1 *before* the AC unit started. The AC pump then tried to prime into the long AC→AH loop with no established flow column. Poor priming → poor flow through the heat exchanger → refrigerant vapor over-cooled → `run.js` vapor-temp freeze protection (`vapor_low_temp`, default 25°F) tripped and stopped the unit. Repeated trips risk cracking the braze-plate HX.

**Fix**: Reordered the state machine so the unit reaches RUNNING with normal storage-loop flow *before* pin1 is opened:
- `ACTIVATE_VALVE` (state 5) renamed to `PREPARE_UNIT` — sets unit mode + `in_direct` flags, dispatches to START_UNIT/WAIT_UNIT, no longer touches pin1.
- New `CONFIRM_FLOW` (state 16) — after WAIT_UNIT sees UNIT_STATE_RUNNING, verifies an `ac_pump` temp_in/out delta ≥ `flow_confirm_threshold` (2.0°F). Errors out after `flow_confirm_timeout` (60s) if no flow develops. Opening pin1 into a confirmed-no-flow HX is exactly what trips freeze protection.
- New `ACTIVATE_VALVE` (state 17) — the only place pin1 is opened, reached only after RUNNING + flow confirmed.

New cold-start order: `... → PREPARE_UNIT → START_UNIT → WAIT_UNIT → CONFIRM_FLOW → ACTIVATE_VALVE → RUNNING_OPEN → ...`

### WAIT_UNIT Timeout Killed Healthy Startups (direct.js)
**Problem**: `unit_timeout` (config 120s) was checked in WAIT_UNIT regardless of unit sub-state. On low-sun days PA reserve takes ~75–115s, then the pump primer+warmup chain adds ~40s — total ~135s. The 120s timer fired mid-Warmup, ~30s before the unit would have reached RUNNING, killing it and forcing an endless retry loop.

**Fix**: The timeout now only fires while `unit.state == UNIT_STATE_RESERVE` — the actual PA-denial case. Once the unit progresses past Reserve into the pump-start chain, the unit's own state machine owns failure detection.

### Sticky Stage-2 Valve — Reset on Every AC Start (run.js, unit.js)
**Problem**: The motorized auto-return stage-2 (pin2) valve sometimes fails to mechanically return to its default position when de-energized at the end of a direct cycle (capacitor/return-spring can't overcome it held in the AH-out→AC-in position). A stuck valve leaves the loop geometry wrong for the *next* AC start — direct cycle or charge.js storage run alike — causing HX under-flow and vapor-temp freeze trips. Manual fix is to re-energize pin2 ~10s then de-energize, and it returns.

**Fix**: Added two states to the **unit** start sequence in `run.js` (not direct mode — the valve must be reset on every AC start regardless of how it was triggered):
- `UNIT_STATE_RESET_VALVE` / `UNIT_STATE_WAIT_VALVE_RESET` (appended after ERROR; the run.js:575 startup guard was extended to treat them as pre-RELEASE states).
- Run between RESERVE and START_PUMP — PA granted, pump still off (water off).
- Toggle pin2 on, hold `valve_reset_time` (direct-group config, default 10s), pin2 off, then START_PUMP.
- The unit finds its pin2 via `unit.in_direct_group → directs[grp]`; skipped if the unit has no direct group / no pin2. Reuses `dg_valve2_on/off`.
- Self-healing: if a stop interrupts the reset and leaves pin2 energized, the next start's RESET_VALVE cycles it correctly.

## Bug Fixes (2026-05-23)

### `flow_confirm_timeout` Default Raised 60s → 180s (direct.js)
**Problem**: With pin1 closed, `ac_pump` is circulating the storage loop. The 60s default for CONFIRM_FLOW wasn't enough for the temp_in/out delta to develop in some conditions (water near-equilibrium, slower compressor pull-down), causing the direct cycle to error out and retry even when flow was actually fine.

**Fix**: Bumped the `flow_confirm_timeout` default in `direct_props` from 60 → 180. Persisted per-group config also updated on dg1 via `direct_set dg1 flow_confirm_timeout=180`.

### Sticky Stage-2 Valve Reset — Removed (run.js, unit.js)
**Problem**: The 2026-05-19 fix added `UNIT_STATE_RESET_VALVE` / `WAIT_VALVE_RESET` to cycle pin2 before every pump start. It was extended on 2026-05-22 to two full cycles with an off-settle between (`VALVE_OFF_WAIT`, `RESET_VALVE2`, `WAIT_VALVE_RESET2`, `VALVE_OFF_WAIT2`) after observing the primer pump start before the valve had returned. Neither single-cycle nor two-cycle reset prevented the HX freeze trips — including a trip during plain charge.js storage-loop operation with no direct-mode plumbing involved. Valves were verified visually as oriented correctly. Conclusion: the freezes aren't caused by pin2 sticking — they're caused by the valves themselves being undersized / partially obstructed on actuation, which a "reset" can't fix.

**Fix**: Removed all six valve-reset states and their handlers from `unit.js` / `run.js`. Unit start is now `STOPPED → RESERVE → START_PUMP` (or `STOPPED → START_PUMP` with no reserve), as it was before 2026-05-19. The `valve_reset_time` prop was removed from `direct_props`.

Direct mode is **currently disabled on dg1** (`enabled: false`) pending replacement of the motorized valves with the new in-stock units. Charge.js storage-loop operation is unaffected.

## Changes (2026-06-10)

### Two-relay (power + direction) valve control
**Why**: The flaky single-wire valves were replaced with bidirectional 3-wire actuators on
a 24V bus. Each valve set is now driven by **two relays** instead of one steady GPIO level:
- **pwr relay (relay1)**: COMM=+24V, NO → dir relay COMM. Gates power.
- **dir relay (relay2)**: COMM=+24V (from relay1), NC=storage, NO=direct. Selects direction.

**Implementation (direct.js)**:
- Config: replaced `pin1`/`pin2` with `valve1_power`/`valve1_select`/`valve2_power`/`valve2_select` and added
  `valve_time` (default 10s). Runtime fields `valve1_power_time`/`valve2_power_time` track when a
  move started.
- `dg_valve_move()`: sets the dir relay first, then energizes the pwr relay and stamps the
  time. `dg_valve1_on/off`, `dg_valve2_on/off` call it (to_direct true/false).
- `dg_valve_power_check()`: cuts both relays LOW once `valve_time` elapses; called for every
  group from `direct_main`. Rest state = both relays de-energized (storage); the valve holds
  its last position when unpowered.
- `_direct_init` / `direct_set` set `pinMode(OUTPUT)` + LOW on all four pins; `direct_del`
  drives them LOW on removal. `start.js` pulses both sets to storage on boot.
- Migration: groups configured with the old `pin1`/`pin2` must be reconfigured.

### Direct state machine moved to the 1s run loop
**Why**: `direct_main()` was called from `read.js` (read cadence, ~10s), while every other
component state machine (fan/pump/unit) runs in `run.js` every ~1s. The 10s cadence made the
timed valve power-cut imprecise and was inconsistent with the rest of the agent.

**Fix**: Moved the `direct_main()` call from `read.js` into `run_main` (`run.js`). The direct
state machine and the valve power-cut now run at ~1s. The state machine code stays in
`direct.js`; transitions are gated by `time()`/state checks, so 1s polling is just more
responsive. Fixed the `direct.js` water-temp log throttle (was `% 60 < ac.interval`,
assuming the 10s cadence) to `% 60 == 0`.

### jconfig_add rollback fix (lib/sd/jconfig.js)
**Why**: A failed init callback (e.g. `add_direct` with a missing `fan=`) left the
half-created object orphaned in the in-memory map — `add_*` then reported "exists" while the
on-disk JSON stayed empty.

**Fix**: `jconfig_add` now `delete objs[name]` when the init callback returns nonzero,
matching the existing opts-failure rollback. Affects all `add_*` paths (pump/fan/unit/direct).

## Changes (2026-07-02)

### Run BOTH pumps in the direct loop (no AH-pump bypass)
**Why**: The plumbing was rebuilt (see Architecture + ccmemory `ac-direct-topology`): direct
mode is a **closed loop** between the AC BPHX and the air handler, and the **½ HP AC pump and
¾ HP AH pump run in series** around it. The old code STOPPED the AH pump and pushed the whole
loop with the AC pump alone — with the new hardware an idle AH pump chokes the AC discharge →
BPHX under-flow → vapor-temp freeze trip.

**What changed** (surgical — the two-stage closed-loop machinery, primer, water-temp gate, and
freeze-safe ordering all stay):
- `STOP_FAN_PUMP`/`WAIT_FAN_PUMP` (which *stopped* the AH pump at the start) became
  `START_FAN_PUMP`/`WAIT_FAN_PUMP` (which *start* it and wait for RUNNING), repositioned to
  **after CONFIRM_FLOW, before ACTIVATE_VALVE**. Invariant: both pumps RUNNING before any valve flips.
- `direct_on` routes straight to PREPARE_UNIT (no fan-pump-stop dispatch).
- RUNNING monitors the unit **and both pumps**; loss of the AH pump tears down.
- The blower (`fan_on`) is still gated on water temp (`WAIT_WATER_TEMP`); the fan **pump**
  comes up earlier for the hydraulics.

### Fan-pump refcount ownership (`dg.direct_fan_pump_ref`)
**Why**: With the AH pump kept running, its refcount must be owned cleanly across two entry
paths — a **fresh start** (fan dormant, run.js skips it) and an **already-running** fan
(triggered from FAN_STATE_RUNNING on water-temp). The first design leaked the pump on any
error before the blower started, because the dormant fan never runs to release it.

**Fix**:
- `dg.direct_fan_pump_ref` tracks whether DIRECT owns the ref. START_FAN_PUMP keys on
  `fan.state == FAN_STATE_RUNNING` (not pump state): already-running → the fan owns it, skip;
  else → `pump_start` and DIRECT owns it.
- At the RUNNING transition the blower start is guarded (`fan.state != RUNNING`) and ownership
  is handed to the fan (`direct_fan_pump_ref = false`; `fan_off` releases it).
- `direct_release_fan_pump()` — called by every error path at/after START_FAN_PUMP and by
  `direct_off` — clears `pump.direct` unconditionally and force-stops the pump only if DIRECT
  owns the ref. `direct_off`'s old `pump_start(fan_pump)` (storage handoff) was deleted (the
  pump is already running; re-starting double-refs).
- `pump.direct` now means "under direct control — don't cycle/sample" (RUNNING, not stopped):
  set in START_FAN_PUMP for **both** scenarios (deferred from `direct_on` so a pre-START_FAN_PUMP
  abort can't strand it — a stranded flag would block the fan from starting later), and cleared
  unconditionally by `direct_release_fan_pump`.

### Supporting changes
- `run.js` FAN_STATE_RUNNING monitors the fan's own pump again (dropped the AC-pump swap);
  direct.js watches the AC side.
- `cycle.js` only `pump_start`s a pump whose state is STOPPED (never bumps the now-running AH
  pump's refs in the pre-`pump.direct` window).
- `direct_off` got an `in_teardown` re-entry guard (direct_off → fan_stop → fan_off →
  direct_off runs teardown once).
- `start.js` comment: `dg_valves_off` on boot is MANDATORY for the directional valves (they
  hold position unpowered — no spring return — so a crash mid-direct leaves them in direct).
- Latent bug found & fixed: the OLD code double-reffed an already-running fan via an
  unconditional `fan_start` at the RUNNING transition.
- **pub.js valve reporting**: dropped the eight relay-pin fields (`valve{1,2}_{power,select}`
  + `_state` — they read 0 at rest for a holding valve, so the UI could never show position)
  and replaced them with `valve1`/`valve2` = `Direct`/`Storage`/`None`, computed **inline** from
  `dg.active` (valve1) and `dg.state` (valve2 — Direct only once the loop is closed,
  START_PRIMER..RUNNING). **No helper functions on purpose:** pub.js is inline logic that
  hot-reloads, whereas a NEW function in direct.js does NOT hot-reload (it needs a full agent
  restart — see [[ac-deploys-from-opt-sd]]). So this change is pub.js-only.

**Tests**: `test/test_direct.js` — 48 pass (rewrote the STOP_FAN_PUMP-entry and
CONFIRM_FLOW→ACTIVATE_VALVE tests; added START_FAN_PUMP sc.1/sc.2, WAIT_FAN_PUMP transitions,
fan-pump force-release on error, direct_off ownership, both-pumps monitor). Full suite 152 pass.

**Deploy status**: the both-pumps rework (direct.js/run.js/cycle.js/start.js) is LIVE on the
`ac` host as of 2026-07-02 14:24 and ran a full cycle correctly at 14:31 (stage 1 → 3½-min
RUNNING_OPEN wait → stage 2 → loop closed → running, scenario-2 fast path). The pub.js
valve-reporting change is **pub.js-only** — deploy `pub.js`, hot-reload, no restart.

**Still open**: `RUNNING_OPEN` has no timeout — if the ≥`temp_threshold` change never arrives,
it sits with valve1 open and valve2 never firing (no air). Add a timeout that force-closes
stage 2 or errors out.
