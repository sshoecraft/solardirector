
#ifndef __SC_H
#define __SC_H

#include "agent.h"

struct sc_session {
	solard_agent_t *ap;
#ifdef JS
        JSPropertySpec *props;
        JSFunctionSpec *funcs;
        jsval agent_val;
#endif
};
typedef struct sc_session sc_session_t;

extern solard_driver_t sc_driver;

json_value_t *sc_get_info(sc_session_t *s);

#ifdef JS
int jsinit(sc_session_t *s);
#endif

#endif
