/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __SC_H
#define __SC_H

#include "agent.h"
#include <dirent.h>
#include <sys/stat.h>

/* SC session structure */
struct sc_session {
	solard_agent_t *ap;			/* Agent instance */
};
typedef struct sc_session sc_session_t;

#ifdef JS
/* JavaScript function initialization */
int sc_jsfunc_init(void *js_engine);
#endif

#endif /* __SC_H */