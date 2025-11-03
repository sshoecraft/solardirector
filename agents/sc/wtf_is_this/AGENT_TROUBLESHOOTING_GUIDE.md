# Agent Management Troubleshooting Guide

This guide provides detailed troubleshooting procedures for common issues encountered when using the Service Controller's agent management functionality.

## Quick Diagnostics Checklist

Before diving into specific issues, run through this quick checklist:

```bash
# 1. Verify SC is running
ps aux | grep -v grep | grep sc

# 2. Check Flask web interface
curl -s http://localhost:5000/api/system/status | jq .

# 3. Verify SOLARD_BINDIR
echo "SOLARD_BINDIR: $SOLARD_BINDIR"

# 4. Check agent directory permissions
ls -la $SOLARD_BINDIR

# 5. Test a known agent
/opt/sd/bin/test_agent -I 2>/dev/null | jq .

# 6. Check managed agents file
cat /tmp/sc_managed_agents.json | jq .
```

## Environment and Configuration Issues

### SOLARD_BINDIR Not Working

**Symptoms:**
- Agents not discovered from custom directory
- Available agents list empty
- Web interface shows default /opt/sd/bin

**Diagnosis:**
```bash
# Check environment variable is set
echo $SOLARD_BINDIR

# Check Flask configuration
curl http://localhost:5000/api/available-agents | jq .agent_dir

# Verify directory exists and has agents
ls -la $SOLARD_BINDIR
```

**Solutions:**
```bash
# Set environment variable for current session
export SOLARD_BINDIR="/custom/agent/path"

# Set for Flask web interface
SOLARD_BINDIR="/custom/agent/path" ./start_web.sh

# Make permanent in shell profile
echo 'export SOLARD_BINDIR="/custom/agent/path"' >> ~/.bashrc

# Restart web interface after setting
./start_web.sh --restart
```

### Agent Directory Permission Issues

**Symptoms:**
- "Permission denied" errors when scanning for agents
- Empty available agents list despite agents being present
- Flask logs show access errors

**Diagnosis:**
```bash
# Check directory permissions
ls -ld $SOLARD_BINDIR

# Check agent file permissions
ls -la $SOLARD_BINDIR/

# Check Flask process user
ps aux | grep flask | grep -v grep
```

**Solutions:**
```bash
# Fix directory permissions
sudo chmod 755 $SOLARD_BINDIR

# Fix agent executable permissions
sudo chmod +x $SOLARD_BINDIR/*

# Add Flask user to appropriate group
sudo usermod -a -G sd_group $USER
```

### Configuration File Corruption

**Symptoms:**
- managed_agents.json contains invalid JSON
- Agent additions fail with file errors
- SC fails to start with config errors

**Diagnosis:**
```bash
# Check JSON validity
cat /tmp/sc_managed_agents.json | jq .

# Check file permissions
ls -la /tmp/sc_managed_agents.json

# Check disk space
df -h /tmp
```

**Solutions:**
```bash
# Backup corrupted file
cp /tmp/sc_managed_agents.json /tmp/sc_managed_agents.json.backup

# Reset to empty configuration
echo '{"agents": []}' > /tmp/sc_managed_agents.json

# Or remove file completely (will be recreated)
rm /tmp/sc_managed_agents.json

# Restart SC process
curl -X POST http://localhost:5000/api/system/restart-sc
```

## Agent Discovery Issues

### Agents Not Appearing in Available List

**Symptoms:**
- Known agents in SOLARD_BINDIR not listed
- Available agents count is 0
- Discovery appears to be working but finds nothing

**Diagnosis:**
```bash
# Test agents manually
for agent in $SOLARD_BINDIR/*; do
  echo "Testing: $agent"
  if [ -x "$agent" ]; then
    $agent -I 2>/dev/null | jq . || echo "Failed"
  else
    echo "Not executable"
  fi
done

# Check Flask discovery API directly
curl http://localhost:5000/api/available-agents | jq .
```

**Solutions:**
```bash
# Ensure agents are executable
sudo chmod +x $SOLARD_BINDIR/*

# Fix agents that don't support -I flag
# (These need to be updated to be SD-compatible)

# Check for non-agent binaries in directory
# Move or remove non-SD executables

# Test individual agent
$SOLARD_BINDIR/agent_name -I
```

