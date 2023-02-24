#ifdef MQTT

/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 4
#include "debug.h"

#include <string.h>
#include <MQTTClient.h>
#include <MQTTAsync.h>
#include <stdlib.h>
#include "mqtt.h"
#include "utils.h"
#include "config.h"
#include "uuid.h"

#define TIMEOUT 10000L
static list mqtt_sessions = 0;
#ifdef JS
static list js_ctxs = 0;
#endif

int mqtt_parse_config(mqtt_session_t *s, char *mqtt_info) {
	dprintf(dlevel,"mqtt_info: %s\n", mqtt_info);
	strncpy(s->uri,strele(0,",",mqtt_info),sizeof(s->uri)-1);
	strncpy(s->clientid,strele(1,",",mqtt_info),sizeof(s->clientid)-1);
	strncpy(s->username,strele(2,",",mqtt_info),sizeof(s->username)-1);
	strncpy(s->password,strele(3,",",mqtt_info),sizeof(s->password)-1);
	dprintf(dlevel,"uri: %s, clientid: %s, user: %s, pass: %s\n", s->uri, s->clientid, s->username, s->password);
	return 0;
}

int mqtt_get_config(char *mqtt_info, int size, mqtt_session_t *s, int id) {
	snprintf(mqtt_info,size,"%s,%s,%s,%s",s->uri,(id ? s->clientid : ""),s->username,s->password);
	dprintf(dlevel,"mqtt_info: %s\n", mqtt_info);
	return 0;
}

int mqtt_init(mqtt_session_t *s) {

	dprintf(dlevel,"s: %p\n", s);

	if (!strlen(s->uri)) {
		log_write(LOG_WARNING,"mqtt uri not specified, using localhost\n");
		strcpy(s->uri,"localhost");
	}

	/* Create a new client */
	if (mqtt_newclient(s)) return 1;

	/* Connect to the server */
	if (mqtt_connect(s,20)) s->connected = false;

	return 0;
}

void logProperties(MQTTProperties *props) {
	int i = 0;

	dprintf(dlevel,"props->count: %d\n", props->count);
	for (i = 0; i < props->count; ++i) {
		int id = props->array[i].identifier;
		dprintf(dlevel,"prop: id: %d, name: %s\n", id, MQTTPropertyName(id));
		switch (MQTTProperty_getType(id)) {
		case MQTTPROPERTY_TYPE_BYTE:
			dprintf(dlevel, "value %d", props->array[i].value.byte);
			break;
		case MQTTPROPERTY_TYPE_TWO_BYTE_INTEGER:
			dprintf(dlevel, "value %d", props->array[i].value.integer2);
			break;
		case MQTTPROPERTY_TYPE_FOUR_BYTE_INTEGER:
			dprintf(dlevel, "value %d", props->array[i].value.integer4);
			break;
		case MQTTPROPERTY_TYPE_VARIABLE_BYTE_INTEGER:
			dprintf(dlevel, "value %d", props->array[i].value.integer4);
			break;
		case MQTTPROPERTY_TYPE_BINARY_DATA:
		case MQTTPROPERTY_TYPE_UTF_8_ENCODED_STRING:
			dprintf(dlevel, "value %.*s", props->array[i].value.data.len, props->array[i].value.data.data);
			break;
		case MQTTPROPERTY_CODE_USER_PROPERTY:
		case MQTTPROPERTY_TYPE_UTF_8_STRING_PAIR:
			dprintf(dlevel, "key %.*s value %.*s", props->array[i].value.data.len, props->array[i].value.data.data, props->array[i].value.value.len, props->array[i].value.value.data);
			break;
		}
	}
}

static void mqtt_conlost(void *ctx, char *cause) {
	mqtt_session_t *s = ctx;

	dprintf(dlevel,"cause: %s\n", cause);
	s->connected = false;
}

