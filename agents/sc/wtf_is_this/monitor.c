/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"
#include <ctype.h>

#include "sc.h"

static int sc_agent_sort(void *i1, void *i2) {
	sc_agent_info_t *a1 = (sc_agent_info_t *)i1;
	sc_agent_info_t *a2 = (sc_agent_info_t *)i2;
	
	return strcmp(a1->name, a2->name);
}

/* Config file reading functions for agent directory fallback */
static char *sc_read_config_value(const char *config_path, const char *key) {
	FILE *fp;
	char line[512];
	char *value = NULL;
	char section[64] = "";
	int in_global_section = 0;
	
	dprintf(3, "reading config: %s, key: %s\n", config_path, key);
	
	fp = fopen(config_path, "r");
	if (!fp) {
		dprintf(3, "failed to open config file: %s\n", config_path);
		return NULL;
	}
	
	while (fgets(line, sizeof(line), fp)) {
		char *trimmed = line;
		
		/* Skip leading whitespace */
		while (isspace(*trimmed)) trimmed++;
		
		/* Skip empty lines and comments */
		if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';') continue;
		
		/* Check for section headers */
		if (*trimmed == '[') {
			char *end = strchr(trimmed, ']');
			if (end) {
				*end = '\0';
				strncpy(section, trimmed + 1, sizeof(section) - 1);
				in_global_section = (strlen(section) == 0 || strcmp(section, "global") == 0);
				dprintf(4, "found section: [%s], in_global: %d\n", section, in_global_section);
				continue;
			}
		}
		
		/* Only process lines in global section or no section */
		if (!in_global_section && strlen(section) > 0) continue;
		
		/* Look for key=value pairs */
		char *equals = strchr(trimmed, '=');
		if (equals) {
			*equals = '\0';
			char *config_key = trimmed;
			char *config_value = equals + 1;
			
			/* Trim whitespace from key */
			char *key_end = config_key + strlen(config_key) - 1;
			while (key_end > config_key && isspace(*key_end)) *key_end-- = '\0';
			
			/* Trim whitespace from value */
			while (isspace(*config_value)) config_value++;
			char *value_end = config_value + strlen(config_value) - 1;
			while (value_end > config_value && isspace(*value_end)) *value_end-- = '\0';
			
			dprintf(4, "config pair: '%s' = '%s'\n", config_key, config_value);
			
			/* Check if this is the key we're looking for */
			if (strcmp(config_key, key) == 0) {
				value = malloc(strlen(config_value) + 1);
				if (value) {
					strcpy(value, config_value);
					dprintf(3, "found %s = %s\n", key, value);
				}
				break;
			}
		}
	}
	
	fclose(fp);
	return value;
}

static int sc_get_agent_dir_from_config(char *agent_dir, size_t agent_dir_size) {
	char home[256];
	char config_path[512];
	char *bindir = NULL;
	char *prefix = NULL;
	const char *config_files[] = {
		NULL,  /* Will be set to ~/.sd.conf */
		"/etc/sd.conf",
		"/usr/local/etc/sd.conf"
	};
	int i;
	
	dprintf(2, "searching for agent directory in config files\n");
	
	/* Set up home config path */
	if (gethomedir(home, sizeof(home) - 1) == 0) {
		snprintf(config_path, sizeof(config_path), "%s/.sd.conf", home);
		config_files[0] = config_path;
	}
	
	/* Try each config file in order */
	for (i = 0; i < 3; i++) {
		if (!config_files[i]) continue;
		
		dprintf(3, "checking config file: %s\n", config_files[i]);
		
		if (access(config_files[i], R_OK) != 0) {
			dprintf(3, "config file not accessible: %s\n", config_files[i]);
			continue;
		}
		
		/* First try to get bindir directly */
		bindir = sc_read_config_value(config_files[i], "bindir");
		if (bindir) {
			strncpy(agent_dir, bindir, agent_dir_size - 1);
			agent_dir[agent_dir_size - 1] = '\0';
			dprintf(2, "found bindir in %s: %s\n", config_files[i], agent_dir);
			free(bindir);
			return 0;
		}
		
		/* If no bindir, try prefix and append /bin */
		prefix = sc_read_config_value(config_files[i], "prefix");
		if (prefix) {
			snprintf(agent_dir, agent_dir_size, "%s/bin", prefix);
			dprintf(2, "found prefix in %s, using: %s\n", config_files[i], agent_dir);
			free(prefix);
			return 0;
		}
	}
	
	dprintf(2, "no agent directory found in config files\n");
	return -1;
}