### Agent Validation Failures

**Symptoms:**
- Agents found but fail validation
- "Invalid agent" errors during addition
- -I flag responses are invalid

**Diagnosis:**
```bash
# Test agent -I response
AGENT_PATH="/opt/sd/bin/problematic_agent"
$AGENT_PATH -I

# Check for required JSON fields
$AGENT_PATH -I | jq '.agent_name, .agent_role'

# Test JSON parsing
$AGENT_PATH -I | jq . || echo "Invalid JSON"
```

**Solutions:**
```bash
# Fix agent -I response format
# Agent must return valid JSON with required fields:
# {
#   "agent_name": "name",
#   "agent_role": "role",
#   "agent_version": "version"
# }

# Check agent binary is SD-compatible
# Non-SD binaries need -I flag implementation

# Verify agent role is valid
# Must be one of: controller, storage, battery, inverter, 
# pvinverter, utility, sensor, device
```

### Already Managed Agents Issue

**Symptoms:**
- Agents disappear from available list unexpectedly
- Agents marked as managed but not visible in dashboard
- managed_agents.json and SC data mismatch

**Diagnosis:**
```bash
# Check managed agents file
cat /tmp/sc_managed_agents.json | jq .

# Check SC discovered agents
curl http://localhost:5000/api/agents | jq .

# Compare lists
curl http://localhost:5000/api/available-agents | jq .available_agents[].name
curl http://localhost:5000/api/agents | jq .agents[].name
```

**Solutions:**
```bash
# Clean up managed_agents.json if agents are orphaned
# Edit file to remove non-existent agents

# Restart SC to refresh agent discovery
curl -X POST http://localhost:5000/api/system/restart-sc

# Rebuild managed agents file from scratch if necessary
echo '{"agents": []}' > /tmp/sc_managed_agents.json
```

## Agent Addition Issues

### Path Validation Failures

**Symptoms:**
- "Agent path does not exist" errors
- "Agent path is not executable" errors
- Addition fails immediately

**Diagnosis:**
```bash
# Check path exists
AGENT_PATH="/path/to/agent"
ls -la "$AGENT_PATH"

# Check executable permission
[ -x "$AGENT_PATH" ] && echo "Executable" || echo "Not executable"

# Check path format
echo "$AGENT_PATH" | grep -E '^/[^[:space:]]+$'
```

**Solutions:**
```bash
# Fix path (use absolute paths)
AGENT_PATH="/full/absolute/path/to/agent"

# Fix permissions
sudo chmod +x "$AGENT_PATH"

# Verify file is not a directory
[ -f "$AGENT_PATH" ] && echo "File" || echo "Not a file"

# Check for special characters in path
# Avoid spaces and special characters in paths
```

### Agent Compatibility Issues

**Symptoms:**
- "Invalid agent" error after path validation
- Agent doesn't respond to -I flag correctly
- JSON parsing failures

**Diagnosis:**
```bash
# Test -I flag manually
/path/to/agent -I

# Check exit code
/path/to/agent -I; echo "Exit code: $?"

# Check output format
/path/to/agent -I | jq . 2>&1

# Check for required fields
/path/to/agent -I | jq '.agent_name // "MISSING", .agent_role // "MISSING"'
```

**Solutions:**
```bash
# Ensure agent is SD-compatible
# Non-SD agents need modification to support -I flag

# Fix JSON output format
# Must be valid JSON with required fields

# Check agent dependencies
ldd /path/to/agent

# Test with verbose output
/path/to/agent -I -v
```

### Duplicate Agent Names

**Symptoms:**
- "Agent already managed" errors
- Addition fails despite agent not being visible
- Conflicting agent names

**Diagnosis:**
```bash
# Check for existing agent
curl http://localhost:5000/api/agents | jq '.agents[] | select(.name == "agent_name")'

# Check managed_agents.json
cat /tmp/sc_managed_agents.json | jq '.agents[] | select(.name == "agent_name")'

# List all managed agent names
cat /tmp/sc_managed_agents.json | jq '.agents[].name'
```

**Solutions:**
```bash
# Use unique agent name
# Change agent name to be unique

# Remove conflicting entry if orphaned
# Edit managed_agents.json to remove old entry

# Clear managed agents if needed
echo '{"agents": []}' > /tmp/sc_managed_agents.json
```

