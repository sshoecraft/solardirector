# Direct Mode Implementation

## Overview

Direct mode bypasses thermal storage to directly heat/cool air handlers when:
1. Fan requests a mode that doesn't match storage mode (mode mismatch)
2. Storage tank temperature is outside acceptable range (storage depleted)

## Architecture

### Water Flow

**Storage Mode (Normal):**
```
AC Unit → AC Pump → Storage Tank → Fan Pump → Air Handler → House
```

**Direct Mode Stage 1 (pin1 on, open loop):**
```
AC Unit → AC Pump → [Valve1: to AH] → Air Handler → House → Storage Tank
                                              ↓
                                     Fan Pump OFF (bypassed)
```

**Direct Mode Stage 2 (pin1 + pin2 on, closed loop):**
```
AC Unit → AC Pump → [Valve1+2] → Air Handler → [Valve3+4] → AC Unit input
        ↑                                                        ↓
        └────────────────────────────────────────────────────────┘
                    (complete loop, tank fully bypassed)
```

### Physical Valve Setup

**Stage 1 valves (pin1):**
- Valve 1: AC pump output - switches from "to tank" → "to AH input"
- Valve 2: AH input - switches from "from tank" → "from AC output"

**Stage 2 valves (pin2):**
- Valve 3: AH output - switches from "to tank" → "to AC input"
- Valve 4: AC input - switches from "from tank" → "from AH output"

Both valves in each stage are wired together (single pin control per stage).

### Key Constraints

- 3-way valves connect AC circuit directly to fan circuit
- AC pump circulates water through direct circuit
- Fan pump MUST be OFF (bypassed) - valve cannot activate until pump is stopped
- Fan pump is marked with `pump.direct = true` to prevent cycle.js and sample.js from starting it
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
| Stop fan pump | 1 | Stopping the fan pump |
| Wait fan pump | 2 | Waiting for fan pump to reach STOPPED |
| Stop unit | 3 | Stopping unit for mode change |
| Wait unit stop | 4 | Waiting for unit to reach STOPPED |
| Activate valve | 5 | Turn on pin1 (stage 1), set unit mode, set priority |
| Start unit | 6 | Start AC unit |
| Wait unit | 7 | Wait for unit to reach RUNNING |
| Wait water temp | 8 | Wait for water to reach temp threshold before starting fan |
| Running open | 9 | Stage 1 active - monitoring for temp change |
| Wait temp | 10 | (reserved) |
| Activate pin2 | 11 | Temp change detected, activate pin2 to close loop |
| Start primer | 12 | Start primer pump to bleed air |
| Wait primer | 13 | Wait for primer to run (primer_time seconds) |
| Running | 14 | Fully closed loop active |
| Error | 15 | Error occurred |

### Two-Stage Activation Sequence

1. **Stage 1** (pin1): Connect AC output to AH input
   - Water flows: AC → AH → back to storage tank (open loop)
   - Monitor AH pump temp_out for temperature change
   - This confirms water is flowing through AH

2. **Stage 2** (pin2): Close the loop
   - When temp change detected (>= temp_threshold), activate pin2
   - AH output now connects back to AC input
   - Run primer for primer_time seconds to bleed air from the closed loop
   - Air relief valve at high point of AC pump output bleeds air

### State Transitions

```
STOPPED → (direct_on) → STOP_FAN_PUMP → WAIT_FAN_PUMP
                                  ↓ (if pump already stopped)
                                  └──────────────────────┘
                                                         ↓
                                 ┌───────────────────────┤
                                 ↓ (if unit running      ↓ (if unit stopped or
                                    in wrong mode)          already correct mode)
                           STOP_UNIT → WAIT_UNIT_STOP → ACTIVATE_VALVE (pin1)
                                                               ↓
                           START_UNIT → WAIT_UNIT → RUNNING_OPEN (no fan yet)
                                                         ↓
                              (monitor for temp change on fan_pump.temp_out)
                                                         ↓
                           ACTIVATE_PIN2 → START_PRIMER → WAIT_PRIMER
                                                               ↓
                                                      WAIT_WATER_TEMP (loop closed)
                                                               ↓
                                                    (water reaches temp) → fan_start → RUNNING
                                                                          ↓
RUNNING → (direct_off) → STOPPED
          - If mode mismatch: stop unit, set mode to storage mode, hand off stopped
          - If mode matches: hand off running with repri
```

## Configuration

File: `/opt/sd/etc/ac_directs.json`

