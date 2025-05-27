
#ifndef __PVC_H
#define __PVC_H

#include "agent.h"
#include "pvinverter.h"

struct pvc_session {
	solard_agent_t *ap;
	char data_source[128];
	mqtt_session_t *m;
	bool log_power;
	int last_power;
	list agents;
};
typedef struct pvc_session pvc_session_t;

struct _pvc_agentinfo {
	char name[SOLARD_NAME_LEN];
	char role[SOLARD_ROLE_LEN];
	bool have_data;
	solard_pvinverter_t data;
};
typedef struct _pvc_agentinfo pvc_agentinfo_t;

extern solard_driver_t pvc_driver;

#endif
