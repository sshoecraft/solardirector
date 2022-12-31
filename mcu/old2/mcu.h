
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

struct mcu_agent {
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
typedef struct mcu_agent mcu_agent_t;

#define AGENTINFO_STATUS_STARTED 0x01		/* Agent has started */
#define AGENTINFO_STATUS_WARNED 0x02		/* Agent has not reported */
#define AGENTINFO_STATUS_ERROR 0x04		/* Agent is gone */
#define AGENTINFO_STATUS_MASK 0x0F
#define AGENTINFO_NOTIFY_GONE 0x10		/* Agent has not reported since XXX */

struct mcu_session {
	solard_agent_t *ap;
	solard_client_t *c;
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
typedef struct mcu_session mcu_session_t;

extern solard_driver_t sd_driver;

#if 0
/* Name used in config props */
#define SD_SECTION_NAME "solard"

int mcu_agent_init(int argc, char **argv, opt_proctab_t *sd_opts, mcu_session_t *sd);
json_value_t *sd_get_info(void *handle);

/* agents */
int agent_get_config(mcu_session_t *sd, mcu_agent_t *info);
int agent_set_config(mcu_session_t *sd, mcu_agent_t *info);
int agent_start(mcu_session_t *sd, mcu_agent_t *info);
int agent_stop(mcu_session_t *sd, mcu_agent_t *info);
void agent_check(mcu_session_t *sd, mcu_agent_t *info);

/* config */
int mcu_config(void *h, int req, ...);
int mcu_read_config(mcu_session_t *sd);
int mcu_write_config(mcu_session_t *sd);

#if 0
int mcu_send_status(mcu_session_t *sd, char *func, char *action, char *id, int status, char *message);
int mcu_add_config(mcu_session_t *sd, char *label, char *value, char *errmsg);

//void getinv(mcu_session_t *sd, client_agentinfo_t *ap);
//void getpack(mcu_session_t *sd, client_agentinfo_t *ap);

#if 0
/* agents */
mcu_agent_t *agent_find(mcu_session_t *sd, char *name);
int agent_get_role(mcu_session_t *sd, mcu_agent_t *info);
void add_agent(mcu_session_t *sd, char *role, json_value_t *v);
void mcu_monitor(mcu_session_t *sd);

/* agentinfo */
json_value_t *agentinfo_to_json(mcu_agent_t *info);
void agentinfo_pub(mcu_session_t *sd, mcu_agent_t *info);
void agentinfo_getcfg(cfg_info_t *cfg, char *sname, mcu_agent_t *info);
void agentinfo_setcfg(cfg_info_t *cfg, char *sname, mcu_agent_t *info);
int agentinfo_set(mcu_agent_t *, char *, char *);
mcu_agent_t *agentinfo_add(mcu_session_t *sd, mcu_agent_t *info);
int agentinfo_get(mcu_session_t *sd, char *);
void agentinfo_newid(mcu_agent_t *info);
#endif
#endif
#endif

#ifdef JS
int mcu_jsinit(mcu_session_t *);
#endif

#endif
