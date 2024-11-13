
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "sc.h"

extern char *sd_version_string;

static void *sc_new(void *driver, void *driver_handle) {
	sc_session_t *sc;

	sc = calloc(sizeof(*sc),1);
	if (!sc) {
		log_syserr("sc_new: calloc");
		return 0;
	}

	dprintf(dlevel,"returning: %p\n", sc);
	return sc;
}

static int sc_destroy(void *h) {
	sc_session_t *s = h;

	if (!s) return 1;

#if 0
#ifdef JS
	if (s->props) free(s->props);
	if (s->funcs) free(s->funcs);
	if (s->data_props) free(s->data_props);
	if (s->ap && s->ap->js.e) {
		JSContext *cx = JS_EngineGetCX(s->ap->js.e);
		if (cx) {
			void *p = JS_GetPrivate(cx,s->can_obj);
			if (p) JS_free(cx,p);
		}
	}
#endif
#endif

	if (s->ap) agent_destroy_agent(s->ap);

	dprintf(dlevel,"freeing session!\n");
        free(s);

	return 0;
}

static int sc_open(void *h) {
	sc_session_t *s = h;
	int r;

	dprintf(dlevel,"s: %p\n", s);

	r = 0;
	dprintf(dlevel,"returning: %d\n", r);
	return r;
}

static int sc_close(void *handle) {
	sc_session_t *s = handle;

	dprintf(dlevel,"s: %p\n", s);
	return 0;
}

static int sc_read(void *handle, uint32_t *control, void *buf, int buflen) {
//	sc_session_t *s = handle;

//	dprintf(dlevel,"disable_driver_read: %d\n", s->disable_driver_read);
//	if (s->disable_driver_read) return 0;

	return 0;
}

static int sc_write(void *handle, uint32_t *control, void *buffer, int len) {
//	sc_session_t *s = handle;

//	dprintf(dlevel,"disable_driver_write: %d\n", s->disable_driver_write);
//	if (s->disable_driver_write) return 0;

	return 0;
}

solard_driver_t sc_driver = {
	"sc",
	sc_new,	/* New */
	sc_destroy,	/* Free */
	sc_open,	/* Open */
	sc_close,	/* Close */
	sc_read,	/* Read */
	sc_write,	/* Write */
	sc_config,	/* Config */
};