```json
{
    "dg1": {
        "enabled": true,
        "pin1": 2,
        "pin2": 28,
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
| pin1 | int | -1 | GPIO pin for stage 1 valves (AC→AH input) |
| pin2 | int | -1 | GPIO pin for stage 2 valves (AH→AC input, closes loop) |
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
- `unit.direct_group`: Links unit to its direct group

**Runtime state** (managed by state machine):
- `dg.state`: Current state (0-15)
- `dg.active`: true when direct mode is active (stage 1 or 2)
- `dg.pending_fan`: Fan waiting for direct mode to start
- `dg.active_fan`: Fan currently using direct mode
- `dg.target_mode`: Requested mode (FAN_MODE_COOL/HEAT)
- `dg.mode`: Current active mode
- `dg.initial_temp`: Fan pump temp_out when entering RUNNING_OPEN
- `dg.primer_start_time`: Time when primer was started
- `dg.unit_start_time`: Time when unit_start was called (for timeout)
- `dg.water_wait_time`: Time when WAIT_WATER_TEMP state was entered (for timeout)

**Note**: For fans in a direct group, `stop_wt` is ignored. Direct mode handles water
temperature limits using exact thresholds (cool_high_temp / heat_low_temp) instead of
stopping the fan.

## Manual Control

Direct mode can be manually controlled via sdconfig:

```bash
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
sdconfig ac dg_valve_on dg1            # manually activate pin1 (stage 1)
sdconfig ac dg_valve_off dg1           # manually deactivate all valves
sdconfig ac dg_valve2_on dg1           # manually activate pin2 (stage 2)
```

## Files Modified

### direct.js
- State machine implementation with two-stage activation
- `direct_on()`, `direct_off()`: Turn direct mode on/off (starts/stops state machine)
- `direct_enable()`, `direct_disable()`: Enable/disable direct group
- `direct_main()`: Process all direct groups each loop
- `direct_is_pending()`, `direct_is_active()`, `direct_is_error()`: State query helpers
- `direct_get_pump()`: Get AC pump name for a direct group
- `dg_valve_on()`, `dg_valve_off()`: Stage 1 valve control (pin1)
- `dg_valve2_on()`: Stage 2 valve control (pin2)
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

### read.js
- Calls `direct_main()` each loop to process direct state machine

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
- Set by `dg_valve_on()`, cleared by `dg_valve_off()`

### Storage Temperature Monitoring
- **Unit shutdown on target reached**: When unit is RUNNING and water_temp reaches target (cool_low_temp - 0.5 for cooling, heat_high_temp + 0.5 for heating), unit stops automatically. If in direct mode, fan transitions to storage mode (keeps running).
- **Fan direct mode trigger**: When fan is RUNNING and water_temp exceeds limits (cool_high_temp for cooling, heat_low_temp for heating), direct mode activates if available

### Two-Stage Temperature Monitoring
- In RUNNING_OPEN state, monitors `fan_pump.temp_out` for change
- When temp changes by >= `temp_threshold` from initial reading, triggers stage 2
- Stage 2 closes the loop and runs primer to bleed air

### Startup Reset
- All valves set LOW (pin1 and pin2)
- All direct group states reset to STOPPED
- All flags (`dg.active`, `dg.pending_fan`, `dg.active_fan`) cleared

## Exposed Functions

Functions available via sdconfig:

| Function | Args | Description |
|----------|------|-------------|
| `direct_on` | group_name, [mode] | Turn on direct mode (mode: "heat" or "cool", default: current ac.mode) |
| `direct_off` | group_name, [stop_fan] | Turn off direct mode (stop_fan: true=stop fan (default), false=keep fan running on storage) |
| `direct_enable` | group_name | Enable direct group (allow it to be turned on) |
| `direct_disable` | group_name | Disable direct group (turn off if active, prevent future activation) |
| `dg_valve_on` | group_name | Activate pin1 (stage 1 valves) |
| `dg_valve_off` | group_name | Deactivate all valves (pin1 and pin2) |
| `dg_valve2_on` | group_name | Activate pin2 (stage 2 valves) |

Internal helper functions (not exposed via sdconfig):

| Function | Args | Description |
|----------|------|-------------|
| `direct_get_pump` | group_name | Returns AC pump name for the group |
| `direct_is_pending` | group_name, fan_name | Check if direct mode is pending for fan |
| `direct_is_active` | group_name, fan_name | Check if direct mode is active for fan |
| `direct_is_error` | group_name | Check if direct mode is in error state |
| `direct_main` | none | Process all direct groups (called from read.js) |

## InfluxDB

Measurement: `directs`

| Field | Type | Description |
|-------|------|-------------|
| name | string | Direct group name |
| enabled | int | 1 if enabled, 0 if disabled |
| state | string | Current state name |
| active | int | 1 if direct mode is active |
| pin1 | int | GPIO pin number for stage 1 valves |
| pin1_state | int | Current pin1 state (0=off, 1=on) |
| pin2 | int | GPIO pin number for stage 2 valves |
| pin2_state | int | Current pin2 state (0=off, 1=on) |
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