## Service Control Issues

### systemctl Permission Denied

**Symptoms:**
- Service start/stop/restart operations fail
- "Permission denied" errors in Flask logs
- systemctl operations timeout

**Diagnosis:**
```bash
# Test systemctl manually
sudo systemctl status agent_name.service

# Check sudo permissions
sudo -l | grep systemctl

# Test without password prompt
sudo -n systemctl status agent_name.service
```

**Solutions:**
```bash
# Configure passwordless sudo for systemctl
sudo visudo

# Add line (replace username):
username ALL=(ALL) NOPASSWD: /bin/systemctl

# Or more specific:
username ALL=(ALL) NOPASSWD: /bin/systemctl start *, /bin/systemctl stop *, /bin/systemctl restart *, /bin/systemctl enable *, /bin/systemctl disable *

# Test configuration
sudo -n systemctl status agent_name.service
```

### Service Not Found Errors

**Symptoms:**
- "Service not found" errors
- systemctl operations fail
- Services don't exist

**Diagnosis:**
```bash
# Check if service file exists
ls -la /etc/systemd/system/agent_name.service

# List all SD services
systemctl list-unit-files | grep -E '\.(service)' | grep -v '@'

# Check service status
systemctl list-unit-files agent_name.service
```

**Solutions:**
```bash
# Install agent service
# Agents need proper installation with service files

# Check agent Makefile for install target
cd /path/to/agent/source
make install

# Manually create service file if needed
sudo systemctl daemon-reload
```

### Service Operation Timeouts

**Symptoms:**
- Flask API operations timeout
- Long delays in web interface
- systemctl commands hang

**Diagnosis:**
```bash
# Test systemctl timing
time sudo systemctl status agent_name.service

# Check system load
top
iostat 1 5

# Check for deadlocks
ps aux | grep systemctl
```

**Solutions:**
```bash
# Increase Flask timeout in app.py
# Modify subprocess timeout values

# Fix underlying systemctl issues
sudo systemctl daemon-reload

# Check for system resource issues
free -h
df -h
```

## Web Interface Issues

### Flask Server Not Starting

**Symptoms:**
- start_web.sh fails
- "Address already in use" errors
- Python dependency errors

**Diagnosis:**
```bash
# Check port availability
netstat -tulpn | grep :5000

# Check Python environment
python3 --version
pip list | grep -E '(flask|cors|psutil)'

# Check for error messages
./start_web.sh 2>&1 | tail -20
```

**Solutions:**
```bash
# Kill existing Flask process
pkill -f flask
pkill -f "python.*app.py"

# Use different port
PORT=5001 ./start_web.sh

# Reinstall dependencies
./start_web.sh --clean

# Check virtual environment
source venv/bin/activate
pip install -r requirements.txt
```

### API Endpoints Returning Errors

**Symptoms:**
- 500 Internal Server Error responses
- JSON parsing errors
- API calls fail unexpectedly

**Diagnosis:**
```bash
# Check Flask logs
tail -f flask_log_output

# Test API endpoints individually
curl -v http://localhost:5000/api/agents
curl -v http://localhost:5000/api/available-agents

# Check SC process status
curl http://localhost:5000/api/system/status | jq .flask_server
```

**Solutions:**
```bash
# Restart Flask server
./start_web.sh --restart

# Check SC process is running
curl -X POST http://localhost:5000/api/system/restart-sc

# Clear browser cache
# Use incognito/private browsing mode

# Check file permissions
ls -la /tmp/sc_data.json
ls -la /tmp/sc_managed_agents.json
```

### Dashboard Not Updating

**Symptoms:**
- Agent status not refreshing
- Changes not reflected in dashboard
- Stale data displayed

**Diagnosis:**
```bash
# Check auto-refresh is working
# Open browser developer tools, check Network tab

# Verify data file is being updated
stat /tmp/sc_data.json

# Check SC process is writing data
ls -la /tmp/sc_data.json*
```

**Solutions:**
```bash
# Force browser refresh (Ctrl+F5)

# Check SC web export mode
ps aux | grep "sc.*-w"

# Restart SC process
curl -X POST http://localhost:5000/api/system/restart-sc

# Clear browser cache completely
```

## Performance Issues

### Slow Agent Discovery

**Symptoms:**
- Available agents API takes long time
- Dashboard loads slowly
- High CPU usage during discovery

