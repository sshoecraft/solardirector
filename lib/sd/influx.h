
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __INFLUX_H
#define __INFLUX_H

#include "list.h"

struct influx_session;
typedef struct influx_session influx_session_t;
struct influx_result;
typedef struct influx_result influx_result_t;
struct influx_response;
typedef struct influx_response influx_response_t;

struct influx_value {
	int type;
	void *data;
	int len;
};
typedef struct influx_value influx_value_t;

// Series/Results/Responses are all lists so they can be deleted when no more JS refs
struct influx_series {
	char name[128];
	char **columns;
	int column_count;
	influx_value_t ***values;
	int value_count;
#ifdef JS
	bool convdt;
#endif
	int refs;
	influx_result_t *parent;
};
typedef struct influx_series influx_series_t;

struct influx_result {
	int statement_id;
	char errmsg[128];
//	influx_series_t **series;
//	int series_count;
	list series;
	int refs;
	influx_response_t *parent;
};

struct influx_response {
	int error;
	char errmsg[128];
//	influx_result_t **results;
//	int result_count;
	list results;
	int refs;
	influx_session_t *parent;
};

influx_session_t *influx_new(void);
int influx_parse_config(influx_session_t *, char *influx_info);
int influx_enable(influx_session_t *s, int enabled);
int influx_set_db(influx_session_t *, char *name);
void influx_destroy_session(influx_session_t *s);
void influx_shutdown(void);
char *influx_mkurl(influx_session_t *s, char *action, char *query);
influx_response_t *influx_get_response(influx_session_t *);
int influx_release_response(influx_response_t *r);
//int influx_destroy_response(influx_response_t *r);

int influx_ping(influx_session_t *s);
int influx_health(influx_session_t *s);
int influx_connect(influx_session_t *s);
int influx_connected(influx_session_t *s);
influx_response_t *influx_query(influx_session_t *s, char *query);
influx_response_t *influx_write(influx_session_t *s, char *mm, char *string);
int influx_get_first_value(influx_response_t *r, double *d, char *t, int l);

#ifndef NO_CONFIG
#include "config.h"
int influx_write_props(influx_session_t *s, char *name, config_property_t *props);
void influx_add_props(influx_session_t *s, config_t *cp, char *name);
#endif
#ifndef NO_JSON
#include "json.h"
int influx_write_json(influx_session_t *s, char *name, json_value_t *v);
#endif

#ifdef JS
#include "jsapi.h"
#include "jsengine.h"
int influx_jsinit(JSEngine *e);
JSObject *js_influx_new(JSContext *, JSObject *, influx_session_t *);
#endif

#endif
