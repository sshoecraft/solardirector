# PA Agent - Power Allocator

## Overview

The Power Allocator (PA) agent is a centralized power management system that controls power reservations across multiple agents in a solar power system. It monitors available power from solar panels, grid, and batteries, then grants or revokes power reservations to requesting agents (AC units, fans, pumps, etc.) based on real-time power availability and configured limits.

## Key Features

- **Centralized Power Management**: Single point of control for all power-consuming agents
- **Real-time Power Calculation**: Monitors grid, battery, and solar power to calculate available power
- **Priority-based Reservations**: Supports priority levels (1-100) for intelligent power allocation
- **Battery Protection**: Hard and soft limits prevent excessive battery discharge
- **Day/Night Modes**: Different power budgets and approval rules for day vs. night operation
- **Immediate Revocation**: Critical shutdown capability for battery protection
- **Multi-broker Support**: Can read data from agents on different MQTT brokers
- **Data Staleness Detection**: Detects and handles stale sensor data

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        PA Agent                              │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   Inverter   │  │      PV      │  │   Battery    │      │
│  │  MQTT Data   │  │  MQTT Data   │  │  MQTT Data   │      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
│         │                  │                  │              │
│         └──────────────────┴──────────────────┘              │
│                            │                                 │
│                    ┌───────▼────────┐                        │
│                    │ Power          │                        │
│                    │ Calculator     │                        │
│                    └───────┬────────┘                        │
│                            │                                 │
│         ┌──────────────────┴──────────────────┐             │
│         │                                      │             │
│  ┌──────▼────────┐                   ┌────────▼──────┐      │
│  │ Reservation   │                   │  Revocation   │      │
│  │ Manager       │                   │  Manager      │      │
│  └───────────────┘                   └───────────────┘      │
│                                                              │
└─────────────────────────────────────────────────────────────┘
                            │
                            │ MQTT Functions
                            │ (reserve, release, repri, revoke)
                            │
        ┌───────────────────┴───────────────────┐
        │                   │                   │
   ┌────▼────┐         ┌────▼────┐       ┌─────▼─────┐
   │ AC Unit │         │   Fan   │       │   Pump    │
   │  Agent  │         │  Agent  │       │   Agent   │
   └─────────┘         └─────────┘       └───────────┘
