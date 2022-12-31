
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __BTC_H
#define __BTC_H

#include "agent.h"
#include "battery.h"
#include "client.h"

struct btc_session {
	solard_agent_t *ap;
	solard_client_t *c;
	int interval;
	list bats;
	float last_power;
	bool log_power;
};
typedef struct btc_session btc_session_t;

extern solard_driver_t btc_driver;

#endif