static int mqtt_getmsg(void *ctx, char *topicName, int topicLen, MQTTClient_message *message) {
	char topic[256],*replyto;
	mqtt_session_t *s = ctx;
	int len;

	/* Ignore zero length messages */
	dprintf(dlevel,"payloadlen: %d\n", message->payloadlen);
	if (!message->payloadlen) goto mqtt_getmsg_skip;

	dprintf(dlevel,"topicLen: %d\n", topicLen);
	if (topicLen) {
		len = topicLen > sizeof(topic)-1 ? sizeof(topic)-1 : topicLen;
	} else {
		len = strlen(topicName);
		if (len > sizeof(topic)-1) len = sizeof(topic)-1;
	}
	dprintf(dlevel,"len: %d\n", len);
	memcpy(topic,topicName,len);
	topic[len] = 0;
	dprintf(dlevel,"topic: %s\n",topic);

//	logProperties(&message->properties);

	/* Do we have a replyto user property? */
	replyto = 0;
	dprintf(dlevel,"hasProperty UP: %d\n", MQTTProperties_hasProperty(&message->properties, MQTTPROPERTY_CODE_USER_PROPERTY));
	if (MQTTProperties_hasProperty(&message->properties, MQTTPROPERTY_CODE_USER_PROPERTY)) {
		MQTTProperty *prop;

		prop = MQTTProperties_getProperty(&message->properties, MQTTPROPERTY_CODE_USER_PROPERTY);
		if (strncmp(prop->value.data.data,"replyto",prop->value.data.len) == 0) {
			replyto = prop->value.value.data;
		}
	}

	dprintf(dlevel,"cb: %p\n", s->cb);
	if (s->cb) s->cb(s->ctx, topic, message->payload, message->payloadlen, replyto);

mqtt_getmsg_skip:
	MQTTClient_freeMessage(&message);
	MQTTClient_free(topicName);
	dprintf(dlevel,"returning...\n");
	return true;
}

int mqtt_newclient(mqtt_session_t *s) {
	MQTTClient_createOptions opts = MQTTClient_createOptions_initializer;
	int rc;

	/* Create a unique ID for MQTT */
	if (!strlen(s->clientid)) {
		uint8_t uuid[16];

		dprintf(dlevel,"gen'ing MQTT ClientID...\n");
		uuid_generate_random(uuid);
		dprintf(dlevel,"unparsing uuid...\n");
		my_uuid_unparse(uuid, s->clientid);
		dprintf(dlevel,"clientid: %s\n",s->clientid);
	}

	opts.MQTTVersion = (s->v3 ? MQTTVERSION_3_1 : MQTTVERSION_5);
	dprintf(dlevel,"uri: %s\n", s->uri);
	rc = MQTTClient_createWithOptions(&s->c, s->uri, s->clientid, MQTTCLIENT_PERSISTENCE_NONE, NULL, &opts);
	dprintf(dlevel,"create rc: %d\n", rc);
	if (rc != MQTTCLIENT_SUCCESS) {
		sprintf(s->errmsg,"MQTTClient_create failed");
		return 1;
	}

	/* Set callback BEFORE connect */
	dprintf(dlevel,"setting callbacks...\n");
	rc = MQTTClient_setCallbacks(s->c, s, mqtt_conlost, mqtt_getmsg, 0);
	dprintf(dlevel,"setcb rc: %d\n", rc);
	if (rc != MQTTCLIENT_SUCCESS) {
		strcpy(s->errmsg,"MQTTClient_setCallbacks failed");
		return 1;
	}

	return 0;
}

/* Create a new session */
struct mqtt_session *mqtt_new(bool v3, mqtt_callback_t *cb, void *ctx) {
	mqtt_session_t *s;

	s = calloc(sizeof(*s),1);
	if (!s) {
		log_write(LOG_SYSERR,"mqtt_new: calloc");
		return 0;
	}
	s->v3 = v3;
	s->cb = cb;
	s->ctx = ctx;
	s->subs = list_create();
	if (!s->subs) {
		free(s);
		return 0;
	}
	s->ctor = false;
	s->mq = list_create();

	if (!mqtt_sessions) mqtt_sessions = list_create();
	list_add(mqtt_sessions, s, 0);

	return s;
}

int mqtt_set_ssl(mqtt_session_t *s, mqtt_sslinfo_t *info) {
#if 0
char 	struct_id [4]
int 	struct_version
const char * 	trustStore
const char * 	keyStore
const char * 	privateKey
const char * 	privateKeyPassword
const char * 	enabledCipherSuites
int 	enableServerCertAuth
int 	sslVersion
int 	verify
const char * 	CApath
int(* 	ssl_error_cb )(const char *str, size_t len, void *u)
void * 	ssl_error_context
#endif
	return 0;
}

