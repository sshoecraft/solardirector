
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __SD_COMMON_H
#define __SD_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#ifdef JS
#include "jsapi.h"
#endif

#define SOLARD_PATH_MAX		256

#define SOLARD_ID_LEN		38
#define SOLARD_NAME_LEN		32
#define SOLARD_TRANSPORT_LEN	16
#define SOLARD_TARGET_LEN	128
#define SOLARD_TOPTS_LEN	64
#define SOLARD_TOPIC_LEN	128	/* Max topic length */
#define SOLARD_TOPIC_SIZE	256	/* Max topic length */
#define SOLARD_ERRMSG_LEN	128

/* TOPIC / TYPE / NAME / FUNC */
#define SOLARD_TOPIC_ROOT	"SolarD"
#define SOLARD_TOPIC_AGENTS	"Agents"
#define SOLARD_TOPIC_CLIENTS	"Clients"

enum SOLARD_MESSAGE_TYPE {
	SOLARD_MESSAGE_TYPE_NONE,
	SOLARD_MESSAGE_TYPE_AGENT,
	SOLARD_MESSAGE_TYPE_CLIENT
};

#define SOLARD_ROLE_CONTROLLER	"Controller"
#define SOLARD_ROLE_STORAGE	"Storage"
#define SOLARD_ROLE_BATTERY	"Battery"
#define SOLARD_ROLE_INVERTER	"Inverter"
#define SOLARD_ROLE_PVINVERTER	"PVInverter"
#define SOLARD_ROLE_UTILITY	"Utility"
#define SOLARD_ROLE_LEN		16

#define SOLARD_FUNC_STATUS	"Status"
#define SOLARD_FUNC_INFO	"Info"
#define SOLARD_FUNC_CONFIG	"Config"
#define SOLARD_FUNC_DATA	"Data"
#define SOLARD_FUNC_EVENT	"Event"
#define SOLARD_FUNC_LEN		16

struct solard_power {
	union {
		float l1;
		float a;
	};
	union {
		float l2;
		float b;
	};
	union {
		float l3;
		float c;
	};
	float total;
};
typedef struct solard_power solard_power_t;

extern char SOLARD_BINDIR[SOLARD_PATH_MAX], SOLARD_ETCDIR[SOLARD_PATH_MAX], SOLARD_LIBDIR[SOLARD_PATH_MAX], SOLARD_LOGDIR[SOLARD_PATH_MAX];

/* Macros */
#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)
#define set_bit(b,v)	((b) |= (v))
#define clear_bit(b,v)	((b) &= (~v))
#define check_bit(b,v)	(((b) & v) != 0)

#include "types.h"
#include "opts.h"
#include "cfg.h"
#include "config.h"
#include "message.h"
#include "json.h"
#include "utils.h"
#include "debug.h"
#include "state.h"
#include "mqtt.h"
#include "influx.h"
#include "event.h"

#if 0
#define Black "\033[0;30m"
#define Red "\033[0;31m"
#define Green "\033[0;32m"
#define Yellow "\033[0;33m"
#define Blue "\033[0;34m"
#define Purple "\033[0;35m"
#define Cyan "\033[0;36m"
#define White "\033[0;37m"
#endif

int solard_common_init(int argc,char **argv,char *ver,opt_proctab_t *add_opts,int start_opts);
int solard_common_config(cfg_info_t *,char *);
int solard_common_startup(config_t **cp, char *sname, char *configfile, config_property_t *props, config_function_t *funcs,
		event_session_t **e
#ifdef MQTT
		,mqtt_session_t **m, char *lwt, mqtt_callback_t *getmsg, void *mctx,char *mqtt_info, int config_from_mqtt
#endif
#ifdef INFLUX
		,influx_session_t **i, char *influx_info
#endif
#ifdef JS
		,JSEngine **js, int rtsize, int stksize, js_outputfunc_t *jsout
#endif
	);
void solard_common_shutdown(void);


void common_add_props(config_t *, char *);
json_object_t *solard_create_results(int status, char *errmsg, ...);

#endif
