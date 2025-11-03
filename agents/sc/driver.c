
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "sc.h"

static void *sc_new(void *transport, void *transport_handle) {
	sc_session_t *s;

	s = calloc(1,sizeof(*s));
	if (!s) {
		log_syserr("sc_new: calloc");
		return 0;
	}

	return s;
}

static int sc_free(void *handle) {
	sc_session_t *s = handle;

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

int sc_config(void *h, int req, ...) {
	sc_session_t *s = h;
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
				*vp = sc_get_info(s);
				r = 0;
			}
		}
		break;
	}
	return r;
}

solard_driver_t sc_driver = {
	"sc",
	sc_new,			/* New */
	sc_free,			/* Free */
	0,			/* Open */
	0,			/* Close */
	0,			/* Read */
	0,			/* Write */
	sc_config			/* Config */
};