int mqtt_connect(mqtt_session_t *s, int interval) {
	MQTTClient_willOptions will_opts = MQTTClient_willOptions_initializer;
	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	MQTTClient_connectOptions conn_opts5 = MQTTClient_connectOptions_initializer5;
	MQTTResponse response = MQTTResponse_initializer;
	MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
	bool have_ssl;
	int rc;

	if (!s) return 1;
	dprintf(dlevel,"enabled: %d\n", s->enabled);
	if (!s->enabled) return 0;

	dprintf(dlevel,"interval: %d\n", interval);

	dprintf(dlevel,"lwt: %s\n", s->lwt_topic);
	if (strlen(s->lwt_topic)) {
		will_opts.topicName = s->lwt_topic;
		will_opts.message = "Offline";
		will_opts.retained = 1;
		will_opts.qos = 1;
	}

	if (strncmp(s->uri,"ssl://",6) == 0) {
		have_ssl = true;
		ssl_opts.sslVersion = MQTT_SSL_VERSION_TLS_1_2;
		ssl_opts.enableServerCertAuth = 1;
	} else {
		have_ssl = false;
	}

	s->interval = interval;
	if (s->v3) {
		dprintf(dlevel,"connectv3\n");
		conn_opts.MQTTVersion = MQTTVERSION_3_1;
		conn_opts.cleansession = 1;
		conn_opts.keepAliveInterval = interval;
		conn_opts.connectTimeout = 3;
		if (strlen(s->username)) {
			conn_opts.username = s->username;
			if (strlen(s->password)) conn_opts.password = s->password;
			dprintf(dlevel,"username: %s, password: %s\n", conn_opts.username, conn_opts.password);
		}
		if (strlen(s->lwt_topic)) conn_opts.will = &will_opts;
		if (have_ssl) conn_opts.ssl = &ssl_opts;
		conn_opts.serverURIcount = 0;
		conn_opts.serverURIs = NULL;
		rc = MQTTClient_connect(s->c, &conn_opts);
	} else {
		dprintf(dlevel,"connectv5\n");
		conn_opts5.MQTTVersion = MQTTVERSION_5;
		conn_opts5.cleanstart = 1;
		conn_opts5.keepAliveInterval = interval;
		conn_opts5.connectTimeout = 3;
		if (strlen(s->username)) {
			conn_opts5.username = s->username;
			if (strlen(s->password)) conn_opts5.password = s->password;
			dprintf(dlevel,"username: %s, password: %s\n", conn_opts5.username, conn_opts5.password);
		}
		conn_opts5.ssl = s->ssl_opts;
		if (strlen(s->lwt_topic)) conn_opts5.will = &will_opts;
		if (have_ssl) conn_opts5.ssl = &ssl_opts;
		response = MQTTClient_connect5(s->c, &conn_opts5, 0, 0);
		rc = response.reasonCode;
		MQTTResponse_free(response);
	}

	dprintf(dlevel,"rc: %d\n", rc);
	if (rc != MQTTCLIENT_SUCCESS) {
		if (rc == 5) {
			strcpy(s->errmsg,"MQTT: bad username or password");
			return 1;
		} else {
			char *p = (char *)MQTTReasonCode_toString(rc);
			sprintf(s->errmsg,"MQTTClient_connect: %s",p ? p : "cant connect");
		}
		return 1;
	} else if (strlen(s->lwt_topic)) {
		mqtt_pub(s,s->lwt_topic,"Online",1,1);
	}
	s->connected = true;

	/* If we have a list of subs, resub */
	dprintf(dlevel,"count: %d\n", list_count(s->subs));
	if (list_count(s->subs)) mqtt_resub(s);
	return 0;
}

int mqtt_connected(mqtt_session_t *s) {
	if (!s) return 0;
	else return s->connected;
}

int mqtt_disconnect(mqtt_session_t *s, int timeout) {
	int rc;

	dprintf(dlevel,"timeout: %d\n", timeout);

	if (!s) return 1;
	dprintf(dlevel,"enabled: %d\n", s->enabled);
	if (!s->enabled) return 0;

	if (s->v3)
		rc = MQTTClient_disconnect(s->c, timeout);
	else
		rc = MQTTClient_disconnect5(s->c, timeout, MQTTREASONCODE_SUCCESS, 0);
	dprintf(dlevel,"rc: %d\n", rc);
	s->connected = 0;
	return rc;
}

int mqtt_resub(mqtt_session_t *s) {
	char *topic;

	if (!s) return 1;

	dprintf(dlevel,"count: %d\n", list_count(s->subs));
	list_reset(s->subs);
	while((topic = list_get_next(s->subs)) != 0) mqtt_sub(s,topic);
	return 0;
}

int mqtt_reconnect(mqtt_session_t *s) {

	if (!s) return 1;
	dprintf(dlevel,"enabled: %d\n", s->enabled);
	if (!s->enabled) return 0;

	dprintf(dlevel,"reconnecting...\n");
	if (mqtt_disconnect(s,5)) return 1;
	if (mqtt_connect(s,20)) return 1;
	if (mqtt_resub(s)) return 1;
	dprintf(dlevel,"reconnect done!\n");
	return 0;
}

