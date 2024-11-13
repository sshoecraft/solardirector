
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __MCU_H
#define __MCU_H

#include "agent.h"

struct sc_session {
	solard_agent_t *ap;
#if 0
	char location[32];			/* lat/long */
	/* global agent vals */
	int agent_warning;			/* Warning, in seconds, when agent does not send update */
	int agent_error;			/* In seconds, when agent considered lost */
	int agent_notify;			/* In seconds, when monitoring should notify */
	bool notify;				/* Notify enable/disable */
	char notify_path[256];			/* Notify program path */
	double battery_temp_min;
	double battery_temp_max;
#endif
#ifdef JS
	JSPropertySpec *props;
	JSFunctionSpec *funcs;
	jsval agent_val;
	jsval client_val;
#endif
};
typedef struct sc_session sc_session_t;

extern solard_driver_t sc_driver;

int sc_agent_init(int argc, char **argv, opt_proctab_t *sd_opts, sc_session_t *sc);
int sc_config(void *h, int req, ...);

#ifdef JS
int sc_jsinit(sc_session_t *);
#endif

#endif
