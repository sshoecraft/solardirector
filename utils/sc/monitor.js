/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

// Service Controller monitoring functions

// Process agent status update
function processAgentStatus(agent_name, status_data) {
    log_debug("Processing status for agent: " + agent_name);
    
    // Here we can add custom logic for processing status data
    // For example, checking for error conditions, alerts, etc.
    
    if (status_data) {
        try {
            var status = JSON.parse(status_data);
            
            // Check for common error conditions
            if (status.errors && status.errors.length > 0) {
                log_warning("Agent " + agent_name + " reported errors: " + status.errors.join(", "));
            }
            
            // Check for state changes
            if (status.state) {
                log_debug("Agent " + agent_name + " state: " + status.state);
            }
            
        } catch (e) {
            log_error("Failed to parse status JSON for agent " + agent_name + ": " + e.message);
        }
    }
}

// Process agent data update
function processAgentData(agent_name, data) {
    log_debug("Processing data for agent: " + agent_name);
    
    // Custom logic for processing agent data
    // This could include data validation, trend analysis, etc.
}

// Check agent health and generate alerts
function checkAgentHealth(agents) {
    var online_count = 0;
    var warning_count = 0;
    var error_count = 0;
    var total_count = agents.length;
    
    for (var i = 0; i < agents.length; i++) {
        var agent = agents[i];
        var health = getAgentHealth(agent);
        
        switch (health) {
            case AGENT_HEALTH.GOOD:
                online_count++;
                break;
            case AGENT_HEALTH.WARNING:
                warning_count++;
                break;
            case AGENT_HEALTH.POOR:
            case AGENT_HEALTH.DOWN:
                error_count++;
                break;
        }
    }
    
    // Log overall system health
    if (error_count > 0) {
        log_warning(sprintf("System health: %d/%d agents offline/unhealthy", 
            error_count, total_count));
    } else if (warning_count > 0) {
        log_info(sprintf("System health: %d/%d agents have warnings", 
            warning_count, total_count));
    } else {
        log_debug(sprintf("System health: All %d agents healthy", total_count));
    }
    
    return {
        online: online_count,
        warning: warning_count,
        error: error_count,
        total: total_count
    };
}

// Monitor system performance
function monitorSystemPerformance() {
    // This could include monitoring system resources,
    // MQTT message rates, agent response times, etc.
    
    log_debug("Monitoring system performance...");
}

// Generate monitoring report
function generateMonitoringReport(agents) {
    var report = {
        timestamp: time(),
        agent_count: agents.length,
        health_summary: checkAgentHealth(agents),
        agents: []
    };
    
    for (var i = 0; i < agents.length; i++) {
        var agent = agents[i];
        report.agents.push({
            name: agent.name,
            role: agent.role,
            status: getAgentStatus(agent),
            health: getAgentHealth(agent),
            last_seen: agent.last_seen,
            have_status: agent.have_status,
            have_data: agent.have_data
        });
    }
    
    return report;
}