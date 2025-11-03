# Agent Management User Guide

This guide provides comprehensive instructions for managing Solar Director (SD) agents using the Service Controller's web interface and API.

## Overview

The Service Controller (SC) provides dynamic agent management capabilities, allowing you to add, remove, and monitor SD agents without requiring recompilation or manual configuration file editing. This functionality is available through both a web dashboard and REST API.

## Prerequisites

- Service Controller compiled and operational
- Flask web interface running (`./start_web.sh`)
- Appropriate permissions for systemctl operations
- Valid SD agents available for management

## Web Interface Agent Management

### Accessing the Dashboard

1. Start the web interface:
   ```bash
   cd web/
   ./start_web.sh
   ```

2. Open browser to http://localhost:5000

3. The dashboard displays:
   - System overview with agent counts and health metrics
   - Complete list of discovered and managed agents
   - Status filtering options (all/online/offline/warning)
   - Quick action buttons for each agent

### Adding Agents

#### Method 1: Add from SOLARD_BINDIR

1. **Click "Add Agent" Button**: Located at the top of the dashboard

2. **Select from Available Agents**: 
   - The dropdown shows agents discovered in SOLARD_BINDIR
   - Agents are automatically scanned and validated
   - Only compatible agents (responding to -I flag) are listed
   - Already-managed agents are filtered out

3. **Review Agent Information**:
   - Agent name and role are displayed
   - Path is automatically populated
   - Version information is shown if available

4. **Click "Add Agent"**:
   - Agent is validated for compatibility
   - Added to managed_agents.json configuration
   - SC process automatically restarts to begin monitoring
   - Agent appears in dashboard within 5-10 seconds

#### Method 2: Add from Custom Path

1. **Click "Add Agent" Button**

2. **Select "Custom Path" Option**

3. **Enter Agent Details**:
   - **Agent Name**: Unique identifier for the agent
   - **Agent Path**: Full path to the agent executable

4. **Validation Process**:
   - Path existence and executability checked
   - Agent tested with -I flag for compatibility
   - JSON response parsed for agent information
   - Duplicate name detection

5. **Addition Confirmation**:
   - Success message displayed
   - Agent added to monitoring
   - Dashboard updates automatically

### Managing Existing Agents

#### Agent Overview Actions

From the main dashboard, each agent row provides quick actions:

- **Start**: Start the agent's systemd service
- **Stop**: Stop the agent's systemd service  
- **Restart**: Restart the agent's systemd service
- **Enable**: Enable agent to start automatically on boot
- **Disable**: Disable automatic startup

#### Detailed Agent Management

1. **Click Agent Name**: Navigate to detailed agent page

2. **Agent Detail Page Features**:
   - Comprehensive status information
   - Service control buttons with confirmation
   - Real-time status updates
   - Configuration data display
   - Recent status and data information

3. **Available Actions**:
   - All systemctl operations (start/stop/restart/enable/disable)
   - Remove agent from monitoring
   - View raw JSON data (status, config, info)

### Removing Agents

#### From Agent Detail Page

1. **Navigate to Agent**: Click agent name from dashboard

2. **Click "Remove Agent" Button**: Located in the actions section

3. **Confirm Removal**: Confirm in the dialog prompt

4. **Automatic Cleanup**:
   - Agent removed from managed_agents.json
   - SC process restarted
   - Agent disappears from dashboard
   - Service remains installed but unmonitored

## REST API Agent Management

### Authentication

Currently, no authentication is required for the development server. For production deployments, implement proper authentication mechanisms.

### Available Agents Discovery

```bash
# Get list of agents in SOLARD_BINDIR not yet managed
curl http://localhost:5000/api/available-agents

# Response:
{
  "available_agents": [
    {
      "name": "new_battery_agent",
      "path": "/opt/sd/bin/new_battery_agent", 
      "role": "battery",
      "version": "1.2"
    }
  ],
  "count": 1,
  "agent_dir": "/opt/sd/bin"
}
```

### Adding Agents via API

```bash
# Add agent with JSON payload
curl -X POST http://localhost:5000/api/agents/add \
  -H "Content-Type: application/json" \
  -d '{
    "name": "new_battery_agent",
    "path": "/opt/sd/bin/new_battery_agent"
  }'

# Success response:
{
  "success": true,
  "message": "Agent new_battery_agent added successfully",
  "agent": {
    "name": "new_battery_agent",
    "path": "/opt/sd/bin/new_battery_agent",
    "enabled": true,
    "added": 1642123456
  }
}

# Error response:
{
  "success": false,
  "error": "Agent path does not exist: /invalid/path"
}
```

### Removing Agents via API

```bash
# Remove agent by name
curl -X DELETE http://localhost:5000/api/agents/new_battery_agent/remove

# Success response:
{
  "success": true,
  "message": "Agent new_battery_agent removed successfully"
}

# Error response:
{
  "success": false,
  "error": "Agent new_battery_agent not found"
}
```

### Agent Service Control via API

```bash
# Start agent service
curl -X POST http://localhost:5000/api/agents/new_battery_agent/start

# Stop agent service  
curl -X POST http://localhost:5000/api/agents/new_battery_agent/stop

# Restart agent service
curl -X POST http://localhost:5000/api/agents/new_battery_agent/restart

# Enable agent service (auto-start on boot)
curl -X POST http://localhost:5000/api/agents/new_battery_agent/enable

# Disable agent service
curl -X POST http://localhost:5000/api/agents/new_battery_agent/disable
```

## Configuration

### SOLARD_BINDIR Environment Variable

Set the primary directory for agent discovery:

```bash
# For current session
export SOLARD_BINDIR="/custom/agent/directory"

# For Flask web interface
SOLARD_BINDIR="/custom/agent/directory" ./start_web.sh

# Verify configuration
echo $SOLARD_BINDIR
```

