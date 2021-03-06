
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __SD_MESSAGE_H
#define __SD_MESSAGE_H

#include "mqtt.h"

enum SOLARD_MESSAGE_TYPE {
	SOLARD_MESSAGE_DATA,
	SOLARD_MESSAGE_STATUS,
};

/* Request message format is:
	Root (SolarD)
	Role (Controller/Inverter/Battery)
	Agent (pack_01/si/etc)
	Type (Config/Status/etc)
	Action  (Get/Set/Add/Del)
	Id	(Client ID)
*/

/* Reply message format is:
	root (SolarD)
	Role (Controller/Inverter/Battery)
	Name (pack_01/si/etc)
	Type (Data/Config/Info/etc)
	Action  (Get/Set/Add/Del)
	Response (Status)
	Id	(Client ID)
*/

struct solard_message {
	enum SOLARD_MESSAGE_TYPE type;
	char role[32];
	char name[64];
	char func[32];
	union {
		char action[32];
		char param[32];
	};
	char id[64];
	union {
		char data[262144];
		int status;
		char message[256];
	};
	int data_len;
};
typedef struct solard_message solard_message_t;

void solard_getmsg(void *, char *, char *, int);
void solard_ingest(list, int);
void solard_message_dump(solard_message_t *,int);

#endif
