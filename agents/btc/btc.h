
#ifndef __BTC_H
#define __BTC_H

#include "agent.h"
#include "battery.h"

struct btc_session {
	solard_agent_t *ap;
	char data_source[128];
	mqtt_session_t *m;
	bool log_power;
	int last_power;
	list agents;
};
typedef struct btc_session btc_session_t;

struct _btc_agentinfo {
	char name[SOLARD_NAME_LEN];
	char role[SOLARD_ROLE_LEN];
	bool have_data;
	solard_battery_t data;
};
typedef struct _btc_agentinfo btc_agentinfo_t;

extern solard_driver_t btc_driver;

#endif
