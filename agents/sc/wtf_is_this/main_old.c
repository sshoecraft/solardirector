/*
 * Service Controller (SC) - Main Entry Point
 * Minimal C framework with JavaScript business logic
 */

#include "client.h"

#ifdef JS
extern int sc_jsfunc_init(JSContext *cx, JSObject *obj);
#endif

char *sc_version_string = "1.0";
char *sc_name = "sc";

int main(int argc, char **argv) {
    solard_client_t *c;
    int r;

    // Initialize SD client framework
    c = client_init(argc, argv, sc_version_string, 0, &r);
    if (!c) return r;

#ifdef JS
    // Initialize JavaScript functions
    if (sc_jsfunc_init(c->js.cx, c->js.global_object) < 0) {
        printf("Error initializing JavaScript functions\n");
        return 1;
    }
    
    // Load and run the main initialization script
    if (JS_LoadScript(c->js.cx, c->js.global_object, "init.js") != 0) {
        printf("Error loading init.js\n");
        return 1;
    }
#endif

    // Start the main client loop (handles MQTT, periodic tasks, etc.)
    return client_main(c);
		{ "agent_dir", DATA_TYPE_STRING, agent_dir, sizeof(agent_dir), agent_dir, 0 },
		{ "refresh_rate", DATA_TYPE_INT, &refresh_rate, 0, "2", 0 },
		{ 0 }
	};
	sc_session_t *sc;
	int r;

	find_config_file("sc.conf", configfile, sizeof(configfile)-1);
	dprintf(1, "configfile: %s\n", configfile);

	/* Initialize client - this will process config and potentially override agent_dir */
	c = client_init(argc, argv, "sc", SC_VERSION, opts, 
		CLIENT_FLAG_NOINFLUX | CLIENT_FLAG_NOEVENT, props);
	if (!c) return 1;

	/* Create SC session */
	sc = sc_init(c, agent_dir);
	if (!sc) {
		log_error("unable to initialize SC session\n");
		return 1;
	}

	r = 0;
	if (list_agents) {
		r = sc_list_agents(sc);
	} else if (strlen(show_status) > 0) {
		r = sc_show_agent_status(sc, show_status);
	} else if (monitor_mode) {
		r = sc_monitor_agents(sc, refresh_rate);
	} else if (web_export_mode) {
		r = sc_start_web_export_mode(sc, refresh_rate);
	} else if (strlen(export_file) > 0) {
		/* One-time export */
		sc_process_mqtt_messages(sc);
		r = sc_export_json_data(sc, export_file);
	} else {
		/* Default: show help */
		printf("Service Controller (SC) v%s\n", SC_VERSION);
		printf("Usage: %s [options]\n", argv[0]);
		printf("Options:\n");
		printf("  -l              List discovered agents\n");
		printf("  -s <agent>      Show status for specific agent\n");
		printf("  -m              Monitor agents in real-time\n");
		printf("  -w              Web export mode (continuous JSON export)\n");
		printf("  -e <file>       Export data to JSON file (one-time)\n");
		printf("  -r <rate>       Set refresh rate for monitor/web mode (default: 2)\n");
		printf("  -d              Enable debug output\n");
		printf("  -c <config>     Use specified config file\n");
		printf("  -h              Show this help\n");
		r = 0;
	}

	sc_shutdown(sc);
	return r;
}