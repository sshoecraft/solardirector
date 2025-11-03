# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

The Service Controller (SC) is a utility for managing and monitoring Solar Director (SD) agents. This implementation includes both a C-based command-line utility and a Flask web interface for comprehensive agent management capabilities.

## Purpose

SC manages SD agents which are typically located in `/opt/sd/bin`. Key responsibilities:

**Core Functionality:**
- Discover agents via their `-I` (info) argument
- Monitor agent status and health via MQTT
- Interface with agents based on their roles (inverter, battery, controller, etc.)
- Provide centralized management interface for the SD ecosystem

**Web Interface Functionality:**
- Professional web dashboard for agent monitoring
- REST API for agent data and management operations
- Real-time status updates with configurable refresh intervals
- Responsive design for desktop, tablet, and mobile access
- Agent service management (start/stop/restart/enable/disable)

## Agent Roles

Agents report their roles as defined in `/lib/sd/common.h`:
- `SOLARD_ROLE_CONTROLLER` - System controllers
- `SOLARD_ROLE_STORAGE` - Storage systems  
- `SOLARD_ROLE_BATTERY` - Battery management
- `SOLARD_ROLE_INVERTER` - DC/AC inverters
- `SOLARD_ROLE_PVINVERTER` - Solar PV inverters
- `SOLARD_ROLE_UTILITY` - Utility connections
- `SOLARD_ROLE_SENSOR` - Various sensors
- `SOLARD_ROLE_DEVICE` - Generic devices

## Build Commands

```bash
# Build the C utility
make                           # Build using SD framework
make clean                     # Clean build artifacts
make install                   # Install to PREFIX (/opt/sd or ~/)

# Build with debug options
make DEBUG=yes                 # Enable debug output
make DEBUG_MEM=yes             # Enable memory debugging

# Flask web interface setup
cd web/
./start_web.sh                 # Setup environment and start web interface
./start_web.sh --clean         # Clean virtual environment and reinstall
```

## Architecture Notes

### Current Status
- ✅ **IMPLEMENTED** - Service Controller v1.0 complete with Flask Web Interface
- Client-based utility pattern (not agent-based)
- Full MQTT monitoring and agent discovery
- JavaScript integration for extensible business logic
- Flask web dashboard for agent monitoring and management
- REST API for programmatic access
- Real-time status updates and systemctl integration

### Implementation Structure

**C Program Structure** (Following SD utility patterns):
- **main.c** - Command-line interface using `client_init()` pattern
- **sc.h** - Header with `sc_agent_info_t` and `sc_session_t` structures
- **monitor.c** - Core agent discovery, MQTT monitoring, and display logic
- **Makefile** - Standard SD build with JS and MQTT enabled
- **JavaScript modules**:
  - `init.js` - Configuration and initialization
  - `monitor.js` - Health checking and status processing
  - `control.js` - Systemd service management
  - `utils.js` - Utility functions

