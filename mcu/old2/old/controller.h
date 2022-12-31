
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __SD_CONTROLLER_H
#define __SD_CONTROLLER_H

#include "agent.h"

struct solard_controller {
	char name[SOLARD_NAME_LEN];	/* Controller name */
	int errcode;			/* Controller status, updated by agent */
	char errmsg[256];		/* Error message if status !0, updated by agent */
	uint16_t state;			/* Controller state */
	time_t last_update;		/* Set by client */
};
typedef struct solard_controller solard_controller_t;

/* Controller states */
#define CONTROLLER_STATE_UPDATED	0x01
#define CONTROLLER_STATE_OPEN		0x10

void controller_dump(solard_controller_t *,int);
int controller_from_json(solard_controller_t *bp, char *str);
json_value_t *controller_to_json(solard_controller_t *bp);

#endif
