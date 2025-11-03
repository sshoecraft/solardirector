# Service Controller Flask Web Interface

A comprehensive web-based interface for managing and monitoring Solar Director (SD) agents.

## Overview

The Flask web interface provides a professional, responsive dashboard for the Service Controller (SC) utility. It communicates with the C-based SC program via JSON files and provides both a web UI and REST API for agent management.

## Architecture

```
┌─────────────────┐    JSON Files    ┌──────────────────┐
│   SC C Program  │ ◄──────────────► │  Flask Web App   │
│                 │  /tmp/sc_data.json│                  │
│ - Agent Discovery│                  │ - REST API       │
│ - MQTT Monitoring│                  │ - Web Dashboard  │
│ - Health Tracking│                  │ - Agent Management│
└─────────────────┘                  └──────────────────┘
        │                                      │
        ▼                                      ▼
┌─────────────────┐                  ┌──────────────────┐
│   SD Agents     │                  │   Web Browser    │
│ /opt/sd/bin/*   │                  │ http://localhost │
└─────────────────┘                  └──────────────────┘
```

## Features

### Dashboard
- **Real-time Agent Status**: Live view of all discovered agents
- **System Overview**: Summary statistics and health metrics
- **Role-based Grouping**: Agents organized by their roles (inverter, battery, etc.)
- **Status Filtering**: Filter agents by online/offline/warning status
- **Responsive Design**: Works on desktop, tablet, and mobile devices

### Agent Management
- **Service Control**: Start, stop, restart, enable, disable agent services
- **Detailed View**: Individual agent pages with comprehensive information
- **Status Monitoring**: Real-time health and connectivity status
- **Configuration Display**: View agent configuration and status data
- **Dynamic Agent Addition**: Add new agents from SOLARD_BINDIR or custom paths
- **Agent Discovery**: Automatic scanning and listing of available agents
- **Agent Removal**: Remove agents from monitoring via web interface
- **Agent Validation**: Automatic validation using -I flag before addition

### REST API
- **Comprehensive Endpoints**: Full API for programmatic access
- **JSON Responses**: Structured data format for integration
- **Error Handling**: Proper HTTP status codes and error messages
- **Cross-Origin Support**: CORS enabled for external access

## Quick Start

### Prerequisites
- Python 3.7 or higher
- SC binary compiled and available
- SD agents in `/opt/sd/bin` (optional for testing)

### Installation and Startup

```bash
# Navigate to web directory
cd /path/to/sd/utils/sc/web

# Start web interface (handles everything)
./start_web.sh

# Web interface will be available at:
# http://localhost:5000
```

The startup script automatically:
- Creates Python virtual environment
- Installs dependencies
- Builds SC binary if needed
- Sets up environment variables
- Starts Flask application

### Manual Setup

```bash
# Create virtual environment
python3 -m venv venv
source venv/bin/activate

# Install dependencies
pip install -r requirements.txt

# Set environment variables
export SC_BINARY_PATH="../sc"
export SC_DATA_FILE="/tmp/sc_data.json"
export AGENT_DIR="/opt/sd/bin"

# Start Flask application
python app.py
```

## Configuration

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `SC_BINARY_PATH` | `../sc` | Path to SC binary |
| `SC_DATA_FILE` | `/tmp/sc_data.json` | JSON data file location |
| `SOLARD_BINDIR` | `/opt/sd/bin` | Primary agent directory (overrides AGENT_DIR) |
| `AGENT_DIR` | `/opt/sd/bin` | Fallback agent directory |
| `PORT` | `5000` | Flask server port |
| `FLASK_ENV` | `development` | Flask environment |
| `FLASK_DEBUG` | `true` | Enable Flask debug mode |
| `SECRET_KEY` | `dev-key` | Flask secret key (change for production) |

### Flask Configuration

The Flask application can be configured via environment variables or by modifying `app.py`:

```python
# Example production configuration
app.config.update({
    'SECRET_KEY': 'your-production-secret-key',
    'SC_DATA_FILE': '/var/lib/sc/data.json',
    'AGENT_DIR': '/opt/sd/bin'  # Overridden by SOLARD_BINDIR if set
})
```

## API Documentation

### Agent Endpoints

#### GET /api/agents
List all discovered agents with current status.

**Response:**
```json
{
  "agents": [
    {
      "name": "si",
      "role": "Inverter", 
      "status": "Online",
      "health": "Good",
      "service_active": true,
      "service_status": "active",
      "last_seen": 1642123456,
      "last_seen_ago": "2m ago"
    }
  ],
  "count": 1,
  "last_update": 1642123456
}
```

#### GET /api/agents/{name}
Get detailed information for a specific agent.

