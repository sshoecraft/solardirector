
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "sc.h"

extern char *sc_version_string;

int sc_agent_init(int argc, char **argv, opt_proctab_t *sd_opts, sc_session_t *sc) {
	config_property_t sc_props[] = {
#if 0
		{ "agent_notify_time", DATA_TYPE_INT, &sc->agent_notify, 0, "600", 0, },
//			"range", 3, (int []){ 0, 1440, 1 }, 1, (char *[]) { "Dead agent notification timer" }, "S", 1, 0 },
		{ "agent_error_time", DATA_TYPE_INT, &sc->agent_error, 0, "300", 0, },
//                        "range", 3, (int []){ 0, 1440, 1 }, 1, (char *[]) { "Agent error timer" }, "S", 1, 0 },
		{ "agent_warning_time", DATA_TYPE_INT, &sc->agent_warning, 0, "300", 0, },
//                        "range", 3, (int []){ 0, 1440, 1 }, 1, (char *[]) { "Agent warning timer" }, "S", 1, 0 },
		{ "notify", DATA_TYPE_BOOL, &sc->notify, 0, "true", 0, },
//			0, 0, 0, 0, 0, 0, 1, 0 },
		{ "notify_path", DATA_TYPE_STRING, &sc->notify_path, sizeof(sc->notify_path)-1, "", 0, },
//			0, 0, 0, 0, 0, 0, 1, 0 },
		{ "battery_temp_min", DATA_TYPE_DOUBLE, &sc->battery_temp_min, 0, "0", 0, },
		{ "battery_temp_max", DATA_TYPE_DOUBLE, &sc->battery_temp_max, 0, "45", 0, },
#endif
		{0}
	};

	return (agent_init(argc,argv,sc_version_string,sd_opts,&sc_driver,sc,0,sc_props,0) == 0);
}

static json_value_t *sc_get_info(void *handle) {
	sc_session_t *s = handle;
	json_object_t *j;

	j = json_create_object();
	if (!j) return 0;
	json_object_set_string(j,"agent_name","sc");
	json_object_set_string(j,"agent_class","Management");
	json_object_set_string(j,"agent_role","Controller");
	json_object_set_string(j,"agent_description","Site Controller");
	json_object_set_string(j,"agent_version",sc_version_string);
	json_object_set_string(j,"agent_author","Stephen P. Shoecraft");

	config_add_info(s->ap->cp, j);

	return json_object_value(j);
}

int sc_config(void *h, int req, ...) {
	sc_session_t *s = h;
	int r;
	va_list va;

	r = 1;
	dprintf(1,"req: %d\n", req);
	va_start(va,req);
	switch(req) {
	case SOLARD_CONFIG_INIT:
		dprintf(1,"**** INIT *****\n");
		s->ap = va_arg(va,solard_agent_t *);
		dprintf(dlevel,"s->ap: %p\n", s->ap);
#ifdef JS
		r = sc_jsinit(s);
#endif
		break;
	case SOLARD_CONFIG_GET_INFO:
		{
			json_value_t **vp = va_arg(va,json_value_t **);
			dprintf(dlevel,"vp: %p\n", vp);
			if (vp) {
				*vp = sc_get_info(s);
				r = 0;
			}
		}
		break;
	}
	dprintf(1,"r: %d\n", r);
	return r;
}
