# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

siutil is a command-line utility for interacting with SMA Sunny Island (SI) inverters. It provides both CAN bus and SMANet protocol communication for monitoring, configuration, and control operations. The utility is part of the larger SolarDirector framework but can operate standalone.

## Key Components

**Core Files**:
- **main.c**: Command-line interface, argument parsing, protocol initialization, and main execution logic
- **display.c**: Output formatting functions for different formats (text, JSON, CSV)
- **siutil.h**: Header file with utility-specific declarations

**Inherited SI Components** (from agents/si/):
- **can.c**: CAN bus communication for Sunny Island protocol
- **smanet.c**: SMANet protocol implementation for parameter access
- **driver.c**: Hardware interface and data processing
- **info.c**: Inverter information and status queries
- **si.h**: Core SI data structures and function prototypes

## Architecture

### Communication Protocols
The utility supports two communication methods:
- **CAN bus**: Direct hardware communication using SocketCAN (Linux) or serial CAN adapters
- **SMANet**: RS485-based protocol for parameter configuration and monitoring

### Data Flow
```
Hardware Interface → Protocol Layer → Data Processing → Output Formatting
     (CAN/Serial)      (SI/SMANet)     (conversion)      (text/JSON/CSV)
```

### Output Formats
- **Text**: Human-readable columnar format (default)
- **JSON**: Machine-readable JSON output (-j for compact, -J for pretty-printed)
- **CSV**: Comma-separated values (-C flag)

## Common Development Commands

### Build System
```bash
# Build siutil 
make                           # Build utility with SMANET support enabled

# Clean build artifacts
make clean                     # Remove compiled objects and executable

# The Makefile inherits from Makefile.sd with these settings:
# JS=no, MQTT=no, INFLUX=no, SMANET=yes
```

### Testing and Usage
```bash
# Monitor SI inverter in real-time
./siutil -m                    # Basic monitoring mode
./siutil -m -n 5               # Monitor with 5-second intervals

# Get/set parameters via SMANet
./siutil -u serial,/dev/ttyS0,19200 -a    # Display all SMANet values
./siutil ParamName                          # Get specific parameter
./siutil ParamName NewValue                 # Set parameter value

# Start/stop inverter via CAN bus
./siutil -t can,can0,500000 -R  # Start inverter
./siutil -t can,can0,500000 -S  # Stop inverter

# File-based configuration
./siutil -f settings.txt        # Apply settings from file
./siutil -f settings.txt -v     # Verify settings only (dry-run)

# Output formatting
./siutil -j                     # JSON output
./siutil -J                     # Pretty-printed JSON
./siutil -C                     # CSV output
./siutil -o output.json -J      # Save to file
```

### Configuration Files
The utility looks for `siutil.conf` in standard locations:
- Current directory
- /etc/sd/
- /opt/sd/etc/

Configuration sections:
```ini
[siutil]
can_transport = can
can_target = can0
can_topts = 500000
smanet_transport = serial
smanet_target = /dev/ttyS0
smanet_topts = 19200
smanet_channels_path = /path/to/channels.json
```

## Development Patterns

### Adding New Actions
New functionality should follow the ACTION enum pattern:
1. Add enum value in `main.c` (ACTION_*)
2. Add command-line option in opts[] array
3. Implement action logic in main() switch statement
4. Add output formatting in display.c if needed

### Protocol Communication
- **CAN operations**: Use `si_can_*` functions for direct hardware control
- **SMANet operations**: Use `smanet_*` functions for parameter access
- **Error handling**: Check return values and use `log_error()` for reporting

### Output Formatting
Use the global `outfmt` variable to determine output format:
- `outfmt = 0`: Text format
- `outfmt = 1`: CSV format  
- `outfmt = 2`: JSON format

Functions `dint()`, `ddouble()`, `dstr()` in display.c handle format-specific output.

## Testing Approach

### Virtual CAN Testing
```bash
# Set up virtual CAN interface
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# Test with virtual interface
./siutil -t can,vcan0,500000 -m
```

### SMANet Testing
```bash
# Test with serial loopback or hardware
./siutil -u serial,/dev/ttyUSB0,19200 -l  # List available channels
./siutil -u serial,/dev/ttyUSB0,19200 -a  # Display all values
```

### Configuration Testing
```bash
# Test configuration loading
./siutil -c test_siutil.conf -m

# Test file-based settings
echo "ParamName 123.45" > test_settings.txt
./siutil -f test_settings.txt -v  # Verify without applying
```

## Architecture Notes

### Inheritance from SI Agent
siutil reuses significant code from the SI agent through:
- **Makefile vpath**: Sources files from `$(SRC_ROOT)/agents/si`
- **Include path**: Adds SI agent headers with `-I $(SRC_ROOT)/agents/si`
- **Shared objects**: Links same driver, CAN, and SMANet code

### Key Differences from SI Agent
- **No JavaScript runtime**: JS=no in Makefile disables embedded scripting
- **No MQTT**: MQTT=no disables message publishing
- **No InfluxDB**: INFLUX=no disables time-series logging
- **Utility-focused**: Uses `si_config()` stub instead of full agent configuration

### Memory Management
- Uses SolarDirector framework's memory debugging when `DEBUG_MEM=yes`
- JSON objects created/destroyed for output formatting
- Lists and arrays properly cleaned up after use

### Platform Support
- **Linux**: Primary platform with full CAN and serial support
- **Windows**: Limited support with serial-only communication
- **macOS**: Development platform with CAN simulation capabilities

## Dependencies

### Required Libraries
- **libssl-dev**: For cryptographic functions
- **libcurl4-openssl-dev**: For HTTP operations (if enabled)
- **SolarDirector core libraries**: Automatically built as dependencies

### Optional Libraries
- **CAN utils**: For hardware CAN bus support on Linux
- **SMANet channels**: JSON files with protocol definitions