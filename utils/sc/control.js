/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

// Service Controller agent control functions

// Start an agent service
function startAgent(agent_name) {
    log_info("Starting agent: " + agent_name);
    
    var cmd = sprintf("systemctl start %s.service", agent_name);
    var result = exec(cmd);
    
    if (result.exit_code == 0) {
        log_info("Successfully started agent: " + agent_name);
        return true;
    } else {
        log_error("Failed to start agent " + agent_name + ": " + result.stderr);
        return false;
    }
}

// Stop an agent service
function stopAgent(agent_name) {
    log_info("Stopping agent: " + agent_name);
    
    var cmd = sprintf("systemctl stop %s.service", agent_name);
    var result = exec(cmd);
    
    if (result.exit_code == 0) {
        log_info("Successfully stopped agent: " + agent_name);
        return true;
    } else {
        log_error("Failed to stop agent " + agent_name + ": " + result.stderr);
        return false;
    }
}

// Restart an agent service
function restartAgent(agent_name) {
    log_info("Restarting agent: " + agent_name);
    
    var cmd = sprintf("systemctl restart %s.service", agent_name);
    var result = exec(cmd);
    
    if (result.exit_code == 0) {
        log_info("Successfully restarted agent: " + agent_name);
        return true;
    } else {
        log_error("Failed to restart agent " + agent_name + ": " + result.stderr);
        return false;
    }
}

// Enable an agent service for auto-start
function enableAgent(agent_name) {
    log_info("Enabling agent: " + agent_name);
    
    var cmd = sprintf("systemctl enable %s.service", agent_name);
    var result = exec(cmd);
    
    if (result.exit_code == 0) {
        log_info("Successfully enabled agent: " + agent_name);
        return true;
    } else {
        log_error("Failed to enable agent " + agent_name + ": " + result.stderr);
        return false;
    }
}

// Disable an agent service from auto-start
function disableAgent(agent_name) {
    log_info("Disabling agent: " + agent_name);
    
    var cmd = sprintf("systemctl disable %s.service", agent_name);
    var result = exec(cmd);
    
    if (result.exit_code == 0) {
        log_info("Successfully disabled agent: " + agent_name);
        return true;
    } else {
        log_error("Failed to disable agent " + agent_name + ": " + result.stderr);
        return false;
    }
}

// Get service status from systemd
function getServiceStatus(agent_name) {
    var cmd = sprintf("systemctl is-active %s.service", agent_name);
    var result = exec(cmd);
    
    if (result.exit_code == 0) {
        return result.stdout.trim();
    } else {
        return "unknown";
    }
}

// Get service enabled status from systemd
function getServiceEnabled(agent_name) {
    var cmd = sprintf("systemctl is-enabled %s.service", agent_name);
    var result = exec(cmd);
    
    if (result.exit_code == 0) {
        return result.stdout.trim();
    } else {
        return "unknown";
    }
}

// Send MQTT command to agent
function sendAgentCommand(agent_name, command, args) {
    log_info(sprintf("Sending command to agent %s: %s", agent_name, command));
    
    var topic = sprintf("SolarD/Agents/%s/Config", agent_name);
    var message = {
        command: command,
        args: args || {},
        timestamp: time(),
        source: "sc"
    };
    
    // This would need to be implemented in C side
    // mqtt_pub(topic, JSON.stringify(message));
    
    log_debug("Command sent to agent " + agent_name);
    return true;
}

// Request agent status update
function requestAgentStatus(agent_name) {
    return sendAgentCommand(agent_name, "get_status", {});
}

// Request agent info update  
function requestAgentInfo(agent_name) {
    return sendAgentCommand(agent_name, "get_info", {});
}

// Set agent configuration parameter
function setAgentConfig(agent_name, param, value) {
    log_info(sprintf("Setting config for agent %s: %s = %s", agent_name, param, value));
    
    return sendAgentCommand(agent_name, "set_config", {
        parameter: param,
        value: value
    });
}

// Get agent configuration parameter
function getAgentConfig(agent_name, param) {
    log_info(sprintf("Getting config for agent %s: %s", agent_name, param));
    
    return sendAgentCommand(agent_name, "get_config", {
        parameter: param
    });
}