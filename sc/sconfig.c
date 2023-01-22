
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "sc.h"

#define dlevel 0

extern char *sc_version_string;

static int sc_config_add_function(void *ctx, list args, char *errmsg, json_object_t *results) {
//	sc_session_t *s = ctx;
	char **argv, *name, *value;
//	config_property_t *p;

	dprintf(dlevel,"args count: %d\n", list_count(args));
	list_reset(args);
	while((argv = list_get_next(args)) != 0) {
		name = argv[0];
		value = argv[1];
		dprintf(dlevel,"name: %s, value: %s\n", name, value);
	}

	return 0;
}

int sc_agent_init(int argc, char **argv, opt_proctab_t *sd_opts, sc_session_t *sd) {
	config_property_t sc_props[] = {
		{ "agents", DATA_TYPE_STRING_LIST, sd->names, 0, 0, 0 },
		{ "agent_notify_time", DATA_TYPE_INT, &sd->agent_notify, 0, "600", 0, },
//                        "range", 3, (int []){ 0, 1440, 1 }, 1, (char *[]) { "Dead agent notification timer" }, "S", 1, 0 },
		{ "agent_error_time", DATA_TYPE_INT, &sd->agent_error, 0, "300", 0, },
//                        "range", 3, (int []){ 0, 1440, 1 }, 1, (char *[]) { "Agent error timer" }, "S", 1, 0 },
		{ "agent_warning_time", DATA_TYPE_INT, &sd->agent_warning, 0, "300", 0, },
//                        "range", 3, (int []){ 0, 1440, 1 }, 1, (char *[]) { "Agent warning timer" }, "S", 1, 0 },
		{ "notify", DATA_TYPE_STRING, &sd->notify_path, sizeof(sd->notify_path)-1, "", 0, },
//			0, 0, 0, 0, 0, 0, 1, 0 },
		{0}
	};
	config_function_t sc_funcs[] = {
		{ "add", (config_funccall_t *)sc_config_add_function, sd, 2 },
		{0}
	};

	return (agent_init(argc,argv,sc_version_string,sd_opts,&sc_driver,sd,0,sc_props,sc_funcs) == 0);
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
		{
		char mqtt_info[256];

		s->ap = va_arg(va,solard_agent_t *);
		s->c = client_init(0,0,sc_version_string,0,s->ap->instance_name,CLIENT_FLAG_NOJS,0,0,0,0);
		dprintf(dlevel,"s->c: %p\n", s->c);
		if (!s->c) return 1;
		s->c->addmq = true;
		mqtt_disconnect(s->c->m,1);
		mqtt_get_config(mqtt_info,sizeof(mqtt_info),s->ap->m,0);
		mqtt_parse_config(s->c->m,mqtt_info);
		mqtt_newclient(s->c->m);
		mqtt_connect(s->c->m,10);
		mqtt_resub(s->c->m);
		dprintf(1,"name: %s\n", s->ap->instance_name);
		dprintf(1,"s->ap: %p\n", s->ap);
		r = 0;
//		r = sc_read_config(s);

#ifdef JS
		if (!r) r = sc_jsinit(s);
#endif
		}
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
