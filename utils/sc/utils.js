/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

// Service Controller utility functions

// Format time duration
function formatDuration(seconds) {
    if (seconds < 60) {
        return sprintf("%02d:%02d", 0, seconds);
    } else if (seconds < 3600) {
        var mins = Math.floor(seconds / 60);
        var secs = seconds % 60;
        return sprintf("%02d:%02d", mins, secs);
    } else {
        var hours = Math.floor(seconds / 3600);
        var mins = Math.floor((seconds % 3600) / 60);
        var secs = seconds % 60;
        return sprintf("%02d:%02d:%02d", hours, mins, secs);
    }
}

// Get agent status string
function getAgentStatus(agent) {
    if (!agent.last_seen) {
        return AGENT_STATUS.UNKNOWN;
    }
    
    var now = time();
    var age = now - agent.last_seen;
    
    if (age < SC_CONFIG.offline_timeout) {
        return AGENT_STATUS.ONLINE;
    } else {
        return AGENT_STATUS.OFFLINE;
    }
}

// Get agent health string
function getAgentHealth(agent) {
    var status = getAgentStatus(agent);
    
    if (status == AGENT_STATUS.OFFLINE) {
        return AGENT_HEALTH.DOWN;
    } else if (status == AGENT_STATUS.UNKNOWN) {
        return AGENT_HEALTH.DOWN;
    } else if (!agent.have_status && !agent.have_data) {
        return AGENT_HEALTH.POOR;
    } else if (!agent.have_data) {
        return AGENT_HEALTH.WARNING;
    } else {
        return AGENT_HEALTH.GOOD;
    }
}

// Format agent role for display
function formatRole(role) {
    if (!role) return "Unknown";
    return role;
}

// Format agent version for display
function formatVersion(version) {
    if (!version) return "1.0";
    return version;
}

// Check if file exists
function file_exists(filename) {
    return access(filename, 0) == 0;
}

// Log message with timestamp
function log_message(level, message) {
    var now = new Date();
    var timestamp = sprintf("%04d-%02d-%02d %02d:%02d:%02d",
        now.getFullYear(), now.getMonth() + 1, now.getDate(),
        now.getHours(), now.getMinutes(), now.getSeconds());
    
    printf("[%s] %s: %s\n", timestamp, level, message);
}

// Info logging
function log_info(message) {
    log_message("INFO", message);
}

// Warning logging
function log_warning(message) {
    log_message("WARN", message);
}

// Error logging
function log_error(message) {
    log_message("ERROR", message);
}

// Debug logging
function log_debug(message) {
    log_message("DEBUG", message);
}