```

## Operation Modes

### Budget Mode
When `budget` is set to a non-zero value, PA uses a fixed power budget:
```
available_power = budget - reserved_power
```

### Calculated Mode (budget = 0)
PA calculates available power from real-time sensor data:
```
available_power = (grid_export + pv_excess + battery_charge) - (grid_import + battery_discharge) - load_power
```

Averaged over `samples` readings taken every `interval` seconds.

### Day/Night Modes
PA automatically switches between day and night modes based on time:

- **Day Mode**: Uses `budget` and `approve_p1` settings
- **Night Mode**: Uses `night_budget` and `night_approve_p1` settings
- Transition times: Configurable via `night_start` and `day_start` (supports HH:MM format and sunrise/sunset)

## Configuration Parameters

### Data Sources

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `inverter_server_config` | string | "localhost" | MQTT broker for inverter data |
| `inverter_topic` | string | "solar/agents/si/Data" | MQTT topic for inverter data |
| `grid_power_property` | string | "input_power" | Property name for grid power |
| `invert_grid_power` | bool | false | Invert grid power sign |
| `load_power_property` | string | "load_power" | Property name for load power |
| `invert_load_power` | bool | false | Invert load power sign |
| `frequency_property` | string | "output_frequency" | Property name for frequency |
| `charge_mode_property` | string | "charge_mode" | Property name for charge mode |
| `pv_server_config` | string | "localhost" | MQTT broker for PV data |
| `pv_topic` | string | "solar/agents/pvc/Data" | MQTT topic for PV data |
| `pv_power_property` | string | "output_power" | Property name for PV power |
| `invert_pv_power` | bool | false | Invert PV power sign |
| `battery_server_config` | string | "localhost" | MQTT broker for battery data |
| `battery_topic` | string | "solar/agents/si/Data" | MQTT topic for battery data |
| `battery_power_property` | string | "battery_power" | Property name for battery power |
| `invert_battery_power` | bool | false | Invert battery power sign |
| `battery_level_property` | string | "battery_level" | Property name for battery level (%) |

### Power Management

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `budget` | float | 0.0 | Fixed power budget (W). 0 = calculate from sensors |
| `night_budget` | float | 0.0 | Night-time power budget (W). 0 = use budget |
| `buffer` | int | 0 | Power buffer/headroom (W) |
| `limit` | float | 0.0 | Maximum deficit allowed before P1 protection override (W) |
| `reserve_delay` | int | 180 | Delay between reserve attempts (seconds) |
| `deficit_timeout` | int | 300 | Time in deficit before revoking (seconds) |
| `samples` | int | (calculated) | Number of power samples to average |
| `sample_period` | int | 90 | Period over which to average power (seconds) |
| `interval` | int | 15 | Agent read interval (seconds) |
| `data_stale_interval` | int | 3 | Multiplier for staleness timeout (× interval) |

### Battery Protection

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `battery_level_min` | float | 0.0 | Minimum battery level (%) for new reservations |
| `battery_soft_limit` | float | 0.0 | Battery discharge soft limit (W) - sets avail negative |
| `battery_hard_limit` | float | 0.0 | Battery discharge hard limit (W) - immediate revoke all |
| `battery_scale` | bool | true | Scale available power based on battery level |
| `protect_charge` | bool | false | Protect battery charging power from allocation |

### Priority Control

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `approve_p1` | bool | false | Allow P1 reservations even in deficit |
| `night_approve_p1` | bool | false | Allow P1 reservations in deficit during night |

### Location & Time

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `location` | string | "" | Latitude,longitude for sunrise/sunset calculation |
| `night_start` | string | "20:00" | Night period start (HH:MM or "sunset+minutes") |
| `day_start` | string | "06:00" | Day period start (HH:MM or "sunrise+minutes") |

### Frequency Shift Power Control (FSPC)

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `fspc` | bool | false | Enable frequency shift power control |
| `fspc_start` | float | 1.0 | Frequency offset to start reducing power (Hz) |
| `fspc_end` | float | 2.0 | Frequency offset for full power reduction (Hz) |
| `nominal_frequency` | float | 60.0 | Nominal grid frequency (Hz) |

## MQTT Functions

### reserve
Request a power reservation.

**Signature**: `reserve(agent_name, module, item, amount, priority)`

**Parameters**:
- `agent_name` (string): Name of requesting agent
- `module` (string): Module name within agent (e.g., "unit", "fan")
- `item` (string): Item name (e.g., "bedroom_ac")
- `amount` (float): Power requested in watts
- `priority` (int): Priority level 1-100 (1 = highest)

**Returns**:
- `status`: 0 = success, 1 = denied
- `message`: Error message if denied

**Denial Reasons**:
- Battery level below `battery_level_min`
- Insufficient available power (unless P1 and `approve_p1` = true)
- Reserve delay not elapsed
- P1 deficit would exceed `limit`

**Example**:
```javascript
sdconfig(agent, "pa", "reserve", "ac", "unit", "bedroom", 1500, 1);
```

### release
Release a power reservation.

**Signature**: `release(agent_name, module, item, amount)`

**Parameters**:
- `agent_name` (string): Name of agent releasing power
- `module` (string): Module name
- `item` (string): Item name
- `amount` (float): Power to release (not used, matched by ID only)

**Returns**:
- `status`: 0 = success, 1 = not found
- `message`: Error message if not found

**Example**:
```javascript
sdconfig(agent, "pa", "release", "ac", "unit", "bedroom", 1500);
```

### repri
Change priority of existing reservation or create new one.

**Signature**: `repri(agent_name, module, item, amount, priority)`

**Parameters**:
- Same as `reserve`

**Behavior**:
- If reservation exists: Updates priority only
- If reservation doesn't exist: Creates new reservation (same rules as `reserve`)

**Example**:
```javascript
sdconfig(agent, "pa", "repri", "ac", "unit", "bedroom", 1500, 10);
```

### revoke_all
Revoke all active reservations.

**Signature**: `revoke_all(immediate)`

**Parameters**:
- `immediate` (string): "true"/"1" = immediate revoke, "false"/"0" = normal revoke

**Behavior**:
- Queues all active reservations for revocation
- `immediate` flag passed to agents for shutdown behavior

**Example**:
```javascript
sdconfig(agent, "pa", "revoke_all", "true");  // Emergency shutdown
```

## Reservation ID Format

Reservations are identified by:
```
{agent_name}/{module}/{item}
```

Example: `ac/unit/bedroom_ac`

Each unique ID can have only one active reservation. New reserve calls with the same ID replace the previous reservation.

## Power Calculation Details

### Grid Power
- **Positive**: Feeding power to grid (export)
- **Negative**: Drawing power from grid (import)
- Only counted in deficit if importing

### Battery Power
- **Positive**: Charging battery
- **Negative**: Discharging battery
- Charging power can be reallocated unless `protect_charge` = true

### PV Power
Contribution calculated as:
```
pv_contribution = pv_power - load_power
```

If load power not available:
```
load_power = pv_power - grid_export - battery_charge
```

### Power Averaging
PA maintains a rolling average over `samples` readings:
```
samples = sample_period / interval
average_power = sum(last_N_samples) / N
```

Default: 90s period / 15s interval = 6 samples

## Revocation Logic

### Normal Revocation (immediate = false)
Triggered when:
- Available power < 0 for longer than `deficit_timeout` (default 300s)

Behavior:
- Calls agent's revoke function with `immediate = false`
- Agent performs graceful shutdown (respects min_runtime, COOLDOWN states)
- P1 reservations protected unless `limit` exceeded

### Immediate Revocation (immediate = true)
Triggered when:
- Battery discharge exceeds `battery_hard_limit`

Behavior:
- Calls agent's revoke function with `immediate = true`
- Agent performs emergency shutdown (bypasses min_runtime, force stop)
- All reservations revoked immediately

### Soft Limit
When battery discharge exceeds `battery_soft_limit`:
- Sets `available_power = -1` (denies new reservations)
- Does NOT revoke existing reservations
- Prevents deficit from growing

## Battery Protection

### Three-tier Protection

1. **Soft Limit** (`battery_soft_limit`):
   - Sets avail negative to prevent new reservations
   - Existing reservations continue
   - Recovers automatically when discharge drops

2. **Hard Limit** (`battery_hard_limit`):
   - Immediate revocation of ALL reservations
   - Emergency protection for battery
   - Uses immediate flag for fastest shutdown

3. **Minimum Level** (`battery_level_min`):
   - Prevents new reservations when battery level too low
   - Only applies when battery is discharging
   - Existing reservations not affected

### Grid Detection
Battery limits only enforced when **off-grid**:
- On-grid detected when grid import > 100W
- Prevents false triggers during grid power usage

## Data Staleness

PA tracks last update time for each data source:
- `grid_power_time`
- `battery_power_time`
- `pv_power_time`
- `charge_mode_time`
- `battery_level_time`
- `frequency_time`

Data marked stale after:
```
stale_timeout = data_stale_interval × interval
```

Default: 3 × 15s = 45 seconds

Stale data not used in power calculations.

## Integration with Clients

### Client Setup
Clients use `pa_client.js` library:

```javascript
include(sdlib_dir+"/pa_client.js");
pa_client_init(agent);
```

### Reserving Power
```javascript
// Reserve 1500W at priority 1 for bedroom AC unit
pa_client_reserve(agent, "unit", "bedroom_ac", 1500, 1);
```

### Releasing Power
```javascript
pa_client_release(agent, "unit", "bedroom_ac", 1500);
```

### Handling Revoke
Clients must implement module-specific revoke handlers:

```javascript
function unit_revoke(name, amount, immediate) {
    if (immediate) {
        // Emergency shutdown - bypass min_runtime
        return unit_force_stop(name);
    } else {
        // Normal shutdown - respect min_runtime and COOLDOWN
        return unit_stop(name);
    }
}
```

PA calls via MQTT:
```
Topic: solar/agents/{agent_name}
Message: {"revoke": [["unit", "bedroom_ac", "1500", "true"]]}
```

## Multi-Broker Configuration

PA can read data from agents on different MQTT brokers:

```json
{
  "inverter_server_config": "192.168.1.100:1883",
  "pv_server_config": "localhost:1883",
  "battery_server_config": "192.168.1.100:1883"
}
```

Each creates a separate MQTT connection (`inverter_mqtt`, `pv_mqtt`, `battery_mqtt`).

**Note**: Revoke commands always sent via PA's primary MQTT broker. For agents on different brokers, use MQTT bridging to sync topics.

## Day/Night Mode Details

### Mode Determination
```javascript
is_night = current_time >= night_start || current_time < day_start
```

### Mode Transition
When transitioning from day → night:
- Switches to `night_budget`
- Recalculates `available_power = night_budget - reserved`
- Uses `night_approve_p1` for new reservations

When transitioning from night → day:
- Switches to `budget`
- Recalculates `available_power = budget - reserved`
- Uses `approve_p1` for new reservations

### Sunrise/Sunset Support
Requires `location` parameter set:

```json
{
  "location": "34.0522,-118.2437",
  "night_start": "sunset+120",
  "day_start": "sunrise+30"
}
```

Format: `"sunrise+minutes"` or `"sunset+minutes"`

## Logging

PA logs important events:
- Reservations granted/denied
- Revocations (normal and immediate)
- Mode transitions (day/night)
- Battery limit violations
- Power availability changes

Log levels:
- `log_info()`: Normal operations
- `log_warning()`: Battery hard limit exceeded
- `log_error()`: Configuration errors

## Testing

### Test Configuration
See `test.json` for example configuration.

### Manual Testing
```bash
# Start PA agent
./pa -c test.json -d 2