**Flask Web Interface Structure** (`web/` directory):
- **app.py** - Main Flask application with REST API endpoints
- **templates/**:
  - `base.html` - Base template with navigation and layout
  - `dashboard.html` - Main dashboard with agent overview
  - `agent.html` - Individual agent detail page
- **static/**:
  - `css/style.css` - Custom CSS with SD-specific styling
  - `js/app.js` - Core JavaScript utilities and API functions
  - `js/dashboard.js` - Dashboard-specific functionality
  - `js/agent.js` - Agent detail page functionality
- **start_web.sh** - Development startup script with environment setup
- **requirements.txt** - Python dependencies (Flask, Flask-CORS, psutil)

### Integration Points

**C Program Integration:**
- **Agent Discovery**: Scan `/opt/sd/bin` for executables with `-I` argument
- **MQTT Communication**: Subscribe to agent topics for monitoring
- **Systemd Integration**: Query service status for managed agents
- **JSON Configuration**: Use SD's config system for settings

**Flask Web Interface Integration:**
- **Data Communication**: C program exports JSON data via `-w` (web export mode)
- **Process Management**: Flask starts and manages SC process as subprocess
- **File-based IPC**: Communication via atomic JSON file updates
- **REST API**: HTTP endpoints for agent data and management operations
- **System Commands**: Safe execution of systemctl operations for agent management

### Historical Context
Previous implementation (in `/agents/sc/`) included:
- `sc.js` - Main JavaScript logic
- `agents.js` - Agent management functions
- Standard agent JS modules (`init.js`, `monitor.js`, `pub.js`, etc.)

## Development Commands

```bash
# Agent discovery and monitoring
./sc -l                        # List discovered agents
./sc -s <agent>                # Show specific agent status  
./sc -m                        # Monitor agents in real-time
./sc -w                        # Web export mode (continuous JSON export)
./sc -e <file>                 # One-time JSON export
./sc -r 5 -m                   # Monitor with 5-second refresh rate
./sc -c test.json -l           # Use test configuration

# SOLARD_BINDIR environment variable usage
SOLARD_BINDIR=/custom/path ./sc -l    # Use custom agent directory
export SOLARD_BINDIR=/opt/sd/bin      # Set for session
./sc -l                        # Uses custom directory
unset SOLARD_BINDIR            # Revert to default /opt/sd/bin

# Web Interface
cd web
./start_web.sh                 # Start Flask web dashboard
# Open browser to http://localhost:5000

# Agent Management via Web Interface
# - Navigate to dashboard for agent overview
# - Use "Add Agent" button to add new agents from SOLARD_BINDIR
# - Click agent names for detailed status and controls
# - Use action buttons for start/stop/restart/enable/disable

# Debug development
gdb ./sc                       # Debug with gdb
valgrind --leak-check=full ./sc -l  # Memory leak checking
strace ./sc                    # System call tracing

# Test with mock agents
chmod +x test_agent fake_agent  # Make test agents executable
./sc -c test.json -l           # List will find test agents
./sc -c test.json -m           # Monitor mode
```

## Flask REST API Endpoints

The Flask web interface provides a comprehensive REST API:

```bash
# Agent Management
GET  /api/agents                    # List all agents with status
GET  /api/agents/<name>             # Get specific agent details
GET  /api/agents/<name>/status      # Get agent status only
POST /api/agents/<name>/start       # Start agent service
POST /api/agents/<name>/stop        # Stop agent service
POST /api/agents/<name>/restart     # Restart agent service
POST /api/agents/<name>/enable      # Enable agent service
POST /api/agents/<name>/disable     # Disable agent service

# Agent Management (New)
GET  /api/available-agents          # List agents in SOLARD_BINDIR not yet managed
POST /api/agents/add                # Add agent to monitoring
DELETE /api/agents/<name>/remove    # Remove agent from monitoring

# System Information  
GET  /api/system/status             # Get system status and metrics
POST /api/system/restart-sc         # Restart SC background process

# Web Pages
GET  /                              # Main dashboard
GET  /agent/<name>                  # Agent detail page
```

### API Response Format
All API endpoints return JSON responses with consistent structure:
```json
{
  "success": true,
  "data": { ... },
  "error": null,
  "timestamp": 1642123456
}
```

## Implementation Details

### Agent Discovery
```c
// Implemented in monitor.c:sc_discover_agents()
DIR *dp = opendir(sc->agent_dir);
sprintf(command, "%s -I 2>/dev/null", path);
fp = popen(command, "r");
json_value_t *v = json_parse(output);
name = json_object_dotget_string(o, "agent_name");
```

### MQTT Monitoring  
```c
// Implemented with message queue pattern
c->addmq = true;  // Enable message queue
mqtt_sub(c->m, SOLARD_TOPIC_ROOT"/"SOLARD_TOPIC_AGENTS"/#");
// Process in main loop
while((msg = list_get_next(sc->c->mq)) != 0) {
    sc_update_agent_status(sc, msg);
}
```

### Command-Line Interface
Implemented arguments:
- `-l` - List all discovered agents
- `-s <agent>` - Show detailed status for specific agent
- `-m` - Real-time monitoring mode with auto-refresh
- `-r <rate>` - Set refresh rate for monitor mode (default: 2)
- `-c <config>` - Use specified config file
- `-d` - Enable debug output (not fully implemented)
- `-h` - Show help (default when no args)

## Testing Approach

```bash
# Unit testing (when implemented)
./sc -c test.json              # Test configuration

# Integration testing
# Start test agents in /opt/sd/bin
# Verify SC discovers and monitors them

# MQTT testing
mosquitto_sub -t 'solar/+/+'   # Monitor all agent messages
mosquitto_pub -t test -m test  # Test MQTT connectivity
```

## Dependencies

- SD core libraries (`libsd.a`, `libjs.a`)
- MQTT client library (if MQTT enabled)
- Standard SD build environment

## Related Components

- **sdconfig** - Configuration management utility
- **Agent framework** - Standard agent structure in `/agents/`
- **SD libraries** - Core functionality in `/lib/sd/`

## Key Implementation Patterns

### Client-Based Utility Pattern
SC uses `client_init()` instead of `agent_init()`, making it a utility rather than an agent:
```c
c = client_init(argc, argv, "sc", SC_VERSION, opts, 
    CLIENT_FLAG_NOINFLUX | CLIENT_FLAG_NOEVENT, props);
```

### Memory-Safe JSON Handling
Always create new JSON objects to ensure proper ownership:
```c
char *json_str = json_dumps(v, 0);
json_value_t *new_v = json_parse(json_str);
info->status_data = json_value_object(new_v);
free(json_str);
```

### JavaScript Integration
Business logic in JavaScript, core functionality in C:
- C handles: discovery, MQTT, display, performance-critical ops
- JavaScript handles: health checks, monitoring rules, service control

## Flask Web Interface

The Service Controller includes a complete Flask-based web dashboard:

### Features
- **Real-time Dashboard**: Live monitoring of all discovered agents
- **Agent Management**: systemctl integration for service control (start/stop/restart/enable/disable)
- **System Overview**: Summary statistics, role distribution, health metrics
- **Agent Detail Pages**: Comprehensive view with status, configuration, and runtime data
- **REST API**: Full JSON API for programmatic access
- **Responsive Design**: Mobile-friendly Bootstrap interface
- **Auto-refresh**: Configurable automatic updates (default 5 seconds)

### Quick Start
```bash
cd web
./start_web.sh                 # Automated setup and launch
# Open browser to http://localhost:5000
```

### Manual Setup
```bash
cd web
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
export SC_BINARY_PATH="../sc"
python app.py
```

### API Endpoints
- `GET /api/agents` - List all agents with status
- `GET /api/agents/{name}` - Get specific agent details  
- `POST /api/agents/{name}/{action}` - Perform systemctl actions
- `GET /api/system/status` - Get system status and statistics

### Architecture
```
SC Process (C) → JSON Export → Flask App → Web Dashboard
     ↓                           ↓
MQTT + Agents              REST API + Browser
```

### Configuration
Environment variables:
- `SC_BINARY_PATH` - Path to SC binary (default: `../sc`)
- `SC_DATA_FILE` - JSON data file (default: `/tmp/sc_data.json`)  
- `SOLARD_BINDIR` - Agent directory (overrides AGENT_DIR, default: `/opt/sd/bin`)
- `AGENT_DIR` - Agent directory fallback (default: `/opt/sd/bin`)
- `PORT` - Flask port (default: `5000`)

### Agent Management Usage

**Adding Agents via Web Interface:**
1. Navigate to the dashboard (http://localhost:5000)
2. Click "Add Agent" button
3. Select from available agents in SOLARD_BINDIR dropdown, or
4. Enter custom path for agents outside SOLARD_BINDIR
5. Click "Add" to validate and add the agent
6. Agent will be added to managed_agents.json and SC process restarted

**Adding Agents via API:**
```bash
# Get available agents from SOLARD_BINDIR
curl http://localhost:5000/api/available-agents

# Add an agent
curl -X POST http://localhost:5000/api/agents/add \
  -H "Content-Type: application/json" \
  -d '{"name": "new_agent", "path": "/opt/sd/bin/new_agent"}'

# Remove an agent
curl -X DELETE http://localhost:5000/api/agents/new_agent/remove
```

**Agent Validation:**
- All agents must respond to `-I` flag with valid JSON
- Agents must be executable and accessible
- Duplicate agents are prevented
- Invalid agents are rejected with error messages

**Configuration Files:**
- `managed_agents.json` - Stores user-added agents (persistent)
- Automatic backup and atomic updates prevent corruption
- SC process automatically restarts after agent changes

## Future Enhancements

1. ✅ **Web Interface** - COMPLETED: Full Flask dashboard with REST API
2. ✅ **Agent Management** - COMPLETED: Dynamic agent add/remove via web interface
3. ✅ **SOLARD_BINDIR Support** - COMPLETED: Configurable agent directory
4. **Event System** - JavaScript event handlers for automation  
5. **Plugin Support** - Dynamic loading of agent-specific modules
6. **Metrics Export** - Prometheus/InfluxDB integration
7. **Multi-Platform** - Support for non-systemd systems
8. **Service Mode** - Run SC as a systemd service
9. **Authentication** - User authentication for web interface
10. **WebSocket** - Real-time updates via WebSocket instead of polling
11. **Bulk Operations** - Add/remove multiple agents simultaneously
12. **Agent Templates** - Pre-configured agent deployment templates