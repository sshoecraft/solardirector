# AC Agent - Air Conditioning Control

The AC agent manages HVAC equipment including air conditioning units, fans, and pumps for the SolarDirector system. It integrates with the Power Agent (PA) for power management and uses temperature-based control to optimize thermal storage charging and space conditioning.

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Components](#components)
  - [Units](#units)
  - [Fans](#fans)
  - [Pumps](#pumps)
- [State Machines](#state-machines)
- [Temperature Management](#temperature-management)
- [Power Integration (PA)](#power-integration-pa)
- [Min Runtime Protection](#min-runtime-protection)
- [Configuration](#configuration)
- [Communication](#communication)
- [Development](#development)

---

## Overview

The AC agent controls three types of components:

1. **Units** - HVAC compressor units (AC/heat pump) with GPIO control
2. **Fans** - Air handlers that receive control data via MQTT
3. **Pumps** - Water circulation pumps with temperature sensors

The agent operates in multiple modes:
- **Thermal Storage Charging** - Heat/cool water storage during solar production
- **Space Conditioning** - Normal HVAC operation for building comfort
- **Power-Managed Operation** - Integrates with PA agent for power allocation

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        AC Agent                              │
├─────────────────────────────────────────────────────────────┤
│  Components:                                                 │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                  │
│  │  Units   │  │  Fans    │  │  Pumps   │                  │
│  │ (GPIO)   │  │ (MQTT)   │  │ (GPIO)   │                  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘                  │
│       │             │             │                          │
│       └─────────────┴─────────────┘                          │
│                     │                                        │
│  ┌──────────────────┴────────────────────┐                  │
│  │         State Machine (run.js)        │                  │
│  │  - Start/Stop Control                 │                  │
│  │  - Min Runtime Enforcement             │                  │
│  │  - Safety Monitoring                   │                  │
│  └──────────────────┬────────────────────┘                  │
│                     │                                        │
│  ┌──────────────────┴────────────────────┐                  │
│  │    Temperature & Mode Control         │                  │
│  │  - Mode Selection (mode.js)           │                  │
│  │  - Thermal Charging (charge.js)       │                  │
│  │  - Temperature Sampling (sample.js)   │                  │
│  └──────────────────┬────────────────────┘                  │
└───────────────────┬─┴───────────────────────────────────────┘
                    │
      ┌─────────────┴─────────────┐
      │                           │
┌─────▼──────┐            ┌───────▼────────┐
│ PA Agent   │            │ Hardware       │
│ (Power)    │            │ - CAN Bus      │
│            │            │ - GPIO         │
│ - Reserve  │            │ - Temp Sensors │
│ - Release  │            └────────────────┘
│ - Repri    │
│ - Revoke   │
└────────────┘
```

### File Structure

```
ac/
├── main.c              # C entry point
├── driver.c            # Driver implementation
├── can.c              # CAN bus communication
├── jsfuncs.c          # C functions exposed to JavaScript
├── ac.h               # Header definitions
│
├── init.js            # Initialization
├── run.js             # Main state machine loop
├── mode.js            # Mode selection logic
├── charge.js          # Thermal storage charging
├── cycle.js           # HVAC cycling logic
├── sample.js          # Temperature sampling
│
├── unit.js            # Unit component management
├── fan.js             # Fan component management
├── pump.js            # Pump component management
├── utils.js           # Utility functions
└── common.js          # Common component functions
```

---

## Components

### Units

HVAC compressor units (air conditioners or heat pumps) controlled via GPIO pins.

**Key Features:**
- Cooling and heating modes with reversing valve control
- GPIO pin control for compressor and valves
- Pump dependency (requires running pump for heat exchange)
- Min runtime protection (15 minutes default) to prevent compressor damage
- PA power integration with dynamic priority

**Properties:**
- `enabled` - Enable/disable the unit
- `pump` - Associated pump name (required)
- `pump_timeout` - Timeout waiting for pump to start (60s)
- `min_runtime` - Minimum runtime before allowing stop (900s / 15min)
- `coolpin` - GPIO pin for cooling contactor
- `heatpin` - GPIO pin for heating contactor
- `rvpin` - GPIO pin for reversing valve
- `rvcool` - Reversing valve polarity (true = energized for cooling)
- `rvevery` - Energize RV every cycle vs. mode-based
- `liquid_temp_sensor` - Optional liquid line temperature sensor
- `vapor_temp_sensor` - Optional vapor line temperature sensor
- `direct` - Direct mode (bypass normal control)
- `reserve` - Power reservation amount (watts)
- `priority` - PA priority (1-100, 1=highest)

**State Machine:**
```
STOPPED → RESERVE → START_PUMP → WAIT_PUMP → START → RUNNING
                                                         ↓
                                                   MIN_RUNTIME_WAIT
                                                         ↓
                                                      RELEASE → STOPPED
```

**Example Configuration:**
```json
{
  "units": {
    "ac1": {
      "enabled": true,
      "pump": "ac1",
      "coolpin": 22,
      "heatpin": 23,
      "rvpin": 24,
      "rvcool": true,
      "reserve": 6500,
      "priority": 50,
      "min_runtime": 900
    }
  }
}
```

### Fans

Air handler units controlled remotely via MQTT, subscribing to fan controller data.

**Key Features:**
- MQTT-based control (subscribes to fan controller topic)
- Optional pump association for water-source operation
- Min runtime protection (10 minutes default) to prevent pump cycling
- Water temperature limit enforcement
- Thermal cooldown support (separate from min runtime)

**Properties:**
- `enabled` - Enable/disable the fan
- `topic` - MQTT topic to subscribe for fan data
- `pump` - Associated pump name (optional)
- `start_timeout` - Timeout waiting for fan to report started (60s)
- `pump_timeout` - Timeout waiting for pump to start (40s)
- `min_runtime` - Minimum runtime before allowing stop (600s / 10min)
- `cooldown` - Thermal cooldown period after stopping (30s)
- `cooldown_threshold` - Temperature threshold for cooldown (25.0°F)
- `reserve` - Power reservation amount (watts)
- `priority` - PA priority (1-100)
- `stop_wt` - Stop if water temp out of limits (true)
- `wt_thresh` - Water temp threshold offset (3°F)

**State Machine:**
```
STOPPED → RESERVE → START_PUMP → WAIT_PUMP → START → WAIT_START → RUNNING
                                                                       ↓
                                                                 MIN_RUNTIME_WAIT
                                                                       ↓
                                                                    RELEASE → STOPPED
```

**Example Configuration:**
```json
{
  "fans": {
    "fan1": {
      "enabled": true,
      "topic": "solar/hvac/fan1/data",
      "pump": "pump1",
      "reserve": 2000,
      "priority": 100,
      "min_runtime": 600,
      "cooldown": 30,
      "stop_wt": true,
      "wt_thresh": 3
    }
  }
}
```

### Pumps

Water circulation pumps with temperature monitoring.

**Key Features:**
- GPIO pin control
- Temperature monitoring (inlet/outlet)
- Flow rate monitoring (optional)
- Primer pump support (for air purging)
- Warmup and cooldown periods
- Reference counting (multiple units/fans can share a pump)

**Properties:**
- `enabled` - Enable/disable the pump
- `pin` - GPIO pin for pump control
- `primer` - Primer pump name (optional, for air purging)
- `primer_timeout` - Timeout waiting for primer (30s)
- `flow_sensor` - Flow sensor name (optional)
- `min_flow` - Minimum flow rate (optional)
- `wait_time` - Initial wait after starting (10s)
- `flow_wait_time` - Wait for stable flow (5s)
- `flow_timeout` - Timeout waiting for flow (20s)
- `temp_in_sensor` - Inlet temperature sensor
- `temp_out_sensor` - Outlet temperature sensor
- `warmup` - Warmup period before considering running (30s)
- `warmup_threshold` - Temperature change threshold (10.0°F)
- `cooldown` - Cooldown period after stopping (0s)
- `cooldown_threshold` - Temperature threshold for cooldown (10.0°F)
- `direct_group` - Direct mode group name
- `reserve` - Power reservation amount (watts)
- `priority` - PA priority (1-100)

**State Machine:**
```
STOPPED → RESERVE → START_PRIMER → WAIT_PRIMER → START → WAIT_PUMP
                                                             ↓
                                                           FLOW → WARMUP → RUNNING
                                                                             ↓
                                                                         COOLDOWN → RELEASE → STOPPED
```

**Example Configuration:**
```json
{
  "pumps": {
    "ac1": {
      "enabled": true,
      "pin": 17,
      "primer": "primer",
      "temp_in_sensor": "ac1_temp_in",
      "temp_out_sensor": "ac1_temp_out",
      "warmup": 30,
      "reserve": 500,
      "priority": 100
    },
    "primer": {
      "enabled": true,
      "pin": 18,
      "warmup": 0,
      "cooldown": 0
    }
  }
}
```

---

## State Machines

### Common State Machine Pattern

All components follow a similar state machine pattern:

1. **STOPPED** - Component is off, waiting for refs > 0
2. **RESERVE** - Request power from PA (if `reserve` > 0)
3. **START_XXX** - Begin startup sequence
4. **WAIT_XXX** - Wait for dependencies/conditions
5. **RUNNING** - Normal operation, monitor safety conditions
6. **MIN_RUNTIME_WAIT** - Waiting for minimum runtime to be satisfied
7. **RELEASE** - Release power back to PA
8. **ERROR** - Error state, component disabled

### State Progression Control

Components use a **reference counting** system (`refs` field):
- `refs = 0` - Component should stop
- `refs > 0` - Component should run
- Increment refs to start: `component.refs++` or `component.refs = 1`
- Decrement refs to stop: `component.refs--` or `component.refs = 1` (in stop functions)

### MIN_RUNTIME_WAIT State

Special state to enforce minimum runtime requirements:

**Purpose:** Prevent short cycling damage to compressors and pumps

**Entry Conditions:**
- `unit_stop()` or `fan_stop()` called while in RUNNING state
- Component has `min_runtime` configured
- Component transitions to MIN_RUNTIME_WAIT instead of immediate stop

**Exit Conditions:**
- `time() - start_time >= min_runtime` - Minimum runtime satisfied → transition to RELEASE/STOPPED
- `immediate` revoke flag from PA - Emergency shutdown → force stop

**During MIN_RUNTIME_WAIT:**
- Compressor/fan continues running (not actually stopped yet)
- Pump must remain RUNNING - errors if pump fails
- Water temp limits still enforced for fans
- All normal RUNNING safety checks apply

**Example:**
```javascript
// Unit has been running for 4 minutes, PA requests revoke
unit.time = 1000;           // Started at timestamp 1000
time() = 1240;              // Current time (4 minutes later)
unit.min_runtime = 900;     // 15 minute requirement

// Stop requested
unit_stop() → unit_cooldown() → UNIT_STATE_MIN_RUNTIME_WAIT

// In run.js loop:
diff = time() - unit.time;  // 240 seconds (4 minutes)
if (diff >= unit.min_runtime) unit_off()  // false, keeps running

// 11 minutes later...
diff = time() - unit.time;  // 900 seconds (15 minutes)
if (diff >= unit.min_runtime) unit_off()  // true, stops now
```

---

## Temperature Management

### Mode Selection (mode.js)

Automatic mode selection based on water temperature:

**Cooling Mode:**
- Enter: `water_temp >= cool_high_temp`
- Exit: `water_temp < cool_low_temp`

**Heating Mode:**
- Enter: `water_temp <= heat_low_temp`
- Exit: `water_temp > heat_high_temp`

**Water Temperature Calculation:**
```javascript
// Weighted average of all pump temperatures
water_temp = Σ(pump_temp_in * (1 - weight) + pump_temp_out * weight)
```

**Configuration:**
- `waterTempWeight` - Weighting (0.0 = temp_in only, 1.0 = temp_out only, default 0.5)
- `cool_high_temp` - Start cooling threshold (60°F)
- `cool_low_temp` - Stop cooling threshold (35°F)
- `heat_high_temp` - Stop heating threshold (125°F)
- `heat_low_temp` - Start heating threshold (100°F)

### Thermal Storage Charging (charge.js)

Manages thermal storage charging during solar production:

**Charge States:**
```
STOPPED → START → WAIT_START → RUNNING → STOP → STOPPED
```

**Dynamic Priority:**
- Priority adjusts based on temperature progress
- Higher priority as storage approaches target temperature
- Reprioritization occurs when temp changes by `repri_interval` (default 2.5°F)
- **Critical:** If reprioritization fails (MQTT down), unit automatically stops

**Charge Temperature Thresholds:**
- `charge_cool_high_temp` - Cooling target during charging
- `charge_heat_low_temp` - Heating target during charging

**Safety:**
```javascript
if (pa_client_repri(ac, "unit", name, unit.reserve, pri)) {
    // Reprioritization failed - MQTT/PA unreachable
    do_stop = true;  // Must stop immediately
}
```

---

## Power Integration (PA)

The AC agent integrates with the Power Agent for power allocation management.

### PA Client Functions

**1. Reserve Power**
```javascript
pa_client_reserve(instance, module, item, amount, priority)
```
- Request power allocation from PA
- Returns 0 on success (approved), 1 on failure (denied)
- Component stays in RESERVE state until approved
- Example: `pa_client_reserve(ac, "unit", "ac1", 6500, 50)`

**2. Release Power**
```javascript
pa_client_release(instance, module, item, amount)
```
- Release power back to PA when stopping
- Always proceeds to STOPPED regardless of return value
- Stale reservations auto-cleaned by PA on next reserve
- Example: `pa_client_release(ac, "unit", "ac1", 6500)`

**3. Reprioritize**
```javascript
pa_client_repri(instance, module, item, amount, new_priority)
```
- Update priority while running (used during thermal charging)
- **Critical:** If fails, component must stop immediately
- Example: `pa_client_repri(ac, "unit", "ac1", 6500, 35)`

**4. Revoke (PA → AC)**
```javascript
unit_revoke(name, amount, immediate)
fan_revoke(name, amount, immediate)
```
- Called by PA when power needed back
- `immediate = false/0` - Normal revoke, respects min_runtime
- `immediate = true/1` - Emergency revoke, force stops immediately
- Example: PA calls `revoke(unit, ac1, 6500, 0)` via sdconfig

### Power Management Flow

**Normal Operation:**
```
1. Component needs to start
   └→ refs++ (increment references)
   └→ RESERVE state
   └→ pa_client_reserve() → approved
   └→ START sequence
   └→ RUNNING

2. Component needs to stop
   └→ refs-- (decrement references)
   └→ MIN_RUNTIME_WAIT (if min_runtime not met)
   └→ RELEASE state
   └→ pa_client_release()
   └→ STOPPED
```

**PA Revoke Flow:**
```
1. Normal Revoke (immediate=0)
   └→ unit_revoke(name, amount, 0)
   └→ unit_stop()
   └→ MIN_RUNTIME_WAIT (enforces min_runtime)
   └→ RELEASE after min_runtime met
   └→ STOPPED

2. Emergency Revoke (immediate=1)
   └→ unit_revoke(name, amount, 1)
   └→ unit_force_stop()
   └→ Immediate STOPPED (bypasses min_runtime)
```

**Reprioritization Failure:**
```
Thermal charging running
└→ Temperature changes by repri_interval
   └→ pa_client_repri() fails (MQTT down)
      └→ do_stop = true
      └→ unit_stop()
      └→ Component safely shuts down
```

### PA Reservation Auto-Cleanup

PA automatically cleans up stale reservations:
- When `reserve(id, amount)` called, removes existing reservation with same id+amount
- Prevents accumulation of stale reservations from failed releases
- Makes release error handling non-critical

---

## Min Runtime Protection

### Purpose

Prevents short cycling damage to:
- **Compressors** (units) - Mechanical wear, reduced efficiency, reduced lifespan
- **Pumps** (via fans) - Excessive start/stop cycles

### Configuration

**Units:**
- `min_runtime` - Default 900s (15 minutes)
- Prevents compressor short cycling

**Fans:**
- `min_runtime` - Default 600s (10 minutes)
- Prevents pump cycling (fan shares pump with other components)

### Implementation

**State Transition:**
```javascript
// Normal stop requested
unit_stop(name)
  ↓
unit_cooldown(name, unit)
  ↓
if (unit.min_runtime > 0)
  unit_set_state(UNIT_STATE_MIN_RUNTIME_WAIT)
  // Keep compressor running
else
  unit_off()  // Stop immediately
```

**Enforcement in run.js:**
```javascript
case UNIT_STATE_MIN_RUNTIME_WAIT:
    // Compressor still running - safety checks apply
    if (pump_state != PUMP_STATE_RUNNING) {
        // CRITICAL ERROR - pump failed during min runtime
        error_set("unit", name, "pump not running during min runtime wait")
        unit_set_state(UNIT_STATE_ERROR)
    }

    // Check if min runtime satisfied
    diff = time() - unit.time  // unit.time set when entered RUNNING
    if (diff >= unit.min_runtime) {
        unit_off(name, unit)  // Now safe to stop
    }
```

**Immediate Override:**
```javascript
// Emergency shutdown (PA immediate revoke)
unit_revoke(name, amount, immediate=1)
  ↓
unit_force_stop(name)  // Bypass MIN_RUNTIME_WAIT
  ↓
unit_off()  // Stop immediately
```

### Safety During MIN_RUNTIME_WAIT

**Units:**
- Pump must remain RUNNING (errors if pump stops)
- Compressor continues operation
- All normal RUNNING state safety checks apply

**Fans:**
- Pump must remain RUNNING (errors if pump stops)
- Water temp limits enforced (stops if temp out of range)
- Fan continues blowing air

**Behavior:**
```javascript
// Unit running for 4 minutes, stop requested
time() - unit.time = 240s (4 min)
min_runtime = 900s (15 min)

State: MIN_RUNTIME_WAIT
→ Compressor: ON
→ Pump: ON (enforced)
→ Continues for 11 more minutes
→ Then transitions to STOPPED
```

---

## Configuration

### Global AC Agent Properties

```javascript
{
  "interval": 10,              // Main loop interval (seconds)
  "sample_interval": 300,      // Temperature sampling interval (seconds)
  "location": "lat,lon",       // Location for sunrise/sunset
  "standard": 0,               // 0=US/Imperial, 1=Metric

  // Temperature thresholds
  "cool_high_temp": 60,        // Start cooling above this (°F)
  "cool_low_temp": 35,         // Stop cooling below this (°F)
  "heat_high_temp": 125,       // Stop heating above this (°F)
  "heat_low_temp": 100,        // Start heating below this (°F)

  // Charging thresholds
  "charge_cool_high_temp": 75, // Cooling target during charging (°F)
  "charge_heat_low_temp": 90,  // Heating target during charging (°F)
  "charge_threshold": 3,       // Temperature threshold for charge mode (°F)
  "repri_interval": 2.5,       // Reprioritization temp interval (°F)

  // Hardware
  "can_target": "can0",        // CAN interface name
  "can_topts": "500000",       // CAN speed (bps)
  "pump_failsafe_enable": 27,  // GPIO pin for pump failsafe
  "waterTempWeight": 0.5       // Temp averaging weight (0-1)
}
```

### Configuration Commands

```bash
# List all AC properties
sdconfig ac list

# Get specific property
sdconfig ac get interval
sdconfig ac get cool_high_temp

# Set property
sdconfig ac set cool_high_temp 65
sdconfig ac set repri_interval 2.0

# Component management
sdconfig ac jsexec 'pump_add("pump1", { pin: 17, temp_in_sensor: "sensor1" })'
sdconfig ac jsexec 'unit_add("ac1", { pump: "pump1", coolpin: 22, reserve: 6500 })'
sdconfig ac jsexec 'fan_add("fan1", { topic: "solar/hvac/fan1/data", pump: "pump1" })'

# Component control
sdconfig ac jsexec 'unit_start("ac1")'
sdconfig ac jsexec 'unit_stop("ac1")'
sdconfig ac jsexec 'unit_enable("ac1")'
sdconfig ac jsexec 'unit_disable("ac1")'
```

---

## Communication

### MQTT Topics

**Published:**
- `solar/ac/data` - Main AC state, temperatures, mode
- Component-specific data published via InfluxDB integration

**Subscribed:**
- `solar/hvac/+/data` - Fan controller data
- PA command topics via sdconfig

**Message Format:**
```json
{
  "name": "fan1",
  "fan_state": "on",
  "heat_state": "off",
  "cool_state": "on",
  "air_in": 75.0,
  "air_out": 55.0
}
```

### CAN Bus

**CAN IDs:** 0x450-0x45F

**Configuration:**
- Interface: Configurable via `can_target` (default: "can0")
- Speed: Configurable via `can_topts` (default: "500000" = 500kbps)
- Frame format: Standard 11-bit identifiers

### InfluxDB

**Measurements:**
- `ac` - Main temperatures, modes, state
- `fans` - Individual fan states and speeds
- `pumps` - Pump states and temperatures
- `units` - Unit modes and pin states
- `errors` - Error events with descriptions

---

## Development

### Building

```bash
# Build AC agent
cd agents/ac
make

# Clean build
make clean all

# Install
make install  # Installs to PREFIX (/opt/sd or ~/)
```

### Testing

```bash
# Test with virtual CAN
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# Run with test config
./ac -c test.json -v -d

# Monitor MQTT
mosquitto_sub -t 'solar/ac/+' -v
mosquitto_sub -t 'solar/hvac/+/data' -v

# Monitor logs
tail -f /opt/sd/log/ac.log
```

### Debug Levels

```bash
./ac -d 0  # Errors only
./ac -d 1  # + Warnings
./ac -d 2  # + Info
./ac -d 3  # + Debug
./ac -d 4  # + Verbose debug
```

### Service Management

```bash
# Systemd service
systemctl start ac.service
systemctl stop ac.service
systemctl restart ac.service
systemctl status ac.service

# View logs
journalctl -u ac.service -f
tail -f /opt/sd/log/ac.log
```

### Common Operations

**Start thermal charging:**
```bash
sdconfig ac jsexec 'ac.mode = AC_MODE_COOL'
# Units will start automatically if enabled and refs > 0
```

**Emergency stop all:**
```bash
sdconfig ac jsexec 'for(let n in units) unit_force_stop(n)'
sdconfig ac jsexec 'for(let n in fans) fan_force_stop(n)'
sdconfig ac jsexec 'for(let n in pumps) pump_force_stop(n)'
```

**Check component states:**
```bash
sdconfig ac jsexec 'for(let n in units) printf("%s: %s\\n", n, unit_statestr(units[n].state))'
sdconfig ac jsexec 'for(let n in fans) printf("%s: %s\\n", n, fan_statestr(fans[n].state))'
sdconfig ac jsexec 'for(let n in pumps) printf("%s: %s\\n", n, pump_statestr(pumps[n].state))'
```

### Troubleshooting

**Unit won't start:**
```bash
# Check if enabled
sdconfig ac jsexec 'units.ac1.enabled'

# Check refs
sdconfig ac jsexec 'units.ac1.refs'

# Check state
sdconfig ac jsexec 'unit_statestr(units.ac1.state)'

# Check error
sdconfig ac jsexec 'units.ac1.error'

# Enable and start
sdconfig ac jsexec 'unit_enable("ac1"); units.ac1.refs = 1'
```

**Pump not running:**
```bash
# Check pump state
sdconfig ac jsexec 'pump_statestr(pumps.ac1.state)'

# Check references (multiple components can reference)
sdconfig ac jsexec 'pumps.ac1.refs'

# Check primer
sdconfig ac jsexec 'pumps.ac1.primer'
```

**Min runtime issues:**
```bash
# Check current runtime
sdconfig ac jsexec 'time() - units.ac1.time'

# Check min_runtime setting
sdconfig ac get_unit ac1 | grep min_runtime

# Adjust min_runtime
sdconfig ac jsexec 'units.ac1.min_runtime = 300'  # 5 minutes for testing
```

**PA communication issues:**
```bash
# Check MQTT connectivity
mosquitto_sub -t '#' -v

# Check PA agent running
systemctl status pa.service

# Test reserve manually
sdconfig ac jsexec 'pa_client_reserve(ac, "unit", "test", 1000, 50)'

# Check logs for repri failures
grep "repri" /opt/sd/log/ac.log
```

---

## Key Design Decisions

### 1. Reference Counting for Pump Sharing
Multiple units/fans can share the same pump. The pump uses reference counting to stay running as long as any component needs it.

### 2. Self-Correcting PA Reservations
PA automatically cleans up stale reservations when a new reservation with the same ID+amount is made. This makes release failures non-critical.

### 3. Fail-Safe on Communication Errors
If reprioritization fails during thermal charging (MQTT/PA unreachable), the unit must stop immediately to prevent running without PA awareness.

### 4. Min Runtime via State Machine
Min runtime is enforced as a state (MIN_RUNTIME_WAIT) rather than a timer, making it visible in logs and allowing safety checks to continue during the wait period.

### 5. Immediate Flag for Emergency Shutdown
PA can force immediate shutdown via `immediate` flag in revoke, bypassing min_runtime for battery protection or other critical situations.

### 6. Temperature-Based Mode Selection
Mode selection is fully automatic based on water temperature thresholds with hysteresis to prevent mode flapping.

---

## Future Enhancements

- [ ] Web dashboard integration
- [ ] Historical temperature trending
- [ ] Predictive mode switching based on forecast
- [ ] Multi-zone temperature management
- [ ] Advanced scheduling integration
- [ ] Energy efficiency metrics

---

## References

- **Main Documentation:** `/home/steve/src/sd/CLAUDE.md`
- **PA Agent:** `/home/steve/src/sd/agents/pa/`
- **PA Client Library:** `/home/steve/src/sd/lib/sd/pa_client.js`
- **SolarDirector Framework:** `/home/steve/src/sd/lib/sd/`

---

**Last Updated:** 2025-12-30
**Version:** 1.0
