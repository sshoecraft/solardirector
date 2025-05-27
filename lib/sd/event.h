
#ifndef __SDEVENT_H
#define __SDEVENT_H

#include "common.h"

typedef void (event_handler_t)(void *ctx, char *name, char *action, char *reason);

struct event_info {
	char name[SOLARD_NAME_LEN];
	char module[32];
	char action[64];
};
typedef struct event_info event_info_t;

struct event_session;
typedef struct event_session event_session_t;

event_session_t *event_init(void);
void event_destroy(event_session_t *);
int event_handler(event_session_t *e, event_handler_t handler, void *ctx, char *name, char *module, char *action);
void event_signal(event_session_t *e, char *name, char *module, char *action);

#ifdef JS
JSObject *js_event_new(JSContext *cx, JSObject *parent, void *priv);
JSObject *js_InitEventClass(JSContext *cx, JSObject *parent);
#endif

#endif