**Diagnosis:**
```bash
# Time the discovery process
time curl http://localhost:5000/api/available-agents

# Check number of files in agent directory
ls $SOLARD_BINDIR | wc -l

# Monitor system resources
top -p $(pgrep -f flask)
```

**Solutions:**
```bash
# Reduce number of non-agent files in SOLARD_BINDIR
# Move non-SD binaries to different directory

# Optimize agent -I responses
# Ensure agents respond quickly to -I flag

# Consider agent directory organization
# Use subdirectories for different agent types
```

### Memory Usage Issues

**Symptoms:**
- High memory usage by Flask process
- System becomes slow
- Out of memory errors

**Diagnosis:**
```bash
# Check memory usage
ps aux | grep -E '(flask|python.*app.py)'
free -h

# Monitor memory over time
watch "ps aux | grep flask | grep -v grep"
```

**Solutions:**
```bash
# Restart Flask periodically
./start_web.sh --restart

# Reduce agent polling frequency
# Modify refresh rates in configuration

# Check for memory leaks
# Monitor long-running processes
```

## Data Consistency Issues

### SC Data vs Managed Agents Mismatch

**Symptoms:**
- Agents shown in dashboard but not in managed_agents.json
- Managed agents not appearing in SC data
- Inconsistent agent counts

**Diagnosis:**
```bash
# Compare data sources
echo "SC Data:"
curl http://localhost:5000/api/agents | jq '.agents[].name'

echo "Managed Agents:"
cat /tmp/sc_managed_agents.json | jq '.agents[].name'

echo "Available Agents:"
curl http://localhost:5000/api/available-agents | jq '.available_agents[].name'
```

**Solutions:**
```bash
# Restart SC to refresh discovery
curl -X POST http://localhost:5000/api/system/restart-sc

# Clean up managed_agents.json
# Remove entries for non-existent agents

# Force full rediscovery
rm /tmp/sc_data.json
curl -X POST http://localhost:5000/api/system/restart-sc
```

## Recovery Procedures

### Complete Reset

When all else fails, perform a complete reset:

```bash
# 1. Stop all processes
pkill -f flask
pkill -f sc

# 2. Clear all data files
rm -f /tmp/sc_data.json*
rm -f /tmp/sc_managed_agents.json*

# 3. Reset environment
unset SOLARD_BINDIR
export SOLARD_BINDIR="/opt/sd/bin"

# 4. Restart everything
cd web/
./start_web.sh --clean

# 5. Verify basic functionality
curl http://localhost:5000/api/system/status
```

### Backup and Restore

Create backups before making major changes:

```bash
# Create backup
mkdir -p ~/sc_backup/$(date +%Y%m%d_%H%M%S)
cp /tmp/sc_managed_agents.json ~/sc_backup/$(date +%Y%m%d_%H%M%S)/
cp /tmp/sc_data.json ~/sc_backup/$(date +%Y%m%d_%H%M%S)/

# Restore from backup
BACKUP_DIR="~/sc_backup/20250101_120000"
cp $BACKUP_DIR/sc_managed_agents.json /tmp/
curl -X POST http://localhost:5000/api/system/restart-sc
```

## Logging and Monitoring

### Enable Debug Logging

```bash
# Flask debug mode
FLASK_DEBUG=true ./start_web.sh

# SC debug mode
./sc -d -w -r 5

# System logging
journalctl -f | grep -E '(sc|flask)'
```

### Log File Locations

- **Flask logs**: Console output when running ./start_web.sh
- **SC logs**: Console output or syslog if running as service
- **systemd logs**: `journalctl -u agent_name.service`
- **System logs**: `/var/log/syslog` or `/var/log/messages`

## Getting Help

If you continue to experience issues:

1. **Gather Information**:
   - Error messages and log outputs
   - Environment configuration (SOLARD_BINDIR, etc.)
   - Agent details and -I flag responses
   - System information (OS, Python version, etc.)

2. **Check Documentation**:
   - Agent Management User Guide
   - Project Documentation
   - Flask README

3. **Test in Isolation**:
   - Test individual agents manually
   - Use minimal configuration
   - Try with test/mock agents

4. **Community Support**:
   - Check SD project documentation
   - Review similar issues in project history
   - Contact SD development team if needed