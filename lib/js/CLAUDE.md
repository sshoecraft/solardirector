# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

This is a custom JavaScript engine library based on Mozilla's SpiderMonkey 1.8-RC1 (the last C-based version before Mozilla moved to C++). The library is part of the SolarDirector (SD) project and provides embedded JavaScript runtime capabilities with extensive hardware integration for solar power and battery management systems.

## Key Components

### Core JavaScript Engine
- **SpiderMonkey 1.8-RC1 Base**: Full JavaScript engine with garbage collection, JIT compilation, and ECMAScript compliance
- **Custom Extensions**: Extensive additions for hardware control, file I/O, networking, and system integration
- **JSEngine Wrapper**: High-level C API (`jsengine.h`) for embedding JavaScript in C applications

### Hardware Integration Objects
- **File/Directory Objects**: File system operations and directory manipulation
- **CAN Object**: SocketCAN interface for CAN bus communication (Linux only)
- **SMANET Object**: SMA inverter protocol implementation
- **Socket Object**: Network socket operations
- **Influx Object**: InfluxDB time-series database integration
- **MQTT Object**: MQTT pub/sub messaging via paho.mqtt.c
- **HTTP Object**: HTTP client operations using CURL
- **Serial/Bluetooth Objects**: Hardware communication interfaces

### Global Functions
Extended JavaScript global scope with system functions:
- **I/O Functions**: `printf`, `sprintf`, `dprintf`, `putstr`, `print`
- **System Functions**: `system`, `run`, `sleep`, `exit`, `abort`, `quit`
- **Utility Functions**: `dirname`, `basename`, `timestamp`, `systime`, `gc`
- **Debugging Functions**: `memused`, `sysmemused`, `log_info`, `log_warning`, `log_error`

## Architecture

### Build System Integration
The JavaScript engine is conditionally compiled based on `JS=yes` in `make.opts`:
- **Conditional Compilation**: Only built when JS support is enabled
- **Dependency Management**: Automatically links required libraries (libjs.a)
- **Cross-Platform**: Supports Linux, macOS, and Windows cross-compilation
- **Feature Flags**: XP_UNIX/XP_WIN platform-specific compilation

### Property Generation System
Custom tooling generates JavaScript property definitions from C structs:
- **mkjs.sh Script**: Converts C header files to JavaScript property definitions
- **Generated Headers**: `*_defs.h` files provide property mappings
- **Dynamic Binding**: C structures automatically exposed to JavaScript

### Memory Management
- **SpiderMonkey GC**: Garbage collection for JavaScript objects
- **Debug Support**: Optional memory debugging with `DEBUG_MEM=yes`
- **Arena Allocation**: Efficient memory allocation for JavaScript engine

## Common Development Commands

### Build Commands
```bash
# Build JavaScript library only
make -C lib/js

# Build with debug enabled (default)
DEBUG=yes make -C lib/js

# Build release version
RELEASE=yes make -C lib/js

# Build with memory debugging
DEBUG_MEM=yes make -C lib/js

# Clean build artifacts
make -C lib/js clean
```

### Testing and Development
```bash
# Interactive JavaScript REPL
utils/sdjs/sdjs

# Execute JavaScript file
utils/sdjs/sdjs -f script.js

# Test with agent configurations
agents/template/template -c agents/template/test.json -v -d
```

### Property Definition Generation
```bash
# Generate property definitions from C headers
tools/mkjs.sh header.h objectname "prefix->" buffer_size access_mode
```

## Code Architecture

### JSEngine Structure
The `JSEngine` struct provides the high-level interface:
- **Runtime/Context**: SpiderMonkey runtime and execution context
- **Thread Safety**: Mutex-protected context access
- **Script Management**: Dynamic script loading and execution
- **Initialization**: Pluggable initialization functions for objects
- **Memory Management**: Configurable runtime and stack sizes

### Integration Pattern
JavaScript engine integration follows this pattern:
```c
// Initialize engine
JSEngine *engine = JS_EngineInit(rtsize, stksize, output_func);

// Add initialization functions
JS_EngineAddInitFunc(engine, "objectname", init_func, private_data);

// Execute JavaScript
JS_EngineExec(engine, "script.js", "function_name", argc, argv, newcx);
```

### C-JavaScript Bridge
- **jsfuncs.c Files**: Agent-specific C functions exposed to JavaScript
- **Property Definitions**: Generated header files map C structs to JS properties
- **Type Conversion**: Automatic conversion between C types and JavaScript values

## Build Configuration

### Feature Flags (`make.opts`)
- `JS=yes` - Enable JavaScript engine (default)
- `DEBUG=yes` - Debug build with symbols
- `DEBUG_MEM=no` - Memory debugging
- `STATIC=no` - Library type (shared/static)

### Platform Support
- **Linux**: Primary target with full hardware integration
- **macOS**: Development platform with dynamic linking
- **Windows**: Cross-compilation support

## Important Technical Notes

### JSVAL_VOID Fix
Line 213 in `jsapi.h` contains a fix for undefined behavior in the original SpiderMonkey `JSVAL_VOID` macro. This addresses negative value shifting issues that caused crashes in the original implementation.

### Thread Safety
The JavaScript engine uses mutex protection for context access, enabling multi-threaded applications to safely execute JavaScript code.

### Agent Integration
Each SD agent embeds the JavaScript engine for business logic:
- **Isolated Contexts**: Each agent has its own JavaScript environment
- **Hot Reload**: JavaScript files can be updated without recompiling C code
- **Configuration Integration**: JSON configuration drives JavaScript execution

## Testing Approach

### Unit Testing
- **Interactive REPL**: Use `sdjs` for testing JavaScript code
- **Agent Testing**: Use `test.json` configurations with verbose debug output
- **Memory Testing**: Enable `DEBUG_MEM` for memory leak detection

### Integration Testing
- **Virtual CAN**: Use `vcan0` for CAN bus simulation
- **MQTT Testing**: Local MQTT broker for agent communication testing
- **Hardware Mocking**: JSON configurations provide virtual hardware interfaces

The JavaScript engine is central to the SD system's flexibility, providing embedded scripting capabilities for complex solar power management logic while maintaining the performance and reliability of a C-based foundation.