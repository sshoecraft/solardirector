# SolarDirector Project Planning

## Project Overview
SolarDirector is a comprehensive solar power management system written in C that provides MQTT-based agents for monitoring and controlling various solar hardware components including BMS (Battery Management Systems), inverters, and other power electronics.

## Architecture

### Core Components
- **Agents**: Individual hardware interface modules located in `/agents/`
  - BMS agents: `jbd`, `jk` (JBD and JiKong BMS systems)
  - Inverter agents: `si` (SMA Sunny Island), `sb` (SMA Sunny Boy)
  - Other agents: `ac`, `ah`, `btc`, `pa`, `pvc`, `rheem`, `sc`
- **Libraries**: Shared code and utilities in `/lib/`
- **Utilities**: Configuration and management tools in `/utils/`
- **Clients**: Client applications in `/clients/`

### Communication
- **MQTT**: Primary communication protocol for agent reporting and configuration
- **Bluetooth**: Optional connectivity for BMS devices (requires pairing)
- **Network**: Standard network protocols for inverter communication

## Development Guidelines

### Code Structure & Modularity
- Each agent is self-contained in its own directory under `/agents/`
- Shared functionality should be placed in `/lib/`
- Never create source files longer than 500 lines - split into modules
- Use consistent naming conventions across agents
- Follow the existing Makefile structure patterns

### Build System
- Uses GNU Make with modular Makefiles
- Main Makefile includes `Makefile.sys` for system-specific settings
- Each agent has its own Makefile
- Build options controlled via `make.opts`

### Dependencies
Required system packages:
- `libssl-dev` - SSL/TLS support
- `libcurl4-openssl-dev` - HTTP/HTTPS client
- `libreadline-dev` - Command line interface
- `libbluetooth-dev` - Bluetooth support (optional)

External libraries:
- **paho.mqtt.c** - MQTT client library
- **gattlib** - Bluetooth Low Energy GATT library (for BLE devices)

### Platform Support
- Primary target: Linux (Debian-based)
- Current focus: macOS compilation support
- Build system designed for cross-platform compatibility

## Configuration Management
- Agents report to MQTT at regular intervals
- Configuration via MQTT messages
- `sdconfig` utility for parameter management:
  ```bash
  sdconfig <agent> get <parameter>
  sdconfig <agent> set <parameter> <value>
  sdconfig -l <agent>  # list parameters
  ```

## Testing Strategy
- Manual testing with actual hardware components
- Integration testing via MQTT message verification
- Platform-specific build testing (Linux, macOS)

## File Organization
```
/agents/           # Hardware interface agents
  /template/       # Template for new agents
  /jbd/           # JBD BMS agent
  /jk/            # JiKong BMS agent
  /si/            # SMA Sunny Island agent
  /sb/            # SMA Sunny Boy agent
  [other agents]
/lib/             # Shared libraries and utilities
/utils/           # Configuration and management tools
/clients/         # Client applications
/tools/           # Development and build tools
/etc/             # Configuration files
/reports/         # System reports and logs
```

## Current Development Focus
- **macOS Build Support**: Ensuring compilation compatibility on macOS
- **Cross-Platform Dependencies**: Managing library dependencies across platforms
- **Build System Optimization**: Streamlining the make-based build process

## Code Style & Conventions
- Standard C99/C11 compliance
- Consistent indentation (tabs or 4 spaces)
- Clear function and variable naming
- Comprehensive error handling
- Minimal external dependencies where possible
- Documentation in code comments for complex logic

## Agent Development Pattern
When creating new agents:
1. Use `/agents/template/` as starting point
2. Implement standard MQTT reporting interface
3. Follow existing configuration parameter patterns
4. Include proper error handling and logging
5. Document hardware-specific communication protocols