sc_session_t *sc_init(solard_client_t *c, const char *agent_dir) {
	sc_session_t *sc;

	sc = calloc(1, sizeof(*sc));
	if (!sc) {
		log_syserror("sc_init: calloc");
		return 0;
	}

	sc->c = c;
	sc->agents = list_create();
	if (!sc->agents) {
		log_error("sc_init: unable to create agents list\n");
		free(sc);
		return 0;
	}
	
	sc->managed_agents = list_create();
	if (!sc->managed_agents) {
		log_error("sc_init: unable to create managed agents list\n");
		list_destroy(sc->agents);
		free(sc);
		return 0;
	}

	/* Use the agent directory passed from main() which already has fallback logic */
	strncpy(sc->agent_dir, agent_dir, sizeof(sc->agent_dir)-1);
	sc->agent_dir[sizeof(sc->agent_dir)-1] = '\0';
	dprintf(1, "using agent directory: %s\n", sc->agent_dir);
	
	/* Set up managed agents config file path */
	sprintf(sc->managed_agents_file, "/tmp/sc_managed_agents.json");
	dprintf(1, "managed_agents_file: %s\n", sc->managed_agents_file);
	
	/* Load managed agents */
	sc_load_managed_agents(sc);

	/* Set discovery interval */
	sc->discovery_interval = 60; /* Re-discover every 60 seconds */

#ifdef MQTT
	/* Enable MQTT message queue */
	if (c->m) {
		c->addmq = true;
		/* Subscribe to all agent topics */
		mqtt_sub(c->m, SOLARD_TOPIC_ROOT"/"SOLARD_TOPIC_AGENTS"/#");
	}
#endif

#ifdef JS
	/* Set up JavaScript file paths */
	sprintf(sc->js.init_script, "%s/init.js", ".");
	sprintf(sc->js.monitor_script, "%s/monitor.js", ".");
	sprintf(sc->js.control_script, "%s/control.js", ".");
	sprintf(sc->js.utils_script, "%s/utils.js", ".");
#endif

	/* Initial agent discovery */
	sc_discover_agents(sc);

	return sc;
}

void sc_shutdown(sc_session_t *sc) {
	sc_agent_info_t *info;

	if (!sc) return;

	/* Free agent info structures */
	if (sc->agents) {
		list_reset(sc->agents);
		while((info = list_get_next(sc->agents)) != 0) {
			if (info->status_data) json_destroy_object(info->status_data);
			if (info->info_data) json_destroy_object(info->info_data);
			if (info->config_data) json_destroy_object(info->config_data);
		}
		list_destroy(sc->agents);
	}
	
	/* Free managed agents list */
	if (sc->managed_agents) {
		list_destroy(sc->managed_agents);
	}
	
	free(sc);
}

int sc_discover_agents(sc_session_t *sc) {
	sc_managed_agent_t *managed_agent;
	sc_agent_info_t new_info;
	sc_agent_info_t *existing;
	time_t now;
	int count = 0;

	dprintf(1, "discovering managed agents\n");

	time(&now);
	sc->discovering = true;

	/* Process each managed agent */
	list_reset(sc->managed_agents);
	while((managed_agent = list_get_next(sc->managed_agents)) != 0) {
		if (!managed_agent->enabled) {
			dprintf(2, "skipping disabled agent: %s\n", managed_agent->name);
			continue;
		}

		dprintf(2, "checking managed agent: %s at %s\n", managed_agent->name, managed_agent->path);
		printf("SC DEBUG: Checking agent %s at %s\n", managed_agent->name, managed_agent->path);

		/* Try to get agent info */
		memset(&new_info, 0, sizeof(new_info));
		int result = sc_get_agent_info(sc, managed_agent->path, &new_info);
		printf("SC DEBUG: sc_get_agent_info returned %d for %s\n", result, managed_agent->name);
		if (result == 0) {
			/* Check if agent already exists */
			existing = sc_find_agent(sc, new_info.name);
			if (!existing) {
				/* Add new agent */
				dprintf(1, "adding managed agent: %s (%s)\n", new_info.name, new_info.role);
				list_add(sc->agents, &new_info, sizeof(new_info));
				list_sort(sc->agents, sc_agent_sort, 0);
				count++;
			} else {
				/* Update existing agent path if changed */
				if (strcmp(existing->path, managed_agent->path) != 0) {
					dprintf(1, "updating path for agent %s: %s\n", existing->name, managed_agent->path);
					strcpy(existing->path, managed_agent->path);
				}
			}
		} else {
			dprintf(2, "failed to get info for managed agent: %s\n", managed_agent->name);
		}
	}

	sc->discovering = false;
	sc->last_discovery = now;

	dprintf(1, "discovered %d managed agents\n", count);
	return count;
}

sc_agent_info_t *sc_find_agent(sc_session_t *sc, char *name) {
	sc_agent_info_t *info;

	list_reset(sc->agents);
	while((info = list_get_next(sc->agents)) != 0) {
		if (strcmp(info->name, name) == 0) {
			return info;
		}
	}
	return 0;
}

