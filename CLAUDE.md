# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

This is a C-based solar power and battery management system (SolarDirector) with embedded JavaScript scripting capabilities. The system manages various hardware components including battery management systems (BMS), inverters, A/C units, and other solar equipment through a modular agent framework.

### Key Components

**Agents** (`agents/`) - Hardware-specific control agents:
- **BMS agents**: `jbd/` (JBD BMS), `jk/` (JiKong BMS)
- **Inverter agents**: `si/` (SMA Sunny Island), `sb/` (SMA Sunny Boy)
- **HVAC agent**: `ac/` (Air conditioning control)
- **Power agents**: `pa/`, `pvc/` (Power monitoring)
- **Template agent**: `template/` (Reference for new agents)

**Core Libraries** (`lib/`):
- **lib/js/**: Custom SpiderMonkey JavaScript engine integration
- **lib/sd/**: Core SolarDirector framework (agent system, MQTT, CAN, configuration)
- **lib/smanet/**: SMA protocol implementation for Sunny Island/Boy communication
- **lib/wpi/**: Hardware interface library (GPIO, sensors)

**Utilities** (`utils/`):
- **sdconfig**: Configuration management tool
- **sdjs**: JavaScript runtime for testing and development
- **rdevserver**: Remote device server for network hardware access
- **sc**: Service Controller for centralized agent management and monitoring

## Architecture

### Agent Framework
Each agent is a standalone C program with embedded JavaScript runtime:
- **Standardized structure**: `main.c` (entry point), `driver.c` (hardware interface), `*.js` (business logic)
- **MQTT communication**: All agents communicate via MQTT pub/sub
- **Configuration system**: JSON-based configuration with hot-reload
- **JavaScript integration**: Embedded SpiderMonkey for flexible scripting

### Communication Patterns
- **MQTT**: Primary inter-agent communication (`solar/<agent>/<function>`)
- **CAN bus**: Hardware communication for inverters and BMS
- **SMANet**: SMA-specific protocol for Sunny Island/Boy inverters
- **Serial/Bluetooth**: Alternative transport methods

## Common Development Commands

### Build System
```bash
# Build all agents and libraries
make                           # Build everything with parallel jobs
make clean                     # Clean build artifacts
make install                   # Install to PREFIX (/opt/sd or ~/)

# Build specific components
make -C lib                    # Build libraries only
make -C agents/si              # Build specific agent
make -C utils/sdconfig         # Build utilities
```

### Configuration Management
```bash
# List agent variables and functions
sdconfig -n si -L              # List SI agent variables
sdconfig -n ac -F              # List AC agent functions

# Get/set configuration values
sdconfig si get GdManStr       # Get SI parameter
sdconfig si set GdManStr Start # Set SI parameter
sdconfig pack_01 get BalanceWindow    # Get BMS parameter
sdconfig pack_01 set BalanceWindow 0.15  # Set BMS parameter
```

### Service Controller (Agent Management)
```bash
# Build Service Controller
make -C utils/sc               # Build SC utility

# Command-line usage
./utils/sc/sc -l               # List discovered agents
./utils/sc/sc -s si            # Show SI agent status
./utils/sc/sc -m               # Monitor agents in real-time

# Web interface
cd utils/sc/web
./start_web.sh                 # Start Flask web dashboard
# Open http://localhost:5000 for web-based agent management
```

### Testing and Development
```bash
# Run agent in test mode
./si -c test.json -v -d        # Verbose debug mode with test config

# JavaScript runtime for testing
sdjs                           # Interactive JavaScript REPL
sdjs -f script.js              # Execute JavaScript file

# Service management (Linux)
systemctl start si.service     # Start SI agent service
systemctl status ac.service    # Check AC agent status
systemctl daemon-reload        # Reload after install
```

### Development Tools
```bash
# Create new agent from template
cd agents && ./mkagent newagent "New Agent Description"

# Virtual CAN for testing
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# Monitor MQTT traffic
mosquitto_sub -t '+/+/+'       # All agent messages
mosquitto_sub -t 'solar/si/+'  # SI agent messages only

# Service Controller for agent management
utils/sc/sc -l                 # List all discovered agents
utils/sc/sc -s si              # Show SI agent status
utils/sc/sc -m                 # Monitor all agents in real-time
```

## Architecture Notes

### Build Configuration
- **make.opts**: Global build options (JS, MQTT, INFLUX, BLUETOOTH support)
- **Makefile.sys**: Platform-specific settings (Linux, macOS, Windows cross-compilation)
- **Makefile.sd**: Core build templates for agents and libraries

### Agent Development Pattern
Each agent follows a standard structure:
1. **main.c**: Command-line interface, configuration loading, main loop
2. **driver.c**: Hardware-specific communication (CAN, serial, SMANet)
3. **jsfuncs.c**: C functions exposed to JavaScript
4. **JavaScript files**: Business logic (`init.js`, `read.js`, `write.js`, `pub.js`)
5. **test.json**: Test configuration with virtual interfaces
6. **Makefile**: Agent-specific build configuration

### Utility Development Pattern
Utilities (non-agents) use a different pattern:
1. **main.c**: Uses `client_init()` instead of `agent_init()`
2. **Core functionality in C**: Performance-critical operations
3. **Optional JavaScript**: Business logic and extensibility
4. **Client flags**: `CLIENT_FLAG_NOINFLUX | CLIENT_FLAG_NOEVENT`
5. **Examples**: `sdconfig`, `sc`, `sdjs`

### JavaScript Integration
- **Embedded SpiderMonkey**: Each agent runs JavaScript in embedded context
- **C-JS bridge**: Native functions exposed via `jsfuncs.c`
- **Hot-reload**: JavaScript can be updated without recompiling C code
- **Agent-specific context**: Isolated JavaScript environments per agent

### Configuration System
- **JSON-based**: All configuration in JSON format
- **Hierarchical**: Compile-time defaults → JSON files → MQTT updates → environment variables
- **Hot-reload**: Configuration changes applied without restart
- **MQTT distribution**: Configuration distributed via MQTT to running agents

## Dependencies

### Required System Libraries
```bash
# Debian/Ubuntu
apt-get install libssl-dev libcurl4-openssl-dev libreadline-dev

# Optional Bluetooth support
apt-get install libglib2.0-dev libbluetooth-dev

# macOS
brew install paho-mqtt-c
```

### MQTT Library (Linux)
```bash
# Paho MQTT C library
git clone https://github.com/eclipse/paho.mqtt.c.git
cd paho.mqtt.c
mkdir build && cd build
cmake -DPAHO_WITH_SSL=ON -DPAHO_BUILD_SHARED=TRUE -DPAHO_BUILD_STATIC=TRUE -DPAHO_ENABLE_TESTING=FALSE -DPAHO_BUILD_SAMPLES=FALSE ..
make && make install
```

## Platform Support

- **Linux**: Primary target platform with full feature support
- **macOS**: Development platform with dynamic linking
- **Raspberry Pi**: Embedded target with GPIO support
- **Windows**: Cross-compilation support (limited testing)

## Testing Approach

Agents use JSON configuration files for testing with virtual interfaces:
- **test.json**: Agent-specific test configuration
- **Virtual CAN**: `vcan0` interface for CAN bus simulation
- **Local MQTT**: `tcp://localhost:1883` for testing
- **Memory debugging**: Enabled in test configurations
- **JavaScript testing**: Business logic testing via embedded scripts

### Running Tests
```bash
# Test individual agents with debug output
./agents/si/si -c agents/si/test.json -v -d

# Test JavaScript functions in isolation
sdjs -f agents/pa/pa.js

# Monitor all MQTT traffic during tests
mosquitto_sub -t '#' -v

# Test with memory debugging enabled
DEBUG_MEM=yes make clean all
./agent -c test.json 2>&1 | grep "Memory"
```

## Code Quality and Linting

Currently no automated linting, but follow these conventions:
- **C code**: K&R style with tabs for indentation
- **JavaScript**: Business logic in separate .js files
- **Naming**: snake_case for C, camelCase for JavaScript
- **Headers**: Include guards in all .h files

## Important Technical Notes

### JavaScript Engine Fix
The SpiderMonkey integration had undefined behavior issues with negative value shifting in JSVAL_VOID macro. This has been fixed in `/lib/js/jsapi.h` line 213. See `JS_ENGINE_FIX.md` for details.

### Agent Communication Flow
```
Hardware ←→ Driver (C) ←→ JavaScript ←→ MQTT ←→ Other Agents
                ↓
          Configuration (JSON)
```

### Service Installation
```bash
# Agents install as systemd services
make install                    # Installs to PREFIX
systemctl daemon-reload         # Reload systemd
systemctl enable si.service     # Enable on boot
systemctl start si.service      # Start service
journalctl -u si.service -f     # View logs
```

### Common Troubleshooting
```bash
# Check MQTT connectivity
mosquitto_pub -t test -m test
mosquitto_sub -t test

# Verify CAN interface
ip link show can0
candump can0

# Debug agent startup
strace ./agent -c config.json

# Check library dependencies
ldd ./agent
```