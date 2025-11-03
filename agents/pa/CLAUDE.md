# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

This is the **Power Agent (PA)** within the Solar Director framework - a sophisticated power management system that acts as a **power advisor and reservation system** for solar energy installations.

### Primary Functions

**Power Budget Management**: Monitors available power from multiple sources (grid, solar PV, battery) and calculates excess power available for allocation to loads.

**Power Reservation System**: Allows other agents to reserve power for specific loads/devices with priority-based allocation (1-100 priority levels).

**Dynamic Power Monitoring**: Real-time tracking of power flows with automatic revocation of reservations when power becomes insufficient.

**Night Budget Mode**: Special budget allocation for nighttime operations with configurable time periods and sunrise/sunset calculations.

## Architecture

### Code Structure
```
├── main.c          # Entry point and command-line interface
├── pa.h            # Session structure and constants
├── driver.c        # Transport layer (IP, serial, CAN, Bluetooth)
├── config.c        # Configuration initialization and callbacks
├── info.c          # Agent metadata and info reporting
├── jsfuncs.c       # JavaScript-C bridge functions
└── pa.js           # Main business logic (764 lines)
```

### JavaScript Business Logic
The core intelligence is implemented in `pa.js` with functions for:
- **Power calculation and averaging**: `pa_get_number()`, power sampling and averaging
- **Reservation management**: `pa_reserve()`, `pa_release()`, `pa_repri()`, `pa_revoke()`
- **MQTT message processing**: `pa_process_messages()` for real-time data
- **Night budget handling**: `pa_is_night_period()`, time-based budget switching

### Communication Pattern
```
MQTT Topic Structure: solar/{agent}/{function}
- Subscribes to: solar/si/data, solar/pvc/data (power data)
- Publishes to: solar/pa/data (reservation status)
```

## Common Development Commands

### Build and Test
```bash
# Build PA agent
make

# Run in test mode with verbose debug
./pa -c test.json -v -d

# Test with different configurations
./pa -c test.json -s script_dir=.

# Build from parent directory
make -C ../../agents/pa
```

### Configuration Management
```bash
# List PA agent variables
sdconfig -n pa -L

# Get/set PA configuration
sdconfig pa get budget
sdconfig pa set budget 5000
sdconfig pa get night_budget
sdconfig pa set night_start "22:00"
```

### Service Management
```bash
# Install and start PA service
make install
systemctl start pa.service
systemctl status pa.service

# Monitor PA MQTT messages
mosquitto_sub -t 'solar/pa/+'
```

### JavaScript Development
```bash
# Test JavaScript functions interactively
sdjs -f pa.js

# Debug power calculations
./pa -c test.json -v -d | grep -E "(power|budget|reserve)"
```

## Architecture Notes

### Power Source Integration
The PA agent integrates with multiple power sources:
- **Inverter Agent (SI)**: Provides grid power, load power, frequency data via SMANet
- **PV Controller (PVC)**: Provides solar PV power data  
- **Battery Agents (JBD/JK)**: Provides battery power and level data
- **Load Agents**: Request power reservations via MQTT function calls

### Key Configuration Properties
```json
{
    "inverter_server_config": "tcp://localhost:1883",
    "inverter_topic": "solar/si/data",
    "pv_server_config": "tcp://localhost:1883", 
    "pv_topic": "solar/pvc/data",
    "battery_server_config": "tcp://localhost:1883",
    "battery_topic": "solar/pack_01/data",
    "budget": 0,
    "night_budget": 1000,
    "samples": 5,
    "sample_period": 10,
    "reserve_delay": 30,
    "protect_charge": true,
    "approve_p1": true,
    "night_start": "22:00",
    "day_start": "06:00"
}
```

### Priority System
- **P1 (Priority 1)**: Critical loads with auto-approval
- **P2-P100**: Standard priority levels with manual approval
- **Automatic revocation**: When power becomes insufficient, lowest priority reservations are revoked first

### Build Configuration
```makefile
NAME=pa
JS=yes        # JavaScript engine enabled
MQTT=yes      # MQTT communication enabled
INFLUX=yes    # InfluxDB logging enabled
```

## Development Patterns

### Adding New Power Sources
1. Add server configuration and topic properties to `config.c`
2. Update `pa_process_messages()` in `pa.js` to handle new data
3. Modify power calculation logic to incorporate new source
4. Update property mappings for power values

### Extending Reservation System
1. Modify reservation structure in `pa.js`
2. Add new MQTT function handlers for reservation management
3. Update priority logic and revocation algorithms
4. Test with multiple concurrent reservations

### Testing Power Calculations
Use the test configuration with virtual data sources to verify:
- Power averaging and sampling
- Reservation allocation and revocation
- Night/day budget switching
- Priority-based load management