int sc_get_agent_info(sc_session_t *sc, char *path, sc_agent_info_t *info) {
	char command[512];
	FILE *fp;
	char output[16384];  /* Increased buffer for large agent JSON output */
	size_t len;
	json_value_t *v;
	json_object_t *o;
	char *role, *name, *version;

	/* Execute agent with -I argument to get info */
	sprintf(command, "%s -I 2>/dev/null", path);
	dprintf(3, "executing: %s\n", command);

	fp = popen(command, "r");
	if (!fp) {
		dprintf(2, "failed to execute: %s\n", command);
		return -1;
	}

	/* Read output */
	len = fread(output, 1, sizeof(output)-1, fp);
	output[len] = 0;
	int exit_code = pclose(fp);

	printf("SC DEBUG: Command '%s' read %zu bytes, exit code %d\n", command, len, exit_code);
	printf("SC DEBUG: Output: '%.100s%s'\n", output, len > 100 ? "..." : "");
	dprintf(3, "output: %s\n", output);

	/* Parse JSON output */
	printf("SC DEBUG: About to parse JSON output for %s\n", path);
	v = json_parse(output);
	if (!v) {
		printf("SC DEBUG: Failed to parse JSON output for %s\n", path);
		dprintf(2, "failed to parse JSON output\n");
		return -1;
	}
	printf("SC DEBUG: Successfully parsed JSON for %s\n", path);

	o = json_value_object(v);
	if (!o) {
		json_destroy_value(v);
		return -1;
	}

	/* Extract agent information */
	printf("SC DEBUG: Extracting agent information for %s\n", path);
	name = json_object_dotget_string(o, "agent_name");
	role = json_object_dotget_string(o, "agent_role");
	version = json_object_dotget_string(o, "agent_version");
	
	printf("SC DEBUG: name=%s, role=%s, version=%s\n", 
		name ? name : "NULL", 
		role ? role : "NULL", 
		version ? version : "NULL");

	if (!name || !role) {
		printf("SC DEBUG: Missing required fields for %s\n", path);
		dprintf(2, "missing required agent info fields\n");
		json_destroy_value(v);
		return -1;
	}
	printf("SC DEBUG: Successfully extracted info for %s\n", path);

	/* Fill in agent info structure */
	strncpy(info->name, name, sizeof(info->name)-1);
	strncpy(info->role, role, sizeof(info->role)-1);
	if (version) strncpy(info->version, version, sizeof(info->version)-1);
	strncpy(info->path, path, sizeof(info->path)-1);
	
	/* Store the info data as JSON string for later parsing */
	char *json_str = json_dumps(v, 0);
	if (json_str) {
		json_value_t *new_v = json_parse(json_str);
		if (new_v) {
			info->info_data = json_value_object(new_v);
		}
		free(json_str);
	}

	json_destroy_value(v);
	return 0;
}

int sc_process_mqtt_messages(sc_session_t *sc) {
	solard_message_t *msg;
	int count = 0;

#ifdef MQTT
	if (!sc->c->m) return 0;

	list_reset(sc->c->mq);
	while((msg = list_get_next(sc->c->mq)) != 0) {
		sc_update_agent_status(sc, msg);
		count++;
	}
	list_purge(sc->c->mq);
#endif

	return count;
}

void sc_update_agent_status(sc_session_t *sc, solard_message_t *msg) {
	sc_agent_info_t *info;
	time_t now;

	info = sc_find_agent(sc, msg->name);
	if (!info) {
		dprintf(2, "received message from unknown agent: %s\n", msg->name);
		return;
	}

	time(&now);
	info->last_seen = now;
	info->online = true;

	dprintf(3, "message from %s: func=%s\n", msg->name, msg->func);

	if (strcmp(msg->func, SOLARD_FUNC_STATUS) == 0) {
		info->last_status = now;
		info->have_status = true;
		if (info->status_data) json_destroy_object(info->status_data);
		info->status_data = 0;
		if (strlen(msg->data) > 0) {
			json_value_t *v = json_parse(msg->data);
			if (v) {
				json_object_t *o = json_value_object(v);
				if (o) {
					/* Create new object from parsed data */
					char *json_str = json_dumps(v, 0);
					if (json_str) {
						json_value_t *new_v = json_parse(json_str);
						if (new_v) {
							info->status_data = json_value_object(new_v);
						}
						free(json_str);
					}
				}
				json_destroy_value(v);
			}
		}
	} else if (strcmp(msg->func, SOLARD_FUNC_DATA) == 0) {
		info->last_data = now;
		info->have_data = true;
	} else if (strcmp(msg->func, SOLARD_FUNC_CONFIG) == 0) {
		if (info->config_data) json_destroy_object(info->config_data);
		info->config_data = 0;
		if (strlen(msg->data) > 0) {
			json_value_t *v = json_parse(msg->data);
			if (v) {
				json_object_t *o = json_value_object(v);
				if (o) {
					/* Create new object from parsed data */
					char *json_str = json_dumps(v, 0);
					if (json_str) {
						json_value_t *new_v = json_parse(json_str);
						if (new_v) {
							info->config_data = json_value_object(new_v);
						}
						free(json_str);
					}
				}
				json_destroy_value(v);
			}
		}
	}
}

int sc_list_agents(sc_session_t *sc) {
	sc_agent_info_t *info;
	char status_str[32];
	int count = 0;

	printf("Discovered Agents:\n");
	printf("%-16s %-12s %-8s %-8s %s\n", "Name", "Role", "Version", "Status", "Path");
	printf("%-16s %-12s %-8s %-8s %s\n", "----", "----", "-------", "------", "----");

	list_reset(sc->agents);
	while((info = list_get_next(sc->agents)) != 0) {
		strcpy(status_str, sc_get_agent_status_string(info));
		printf("%-16s %-12s %-8s %-8s %s\n", 
			info->name, info->role, info->version, status_str, info->path);
		count++;
	}

	printf("\nTotal agents: %d\n", count);
	return 0;
}

