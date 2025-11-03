/*
 * Service Controller (SC) - Initialization
 * Manages Solar Director agent discovery and monitoring
 */

// Global configuration and state
var config = {
    agent_dir: "/Users/steve/bin",
    managed_agents_file: "/tmp/sc_managed_agents.json",
    data_export_file: "/tmp/sc_data.json",
    discovery_interval: 60,
    mqtt_topics: [
        "solar/+/Status",
        "solar/+/Info", 
        "solar/+/Config",
        "solar/+/Data"
    ]
};

var state = {
    agents: {},
    managed_agents: [],
    discovering: false,
    last_discovery: 0
};

// Initialize SC
function init() {
    printf("SC: Initializing Service Controller...\n");
    
    // Load managed agents list
    load_managed_agents();
    
    // Subscribe to MQTT topics for agent monitoring
    if (typeof mqtt !== 'undefined') {
        config.mqtt_topics.forEach(function(topic) {
            printf("SC: Subscribing to MQTT topic: %s\n", topic);
            mqtt.sub(topic);
        });
    }
    
    // Start initial discovery
    discover_agents();
    
    // Schedule periodic discovery
    setInterval(function() {
        discover_agents();
    }, config.discovery_interval * 1000);
    
    // Schedule periodic data export
    setInterval(function() {
        export_data();
    }, 5000);  // Export every 5 seconds
    
    printf("SC: Initialization complete\n");
}

// Load managed agents from JSON file
function load_managed_agents() {
    try {
        var content = read_file(config.managed_agents_file);
        if (content) {
            var data = JSON.parse(content);
            state.managed_agents = data.agents || [];
            printf("SC: Loaded %d managed agents\n", state.managed_agents.length);
        }
    } catch (e) {
        printf("SC: Error loading managed agents: %s\n", e.toString());
        state.managed_agents = [];
    }
}

// Discover agents from managed list
function discover_agents() {
    if (state.discovering) {
        return;
    }
    
    state.discovering = true;
    state.last_discovery = time();
    
    printf("SC: Starting agent discovery...\n");
    
    // Clear existing agents
    state.agents = {};
    
    // Process each managed agent
    state.managed_agents.forEach(function(managed) {
        if (!managed.enabled) {
            return;
        }
        
        printf("SC: Discovering agent: %s at %s\n", managed.name, managed.path);
        
        try {
            // Get agent info using -I flag
            var info_json = get_agent_info(managed.path);
            if (info_json) {
                var agent = {
                    name: managed.name,
                    path: managed.path,
                    role: info_json.agent_role || "Unknown",
                    version: info_json.agent_version || "Unknown",
                    status: "Offline",
                    online: false,
                    have_status: false,
                    have_data: false,
                    last_seen: 0,
                    last_status: 0,
                    last_data: 0,
                    current_time: time(),
                    health: "Unknown",
                    info_data: info_json
                };
                
                state.agents[managed.name] = agent;
                printf("SC: Added agent %s (role: %s, version: %s)\n", 
                       agent.name, agent.role, agent.version);
            } else {
                printf("SC: Failed to get info for agent: %s\n", managed.name);
            }
        } catch (e) {
            printf("SC: Error discovering agent %s: %s\n", managed.name, e.toString());
        }
    });
    
    state.discovering = false;
    printf("SC: Discovery complete, found %d agents\n", Object.keys(state.agents).length);
}

// Get agent info by calling agent with -I flag
function get_agent_info(agent_path) {
    try {
        var cmd = agent_path + " -I";
        var output = system_exec(cmd);
        
        if (output && output.length > 0) {
            return JSON.parse(output);
        }
    } catch (e) {
        printf("SC: Error getting agent info for %s: %s\n", agent_path, e.toString());
    }
    
    return null;
}

