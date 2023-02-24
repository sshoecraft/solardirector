
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 4
#include "debug.h"

#include "common.h"

struct event_handler_info {
	event_handler_t *handler;
	void *ctx;
	event_info_t scope;
	char *name;
	char *module;
	char *action;
};
typedef struct event_handler_info event_handler_info_t;

struct event_session {
	list handlers;
};
typedef struct event_session event_session_t;

event_session_t *event_init(void) {
	event_session_t *e;

	e = malloc(sizeof(*e));
	if (!e) return 0;
	memset(e,0,sizeof(*e));
	e->handlers = list_create(); 

	return e;
}

void event_destroy(event_session_t *e) {
	struct event_handler_info *info;

	dprintf(dlevel,"handler count: %d\n", list_count(e->handlers));
	list_reset(e->handlers);
	while((info = list_get_next(e->handlers)) != 0) {
		dprintf(dlevel,"destroying handler: %p\n", info->handler);
		info->handler(info->ctx,0,0,"__destroy__");
		if (info->name) free(info->name);
		if (info->module) free(info->module);
		if (info->action) free(info->action);
		free(info);
	}
	list_destroy(e->handlers);
	free(e);
}

int event_handler(event_session_t *e, event_handler_t handler, void *ctx, char *name, char *module, char *action) {
	struct event_handler_info *newinfo;

	newinfo = malloc(sizeof(*newinfo));
	if (!newinfo) return 1;
	memset(newinfo,0,sizeof(*newinfo));
	newinfo->handler = handler;
	newinfo->ctx = ctx;
	if (name) newinfo->name = strdup(name);
	if (module) newinfo->module = strdup(module);
	if (action) newinfo->action = strdup(action);
	list_add(e->handlers,newinfo,0);
	return 0;
}

void event(event_session_t *e, char *name, char *module, char *action) {
	struct event_handler_info *info;

//	log_info("EVENT: name: %s, module: %s, action: %s\n", name, module, action);

	dprintf(dlevel,"handler count: %d\n", list_count(e->handlers));
	list_reset(e->handlers);
	while((info = list_get_next(e->handlers)) != 0) {
		dprintf(dlevel,"info: handler: %p, ctx: %p, name: %s, module: %s, action: %s\n",
			info->handler, info->ctx, info->name, info->module, info->action);
		dprintf(dlevel,"calling handler: %p\n", info->handler);
		info->handler(info->ctx, name, module, action);
	}
	dprintf(dlevel,"done!\n");
}