int sc_show_agent_status(sc_session_t *sc, char *agent_name) {
	sc_agent_info_t *info;
	time_t now;

	/* Process any pending MQTT messages */
	sc_process_mqtt_messages(sc);

	info = sc_find_agent(sc, agent_name);
	if (!info) {
		printf("Agent '%s' not found\n", agent_name);
		return 1;
	}

	time(&now);

	printf("Agent: %s\n", info->name);
	printf("  Role: %s\n", info->role);
	printf("  Version: %s\n", info->version);
	printf("  Path: %s\n", info->path);
	printf("  Status: %s\n", sc_get_agent_status_string(info));
	
	if (info->last_seen > 0) {
		printf("  Last seen: %s ago\n", sc_format_duration(info->last_seen, now));
	} else {
		printf("  Last seen: Never\n");
	}
	
	if (info->last_status > 0) {
		printf("  Last status: %s ago\n", sc_format_duration(info->last_status, now));
	} else {
		printf("  Last status: Never\n");
	}
	
	if (info->last_data > 0) {
		printf("  Last data: %s ago\n", sc_format_duration(info->last_data, now));
	} else {
		printf("  Last data: Never\n");
	}

	if (info->status_data) {
		json_value_t *val = json_object_value(info->status_data);
		if (val) {
			char *json_str = json_dumps(val, 0);
			if (json_str) {
				printf("  Status data: %s\n", json_str);
				free(json_str);
			}
		}
	}

	return 0;
}

int sc_monitor_agents(sc_session_t *sc, int refresh_rate) {
	time_t now, last_discovery_check = 0;

	printf("Service Controller Monitor Mode (Ctrl+C to exit)\n");
	printf("Refresh rate: %d seconds\n\n", refresh_rate);

	while(1) {
		time(&now);

		/* Check if we need to re-discover agents */
		if (now - last_discovery_check >= sc->discovery_interval) {
			sc_discover_agents(sc);
			last_discovery_check = now;
		}

		/* Process MQTT messages */
		sc_process_mqtt_messages(sc);

		/* Clear screen and display status */
		sc_clear_screen();
		
		printf("Service Controller Monitor - %s", ctime(&now));
		printf("=======================================================\n");
		
		sc_monitor_display(sc);
		
		sleep(refresh_rate);
	}

	return 0;
}

void sc_monitor_display(sc_session_t *sc) {
	sc_agent_info_t *info;
	time_t now;
	int online_count = 0, total_count = 0;
	char last_seen_str[32], status_str[16];

	time(&now);

	printf("%-16s %-12s %-8s %-12s %s\n", 
		"Agent", "Role", "Status", "Last Seen", "Health");
	printf("%-16s %-12s %-8s %-12s %s\n", 
		"-----", "----", "------", "---------", "------");

	list_reset(sc->agents);
	while((info = list_get_next(sc->agents)) != 0) {
		total_count++;
		
		/* Determine if agent is online (seen within 5 minutes) */
		if (info->last_seen > 0 && (now - info->last_seen) < 300) {
			info->online = true;
			online_count++;
			strcpy(status_str, "Online");
		} else {
			info->online = false;
			strcpy(status_str, "Offline");
		}

		/* Format last seen */
		if (info->last_seen > 0) {
			strcpy(last_seen_str, sc_format_duration(info->last_seen, now));
		} else {
			strcpy(last_seen_str, "Never");
		}

		/* Simple health indicator */
		char health_str[16];
		if (info->online && info->have_status && info->have_data) {
			strcpy(health_str, "Good");
		} else if (info->online && info->have_status) {
			strcpy(health_str, "Warning");
		} else if (info->online) {
			strcpy(health_str, "Poor");
		} else {
			strcpy(health_str, "Down");
		}

		printf("%-16s %-12s %-8s %-12s %s\n", 
			info->name, info->role, status_str, last_seen_str, health_str);
	}

	printf("\nSummary: %d/%d agents online\n", online_count, total_count);
}

/* Utility functions */
void sc_clear_screen(void) {
	printf("\033[2J\033[H");
	fflush(stdout);
}

char *sc_format_duration(time_t start, time_t end) {
	static char duration_str[32];
	int diff, mins, hours;

	diff = (int)difftime(end, start);
	if (diff < 0) diff = 0;

	hours = diff / 3600;
	mins = (diff % 3600) / 60;
	diff = diff % 60;

	if (hours > 0) {
		sprintf(duration_str, "%02d:%02d:%02d", hours, mins, diff);
	} else {
		sprintf(duration_str, "%02d:%02d", mins, diff);
	}

	return duration_str;
}

