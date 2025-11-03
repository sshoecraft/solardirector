# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

This is the **lib/sd** directory - the core C library of the SolarDirector framework that provides essential functionality for all agents and components in the system. This library handles agent infrastructure, communication protocols, configuration management, and hardware interfaces.

### Key Components

**Core Infrastructure**:
- **agent.c**: Agent framework with lifecycle management and message handling
- **client.c**: MQTT/HTTP client implementation for agent communication
- **config.c**: Configuration system with JSON parsing and property management
- **message.c**: Message processing and routing between agents

**Communication Protocols**:
- **mqtt.c**: MQTT pub/sub implementation for inter-agent messaging
- **can.c**: CAN bus interface for hardware communication
- **serial.c**: Serial port handling for direct device connections
- **rdev.c**: Remote device client for network-based hardware access

**Utilities**:
- **list.c**: Generic linked list implementation used throughout
- **json.c**: JSON parsing and generation utilities
- **conv.c**: Data type conversion utilities
- **buffer.c**: Dynamic buffer management

**Hardware Support**:
- **battery.c**: Battery data structures and calculations
- **pvinverter.c**: Solar inverter data management
- **alarm.c**: System alarm handling

## Architecture

### Library Build System
The library supports multiple build configurations controlled by `make.opts`:
- **Standard build**: Static library (libsd.a)
- **With JS**: JavaScript engine integration (libsd-js.a)
- **With MQTT**: MQTT support (libsd-mqtt.a)
- **With InfluxDB**: Time-series logging (libsd-influx.a)
- **Combined**: Full features (libsd-js-mqtt-influx.a)

### Key Data Structures
```c
// Agent session (agent.h)
struct agent_session {
    char name[AGENT_NAME_LEN];      // Agent identifier
    agent_config_t *config;         // Configuration
    mqtt_session_t *mqtt;           // MQTT connection
    // ... message queues, callbacks, etc.
};

// Configuration property (config.h)
struct config_property {
    char *name;                     // Property name
    int type;                       // DATA_TYPE_*
    void *dest;                     // Value destination
    int dsize;                      // Destination size
    config_property_flags flags;    // Property flags
};
```

## Common Development Commands

### Building the Library
```bash
# Build all library variants
make                    # Builds all configurations in parallel

# Build specific variants
make libsd.a           # Standard library only
make libsd-js.a        # With JavaScript support
make libsd-mqtt.a      # With MQTT support

# Clean build artifacts
make clean
```

### Testing Library Functions
```bash
# Use sdjs to test JavaScript-exposed functions
sdjs
js> var client = new Client("tcp://localhost:1883", "test");
js> client.connect();

# Test configuration parsing
echo '{"name":"test","interval":60}' | ./test_config

# Test CAN interface
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
./test_can vcan0
```

### Debugging
```bash
# Enable memory debugging in make.opts
DEBUG_MEM=yes make clean all

# Use debug logging
export SD_LOGLEVEL=5    # DEBUG level
export SD_LOGFILE=/tmp/sd.log
```

## Architecture Notes

### Memory Management
- Uses custom debug memory allocator when DEBUG_MEM=yes
- All allocations tracked in debugmem.c with leak detection
- String pool optimization for frequently used strings

### Configuration System
1. **Property-based**: Each config item is a typed property
2. **Callbacks**: Change notifications via config_property_set_cb
3. **JSON mapping**: Automatic JSON to C struct conversion
4. **Validation**: Type checking and range validation

### Message Flow
```
Producer Agent → MQTT Broker → Consumer Agent
     ↓                              ↓
  Publish to                    Subscribe to
solar/si/data                 solar/+/data
```

### Thread Safety
- MQTT operations are thread-safe via internal locking
- List operations require external synchronization
- Configuration updates protected by property-level locking

## Integration with Agents

### Agent Lifecycle
1. Agent calls `agent_init()` with config
2. Library sets up MQTT, logging, signal handlers
3. Agent registers properties via `config_add_props()`
4. Main loop processes messages via `agent_run()`
5. Cleanup via `agent_destroy()`

### JavaScript Bridge
When JS=yes, the library exposes functions to JavaScript:
- `Client` class for MQTT operations
- Configuration access via global properties
- Logging functions (log, debug, error)
- Data conversion utilities

### Required Agent Functions
Agents must implement:
```c
void *agent_thread(void *ctx);      // Main agent logic
int agent_read(agent_session_t *s); // Read from hardware
int agent_write(agent_session_t *s); // Write to hardware
```

## Testing Approach

### Unit Testing
Individual components can be tested in isolation:
```c
// Example: Test list operations
list l = list_create();
list_add(l, data, sizeof(data));
assert(list_count(l) == 1);
list_destroy(l);
```

### Integration Testing
Use test.json configurations with virtual interfaces:
```json
{
    "name": "test_agent",
    "transport": "null",
    "mqtt_uri": "tcp://localhost:1883",
    "log_level": 5
}
```

### Memory Testing
```bash
# Run with memory debugging
DEBUG_MEM=yes ./agent -c test.json
# Check for leaks on exit
```

## Common Patterns

### Adding New Data Types
1. Define type constant in `datatypes.h`
2. Add conversion functions in `conv.c`
3. Update JSON serialization in `json.c`
4. Add property type handling in `config.c`

### Extending Communication
1. Implement new transport in driver model
2. Add to transport types in `types.h`
3. Register in `driver.c` initialization
4. Update agent templates

### MQTT Topic Conventions
- Data publishing: `solar/{agent}/data`
- Function calls: `solar/{agent}/func/{function}`
- Configuration: `solar/{agent}/config/{property}`
- Status: `solar/{agent}/status`