// Export current state to JSON file for web UI
function export_data() {
    try {
        var current_time = time();
        var agent_list = [];
        var role_counts = {
            inverter: 0,
            battery: 0, 
            controller: 0,
            storage: 0,
            sensor: 0,
            utility: 0,
            device: 0,
            other: 0
        };
        
        var online_count = 0;
        var offline_count = 0;
        
        // Process each agent
        for (var name in state.agents) {
            var agent = state.agents[name];
            
            // Update time-based fields
            agent.current_time = current_time;
            agent.last_seen_ago = format_duration(agent.last_seen, current_time);
            agent.last_status_ago = format_duration(agent.last_status, current_time);
            agent.last_data_ago = agent.last_data > 0 ? 
                format_duration(agent.last_data, current_time) : "Never";
            
            // Determine health status
            if (agent.online) {
                if (agent.have_data) {
                    agent.health = "Healthy";
                } else {
                    agent.health = "Warning";
                }
                online_count++;
            } else {
                agent.health = "Offline";
                offline_count++;
            }
            
            // Count by role
            var role_key = agent.role.toLowerCase();
            if (role_counts.hasOwnProperty(role_key)) {
                role_counts[role_key]++;
            } else {
                role_counts.other++;
            }
            
            agent_list.push(agent);
        }
        
        // Create export data
        var export_data = {
            agents: {
                agents: agent_list
            },
            system: {
                timestamp: current_time,
                total_agents: agent_list.length,
                online_agents: online_count,
                offline_agents: offline_count,
                last_discovery: state.last_discovery,
                discovery_interval: config.discovery_interval,
                discovering: state.discovering,
                agent_dir: config.agent_dir,
                role_counts: role_counts
            }
        };
        
        // Write to export file
        write_file(config.data_export_file, JSON.stringify(export_data, null, 4));
    } catch (e) {
        printf("SC: Error exporting data: %s\n", e.toString());
    }
}

// Format duration between two timestamps
function format_duration(start, end) {
    if (start === 0) return "Never";
    
    var diff = end - start;
    if (diff < 60) {
        return "00:" + (diff < 10 ? "0" : "") + diff.toString();
    } else if (diff < 3600) {
        var mins = Math.floor(diff / 60);
        var secs = diff % 60;
        return (mins < 10 ? "0" : "") + mins.toString() + ":" + 
               (secs < 10 ? "0" : "") + secs.toString();
    } else {
        var hours = Math.floor(diff / 3600);
        var mins = Math.floor((diff % 3600) / 60);
        return hours.toString() + "h " + mins.toString() + "m";
    }
}

// MQTT message handler
function mqtt_message(topic, message) {
    try {
        var parts = topic.split('/');
        if (parts.length < 3 || parts[0] !== 'solar') {
            return;
        }
        
        var agent_name = parts[1];
        var function_name = parts[2];
        
        // Find the agent
        var agent = state.agents[agent_name];
        if (!agent) {
            // Agent not in our managed list, ignore
            return;
        }
        
        var current_time = time();
        agent.last_seen = current_time;
        agent.online = true;
        
        // Process different message types
        if (function_name === 'Status') {
            agent.have_status = true;
            agent.last_status = current_time;
            agent.status = "Online";
            
            try {
                agent.status_data = JSON.parse(message);
            } catch (e) {
                printf("SC: Error parsing status JSON for %s: %s\n", agent_name, e.toString());
            }
        } else if (function_name === 'Data') {
            agent.have_data = true;
            agent.last_data = current_time;
        } else if (function_name === 'Config') {
            try {
                agent.config_data = JSON.parse(message);
            } catch (e) {
                printf("SC: Error parsing config JSON for %s: %s\n", agent_name, e.toString());
            }
        }
        
    } catch (e) {
        printf("SC: Error processing MQTT message: %s\n", e.toString());
    }
}

// Add managed agent (called from web UI via file)
function add_managed_agent(name, path) {
    // Check if already exists
    for (var i = 0; i < state.managed_agents.length; i++) {
        if (state.managed_agents[i].name === name) {
            return false; // Already exists
        }
    }
    
    // Add new agent
    var new_agent = {
        name: name,
        path: path,
        enabled: true,
        added: time()
    };
    
    state.managed_agents.push(new_agent);
    save_managed_agents();
    
    // Trigger immediate discovery
    discover_agents();
    
    return true;
}

// Remove managed agent
function remove_managed_agent(name) {
    for (var i = 0; i < state.managed_agents.length; i++) {
        if (state.managed_agents[i].name === name) {
            state.managed_agents.splice(i, 1);
            save_managed_agents();
            
            // Remove from active agents
            delete state.agents[name];
            
            return true;
        }
    }
    return false;
}

// Save managed agents to file
function save_managed_agents() {
    try {
        var data = {
            agents: state.managed_agents
        };
        write_file(config.managed_agents_file, JSON.stringify(data, null, 2));
    } catch (e) {
        printf("SC: Error saving managed agents: %s\n", e.toString());
    }
}

// Initialize when script loads
init();