void mqtt_set_lwt(mqtt_session_t *s, char *new_topic) {

	if (!s) return;

	dprintf(dlevel,"old topic: %s, new topic: %s\n", s->lwt_topic, new_topic);
	if (strcmp(s->lwt_topic,new_topic) != 0) {
		char old_topic[MQTT_TOPIC_LEN];
		int doconnect;

		/* Save old topic */
		strcpy(old_topic,s->lwt_topic);

		/* Set new topic */
		strncpy(s->lwt_topic,new_topic,sizeof(s->lwt_topic)-1);

		/* Clear old topic */
		mqtt_pub(s,old_topic,0,1,0);

		dprintf(dlevel,"enabled: %d\n", s->enabled);
		if (s->enabled) {
			/* If connected, disconnect so we can delete old topic */
			dprintf(dlevel,"connected: %d\n", s->connected);
			doconnect = s->connected;
			if (s->connected) mqtt_disconnect(s,5);

			/* If was connected, reconnect with new LWT */
			if (doconnect) mqtt_connect(s,s->interval);
		}

	}

	return;
}

int mqtt_destroy_session(mqtt_session_t *s) {

	dprintf(dlevel,"s: %p\n", s);
	if (!s) return 1;

	mqtt_disconnect(s,0);
	dprintf(dlevel,"s->c: %p\n", s->c);
	if (s->c) {
		MQTTClient_destroy(&s->c);
	}
	s->c = 0;
	list_destroy(s->subs);
	list_destroy(s->mq);

	if (mqtt_sessions) list_delete(mqtt_sessions,s);
	free(s);
	return 0;
}

void mqtt_shutdown(void) {
	if (mqtt_sessions) {
		mqtt_session_t *s;

		while(true) {

			list_reset(mqtt_sessions);
			s = list_get_next(mqtt_sessions);
			if (!s) break;
			mqtt_destroy_session(s);
		}
		list_destroy(mqtt_sessions);
		mqtt_sessions = 0;
	}
#ifdef JS
	if (js_ctxs) {
		void *p;
		list_reset(js_ctxs);
		while((p = list_get_next(js_ctxs)) != 0) free(p);
		list_destroy(js_ctxs);
		js_ctxs = 0;
	}
#endif
}

int mqtt_pub(mqtt_session_t *s, char *topic, char *message, int wait, int retain) {
	MQTTClient_message pubmsg = MQTTClient_message_initializer;
	MQTTClient_deliveryToken token;
	MQTTProperty property;
	MQTTResponse response = MQTTResponse_initializer;
	int rc,rt,retry;

//	dprintf(dlevel,"s: %d\n", s);
	if (!s) return 1;

//	dprintf(dlevel,"enabled: %d\n", s->enabled);
	if (!s->enabled) return 0;

//	dprintf(dlevel,"topic: %s, msglen: %ld, retain: %d\n", topic, message ? strlen(message) : 0, retain);

	if (message) {
		pubmsg.payload = message;
		pubmsg.payloadlen = strlen(message);
	}

	/* Add a replyto user property */
	rt = 1;
	if (rt) {
		property.identifier = MQTTPROPERTY_CODE_USER_PROPERTY;
		property.value.data.data = "replyto";
		property.value.data.len = strlen(property.value.data.data);
		property.value.value.data = s->clientid;
		property.value.value.len = strlen(property.value.value.data);
		MQTTProperties_add(&pubmsg.properties, &property);
//		logProperties(&pubmsg.properties);
	}

	pubmsg.qos = 2;
	pubmsg.retained = retain;
	token = 0;
	retry = 0;
again:
//	dprintf(dlevel,"publishing...\n");
	if (s->v3) {
		rc = MQTTClient_publishMessage(s->c, topic, &pubmsg, &token);
	} else {
		response = MQTTClient_publishMessage5(s->c, topic, &pubmsg, &token);
		rc = response.reasonCode;
		MQTTResponse_free(response);
	}
//	dprintf(dlevel,"rc: %d\n", rc);
	if (rc != MQTTCLIENT_SUCCESS) {
//		dprintf(dlevel,"publish error\n");
		if (!retry) {
			mqtt_reconnect(s);
			retry = 1;
			goto again;
		} else {
			return 1;
		}
	}
	if (wait) {
//		dprintf(dlevel,"waiting...\n");
		rc = MQTTClient_waitForCompletion(s->c, token, TIMEOUT);
//		dprintf(dlevel,"rc: %d\n", rc);
		if (rc != MQTTCLIENT_SUCCESS) return 1;
	}
//	dprintf(dlevel,"delivered message... token: %d\n",token);
	if (rt) MQTTProperties_free(&pubmsg.properties);
	return 0;
}

int mqtt_setcb(mqtt_session_t *s, void *ctx, MQTTClient_connectionLost *cl, MQTTClient_messageArrived *ma, MQTTClient_deliveryComplete *dc) {
	int rc;

	dprintf(dlevel,"s: %p, ctx: %p, ma: %p\n", s, ctx, ma);
	rc = MQTTClient_setCallbacks(s->c, ctx, cl, ma, dc);
	dprintf(dlevel,"rc: %d\n", rc);
	if (rc) log_write(LOG_ERROR,"MQTTClient_setCallbacks: rc: %d\n", rc);
	return rc;
}