char *sc_get_agent_status_string(sc_agent_info_t *info) {
	static char status_str[16];
	time_t now;

	time(&now);

	if (info->last_seen == 0) {
		strcpy(status_str, "Unknown");
	} else if ((now - info->last_seen) < 300) {  /* 5 minutes */
		strcpy(status_str, "Online");
	} else {
		strcpy(status_str, "Offline");
	}

	return status_str;
}

/* Data export functions for Flask integration */
json_object_t *sc_create_agent_json(sc_agent_info_t *info) {
	json_object_t *agent_obj;
	time_t now;

	agent_obj = json_create_object();
	if (!agent_obj) return 0;

	time(&now);

	json_object_set_string(agent_obj, "name", info->name);
	json_object_set_string(agent_obj, "role", info->role);
	json_object_set_string(agent_obj, "version", info->version);
	json_object_set_string(agent_obj, "path", info->path);
	json_object_set_string(agent_obj, "status", sc_get_agent_status_string(info));
	json_object_set_boolean(agent_obj, "online", info->online);
	json_object_set_boolean(agent_obj, "have_status", info->have_status);
	json_object_set_boolean(agent_obj, "have_data", info->have_data);
	
	/* Timestamps */
	json_object_set_number(agent_obj, "last_seen", (double)info->last_seen);
	json_object_set_number(agent_obj, "last_status", (double)info->last_status);
	json_object_set_number(agent_obj, "last_data", (double)info->last_data);
	json_object_set_number(agent_obj, "current_time", (double)now);
	
	/* Human-readable timestamps */
	if (info->last_seen > 0) {
		json_object_set_string(agent_obj, "last_seen_ago", sc_format_duration(info->last_seen, now));
	} else {
		json_object_set_string(agent_obj, "last_seen_ago", "Never");
	}
	
	if (info->last_status > 0) {
		json_object_set_string(agent_obj, "last_status_ago", sc_format_duration(info->last_status, now));
	} else {
		json_object_set_string(agent_obj, "last_status_ago", "Never");
	}
	
	if (info->last_data > 0) {
		json_object_set_string(agent_obj, "last_data_ago", sc_format_duration(info->last_data, now));
	} else {
		json_object_set_string(agent_obj, "last_data_ago", "Never");
	}

	/* Health indicator */
	char health_str[16];
	if (info->online && info->have_status && info->have_data) {
		strcpy(health_str, "Good");
	} else if (info->online && info->have_status) {
		strcpy(health_str, "Warning");
	} else if (info->online) {
		strcpy(health_str, "Poor");
	} else {
		strcpy(health_str, "Down");
	}
	json_object_set_string(agent_obj, "health", health_str);

	/* Include status data if available */
	if (info->status_data) {
		json_value_t *status_val = json_object_value(info->status_data);
		if (status_val) {
			json_object_set_value(agent_obj, "status_data", status_val);
		}
	}

	/* Include info data if available */
	if (info->info_data) {
		json_value_t *info_val = json_object_value(info->info_data);
		if (info_val) {
			json_object_set_value(agent_obj, "info_data", info_val);
		}
	}

	/* Include config data if available */
	if (info->config_data) {
		json_value_t *config_val = json_object_value(info->config_data);
		if (config_val) {
			json_object_set_value(agent_obj, "config_data", config_val);
		}
	}

	return agent_obj;
}

json_object_t *sc_create_system_status_json(sc_session_t *sc) {
	json_object_t *system_obj;
	sc_agent_info_t *info;
	time_t now;
	int total_count = 0, online_count = 0;
	int role_counts[8] = {0}; /* Count by role */

	system_obj = json_create_object();
	if (!system_obj) return 0;

	time(&now);

	/* Count agents by status and role */
	list_reset(sc->agents);
	while((info = list_get_next(sc->agents)) != 0) {
		total_count++;
		
		if (info->last_seen > 0 && (now - info->last_seen) < 300) {
			online_count++;
		}
		
		/* Count by role - simplified mapping */
		if (strstr(info->role, "inverter")) role_counts[0]++;
		else if (strstr(info->role, "battery")) role_counts[1]++;
		else if (strstr(info->role, "controller")) role_counts[2]++;
		else if (strstr(info->role, "storage")) role_counts[3]++;
		else if (strstr(info->role, "sensor")) role_counts[4]++;
		else if (strstr(info->role, "utility")) role_counts[5]++;
		else if (strstr(info->role, "device")) role_counts[6]++;
		else role_counts[7]++; /* other */
	}

	json_object_set_number(system_obj, "timestamp", (double)now);
	json_object_set_number(system_obj, "total_agents", (double)total_count);
	json_object_set_number(system_obj, "online_agents", (double)online_count);
	json_object_set_number(system_obj, "offline_agents", (double)(total_count - online_count));
	json_object_set_number(system_obj, "last_discovery", (double)sc->last_discovery);
	json_object_set_number(system_obj, "discovery_interval", (double)sc->discovery_interval);
	json_object_set_boolean(system_obj, "discovering", sc->discovering);
	json_object_set_string(system_obj, "agent_dir", sc->agent_dir);

	/* Role counts */
	json_object_t *roles_obj = json_create_object();
	if (roles_obj) {
		json_object_set_number(roles_obj, "inverter", (double)role_counts[0]);
		json_object_set_number(roles_obj, "battery", (double)role_counts[1]);
		json_object_set_number(roles_obj, "controller", (double)role_counts[2]);
		json_object_set_number(roles_obj, "storage", (double)role_counts[3]);
		json_object_set_number(roles_obj, "sensor", (double)role_counts[4]);
		json_object_set_number(roles_obj, "utility", (double)role_counts[5]);
		json_object_set_number(roles_obj, "device", (double)role_counts[6]);
		json_object_set_number(roles_obj, "other", (double)role_counts[7]);
		json_object_set_object(system_obj, "role_counts", roles_obj);
	}

	return system_obj;
}

