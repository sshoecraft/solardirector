
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __SD_AGENT_H
#define __SD_AGENT_H

#include "common.h"
#include "driver.h"
#ifdef JS
#include "jsengine.h"
#endif

//struct solard_agent;
typedef struct solard_agent solard_agent_t;
typedef struct solard_agent agent_t;
typedef int (solard_agent_callback_t)(void *);

struct solard_agent {
	int flags;
	char name[SOLARD_NAME_LEN];
	char configfile[SOLARD_PATH_MAX];
	config_t *cp;
	char section_name[CFG_SECTION_NAME_SIZE];
	solard_driver_t *driver;
	void *handle;
	json_value_t *info;
#ifdef MQTT
	char id[SOLARD_ID_LEN];
	char role[SOLARD_ROLE_LEN];
	char target[SOLARD_ROLE_LEN+SOLARD_NAME_LEN+1];
	list aliases;
	mqtt_session_t *m;
	bool config_from_mqtt;
	char mqtt_topic[SOLARD_TOPIC_SIZE];
	list mq;			/* incoming message queue */
	bool addmq;
#endif
#ifdef INFLUX
	influx_session_t *i;
#endif
	event_session_t *e;
	int interval;
	int run_count;
	uint16_t state;			/* States */
	int pretty;			/* Format json messages for readability (uses more mem) */
	char instance_name[SOLARD_NAME_LEN]; /* Agent instance name */
	char agent_libdir[SOLARD_PATH_MAX];	/* Agent libdir (SOLARD_LIBDIR/agents/<driver_name>) */
#ifdef JS
	struct {
		JSEngine *e;
		int rtsize;
		int stksize;
		bool run_scripts;
		JSPropertySpec *props;
		JSFunctionSpec *funcs;
		jsval agent_val;
		jsval config_val;
		jsval mqtt_val;
		jsval influx_val;
		int gc_interval;
		int ignore_js_errors;
		char script_dir[SOLARD_PATH_MAX];
		char init_script[SOLARD_PATH_MAX];
		bool init_run;
		char start_script[SOLARD_PATH_MAX];
		bool start_run;
		char stop_script[SOLARD_PATH_MAX];
		char read_script[SOLARD_PATH_MAX];
		char write_script[SOLARD_PATH_MAX];
		char run_script[SOLARD_PATH_MAX];
		list roots;
	} js;
#endif
	struct {
		solard_agent_callback_t *func;	/* Called between read and write */
		void *ctx;
	} callback;
	int status;
	char errmsg[128];
	bool debug_mem;
	void *private;			/* Per-instance private data */
};

#ifdef JS
struct js_agent_rootinfo {
	JSContext *cx;
	void *vp;
	char *name;
};
#endif

/* Agent states */
#define SOLARD_AGENT_STATE_RUNNING	0x0001
#define SOLARD_AGENT_STATE_ONLINE	0x0010
//#define SOLARD_AGENT_STATE_CONFIG_DIRTY 0x10

/* Agent flags */
#define AGENT_FLAG_NOMQTT		0x0001          /* Dont't init MQTT */
#define AGENT_FLAG_NOINFLUX		0x0020		/* Don't init influx */
#define AGENT_FLAG_NOJS			0x0040		/* Don't init JSEngine */
#define AGENT_FLAG_NOEVENT		0x0080		/* Don't init Event subsystem */
#define SOLARD_AGENT_FLAG_OBREAD        0x0100          /* Open before reading */
#define SOLARD_AGENT_FLAG_CAREAD        0x0200          /* Close after reading */
#define SOLARD_AGENT_FLAG_OBWRITE       0x0400          /* Open before writing */
#define SOLARD_AGENT_FLAG_CAWRITE       0x0800          /* Close after wrtiing */
#define AGENT_FLAG_NOALL		AGENT_FLAG_NOMQTT | AGENT_FLAG_NOINFLUX | AGENT_FLAG_NOJS | AGENT_FLAG_NOEVENT

/* Special config func */
#define AGENT_CONFIG_GETJS		0x1000		/* Get JSEngine ptr from driver */

solard_agent_t *agent_new(void);
solard_agent_t *agent_init(int, char **, char *, opt_proctab_t *,
		solard_driver_t *, void *, int flags, config_property_t *, config_function_t *);
void agent_destroy(solard_agent_t *);
int agent_event_handler(solard_agent_t *ap, event_handler_t *func, void *ctx, char *name, char *module, char *action);
config_property_t *agent_get_props(solard_agent_t *);
int agent_run(solard_agent_t *ap);
#ifdef MQTT
void agent_mktopic(char *topic, int topicsz, char *name, char *func);
int agent_sub(solard_agent_t *ap, char *name, char *func);
int agent_pub(solard_agent_t *ap, char *func, char *message, int retain);
int agent_pubinfo(solard_agent_t *ap, int disp);
int agent_pubconfig(solard_agent_t *ap);
int agent_pubdata(solard_agent_t *ap, json_value_t *v);
int agent_reply(solard_agent_t *ap, char *topic, int status, char *message);
#endif
int agent_set_callback(solard_agent_t *, solard_agent_callback_t *, void *);
int agent_clear_callback(solard_agent_t *);
int cf_agent_getinfo(void *ctx, list args, char *errmsg, json_object_t *results);
int cf_agent_getconfig(void *ctx, list args, char *errmsg, json_object_t *results);

void agent_event(solard_agent_t *ap, char *action, char *reason);

#include "config.h"

#ifdef JS
int agent_start_script(solard_agent_t *ap, char *name);
int agent_start_jsfunc(solard_agent_t *ap, char *name, char *func, int argc, jsval *argv);
int agent_script_exists(solard_agent_t *ap, char *name);
#define agent_run_script(a,n) agent_start_script(a,n,0)
JSObject *js_InitAgentClass(JSContext *cx, JSObject *global_object);
JSObject *js_agent_new(JSContext *cx, JSObject *parent, void *priv);
int agent_jsexec(solard_agent_t *ap, char *string);
#endif

#endif /* __SD_AGENT_H */