static void mqtt_addsub(mqtt_session_t *s, char *topic) {
	char *p;

	list_reset(s->subs);
	while((p = list_get_next(s->subs)) != 0) {
		if (strcmp(p,topic) == 0) {
			return;
		}
	}
	list_add(s->subs,topic,strlen(topic)+1);
}

int mqtt_sub(mqtt_session_t *s, char *topic) {
	MQTTSubscribe_options opts = MQTTSubscribe_options_initializer;
	MQTTResponse response = MQTTResponse_initializer;
	int rc;

	dprintf(dlevel,"s: %p\n", s);
	if (!s) return 1;
	dprintf(dlevel,"enabled: %d\n", s->enabled);
	if (!s->enabled) return 0;

	/* If we're not connected, add the sub so we sub when we do connect */
	if (!s->connected) {
		mqtt_addsub(s,topic);
		return 0;
	}
	opts.noLocal = 1;
	dprintf(dlevel,"topic: %s\n", topic);
	if (s->v3) {
		rc = MQTTClient_subscribe(s->c, topic, 1);
	} else {
		response = MQTTClient_subscribe5(s->c, topic, 1, &opts, 0);
		rc = response.reasonCode;
		MQTTResponse_free(response);
	}
	if (rc == MQTTREASONCODE_GRANTED_QOS_1) rc = 0;
	dprintf(dlevel,"rc: %d\n", rc);
	if (rc == MQTTCLIENT_SUCCESS) mqtt_addsub(s,topic);
	return rc;
}

int mqtt_submany(mqtt_session_t *s, int count, char **topic) {
	int i,rc,*qos;

	if (!s) return 1;
	dprintf(dlevel,"enabled: %d\n", s->enabled);
	if (!s->enabled) return 0;

	dprintf(dlevel,"s: %p, count: %d\n", s, count);
	qos = calloc(1,sizeof(int)*count);
	if (!qos) {
		log_write(LOG_SYSERR,"mqtt_submany: calloc");
		return 1;
	}
	for(i=0; i < count; i++) qos[i] = 1;
	rc = MQTTClient_subscribeMany(s->c, count, topic, qos);
	free(qos);
	dprintf(dlevel,"rc: %d\n", rc);
	if (rc == MQTTCLIENT_SUCCESS) {
		for(i=0; i < count; i++)
			mqtt_addsub(s,topic[i]);
	}
	return rc;
}

static void mqtt_delsub(mqtt_session_t *s, char *topic) {
	char *p;

	list_reset(s->subs);
	while((p = list_get_next(s->subs)) != 0) {
		if (strcmp(p,topic) == 0) {
			list_delete(s->subs,p);
			break;
		}
	}
}

int mqtt_unsub(mqtt_session_t *s, char *topic) {
	MQTTResponse response = MQTTResponse_initializer;
	int rc;

	if (!s) return 1;
	dprintf(dlevel,"enabled: %d\n", s->enabled);
	if (!s->enabled) return 0;

	dprintf(dlevel,"s: %p, topic: %s\n", s, topic);
	if (s->v3) {
		rc = MQTTClient_unsubscribe(s->c, topic);
	} else {
		response = MQTTClient_unsubscribe5(s->c, topic, 0);
		rc = response.reasonCode;
		MQTTResponse_free(response);
	}
	dprintf(dlevel,"rc: %d\n", rc);
	if (rc == MQTTCLIENT_SUCCESS) mqtt_delsub(s,topic);
	return rc;
}

int mqtt_unsubmany(mqtt_session_t *s, int count, char **topic) {
	int rc;

	if (!s) return 1;
	dprintf(dlevel,"enabled: %d\n", s->enabled);
	if (!s->enabled) return 0;

	dprintf(dlevel,"s: %p, topic: %p\n", s, topic);
	rc = MQTTClient_unsubscribeMany(s->c, count, topic);
	dprintf(dlevel,"rc: %d\n", rc);
	return rc;
}