int sc_export_json_data(sc_session_t *sc, const char *output_file) {
	json_object_t *root_obj, *agents_array_obj, *system_obj;
	json_array_t *agents_array;
	sc_agent_info_t *info;
	FILE *fp;
	char *json_str;
	char temp_file[SOLARD_PATH_MAX];
	int ret = 0;

	dprintf(1, "exporting JSON data to: %s\n", output_file);

	/* Create root object */
	root_obj = json_create_object();
	if (!root_obj) {
		log_error("sc_export_json_data: failed to create root object\n");
		return -1;
	}

	/* Create agents array */
	agents_array = json_create_array();
	if (!agents_array) {
		log_error("sc_export_json_data: failed to create agents array\n");
		json_destroy_object(root_obj);
		return -1;
	}

	/* Add each agent to array */
	list_reset(sc->agents);
	while((info = list_get_next(sc->agents)) != 0) {
		json_object_t *agent_obj = sc_create_agent_json(info);
		if (agent_obj) {
			json_array_add_object(agents_array, agent_obj);
		}
	}

	/* Create system status */
	system_obj = sc_create_system_status_json(sc);

	/* Add to root object */
	agents_array_obj = json_create_object();
	json_object_set_array(agents_array_obj, "agents", agents_array);
	json_object_set_object(root_obj, "agents", agents_array_obj);
	if (system_obj) {
		json_object_set_object(root_obj, "system", system_obj);
	}

	/* Convert to JSON string */
	json_value_t *root_val = json_object_value(root_obj);
	json_str = json_dumps(root_val, 1); /* 1 = pretty print */
	if (!json_str) {
		log_error("sc_export_json_data: failed to serialize JSON\n");
		json_destroy_object(root_obj);
		return -1;
	}

	/* Write to temporary file first for atomic update */
	sprintf(temp_file, "%s.tmp", output_file);
	fp = fopen(temp_file, "w");
	if (!fp) {
		log_syserror("sc_export_json_data: fopen(%s)", temp_file);
		free(json_str);
		json_destroy_object(root_obj);
		return -1;
	}

	if (fprintf(fp, "%s\n", json_str) < 0) {
		log_syserror("sc_export_json_data: fprintf");
		ret = -1;
	}

	fclose(fp);

	if (ret == 0) {
		/* Atomic move */
		if (rename(temp_file, output_file) < 0) {
			log_syserror("sc_export_json_data: rename(%s, %s)", temp_file, output_file);
			ret = -1;
		} else {
			dprintf(2, "successfully exported data to %s\n", output_file);
		}
	}

	/* Cleanup */
	if (ret != 0) {
		unlink(temp_file);
	}
	free(json_str);
	json_destroy_object(root_obj);
	
	return ret;
}

int sc_start_web_export_mode(sc_session_t *sc, int refresh_rate) {
	time_t now, last_discovery_check = 0;
	const char *export_file = "/tmp/sc_data.json";

	printf("Service Controller Web Export Mode (Ctrl+C to exit)\n");
	printf("Exporting data to: %s\n", export_file);
	printf("Refresh rate: %d seconds\n\n", refresh_rate);

	while(1) {
		time(&now);

		/* Check if we need to re-discover agents */
		if (now - last_discovery_check >= sc->discovery_interval) {
			sc_discover_agents(sc);
			last_discovery_check = now;
		}

		/* Process MQTT messages */
		sc_process_mqtt_messages(sc);

		/* Export JSON data */
		if (sc_export_json_data(sc, export_file) < 0) {
			log_error("failed to export JSON data\n");
		}

		sleep(refresh_rate);
	}

	return 0;
}

/* Agent management functions */

