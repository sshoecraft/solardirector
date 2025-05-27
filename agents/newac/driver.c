
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "ac.h"

extern bool ignore_wpi_flag;

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

        /* must be last */
        dprintf(dlevel,"destroying agent...\n");
        if (s->ap) agent_destroy_agent(s->ap);

	free(s);
	return 0;
}

int ac_read(void *handle, uint32_t *what, void *buf, int buflen) {
//	ac_session_t *s = handle;
	return 0;
}

static int ac_config(void *h, int req, ...) {
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
			dprintf(dlevel,"r: %d, ignore_wpi: %d\n", r, ignore_wpi_flag);
			if (r && ignore_wpi_flag) r = 0;
			if (r) log_error("wpi_init failed!\n");
		}
		break;
	case SOLARD_CONFIG_GET_INFO:
		{
			json_value_t **vp = va_arg(va,json_value_t **);
			dprintf(dlevel,"vp: %p\n", vp);
			if (vp) {
				json_object_t *o;

				dprintf(dlevel,"creating info...\n");

				o = json_create_object();
				if (o) {
					json_object_set_string(o,"agent_name",ac_driver.name);
					json_object_set_string(o,"agent_role",SOLARD_ROLE_CONTROLLER);
					json_object_set_string(o,"agent_description","HVAC Controller");
					json_object_set_string(o,"agent_version",s->version);
					json_object_set_string(o,"agent_author","Stephen P. Shoecraft");

					config_add_info(s->ap->cp, o);

					dprintf(dlevel,"returning: %p\n", json_object_value(o));
					*vp = json_object_value(o);
					r = 0;
				} else {
					r = 1;
				}
			}
		}
		break;
	}
	return r;
}

solard_driver_t ac_driver = {
	"ac",
	ac_new,			/* New */
	ac_free,			/* Free */
	0,			/* Open */
	0,			/* Close */
	ac_read,			/* Read */
	0,			/* Write */
	ac_config			/* Config */
};