void mqtt_add_props(mqtt_session_t *s, config_t *cp, char *name) {
	config_property_t mqtt_props[] = {
		/* name, type, dest, dsize, def, flags, scope, values, labels, units, scale, precision */
		{ "mqtt_enabled", DATA_TYPE_BOOL, &s->enabled, sizeof(s->enabled)-1, "yes", 0, 0, 0, 0, 0, 0, 0 },
		{ "mqtt_uri", DATA_TYPE_STRING, s->uri, sizeof(s->uri)-1, "tcp://localhost:1883", 0, 0, 0, 0, 0, 0, 0 },
		{ "mqtt_clientid", DATA_TYPE_STRING, s->clientid, sizeof(s->clientid)-1, "", 0, 0, 0, 0, 0, 0, 0 },
		{ "mqtt_username", DATA_TYPE_STRING, s->username, sizeof(s->username)-1, "", 0, 0, 0, 0, 0, 0, 0 },
		{ "mqtt_password", DATA_TYPE_STRING, s->password, sizeof(s->password)-1, "", 0, 0, 0, 0, 0, 0, 0 },
		{ 0 }
	};

	if (!s) return;

	dprintf(dlevel,"adding props...\n");
	config_add_props(cp, name, mqtt_props, 0);
#if 0
	/* Add the mqtt props to the instance config */
	{
		char *names[] = { "mqtt_host", "mqtt_port", "mqtt_clientid", "mqtt_username", "mqtt_password", 0 };
		config_section_t *s;
		config_property_t *p;
		int i;

		s = config_get_section(cp, name);
		if (!s) {
			sprintf(s->errmsg,"%s section does not exist?!?", name);
			return;
		}
		for(i=0; names[i]; i++) {
			p = config_section_dup_property(cp, &mqtt_props[i], names[i]);
			if (!p) continue;
			dprintf(dlevel,"p->name: %s\n",p->name);
			config_section_add_property(cp, s, p, CONFIG_FLAG_NOID);
		}
	}
#endif
}

#ifdef JS
#include "jsengine.h"
#include "jsfun.h"

enum MQTT_PROPERTY_ID {
	MQTT_PROPERTY_ID_ENABLED=1,
	MQTT_PROPERTY_ID_URI,
	MQTT_PROPERTY_ID_CLIENTID,
	MQTT_PROPERTY_ID_USERNAME,
	MQTT_PROPERTY_ID_PASSWORD,
	MQTT_PROPERTY_ID_ERRMSG,
	MQTT_PROPERTY_ID_CONNECTED,
	MQTT_PROPERTY_ID_MQ,
};

#include "common.h"
#include "message.h"

static JSBool mqtt_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	mqtt_session_t *s;
	int prop_id;

	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx, "private is null!");
		return JS_FALSE;
	}

	dprintf(dlevel,"is_int: %d\n", JSVAL_IS_INT(id));
	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	if (JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case MQTT_PROPERTY_ID_ENABLED:
			*rval = type_to_jsval(cx,DATA_TYPE_BOOL,&s->enabled,0);
			break;
		case MQTT_PROPERTY_ID_URI:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,s->uri,strlen(s->uri));
			break;
		case MQTT_PROPERTY_ID_CLIENTID:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,s->clientid,strlen(s->clientid));
			break;
		case MQTT_PROPERTY_ID_USERNAME:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,s->username,strlen(s->username));
			break;
		case MQTT_PROPERTY_ID_PASSWORD:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,s->password,strlen(s->password));
			break;
		case MQTT_PROPERTY_ID_ERRMSG:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,s->errmsg,strlen(s->errmsg));
			break;
		case MQTT_PROPERTY_ID_CONNECTED:
			*rval = type_to_jsval(cx,DATA_TYPE_BOOL,&s->connected,0);
			break;
		case MQTT_PROPERTY_ID_MQ:
			if (s->ctor)
				*rval = OBJECT_TO_JSVAL(js_create_messages_array(cx, obj, s->mq));
			else
				*rval = JSVAL_VOID;
			break;
		}
	}
	return JS_TRUE;
}

static JSBool mqtt_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	mqtt_session_t *s;
	int prop_id;

	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx, "private is null!");
		return JS_FALSE;
	}
	if (JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"mqtt_setprop: prop_id: %d\n", prop_id);
		switch(prop_id) {
		case MQTT_PROPERTY_ID_ENABLED:
			jsval_to_type(DATA_TYPE_BOOL,&s->enabled,0,cx,*vp);
			break;
		case MQTT_PROPERTY_ID_URI:
			jsval_to_type(DATA_TYPE_STRING,&s->uri,sizeof(s->uri),cx,*vp);
			break;
		case MQTT_PROPERTY_ID_CLIENTID:
			jsval_to_type(DATA_TYPE_STRING,&s->clientid,sizeof(s->clientid),cx,*vp);
			break;
		case MQTT_PROPERTY_ID_USERNAME:
			jsval_to_type(DATA_TYPE_STRING,&s->username,sizeof(s->username),cx,*vp);
			break;
		case MQTT_PROPERTY_ID_PASSWORD:
			jsval_to_type(DATA_TYPE_STRING,&s->password,sizeof(s->password),cx,*vp);
			break;
		}
	}
	return JS_TRUE;
}

static JSClass js_mqtt_class = {
	"MQTT",			/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	mqtt_getprop,		/* getProperty */
	mqtt_setprop,		/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};