int sc_load_managed_agents(sc_session_t *sc) {
	FILE *fp;
	char buffer[8192];
	size_t len;
	json_value_t *root;
	json_object_t *root_obj;
	json_array_t *agents_array;
	int i, count;
	sc_managed_agent_t managed_agent;

	dprintf(1, "loading managed agents from: %s\n", sc->managed_agents_file);

	fp = fopen(sc->managed_agents_file, "r");
	if (!fp) {
		dprintf(1, "no managed agents file found, starting with empty list\n");
		return 0;
	}

	/* Read file contents */
	len = fread(buffer, 1, sizeof(buffer)-1, fp);
	buffer[len] = 0;
	fclose(fp);
	
	printf("SC DEBUG: Read %zu bytes from managed agents file\n", len);
	printf("SC DEBUG: File contents: %.200s...\n", buffer);

	/* Parse JSON */
	root = json_parse(buffer);
	if (!root) {
		log_error("sc_load_managed_agents: failed to parse JSON\n");
		return -1;
	}

	root_obj = json_value_object(root);
	if (!root_obj) {
		json_destroy_value(root);
		return -1;
	}

	agents_array = json_object_get_array(root_obj, "agents");
	if (!agents_array) {
		json_destroy_value(root);
		return 0;
	}

	count = agents_array->count;
	dprintf(1, "loading %d managed agents\n", count);
	printf("SC DEBUG: Found %d agents in JSON array\n", count);

	for (i = 0; i < count; i++) {
		json_value_t *agent_val = agents_array->items[i];
		if (!agent_val) continue;
		
		json_object_t *agent_obj = json_value_object(agent_val);
		if (!agent_obj) continue;

		memset(&managed_agent, 0, sizeof(managed_agent));
		
		char *name = json_object_get_string(agent_obj, "name");
		char *path = json_object_get_string(agent_obj, "path");
		
		/* Get enabled value manually */
		bool enabled = true; /* default to enabled */
		json_value_t *enabled_val = json_object_dotget_value(agent_obj, "enabled");
		if (enabled_val && json_value_get_type(enabled_val) == JSON_TYPE_BOOLEAN) {
			enabled = json_value_get_boolean(enabled_val) != 0;
		}
		
		double added_time = json_object_get_number(agent_obj, "added");

		if (name && path) {
			strncpy(managed_agent.name, name, sizeof(managed_agent.name)-1);
			strncpy(managed_agent.path, path, sizeof(managed_agent.path)-1);
			managed_agent.enabled = enabled;
			managed_agent.added = (time_t)added_time;

			list_add(sc->managed_agents, &managed_agent, sizeof(managed_agent));
			dprintf(2, "loaded managed agent: %s -> %s (enabled: %s)\n", 
				managed_agent.name, managed_agent.path, enabled ? "yes" : "no");
		}
	}

	json_destroy_value(root);
	return 0;
}

int sc_save_managed_agents(sc_session_t *sc) {
	FILE *fp;
	json_object_t *root_obj;
	json_array_t *agents_array;
	sc_managed_agent_t *managed_agent;
	char *json_str;

	dprintf(1, "saving managed agents to: %s\n", sc->managed_agents_file);

	/* Create JSON structure */
	root_obj = json_create_object();
	if (!root_obj) return -1;

	agents_array = json_create_array();
	if (!agents_array) {
		json_destroy_object(root_obj);
		return -1;
	}

	/* Add each managed agent */
	list_reset(sc->managed_agents);
	while((managed_agent = list_get_next(sc->managed_agents)) != 0) {
		json_object_t *agent_obj = json_create_object();
		if (!agent_obj) continue;

		json_object_set_string(agent_obj, "name", managed_agent->name);
		json_object_set_string(agent_obj, "path", managed_agent->path);
		json_object_set_boolean(agent_obj, "enabled", managed_agent->enabled);
		json_object_set_number(agent_obj, "added", (double)managed_agent->added);

		json_array_add_object(agents_array, agent_obj);
	}

	json_object_set_array(root_obj, "agents", agents_array);

	/* Convert to JSON string */
	json_value_t *root_val = json_object_value(root_obj);
	json_str = json_dumps(root_val, 1); /* Pretty print */
	if (!json_str) {
		json_destroy_object(root_obj);
		return -1;
	}

	/* Write to file */
	fp = fopen(sc->managed_agents_file, "w");
	if (!fp) {
		log_syserror("sc_save_managed_agents: fopen(%s)", sc->managed_agents_file);
		free(json_str);
		json_destroy_object(root_obj);
		return -1;
	}

	fprintf(fp, "%s\n", json_str);
	fclose(fp);

	free(json_str);
	json_destroy_object(root_obj);

	dprintf(1, "saved managed agents successfully\n");
	return 0;
}

int sc_add_managed_agent(sc_session_t *sc, const char *name, const char *path) {
	sc_managed_agent_t managed_agent;
	sc_managed_agent_t *existing;

	dprintf(1, "adding managed agent: %s -> %s\n", name, path);

	/* Check if agent already managed */
	existing = sc_find_managed_agent(sc, name);
	if (existing) {
		log_error("agent %s is already managed\n", name);
		return -1;
	}

	/* Verify the agent path is valid */
	sc_agent_info_t test_info;
	memset(&test_info, 0, sizeof(test_info));
	if (sc_get_agent_info(sc, (char *)path, &test_info) != 0) {
		log_error("invalid agent path: %s\n", path);
		return -1;
	}

	/* Create managed agent entry */
	memset(&managed_agent, 0, sizeof(managed_agent));
	strncpy(managed_agent.name, name, sizeof(managed_agent.name)-1);
	strncpy(managed_agent.path, path, sizeof(managed_agent.path)-1);
	managed_agent.enabled = true;
	time(&managed_agent.added);

	/* Add to list */
	list_add(sc->managed_agents, &managed_agent, sizeof(managed_agent));

	/* Save to file */
	if (sc_save_managed_agents(sc) != 0) {
		log_error("failed to save managed agents\n");
		return -1;
	}

	dprintf(1, "successfully added managed agent: %s\n", name);
	return 0;
}

