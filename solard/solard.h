
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __SOLARD_H
#define __SOLARD_H

//#include "client.h"
#include "agent.h"
#include "battery.h"
#include "inverter.h"

struct solard_agentinfo {
	char id[SOLARD_ID_LEN];			/* Unique agent ID */
	char agent[SOLARD_NAME_LEN];		/* Agent name */
	char path[256];				/* Full path to agent */
	char role[SOLARD_ROLE_LEN];		/* Agent role */
	char name[SOLARD_NAME_LEN];		/* Agent instance name */
	char transport[SOLARD_TRANSPORT_LEN];
	char target[SOLARD_TARGET_LEN];
	char topts[SOLARD_TOPTS_LEN];
	int managed;				/* Do we manage this agent (start/stop)? */
	uint16_t state;				/* Agent state */
	pid_t pid;
	time_t started;
};
typedef struct solard_agentinfo solard_agentinfo_t;

#define AGENTINFO_STATUS_STARTED 0x01		/* Agent has started */
#define AGENTINFO_STATUS_WARNED 0x02		/* Agent has not reported */
#define AGENTINFO_STATUS_ERROR 0x04		/* Agent is gone */

struct solard_config {
	char name[SOLARD_NAME_LEN];		/* Site name */
	solard_agent_t *ap;
	list inverters;
	list batteries;
	list producers;
	list consumers;
	list agents;
	int state;
	int interval;				/* Agent check interval */
	int agent_warning;			/* Warning, in seconds, when agent dosnt respond */
	int agent_error;			/* Warning, in seconds, when agent considered lost */
	int status;
	char errmsg[128];
};
typedef struct solard_config solard_config_t;

json_value_t *solard_info(void *handle);

int solard_read_config(solard_config_t *conf);
int solard_write_config(solard_config_t *conf);
int solard_send_status(solard_config_t *conf, char *func, char *action, char *id, int status, char *message);
int solard_config(solard_config_t *conf, solard_message_t *msg);

void getinv(solard_config_t *conf, char *name, char *data);
void getpack(solard_config_t *conf, char *name, char *data);
void solard_setcfg(cfg_info_t *cfg, char *section_name, solard_agentinfo_t *info);

int agent_get_role(solard_config_t *conf, solard_agentinfo_t *info);
void add_agent(solard_config_t *conf, char *role, json_value_t *v);
int check_agents(solard_config_t *conf);

void agentinfo_pub(solard_config_t *conf, solard_agentinfo_t *info);
void agentinfo_getcfg(cfg_info_t *cfg, char *sname, solard_agentinfo_t *info);
void agentinfo_setcfg(cfg_info_t *cfg, char *sname, solard_agentinfo_t *info);
int agentinfo_set(solard_agentinfo_t *, char *, char *);
int agentinfo_add(solard_config_t *conf, solard_agentinfo_t *info);
int agentinfo_get(solard_config_t *conf, char *);
void agentinfo_newid(solard_agentinfo_t *info);

#endif