static JSBool js_mqtt_pub(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	char *topic,*message;
	int retain;
	mqtt_session_t *s;

	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx, "private is null!");
		return JS_FALSE;
	}
	dprintf(dlevel,"argc: %d\n", argc);
	if (argc < 2) {
		JS_ReportError(cx, "mqtt.pub requires 2 arguments: topic (string), message (striing)");
		return JS_FALSE;
	}
	topic = message = 0;
	retain = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "s s / i", &topic, &message, &retain)) return JS_FALSE;
	dprintf(dlevel,"topic: %s, message: %s, retain: %d\n", topic, message, retain);

        mqtt_pub(s,topic,message,1,retain);
	JS_free(cx,topic);
	JS_free(cx,message);
    	return JS_TRUE;
}

static JSBool js_mqtt_sub(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	char *topic;
	mqtt_session_t *s;

	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx, "private is null!");
		return JS_FALSE;
	}
	dprintf(dlevel,"argc: %d\n", argc);
	if (argc < 1) {
		JS_ReportError(cx, "mqtt.pub requires 1 argument: topic (string)");
		return JS_FALSE;
	}
	topic = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "s", &topic)) return JS_FALSE;
	dprintf(dlevel,"topic: %s\n", topic);

	mqtt_sub(s,topic);
	JS_free(cx,topic);
    	return JS_TRUE;
}


static JSBool js_mqtt_rc(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	mqtt_session_t *s;

	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx, "private is null!");
		return JS_FALSE;
	}
	mqtt_reconnect(s);
	return JS_TRUE;
}

static JSBool js_mqtt_dc(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	mqtt_session_t *s;

	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx, "private is null!");
		return JS_FALSE;
	}
	mqtt_disconnect(s,1);
	return JS_TRUE;
}

static JSBool js_mqtt_con(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	mqtt_session_t *s;

	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx, "private is null!");
		return JS_FALSE;
	}
	if (!s->connected) mqtt_connect(s,20);
	return JS_TRUE;
}

static JSBool js_mqtt_rs(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	mqtt_session_t *s;

	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx, "private is null!");
		return JS_FALSE;
	}
	mqtt_resub(s);
	return JS_TRUE;
}

struct _getmsg_ctx {
	mqtt_session_t *s;
	JSContext *cx;
	jsval func;
	jsval arg;
};

/*  XXX we cant call a javascript func from the message handler
    because it runs in a different thread and our JS engine
    cant handle that yet - rework this when it can
*/
#if 1
static void _js_mqtt_getmsg(void *_ctx, char *topic, char *message, int msglen, char *replyto) {
	struct _getmsg_ctx *ctx = _ctx;
	solard_message_t newmsg;

	dprintf(dlevel,"topic: %s\n", topic);
	if (solard_getmsg(&newmsg,topic,message,msglen,replyto)) return;
//	solard_message_dump(&newmsg,0);
	list_add(ctx->s->mq,&newmsg,sizeof(newmsg));
}
#else
static void _js_mqtt_getmsg(void *_ctx, char *topic, char *message, int msglen, char *replyto) {
	struct _getmsg_ctx *ctx = _ctx;
        jsval argv[4];
	jsval rval;
	JSBool ok;

//	dprintf(0,"topic: %s, message: %s, replyto: %s\n", topic, message, replyto);

        argv[0] = (ctx->arg ? ctx->arg : JSVAL_VOID);
        argv[1] = type_to_jsval(ctx->cx, DATA_TYPE_STRING, topic, strlen(topic));
        argv[2] = type_to_jsval(ctx->cx, DATA_TYPE_STRING, topic, strlen(topic));
        argv[3] = type_to_jsval(ctx->cx, DATA_TYPE_STRING, topic, strlen(topic));

	/* XXX doesnt work */
	JS_SetContextThread(ctx->cx);
	JS_BeginRequest(ctx->cx);

	dprintf(0,"func: %x\n", ctx->func);
        ok = JS_CallFunctionValue(ctx->cx, JS_GetGlobalObject(ctx->cx), ctx->func, 4, argv, &rval);
        dprintf(dlevel,"call ok: %d\n", ok);

	JS_EndRequest(ctx->cx);
	JS_ClearContextThread(ctx->cx);

	dprintf(0,"done!\n");
	return;
}
#endif

static JSBool js_mqtt_purgemq(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	mqtt_session_t *s;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	s = JS_GetPrivate(cx, obj);
	if (!s) {
		JS_ReportError(cx,"js_mqtt_purgemq: mqtt private is null!\n");
		return JS_FALSE;
	}
	list_purge(s->mq);
	return JS_TRUE;
}

static JSBool js_mqtt_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	char *uri;
//	jsval func,arg;
	JSPropertySpec ctor_props[] = { 
		{ "mq", MQTT_PROPERTY_ID_MQ, JSPROP_ENUMERATE | JSPROP_READONLY },
		{0}
	};
	JSFunctionSpec ctor_funcs[] = {
		JS_FN("purgemq",js_mqtt_purgemq,0,0,0),
		{ 0 }
	};
	struct _getmsg_ctx *ctx;
	JSObject *newobj;

	uri = 0;