int sc_remove_managed_agent(sc_session_t *sc, const char *name) {
	sc_managed_agent_t *managed_agent;
	sc_agent_info_t *agent_info;

	dprintf(1, "removing managed agent: %s\n", name);

	/* Find and remove from managed agents list */
	managed_agent = NULL;
	list_reset(sc->managed_agents);
	while((managed_agent = list_get_next(sc->managed_agents)) != 0) {
		if (strcmp(managed_agent->name, name) == 0) {
			list_delete(sc->managed_agents, managed_agent);
			break;
		}
	}

	if (!managed_agent) {
		log_error("managed agent %s not found\n", name);
		return -1;
	}

	/* Remove from discovered agents list */
	list_reset(sc->agents);
	while((agent_info = list_get_next(sc->agents)) != 0) {
		if (strcmp(agent_info->name, name) == 0) {
			if (agent_info->status_data) json_destroy_object(agent_info->status_data);
			if (agent_info->info_data) json_destroy_object(agent_info->info_data);
			if (agent_info->config_data) json_destroy_object(agent_info->config_data);
			list_delete(sc->agents, agent_info);
			break;
		}
	}

	/* Save to file */
	if (sc_save_managed_agents(sc) != 0) {
		log_error("failed to save managed agents\n");
		return -1;
	}

	dprintf(1, "successfully removed managed agent: %s\n", name);
	return 0;
}

sc_managed_agent_t *sc_find_managed_agent(sc_session_t *sc, const char *name) {
	sc_managed_agent_t *managed_agent;

	list_reset(sc->managed_agents);
	while((managed_agent = list_get_next(sc->managed_agents)) != 0) {
		if (strcmp(managed_agent->name, name) == 0) {
			return managed_agent;
		}
	}
	return NULL;
}

int sc_list_available_agents(sc_session_t *sc, char ***agents, int *count) {
	DIR *dp;
	struct dirent *entry;
	struct stat st;
	char path[SOLARD_PATH_MAX];
	sc_agent_info_t test_info;
	char **agent_list = NULL;
	int agent_count = 0;
	int max_agents = 100;

	dprintf(1, "listing available agents in: %s\n", sc->agent_dir);

	dp = opendir(sc->agent_dir);
	if (!dp) {
		log_syserror("sc_list_available_agents: opendir(%s)", sc->agent_dir);
		return -1;
	}

	/* Allocate initial agent list */
	agent_list = malloc(max_agents * sizeof(char *));
	if (!agent_list) {
		closedir(dp);
		return -1;
	}

	while ((entry = readdir(dp)) != NULL) {
		/* Skip hidden files and directories */
		if (entry->d_name[0] == '.') continue;

		sprintf(path, "%s/%s", sc->agent_dir, entry->d_name);
		if (stat(path, &st) < 0) continue;

		/* Only process executable files */
		if (!S_ISREG(st.st_mode) || !(st.st_mode & S_IXUSR)) continue;

		/* Try to get agent info */
		memset(&test_info, 0, sizeof(test_info));
		if (sc_get_agent_info(sc, path, &test_info) == 0) {
			/* Check if already managed */
			if (!sc_find_managed_agent(sc, test_info.name)) {
				/* Add to available list */
				if (agent_count >= max_agents) {
					max_agents *= 2;
					agent_list = realloc(agent_list, max_agents * sizeof(char *));
					if (!agent_list) {
						closedir(dp);
						return -1;
					}
				}
				
				agent_list[agent_count] = malloc(strlen(test_info.name) + 1);
				if (agent_list[agent_count]) {
					strcpy(agent_list[agent_count], test_info.name);
					agent_count++;
				}
			}
		}
	}

	closedir(dp);

	*agents = agent_list;
	*count = agent_count;

	dprintf(1, "found %d available agents\n", agent_count);
	return 0;
}

json_object_t *sc_create_available_agents_json(sc_session_t *sc) {
	json_object_t *root_obj;
	json_array_t *agents_array;
	char **available_agents;
	int count, i;

	root_obj = json_create_object();
	if (!root_obj) return NULL;

	agents_array = json_create_array();
	if (!agents_array) {
		json_destroy_object(root_obj);
		return NULL;
	}

	/* Get available agents */
	if (sc_list_available_agents(sc, &available_agents, &count) == 0) {
		for (i = 0; i < count; i++) {
			json_array_add_string(agents_array, available_agents[i]);
			free(available_agents[i]);
		}
		free(available_agents);
	}

	json_object_set_array(root_obj, "available_agents", agents_array);
	json_object_set_number(root_obj, "count", (double)count);
	json_object_set_string(root_obj, "agent_dir", sc->agent_dir);

	return root_obj;
}