
#ifndef __PA_H
#define __PA_H

#include "agent.h"

struct pa_session {
	solard_agent_t *ap;
#ifdef JS
    JSPropertySpec *props;
    JSFunctionSpec *funcs;
    jsval agent_val;
#endif
//	char errmsg[128];
};
typedef struct pa_session pa_session_t;

extern solard_driver_t pa_driver;

#ifdef JS
int jsinit(pa_session_t *s);
#endif

#endif