**Fallback Behavior:**
- If SOLARD_BINDIR is not set, defaults to AGENT_DIR
- If AGENT_DIR is not set, defaults to `/opt/sd/bin`
- Configuration is checked in this order: SOLARD_BINDIR → AGENT_DIR → default

### Managed Agents Configuration

Agents added through the interface are stored in `managed_agents.json`:

```json
{
  "agents": [
    {
      "name": "custom_agent",
      "path": "/custom/path/custom_agent",
      "enabled": true,
      "added": 1642123456
    }
  ]
}
```

**File Location:** `/tmp/sc_managed_agents.json` (configurable)

**Automatic Management:**
- File created automatically when first agent is added
- Atomic updates prevent corruption
- SC process automatically reloads configuration

## Agent Validation Requirements

For an executable to be successfully added as an agent, it must meet these requirements:

### 1. -I Flag Support

```bash
# Agent must respond to -I flag with valid JSON
/path/to/agent -I

# Example valid response:
{
  "agent_name": "my_agent",
  "agent_role": "battery", 
  "agent_version": "1.0",
  "description": "Battery management agent"
}
```

### 2. Required JSON Fields

- **agent_name**: Unique identifier for the agent
- **agent_role**: Role from SD common.h (battery, inverter, controller, etc.)
- Optional fields: agent_version, description

### 3. Executable Requirements

- File must exist and be executable
- Must not require special environment setup
- Should be SD-compatible agent binary

### 4. Unique Naming

- Agent name must be unique across all managed agents
- Case-sensitive comparison
- No special characters recommended

## Best Practices

### Agent Organization

1. **Use SOLARD_BINDIR**: Keep all production agents in a single directory
2. **Consistent Naming**: Use descriptive, consistent naming conventions
3. **Version Control**: Include version information in agent -I responses
4. **Documentation**: Document agent roles and purposes

### Security Considerations

1. **Path Validation**: Only add agents from trusted locations
2. **Permission Management**: Ensure proper sudo permissions for systemctl
3. **Network Security**: Secure Flask interface for production use
4. **Agent Validation**: Always validate agents before adding to production

### Monitoring Best Practices

1. **Health Checks**: Monitor agent status regularly
2. **Service Management**: Use enable/disable for boot-time control
3. **Log Monitoring**: Check agent logs for issues
4. **Performance**: Monitor system resources with multiple agents

## Troubleshooting

### Common Issues

#### Agent Addition Fails

**Problem**: Agent fails validation during addition

**Solutions:**
1. Test agent manually: `/path/to/agent -I`
2. Check agent is executable: `ls -la /path/to/agent`
3. Verify agent is SD-compatible
4. Check for unique agent name

#### Agent Not Discovered

**Problem**: Expected agents don't appear in available agents list

**Solutions:**
1. Verify SOLARD_BINDIR is set correctly
2. Check directory permissions
3. Confirm agents respond to -I flag
4. Refresh browser/API call

#### Service Control Fails

**Problem**: Start/stop/restart operations fail

**Solutions:**
1. Check sudo permissions for systemctl
2. Verify service exists: `systemctl list-unit-files | grep agent_name`
3. Check systemd logs: `journalctl -u agent_name.service`
4. Confirm agent is properly installed

#### Configuration Corruption

**Problem**: managed_agents.json becomes corrupted

**Solutions:**
1. Check file permissions on /tmp/sc_managed_agents.json
2. Stop Flask interface and manually edit file
3. Remove corrupted file (will be recreated)
4. Restore from backup if available

### Debug Commands

```bash
# Check agent directory
ls -la $SOLARD_BINDIR

# Test agent manually
/opt/sd/bin/agent_name -I

# Check managed agents file
cat /tmp/sc_managed_agents.json

# Monitor Flask logs
tail -f flask_log_output

# Check SC process
ps aux | grep sc

# Test systemctl permissions
sudo systemctl status agent_name.service
```

## Integration Examples

### Automated Agent Deployment

```bash
#!/bin/bash
# Script to deploy and add agent automatically

AGENT_NAME="new_agent"
AGENT_PATH="/opt/sd/bin/$AGENT_NAME"
SC_API="http://localhost:5000"

# Copy agent binary
sudo cp $AGENT_NAME $AGENT_PATH
sudo chmod +x $AGENT_PATH

# Add to SC monitoring
curl -X POST $SC_API/api/agents/add \
  -H "Content-Type: application/json" \
  -d "{\"name\": \"$AGENT_NAME\", \"path\": \"$AGENT_PATH\"}"

# Enable and start service
curl -X POST $SC_API/api/agents/$AGENT_NAME/enable
curl -X POST $SC_API/api/agents/$AGENT_NAME/start

echo "Agent $AGENT_NAME deployed and started"
```

### Batch Agent Management

```bash
#!/bin/bash
# Script to manage multiple agents

AGENTS=("agent1" "agent2" "agent3")
ACTION="restart"

for agent in "${AGENTS[@]}"; do
  echo "Performing $ACTION on $agent..."
  curl -X POST http://localhost:5000/api/agents/$agent/$ACTION
  sleep 2
done
```

## Additional Resources

- **Service Controller Documentation**: See `PROJECT_DOCUMENTATION.md`
- **Flask API Documentation**: See `web/README.md`  
- **SD Agent Development**: See `/Users/steve/src/sd/CLAUDE.md`
- **Troubleshooting**: See agent-specific documentation in `/agents/`

## Support

For issues with agent management:

1. Check this user guide for common solutions
2. Review Flask web interface logs
3. Test agents manually with -I flag
4. Check systemd service status and logs
5. Verify SOLARD_BINDIR configuration