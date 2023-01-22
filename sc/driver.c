
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "sc.h"

#define dlevel 0

extern char *sd_version_string;

static void *sc_new(void *driver, void *driver_handle) {
	sc_session_t *s;

	s = malloc(sizeof(*s));
	if (!s) {
		perror("sc_new: malloc");
		return 0;
	}
	memset(s,0,sizeof(*s));
//	s->running = -1;

	dprintf(dlevel,"returning: %p\n", s);
	return s;
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
	if (s->c) client_destroy_client(s->c);

	dprintf(dlevel,"freeing session!\n");
        free(s);

	return 0;
}

static int sc_open(void *h) {
	sc_session_t *s = h;
	int r;

	dprintf(dlevel,"s: %p\n", s);

	r = 0;
#if 0
	if (!check_state(s,SI_STATE_OPEN)) {
		if (s->can->open(s->can_handle) == 0)
			set_state(s,SI_STATE_OPEN);
		else {
			log_error("error opening %s device %s:%s\n", s->can_transport, s->can_target, s->can_topts);
			r = 1;
		}
	}
#endif
	dprintf(dlevel,"returning: %d\n", r);
	return r;
}

static int sc_close(void *handle) {
	sc_session_t *s = handle;

	dprintf(dlevel,"s: %p\n", s);
//	if (check_state(s,SI_STATE_OPEN) && s->can->close(s->can_handle) == 0) clear_state(s,SI_STATE_OPEN);
	return 0;
}

#if 0
#ifdef INFLUX
static double _get_influx_value(influx_session_t *s, char *query) {
	influx_series_t *sp;
	influx_value_t *vp;
	double value;
	list results;

	if (influx_query(s, query)) return 0;
	results = influx_get_results(s);
	dprintf(dlevel,"results: %p\n", results);
	if (!results) return 0;
	dprintf(dlevel,"results count: %d\n", list_count(results));
	list_reset(results);
	sp = list_get_next(results);
	if (!sp) return 0;
	dprintf(dlevel,"sp->values: %p\n", sp->values);
	if (!sp->values) return 0;
	vp = &sp->values[0][1];
	conv_type(DATA_TYPE_FLOAT,&value,0,vp->type,vp->data,vp->len);
	dprintf(dlevel,"value: %f\n", value);
	return value;
}
#endif
#endif

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
