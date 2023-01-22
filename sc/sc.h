
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __MCU_H
#define __MCU_H

#include "agent.h"
#include "client.h"
#include "battery.h"
//#include "inverter.h"

struct sc_agent {
	bool managed;
	char name[SOLARD_NAME_LEN];		/* Agent name */
	char path[256];				/* Full path to agent */
	char conf[256];				/* Full path to config file */
	char log[256];
	bool append;
	bool monitor_topic;
	char topic[SOLARD_TOPIC_SIZE];
	int action;				/* What action to take if the agent stops reporting */
	int notify;				/* Notify if the agent stops reporting */
	bool enabled;
	pid_t pid;
	time_t started;
	time_t updated;
	solard_agent_t *ap;			/* From client */
};
typedef struct sc_agent sc_agent_t;

#define SC_AGENT_STATUS_STARTED 0x01		/* Agent has started */
#define SC_AGENT_STATUS_WARNED 0x02		/* Agent has not reported */
#define SC_AGENT_STATUS_ERROR 0x04		/* Agent is gone */
#define SC_AGENT_STATUS_GONE 0x10		/* Agent has not reported since XXX */

struct sc_session {
	solard_agent_t *ap;
	solard_client_t *c;
	char location[32];
	list names;
	list agents;
	int state;
	int interval;				/* Agent check interval */
	int agent_warning;			/* Warning, in seconds, when agent dosnt respond */
	int agent_error;			/* In seconds, when agent considered lost */
	int agent_notify;			/* In seconds, when monitoring should notify */
	int status;
	char errmsg[128];
	time_t last_check;			/* Last time agents were checked */
	long start;
	char notify_path[256];
#ifdef JS
	JSPropertySpec *props;
	JSFunctionSpec *funcs;
#endif
	int next_num;
};
typedef struct sc_session sc_session_t;

extern solard_driver_t sc_driver;

int sc_agent_init(int argc, char **argv, opt_proctab_t *sd_opts, sc_session_t *sd);
int sc_config(void *h, int req, ...);

#ifdef JS
int sc_jsinit(sc_session_t *);
#endif

#endif