#if 1
	if (!JS_ConvertArguments(cx, argc, argv, "/ s", &uri)) return JS_FALSE;
	dprintf(dlevel,"uri: %s\n", uri);
#else
	func = arg = JSVAL_NULL;
	if (!JS_ConvertArguments(cx, argc, argv, "s v / v", &uri, &func, &arg)) return JS_FALSE;
	dprintf(dlevel,"uri: %s, func: %x (is func: %d), arg: %x\n", uri, func, VALUE_IS_FUNCTION(cx,func), arg);
	if (!VALUE_IS_FUNCTION(cx,func)) {
		JS_ReportError(cx, "js_mqtt_ctor: argument 2 is not a function");
		return JS_FALSE;
	}
#endif

	ctx = malloc(sizeof(*ctx));
	dprintf(dlevel,"ctx: %p\n", ctx);
	if (!ctx) {
		JS_ReportError(cx, "js_mqtt_ctor: malloc ctx");
		return JS_FALSE;
	}
	ctx->cx = cx;
//	ctx->func = func;
//	ctx->arg = arg;
	if (!js_ctxs) js_ctxs = list_create();
	list_add(js_ctxs,ctx,0);

	ctx->s = mqtt_new(false, _js_mqtt_getmsg, ctx);
	dprintf(dlevel,"s: %p\n", ctx->s);
	if (!ctx->s) {
		JS_ReportError(cx, "js_mqtt_ctor: mqtt_new return null!");
		return JS_FALSE;
	}
	ctx->s->ctor = true;
	mqtt_parse_config(ctx->s,uri ? uri : "localhost");
	if (uri) JS_free(cx,uri);
	ctx->s->enabled = true;
	mqtt_newclient(ctx->s);
	mqtt_connect(ctx->s,20);

	newobj = js_mqtt_new(cx, obj, ctx->s);
	dprintf(dlevel,"newobj: %p\n", newobj);
	JS_DefineProperties(cx, newobj, ctor_props);
	JS_DefineFunctions(cx, newobj, ctor_funcs);
	JS_SetPrivate(cx,newobj,ctx->s);
	*rval = OBJECT_TO_JSVAL(newobj);

	dprintf(dlevel,"done!\n");
	return JS_TRUE;
}

JSObject * js_InitMQTTClass(JSContext *cx, JSObject *global_object) {
	JSPropertySpec mqtt_props[] = { 
		{ "enabled",		MQTT_PROPERTY_ID_ENABLED,	JSPROP_ENUMERATE },
		{ "uri",		MQTT_PROPERTY_ID_URI,		JSPROP_ENUMERATE },
		{ "clientid",		MQTT_PROPERTY_ID_CLIENTID,	JSPROP_ENUMERATE },
		{ "username",		MQTT_PROPERTY_ID_USERNAME,	JSPROP_ENUMERATE },
		{ "password",		MQTT_PROPERTY_ID_PASSWORD,	JSPROP_ENUMERATE },
		{ "errmsg",		MQTT_PROPERTY_ID_ERRMSG,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "connected",		MQTT_PROPERTY_ID_CONNECTED,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "mq",			MQTT_PROPERTY_ID_MQ,		JSPROP_ENUMERATE | JSPROP_READONLY },
		{0}
	};
	JSFunctionSpec mqtt_funcs[] = {
		{ "pub",js_mqtt_pub,2 },
		{ "sub",js_mqtt_sub,1 },
		{ "disconnect",js_mqtt_dc,1 },
		{ "connect",js_mqtt_con,1 },
		{ "reconnect",js_mqtt_rc,1 },
		{ "resub",js_mqtt_rs,1 },
		{ 0 }
	};
	JSObject *obj;

	dprintf(2,"Creating %s class...\n", js_mqtt_class.name);
	obj = JS_InitClass(cx, global_object, 0, &js_mqtt_class, js_mqtt_ctor, 1, mqtt_props, mqtt_funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize mqtt class");
		return 0;
	}
//	dprintf(dlevel,"done!\n");
	return obj;
}

JSObject *js_mqtt_new(JSContext *cx, JSObject *parent, mqtt_session_t *s) {
	JSObject *newobj;

	/* Create the new object */
	dprintf(2,"Creating %s object...\n", js_mqtt_class.name);
	newobj = JS_NewObject(cx, &js_mqtt_class, 0, parent);
	if (!newobj) return 0;

	JS_SetPrivate(cx,newobj,s);

	dprintf(dlevel,"newobj: %p\n", newobj);
	return newobj;
}
#endif
#endif
