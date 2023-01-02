
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __SD_MESSAGE_H
#define __SD_MESSAGE_H

#include "mqtt.h"

/* message format is:
	Root (SolarD)
	Role (Controller/Inverter/Battery)
	Agent (pack_01/si/etc)
	Func (Config/Data/etc)
*/

#define SOLARD_MAX_PAYLOAD_SIZE 262144

struct solard_message {
	char topic[SOLARD_TOPIC_SIZE];			/* orig topic */
	int type;					/* 0 = agent, 1 = client */
	union {
		char name[SOLARD_NAME_LEN];		/* if agent, name */
		char id[SOLARD_ID_LEN];			/* if client, id */
	};
	char func[SOLARD_FUNC_LEN];			/* agent func, if any */
	char replyto[SOLARD_ID_LEN];			/* MQTT5 replyto addr */
	char data[SOLARD_MAX_PAYLOAD_SIZE];		/* message data */
	int size;					/* message size (strlen(data) */
};
typedef struct solard_message solard_message_t;

int solard_getmsg(solard_message_t *, char *, char *, int, char *);
void solard_message_dump(solard_message_t *,int);
int solard_message_wait(list, int);
int solard_message_delete(list, solard_message_t *);

typedef int (solard_msghandler_t)(void *,solard_message_t *);

solard_message_t *solard_message_wait_id(list lp, char *id, int timeout);
solard_message_t *solard_message_wait_target(list lp, char *target, int timeout);

#ifdef JS
JSObject *js_create_messages_array(JSContext *cx, JSObject *parent, list l);
JSObject *js_message_new(JSContext *cx, JSObject *parent, solard_message_t *msg);
#endif
#endif
