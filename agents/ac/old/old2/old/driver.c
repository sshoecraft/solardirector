
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "ac.h"

extern char *ac_version_string;
extern bool ignore_wpi;

static void *ac_new(void *transport, void *transport_handle) {
	ac_session_t *s;

	s = calloc(1,sizeof(*s));
	if (!s) {
		log_syserr("ac_new: calloc");
		return 0;
	}

	return s;
}

static int ac_free(void *handle) {
	ac_session_t *s = handle;

        if (!s) return 1;

#ifdef JS
        if (s->props) free(s->props);
        if (s->funcs) free(s->funcs);
#endif

        /* must be last */
        dprintf(dlevel,"destroying agent...\n");
        if (s->ap) agent_destroy_agent(s->ap);

	free(s);
	return 0;
}

json_value_t *ac_get_info(ac_session_t *s) {
	json_object_t *o;

	dprintf(dlevel,"creating info...\n");

	o = json_create_object();
	if (!o) return 0;
	json_object_set_string(o,"agent_name",ac_driver.name);
	json_object_set_string(o,"agent_role",SOLARD_ROLE_UTILITY);
        json_object_set_string(o,"agent_description","HVAC Controller");
	json_object_set_string(o,"agent_version",ac_version_string);
	json_object_set_string(o,"agent_author","Stephen P. Shoecraft");

	config_add_info(s->ap->cp, o);

	dprintf(dlevel,"returning: %p\n", json_object_value(o));
	return json_object_value(o);
}

int ac_config(void *h, int req, ...) {
	ac_session_t *s = h;
	va_list va;
	int r;

	va_start(va,req);
	dprintf(dlevel,"req: %d\n", req);
	r = 1;
	switch(req) {
	case SOLARD_CONFIG_INIT:
		s->ap = va_arg(va,solard_agent_t *);
		if (!s->ap->e) {
			log_error("error: ac_config CONFIG_INIT: JSEngine not init!\n");
			return 1;
		}
		r = ac_can_init(s);
		dprintf(dlevel,"r: %d\n", r);
		if (r) log_error("ac_can_init failed!\n");
#ifdef JS
		else r = jsinit(s);
		dprintf(dlevel,"r: %d\n", r);
		if (r) log_error("jsinit failed!\n");
		else {
			r = wpi_init(s->ap->js.e);
#else
		if (!r) {
			r = wpi_init();
#endif
			dprintf(dlevel,"r: %d, ignore_wpi: %d\n", r, ignore_wpi);
			if (r && ignore_wpi) r = 0;
			if (r) log_error("wpi_init failed!\n");
		}
		break;
	case SOLARD_CONFIG_GET_INFO:
		{
			json_value_t **vp = va_arg(va,json_value_t **);
			dprintf(dlevel,"vp: %p\n", vp);
			if (vp) {
				*vp = ac_get_info(s);
				r = 0;
			}
		}
		break;
	}
	dprintf(dlevel,"r: %d\n", r);
	return r;
}

solard_driver_t ac_driver = {
	AGENT_NAME,
	ac_new,				/* New */
	ac_free,			/* Free */
	0,				/* Open */
	0,				/* Close */
	0,				/* Read */
	0,				/* Write */
	ac_config			/* Config */
};
