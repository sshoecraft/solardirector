# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

This is the AC (Air Conditioning) agent for the SolarDirector system. It manages HVAC equipment including fans, pumps, and temperature-controlled units through CAN bus communication and GPIO control. The agent integrates with the solar power system to optimize HVAC operation based on available energy.

### Key Components

**Core C Framework**:
- **ac.h**: Main header defining AC session structure and component lists
- **main.c**: Entry point using standard SolarDirector agent framework
- **driver.c**: Driver implementation for agent lifecycle management
- **can.c**: CAN bus communication handling (IDs 0x450-0x45F)
- **jsfuncs.c**: JavaScript bindings exposing C functions to JS runtime

**JavaScript Business Logic**:
- **mode.js**: Automatic heat/cool mode selection based on weighted temperature averages
- **fan.js**: Fan control with temperature-based stopping and state management
- **pump.js**: Pump management with reference counting for shared resources
- **unit.js**: HVAC unit control with cooling/heating pin management
- **charge.js**: Temperature threshold management for battery charging integration
- **cycle.js**: HVAC cycling logic for efficiency
- **sample.js**: Periodic temperature sampling from pumps

**Communication**:
- **CAN bus**: Hardware control via SocketCAN (configurable speed, default 500kbps)
- **MQTT**: Inter-agent communication and fan data subscription
- **InfluxDB**: Time-series data storage for temperatures and states
- **GPIO**: Direct hardware control via WiringPi

## Common Development Commands

### Build and Test
```bash
# Build the AC agent
make                           # Build with all features enabled
make clean                     # Clean build artifacts
make install                   # Install to PREFIX (/opt/sd or ~/)

# Test with virtual CAN
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
./ac -c test.json -v -d        # Run in verbose debug mode

# Monitor operation
mosquitto_sub -t 'solar/ac/+'  # Watch AC messages
mosquitto_sub -t 'solar/hvac/+/data'  # Watch fan data
```

### Configuration Management
```bash
# Using sdconfig utility
sdconfig ac get interval       # Get main loop interval
sdconfig ac set cool_high_temp 75.0  # Set cooling threshold
sdconfig ac list               # List all AC properties

# Add components via JavaScript
sdconfig ac jsexec 'pump_add("pump1", { pin: 17 })'
sdconfig ac jsexec 'fan_add("fan1", { topic: "solar/hvac/fan1/data", pump: "pump1" })'
sdconfig ac jsexec 'unit_add("unit1", { coolpin: 22, heatpin: 23, pump: "pump1" })'
```

### Service Management
```bash
systemctl start ac.service     # Start AC service
systemctl status ac.service    # Check status
journalctl -u ac.service -f    # Follow logs
```

## Architecture Notes

### Component Model
The AC agent manages three types of components:
1. **Pumps**: Water circulation with temperature sensors (in/out)
2. **Fans**: Air handlers that subscribe to external fan controller data
3. **Units**: HVAC units with cooling/heating/reversing valve control

### Temperature Management
- **Water temperature**: Average of all pump temperatures drives mode selection
- **Weighted average**: Configurable weighting between 0-1 (0=temp_in only, 1=temp_out only)
- **Mode thresholds**: Separate high/low temperatures for cooling and heating modes
- **Hysteresis**: Built-in to prevent mode flapping

### State Machines
Components use state machines for robust operation:
- **Fans**: STOPPED → STARTING → RUNNING → STOPPING
- **Pumps**: STOPPED ↔ RUNNING (with reference counting)
- **Units**: Mode transitions with configurable delays

### Power Integration
- **PA (Power Agent) integration**: Can disable HVAC based on available power
- **Charge temperature**: Special thresholds during battery charging
- **Load management**: Graceful shutdown when power is limited

## Configuration Properties

### Core Settings
- **interval**: Main loop interval in seconds (default: 10)
- **sample_interval**: Temperature sampling interval (default: 300)
- **location**: "lat,lon" for sunrise/sunset calculations
- **standard**: 0=US/Imperial, 1=Metric units

### Temperature Thresholds
- **cool_high_temp**: Start cooling above this water temp
- **cool_low_temp**: Stop cooling below this water temp  
- **heat_high_temp**: Stop heating above this water temp
- **heat_low_temp**: Start heating below this water temp
- **charge_cool_high_temp**: Cooling threshold during charging
- **charge_heat_low_temp**: Heating threshold during charging

### Hardware Configuration
- **can_target**: CAN interface name (e.g., "can0", "vcan0")
- **can_topts**: CAN options string (default: "500000" for speed)
- **pump_failsafe_enable**: GPIO pin for pump failsafe
- **waterTempWeight**: 0-1 weight for temp averaging

## Development Patterns

### Adding New Features
1. **Hardware interface**: Add to can.c or GPIO control
2. **C bindings**: Expose via jsfuncs.c if needed by JavaScript
3. **Business logic**: Implement in appropriate JavaScript module
4. **State management**: Follow existing patterns in fan.js/pump.js
5. **Error handling**: Use errors.js for consistent error tracking

### Testing Approach
- **Unit testing**: Test JavaScript logic independently
- **Integration testing**: Use test.json with virtual CAN
- **Hardware testing**: GPIO pins can be monitored/simulated
- **MQTT testing**: Simulate external agents with mosquitto_pub

### Debugging
```bash
# Enable debug logging
./ac -c test.json -d 4         # Maximum debug level

# JavaScript debugging
sdconfig ac jsexec 'dprintf(0, "pumps: %s", JSON.stringify(pumps))'

# CAN bus monitoring
candump vcan0                  # Watch CAN traffic
cansend vcan0 450#0102030405   # Send test frames
```

## Integration Points

### MQTT Topics
- **Publish**: `solar/ac/data` - Main AC state and temperatures
- **Subscribe**: `solar/hvac/+/data` - Fan controller data
- **Commands**: `solar/ac/+` - Control commands

### InfluxDB Measurements
- **ac**: Main temperatures and modes
- **fans**: Individual fan states and speeds
- **pumps**: Pump states and temperatures
- **units**: Unit modes and pin states
- **errors**: Error events with descriptions

### CAN IDs
- **0x450-0x45F**: AC agent data range
- **Frame format**: Standard 11-bit identifiers
- **Data encoding**: Application-specific per ID