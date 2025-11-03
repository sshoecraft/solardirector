/*
 * Service Controller (SC) - Main Entry Point
 * Minimal C framework with JavaScript business logic
 */

#include "sc.h"
#include "__sd_build.h"

char *sc_version_string = STRINGIFY(__SD_BUILD);

extern solard_driver_t sc_driver;

json_value_t *sc_info(void *ctx) {
    json_object_t *o = json_create_object();
    if (!o) return 0;

    json_object_set_string(o, "agent_name", "sc");
    json_object_set_string(o, "agent_role", "Controller");
    json_object_set_string(o, "agent_description", "Service Controller");
    json_object_set_string(o, "agent_version", sc_version_string);
    json_object_set_string(o, "agent_author", "Stephen P. Shoecraft");

    return json_object_value(o);
}

int sc_agent_init(int argc, char **argv, opt_proctab_t *opts, sc_session_t *s) {
    config_property_t sc_props[] = {
        { 0 }
    };

    s->ap = agent_init(argc, argv, sc_version_string, opts, &sc_driver, s, 0, sc_props, 0);
    if (!s->ap) return 1;

#ifdef JS
    // Initialize JavaScript functions  
    if (sc_jsfunc_init(s->ap->js.e) < 0) {
        log_error("Error initializing JavaScript functions\n");
        return 1;
    }
#endif

    return 0;
}

int main(int argc, char **argv) {
    sc_session_t *s;
    bool norun;
    opt_proctab_t opts[] = {
        { "-1|dont enter run loop", &norun, DATA_TYPE_BOOL, 0, 0, "no" },
        OPTS_END
    };

    s = sc_driver.new(0, 0);
    if (!s) {
        perror("driver.new");
        return 1;
    }
    
    if (sc_agent_init(argc, argv, opts, s)) return 1;

    if (!norun) {
        agent_run(s->ap);
    }

    sc_driver.destroy(s);
    agent_shutdown();
    
    return 0;
}