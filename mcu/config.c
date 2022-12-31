
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "mcu.h"

#define dlevel 1

extern char *sd_version_string;

static json_value_t *mcu_get_info(void *handle) {
	mcu_session_t *s = handle;
	json_object_t *j;

	j = json_create_object();
	if (!j) return 0;
	json_object_set_string(j,"agent_name","mcu");
	json_object_set_string(j,"agent_class","Management");
	json_object_set_string(j,"agent_role","Controller");
	json_object_set_string(j,"agent_description","Solar Director");
	json_object_set_string(j,"agent_version",sd_version_string);
	json_object_set_string(j,"agent_author","Stephen P. Shoecraft");

	config_add_info(s->ap->cp, j);

	return json_object_value(j);
}

int mcu_config(void *h, int req, ...) {
	mcu_session_t *s = h;
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
		s->c = client_init(0,0,sd_version_string,0,s->ap->instance_name,CLIENT_FLAG_NOJS,0,0);
		dprintf(0,"s->c: %p\n", s->c);
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
//		r = mcu_read_config(s);

#ifdef JS
		if (!r) r = mcu_jsinit(s);
#endif
		}
		break;
	case SOLARD_CONFIG_GET_INFO:
		{
			json_value_t **vp = va_arg(va,json_value_t **);
			dprintf(dlevel,"vp: %p\n", vp);
			if (vp) {
				*vp = mcu_get_info(s);
				r = 0;
			}
		}
		break;
	}
	dprintf(1,"r: %d\n", r);
	return r;
}
