
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "pa.h"

extern char *pa_version_string;

static void *pa_new(void *transport, void *transport_handle) {
	pa_session_t *s;

	s = calloc(1,sizeof(*s));
	if (!s) {
		log_syserr("pa_new: calloc");
		return 0;
	}
	return s;
}

static int pa_free(void *handle) {
	pa_session_t *s = handle;

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

static json_value_t *pa_get_info(pa_session_t *s) {
	json_object_t *o;

	dprintf(dlevel,"creating info...\n");

	o = json_create_object();
	if (!o) return 0;
	json_object_set_string(o,"agent_name",pa_driver.name);
	json_object_set_string(o,"agent_role",SOLARD_ROLE_UTILITY);
	json_object_set_string(o,"agent_version",pa_version_string);
	json_object_set_string(o,"agent_author","Stephen P. Shoecraft");

	config_add_info(s->ap->cp, o);

	dprintf(dlevel,"returning: %p\n", json_object_value(o));
	return json_object_value(o);
}

static int pa_config(void *h, int req, ...) {
	pa_session_t *s = h;
	va_list va;
	int r;

	va_start(va,req);
	dprintf(dlevel,"req: %d\n", req);
	r = 1;
	switch(req) {
	case SOLARD_CONFIG_INIT:
		s->ap = va_arg(va,solard_agent_t *);
		r = jsinit(s);
		break;
	case SOLARD_CONFIG_GET_INFO:
		{
			json_value_t **vp = va_arg(va,json_value_t **);
			dprintf(dlevel,"vp: %p\n", vp);
			if (vp) {
				*vp = pa_get_info(s);
				r = 0;
			}
		}
		break;
	}
	return r;
}

solard_driver_t pa_driver = {
	AGENT_NAME,
	pa_new,			/* New */
	pa_free,			/* Free */
	0,			/* Open */
	0,			/* Close */
	0,			/* Read */
	0,			/* Write */
	pa_config			/* Config */
};