**Response:**
```json
{
  "name": "si",
  "role": "Inverter",
  "version": "1.0",
  "path": "/opt/sd/bin/si",
  "status": "Online",
  "health": "Good",
  "service_active": true,
  "service_status": "active",
  "online": true,
  "last_seen": 1642123456,
  "status_data": { ... },
  "config_data": { ... },
  "info_data": { ... }
}
```

#### POST /api/agents/{name}/{action}
Perform an action on an agent service.

**Actions:** `start`, `stop`, `restart`, `enable`, `disable`

**Response:**
```json
{
  "success": true,
  "message": "Agent si restart successful",
  "output": "...",
  "error": null
}
```

#### GET /api/available-agents
Get list of agents in SOLARD_BINDIR that are not yet managed.

**Response:**
```json
{
  "available_agents": [
    {
      "name": "new_agent",
      "path": "/opt/sd/bin/new_agent",
      "role": "sensor",
      "version": "1.0"
    }
  ],
  "count": 1,
  "agent_dir": "/opt/sd/bin"
}
```

#### POST /api/agents/add
Add an agent to monitoring.

**Request Body:**
```json
{
  "name": "new_agent",
  "path": "/opt/sd/bin/new_agent"
}
```

**Response:**
```json
{
  "success": true,
  "message": "Agent new_agent added successfully",
  "agent": {
    "name": "new_agent",
    "path": "/opt/sd/bin/new_agent",
    "enabled": true,
    "added": 1642123456
  }
}
```

**Validation:**
- Agent path must exist and be executable
- Agent must respond to `-I` flag with valid JSON
- Agent name must not already be managed
- Automatically restarts SC process after successful addition

#### DELETE /api/agents/{name}/remove
Remove an agent from monitoring.

**Response:**
```json
{
  "success": true,
  "message": "Agent new_agent removed successfully"
}
```

### System Endpoints

#### GET /api/system/status
Get overall system status and metrics.

**Response:**
```json
{
  "discovering": true,
  "discovery_interval": 60,
  "timestamp": 1642123456,
  "role_counts": {
    "inverter": 2,
    "battery": 3,
    "controller": 1
  },
  "flask_server": {
    "uptime": 3600,
    "pid": 12345,
    "sc_process_running": true
  },
  "resources": {
    "cpu_percent": 5.2,
    "memory_percent": 45.1,
    "disk_percent": 78.3
  }
}
```

#### POST /api/system/restart-sc
Restart the SC background process.

**Response:**
```json
{
  "success": true,
  "message": "SC process restarted"
}
```

## Web Interface Usage

### Dashboard Navigation
1. **System Overview**: Top section shows summary statistics
2. **Agent List**: Main table with all discovered agents
3. **Filters**: Use radio buttons to filter by status
4. **Actions**: Quick action buttons for each agent
5. **Auto-refresh**: Automatic updates every 5 seconds (configurable)

### Agent Detail Page
1. Navigate to an agent by clicking its name in the dashboard
2. View comprehensive agent information
3. Use action buttons to manage the agent service
4. View raw status, configuration, and info data

### Real-time Updates
- Dashboard automatically refreshes every 5 seconds
- Status changes are reflected immediately
- Loading states shown during operations
- Error messages displayed for failed operations

### Agent Management Workflow

#### Adding Agents
1. **Via Dashboard**: Click "Add Agent" button on main dashboard
2. **Select Source**: Choose from available agents in SOLARD_BINDIR dropdown or enter custom path
3. **Validation**: Agent is automatically validated using -I flag
4. **Addition**: Agent added to managed_agents.json and SC process restarted
5. **Monitoring**: Agent appears in dashboard and begins monitoring

#### Removing Agents
1. **Agent Detail Page**: Navigate to specific agent detail page
2. **Remove Button**: Click "Remove Agent" button
3. **Confirmation**: Confirm removal in dialog
4. **Cleanup**: Agent removed from managed_agents.json and SC process restarted
5. **Update**: Dashboard updates to reflect removal

#### Agent Discovery
- Automatic scanning of SOLARD_BINDIR for executable files
- Testing each executable with -I flag for SD agent compatibility
- Filtering out agents already under management
- Role-based categorization for easy identification

## File Structure

```
web/
├── app.py                 # Main Flask application
├── requirements.txt       # Python dependencies
├── start_web.sh          # Development startup script
├── README.md             # This file
├── templates/            # Jinja2 HTML templates
│   ├── base.html         # Base template with navigation
│   ├── dashboard.html    # Main dashboard page
│   └── agent.html        # Agent detail page
├── static/               # Static web assets
│   ├── css/
│   │   └── style.css     # Custom CSS styles
│   └── js/
│       ├── app.js        # Core JavaScript utilities
│       ├── dashboard.js  # Dashboard functionality
│       └── agent.js      # Agent detail page functionality
└── venv/                 # Python virtual environment (created automatically)
```

