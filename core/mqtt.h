
#ifdef MQTT
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __SOLARD_MQTT_H
#define __SOLARD_MQTT_H

#include "config.h"

/* We use the paho mqtt.c library */
#include <MQTTClient.h>

#define MQTT_URI_LEN 128
#define MQTT_USER_LEN 256
#define MQTT_PASS_LEN 64
#define MQTT_CLIENTID_LEN 64
#define MQTT_TOPIC_LEN 128
#define MQTT_MAX_MESSAGE_SIZE 262144

typedef void (mqtt_callback_t)(void *ctx, char *topic, char *payload, int len, char *replyto);

struct mqtt_session {
	bool v3;
	bool enabled;
	char uri[MQTT_URI_LEN];
	char clientid[MQTT_CLIENTID_LEN];
	char username[MQTT_USER_LEN];
	char password[MQTT_PASS_LEN];
	char lwt_topic[MQTT_TOPIC_LEN];
	MQTTClient c;
	int interval;
	mqtt_callback_t *cb;
	void *ctx;
	MQTTClient_SSLOptions *ssl_opts;
	list subs;
	int connected;
	char errmsg[128];
};
typedef struct mqtt_session mqtt_session_t;

struct mqtt_sslinfo {
#if 0
const char *    trustStore
const char *    keyStore
const char *    privateKey
const char *    privateKeyPassword
const char *    enabledCipherSuites
int     enableServerCertAuth
int     sslVersion
int     verify
const char *    CApath
#endif
	char *key;
	char *keypass;
	char *ciphers;
	char *ca_path;
	bool serverauth;
	int version;
	bool verify;
};
typedef struct mqtt_sslinfo mqtt_sslinfo_t;

struct mqtt_session *mqtt_new(bool v3, mqtt_callback_t *cb, void *ctx);
int mqtt_parse_config(mqtt_session_t *, char *str);
int mqtt_get_config(char *info, int size, mqtt_session_t *s, int id);
int mqtt_init(mqtt_session_t *s);
int mqtt_newclient(mqtt_session_t *);
int mqtt_connect(mqtt_session_t *s, int interval);
int mqtt_connected(mqtt_session_t *s);
int mqtt_disconnect(mqtt_session_t *s, int timeout);
int mqtt_reconnect(mqtt_session_t *s);
int mqtt_destroy(mqtt_session_t *s);
int mqtt_send(mqtt_session_t *s, char *topic, char *message, int timeout);
int mqtt_sub(mqtt_session_t *s, char *topic);
int mqtt_unsub(mqtt_session_t *s, char *topic);
int mqtt_setcb(mqtt_session_t *s, void *ctx, MQTTClient_connectionLost *cl, MQTTClient_messageArrived *ma, MQTTClient_deliveryComplete *dc);
int mqtt_resub(mqtt_session_t *s);
int mqtt_pub(mqtt_session_t *s, char *topic, char *message, int wait, int retain);
void mqtt_set_lwt(mqtt_session_t *s, char *new_topic);

int mqtt_dosend(mqtt_session_t *m, char *topic, char *message);
int mqtt_fullsend(char *address, char *clientid, char *message, char *topic, char *user, char *pass);

void mqtt_add_props(mqtt_session_t *, config_t *, char *);

#ifdef JS
#include "jsapi.h"
#include "jsengine.h"
JSObject * js_InitMQTTClass(JSContext *cx, JSObject *global_object);
JSObject *js_mqtt_new(JSContext *, JSObject *, mqtt_session_t *);
#endif

#endif

#endif /* MQTT */
