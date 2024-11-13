
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __RHEEM_H
#define __RHEEM_H

#include "agent.h"
#include "client.h"
#include <curl/curl.h>

enum RHEEM_DEVICE_TYPE {
	RHEEM_DEVICE_TYPE_UNK,
	RHEEM_DEVICE_TYPE_WH,
	RHEEM_DEVICE_TYPE_HVAC,
};

struct rheem_session;
struct rheem_device {
//	char name[32];
	bool enabled;
	bool running;
	int modecount;
	char modes[16][32];
	char modestr[32];
	int mode;
	int temp;
	int level;
	char status[128];
	char id[32];
	enum RHEEM_DEVICE_TYPE type;
	char mac[16];
	char serial[32];
	bool haveprops;
#ifdef JS
	jsval modes_val;
	JSObject *obj;
	char *class_name;
#endif
	struct rheem_session *s;
};
typedef struct rheem_device rheem_device_t;

#define RHEEM_MAX_DEVICES 16

struct rheem_session {
	solard_agent_t *ap;
	uint16_t state;
	char email[64];
	char password[64];
	char inverter_source[16];
	char inverter_name[64];
	char url[128];
	char endpoint[128];
#ifdef HAVE_CURL
	CURL *curl;
	struct curl_slist *hs;
#endif
#ifdef MQTT
	mqtt_session_t *m;
#endif
	char *buffer;			/* Read buffer */
	int bufsize;			/* Buffer size */
	int bufidx;			/* Current write pos */
	char account_id[64];
	char user_token[1024];
	char reported[256];
	char desired[256];
	int devcount;
	rheem_device_t devices[RHEEM_MAX_DEVICES];
	bool subed;			/* Subscribed */
	int errcode;			/* error indicator */
	char errmsg[256];		/* Error message if errcode !0 */
	int connected;			/* Login succesful, we are connected */
	int lastcon;
	char location[64];		/* Lat,Lon */
	bool readonly;
	bool repub;
#ifdef JS
	JSPropertySpec *props;
	JSFunctionSpec *funcs;
	jsval agent_val;
	jsval client_val;
#endif
};
typedef struct rheem_session rheem_session_t;

extern solard_driver_t rheem_driver;
int rheem_agent_init(int argc, char **argv, rheem_session_t *s);
int rheem_config(void *h, int req, ...);
int rheem_connect_mqtt(rheem_session_t *s);
void rheem_add_whprops(rheem_session_t *s);
rheem_device_t *rheem_get_device(rheem_session_t *s, char *id, bool add);
int rheem_jsinit(rheem_session_t *s);
json_value_t *rheem_request(rheem_session_t *s, char *func, char *fields);

#endif