# In another terminal, test reservations
mosquitto_pub -t 'solar/agents/pa' \
  -m '{"reserve":[["test_agent","module","item","1000","1"]]}'

# Check status
mosquitto_sub -t 'solar/agents/pa/Data'

# Test revoke_all
mosquitto_pub -t 'solar/agents/pa' \
  -m '{"revoke_all":[["true"]]}'
```

### Debug Levels
- Level 0: Errors only
- Level 1: Info + warnings
- Level 2: Detailed operation
- Level 3+: Verbose debugging

## Common Use Cases

### Example 1: Simple Budget Mode
```json
{
  "budget": 5000,
  "approve_p1": false,
  "deficit_timeout": 300
}
```
- Fixed 5kW budget
- No P1 protection
- Revoke after 5 minutes in deficit

### Example 2: Battery Protection
```json
{
  "budget": 0,
  "battery_soft_limit": 3000,
  "battery_hard_limit": 4000,
  "battery_level_min": 20
}
```
- Calculate from sensors
- Warn at 3kW discharge
- Emergency stop at 4kW discharge
- No new reserves below 20% battery

### Example 3: Day/Night Operation
```json
{
  "budget": 3000,
  "night_budget": 1000,
  "approve_p1": true,
  "night_approve_p1": false,
  "night_start": "sunset+120",
  "day_start": "sunrise+30",
  "location": "34.0522,-118.2437"
}
```
- 3kW day budget, 1kW night budget
- P1 allowed during day, denied at night
- Automatic sunrise/sunset tracking

### Example 4: Multi-Agent System
```json
{
  "budget": 0,
  "inverter_server_config": "192.168.1.100:1883",
  "pv_server_config": "192.168.1.101:1883",
  "battery_server_config": "192.168.1.100:1883",
  "sample_period": 120,
  "interval": 20
}
```
- Read from multiple MQTT brokers
- 2-minute averaging period
- 20-second read interval

## Troubleshooting

### Problem: Reservations Always Denied
**Check**:
- Is available power > 0? (check logs or Data topic)
- Is `reserve_delay` preventing rapid reserves?
- Is battery level below `battery_level_min`?
- Is data from inverter/PV/battery fresh?

### Problem: No Revocations
**Check**:
- Is available power actually negative? (check calculations)
- Has `deficit_timeout` elapsed?
- Are all reservations P1 with `approve_p1` = true?

### Problem: Immediate Revoke Not Working
**Check**:
- Is battery discharge > `battery_hard_limit`?
- Is PA detecting on-grid? (Grid import > 100W bypasses limits)
- Are client agents implementing immediate flag correctly?

### Problem: Stale Data
**Check**:
- Are MQTT topics correct?
- Are agents publishing data?
- Is `data_stale_interval` too short?
- Check MQTT broker connectivity

## Files

- `main.c` - Agent entry point
- `driver.c` - Hardware/driver interface (minimal for PA)
- `init.js` - Initialization, property/function registration
- `read.js` - Main power calculation and revocation logic
- `utils.js` - Helper functions (message processing, revoke, time parsing)
- `funcs.js` - MQTT function implementations (reserve, release, repri, revoke_all)
- `test.json` - Test configuration

## Dependencies

- `lib/sd/pa_client.js` - Client library for agents requesting power
- `lib/sd/sdconfig.js` - Remote agent configuration/function calls
- `lib/sd/init.js` - Standard agent initialization
- MQTT broker (Mosquitto recommended)

## Version History

See git log for detailed change history.

Recent major changes:
- Added immediate flag for critical revocations
- Added battery hard/soft limit protection
- Added day/night mode support
- Added multi-broker MQTT support
- Added data staleness detection

## See Also

- `lib/sd/pa_client.js` - Client integration guide
- `agents/ac/README.md` - AC agent implementation example
- `CLAUDE.md` - Overall system architecture