## Development

### Adding New Features

1. **New API Endpoints**: Add routes to `app.py`
2. **New Pages**: Create templates in `templates/`
3. **JavaScript Functionality**: Add to appropriate JS files in `static/js/`
4. **Styling**: Modify `static/css/style.css`

### Testing

```bash
# Start development server
./start_web.sh

# Test API endpoints
curl http://localhost:5000/api/agents
curl -X POST http://localhost:5000/api/agents/si/restart

# Test with different browsers and screen sizes
# Check console for JavaScript errors
```

### Debugging

- Flask debug mode is enabled by default in development
- Check browser console for JavaScript errors
- Flask logs are printed to console
- SC process logs can be monitored separately

## Security Considerations

### Development vs Production

**Development (current):**
- Flask development server
- Debug mode enabled
- No authentication
- HTTP only
- Suitable for trusted local networks

**Production Recommendations:**
- Use WSGI server (Gunicorn, uWSGI)
- Enable HTTPS with SSL certificates
- Implement authentication/authorization
- Set secure session configuration
- Use environment variables for secrets
- Enable proper logging
- Set up reverse proxy (nginx)

### Current Security Features

- Input validation on all endpoints
- No arbitrary command execution
- Subprocess with argument lists (no shell=True)
- Limited to predefined systemctl actions
- CORS configuration for API access
- Error messages don't leak sensitive information

## Production Deployment

### Using Gunicorn

```bash
# Install Gunicorn
pip install gunicorn

# Start with Gunicorn
gunicorn -w 4 -b 0.0.0.0:5000 app:app

# With SSL (recommended)
gunicorn -w 4 -b 0.0.0.0:5000 --certfile=cert.pem --keyfile=key.pem app:app
```

### Systemd Service

Create `/etc/systemd/system/sc-web.service`:

```ini
[Unit]
Description=Service Controller Web Interface
After=network.target

[Service]
Type=simple
User=sduser
WorkingDirectory=/opt/sd/utils/sc/web
Environment=PATH=/opt/sd/utils/sc/web/venv/bin
Environment=SC_BINARY_PATH=/opt/sd/bin/sc
Environment=FLASK_ENV=production
ExecStart=/opt/sd/utils/sc/web/venv/bin/gunicorn -w 4 -b 0.0.0.0:5000 app:app
Restart=always

[Install]
WantedBy=multi-user.target
```

### Nginx Reverse Proxy

Example nginx configuration:

```nginx
server {
    listen 80;
    server_name your-domain.com;
    
    location / {
        proxy_pass http://127.0.0.1:5000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
    
    location /static {
        alias /opt/sd/utils/sc/web/static;
        expires 1y;
        add_header Cache-Control "public, immutable";
    }
}
```

## Troubleshooting

### Common Issues

1. **SC Binary Not Found**
   - Ensure SC is compiled: `cd .. && make`
   - Check SC_BINARY_PATH environment variable

2. **Permission Denied for systemctl**
   - Ensure user has sudo privileges
   - Configure sudoers for passwordless systemctl access

3. **Python Dependencies Missing**
   - Run `./start_web.sh --clean` to reinstall
   - Check Python version (3.7+ required)

4. **Web Interface Not Loading**
   - Check Flask is running on correct port
   - Verify firewall settings
   - Check browser console for errors

5. **No Agents Discovered**
   - Check SOLARD_BINDIR/AGENT_DIR environment variables
   - Ensure agents support -I argument
   - Run SC manually: `../sc -l`
   - Verify agent directory permissions

6. **Agent Addition Fails**
   - Check agent path exists and is executable
   - Verify agent responds to -I flag: `agent_path -I`
   - Check managed_agents.json permissions
   - Ensure agent name is unique

7. **Available Agents List Empty**
   - Verify SOLARD_BINDIR contains valid agents
   - Check agents respond to -I flag correctly
   - Ensure agents are not already managed

### Debug Commands

```bash
# Check SC binary
../sc -l

# Test SC web export mode  
../sc -w -r 5

# Check JSON data file
cat /tmp/sc_data.json

# Test API endpoints
curl -v http://localhost:5000/api/agents
curl -v http://localhost:5000/api/available-agents

# Test agent management
curl -X POST http://localhost:5000/api/agents/add \
  -H "Content-Type: application/json" \
  -d '{"name": "test", "path": "/path/to/test_agent"}'

# Check Python environment
source venv/bin/activate
pip list

# Test SOLARD_BINDIR
SOLARD_BINDIR=/custom/path python app.py

# Check managed agents file
cat /tmp/sc_managed_agents.json
```

## License

This Flask web interface is part of the Solar Director project and follows the same BSD-style license as the main project.