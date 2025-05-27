
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __SD_CLIENT_H
#define __SD_CLIENT_H

#include "common.h"

#ifdef MQTT
#include "mqtt.h"
#endif
#ifdef INFLUX
#include "influx.h"
#endif
#include "config.h"
#ifdef JS
#include "jsengine.h"
#endif

struct solard_client {
	int flags;
	char name[SOLARD_NAME_LEN];		/* Client name */
	char section_name[CFG_SECTION_NAME_SIZE]; /* Config section name */
#ifdef MQTT
	mqtt_session_t *m;			/* MQTT session */
	bool config_from_mqtt;
	int mqtt_init;
	list mq;				/* Messages */
	bool addmq;				/* add messages to q if true */
#endif
#ifdef INFLUX
	influx_session_t *i;			/* Influx session */
#endif
	event_session_t *e;
	config_t *cp;				/* Config */
#ifdef JS
	struct {
		int rtsize;
		int stacksize;
		JSEngine *e;
		JSPropertySpec *props;
		jsval client_val;
		jsval config_val;
		jsval mqtt_val;
		jsval influx_val;
		list roots;
#if 0
		char script_dir[SOLARD_PATH_MAX];
		char init_script[SOLARD_PATH_MAX];
		char start_script[SOLARD_PATH_MAX];
		char stop_script[SOLARD_PATH_MAX];
#endif
	} js;
#endif
//	void *private;
};
typedef struct solard_client solard_client_t;

#define CLIENT_FLAG_NOMQTT		0x0001		/* Dont't init MQTT */
#define CLIENT_FLAG_NOINFLUX		0x0020		/* Don't init influx */
#define CLIENT_FLAG_NOJS		0x0040		/* Don't init JSEngine */
#define CLIENT_FLAG_NOEVENT		0x0080		/* Don't init Event subsystem */
#define CLIENT_FLAG_JSGLOBAL		0x1000		/* Create global config/mqtt/influx objects */

solard_client_t *client_init(int argc,char **argv,char *Cname, char *version, opt_proctab_t *opts, int flags, config_property_t *props);
void client_shutdown(void);

#if 0
typedef int (client_callback_t)(void *ctx, solard_client_t *);
void client_destroy_client(solard_client_t *c);

client_agentinfo_t *client_getagentbyname(solard_client_t *c, char *name);
client_agentinfo_t *client_getagentbyid(solard_client_t *c, char *name);
config_function_t *client_getagentfunc(client_agentinfo_t *info, char *name);
int client_callagentfunc(client_agentinfo_t *info, config_function_t *f, int nargs, char **args);
int client_callagentfuncbyname(client_agentinfo_t *info, char *name, int nargs, char **args);
int client_set_config(solard_client_t *cp, char *op, char *target, char *param, char *value, int timeout);
int client_config_init(solard_client_t *c, config_property_t *client_props, config_function_t *client_funcs);
int client_mqtt_init(solard_client_t *c);
int client_matchagent(client_agentinfo_t *info, char *target, bool exact);
int client_getagentstatus(client_agentinfo_t *info, solard_message_t *msg);
char *client_getagentrole(client_agentinfo_t *info);
#endif

#ifdef JS
JSObject *js_InitClientClass(JSContext *cx, JSObject *parent);
JSObject *js_client_new(JSContext *cx, JSObject *parent, void *priv);
#endif

#endif
