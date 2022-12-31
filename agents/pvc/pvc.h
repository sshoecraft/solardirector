
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __PVC_H
#define __PVC_H

#include "agent.h"
#include "pvinverter.h"
#include "client.h"

struct pvc_session {
	solard_agent_t *ap;
	solard_client_t *c;
	int interval;
	list pvs;
	float last_power;
	bool log_power;
};
typedef struct pvc_session pvc_session_t;

extern solard_driver_t pvc_driver;

#endif
