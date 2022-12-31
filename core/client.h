
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __SD_CLIENT_H
#define __SD_CLIENT_H

#include "agent.h"

#define client_agentinfo_t solard_agent_t

#define CLIENT_AGENTINFO_REPLY		0x0001		/* Function called, expecting a reply */
#define CLIENT_AGENTINFO_STATUS		0x0002		/* Status/errmsg set */
#define CLIENT_AGENTINFO_CHECKED	0x0004		/* Status checked */

struct solard_client {
	int flags;
	char name[SOLARD_NAME_LEN];		/* Client name */
	char section_name[CFG_SECTION_NAME_SIZE]; /* Config section name */
#ifdef MQTT
	mqtt_session_t *m;			/* MQTT session */
	bool config_from_mqtt;
	int mqtt_init;
	list mq;				/* Messages */
#endif
#ifdef INFLUX
	influx_session_t *i;			/* Influx session */
#endif
	event_session_t *e;
	config_t *cp;				/* Config */
	list agents;				/* Agents */
	bool addmq;
	void (*cb)(solard_agent_t *ap, solard_message_t *msg);
#ifdef JS
	JSPropertySpec *props;
	int rtsize;
	int stacksize;
	JSEngine *js;
	jsval config_val;
	jsval mqtt_val;
	jsval influx_val;
	jsval agents_val;
	list roots;
	char script_dir[SOLARD_PATH_MAX];
	char init_script[SOLARD_PATH_MAX];
	char start_script[SOLARD_PATH_MAX];
	char stop_script[SOLARD_PATH_MAX];
#endif
//	list eq;				/* Event queue */
	void *private;
};
typedef struct solard_client solard_client_t;

#define CLIENT_FLAG_NOMQTT               0x0001          /* Dont't init MQTT */
#define CLIENT_FLAG_NOINFLUX             0x0020          /* Don't init influx */
#define CLIENT_FLAG_NOJS                 0x0040          /* Don't init JSEngine */
#define CLIENT_FLAG_NOEVENT              0x0080          /* Don't init Event subsystem */

solard_client_t *client_init(int argc,char **argv,char *version,opt_proctab_t *client_opts,char *Cname,int flags,config_property_t *props,config_function_t *funcs);
void client_destroy(solard_client_t *);

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

#ifdef JS
JSObject *js_InitClientClass(JSContext *cx, JSObject *global_object);
JSObject *js_client_new(JSContext *cx, JSObject *parent, void *c);
int jsclient_init(JSContext *cx, JSObject *parent, void *priv);
int client_jsinit(JSEngine *e, void *priv);
#endif

#endif
