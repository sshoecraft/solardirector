
#ifndef __TEMPLATE_H
#define __TEMPLATE_H

#include "agent.h"

struct template_session {
	solard_agent_t *ap;
#ifdef JS
        JSPropertySpec *props;
        JSFunctionSpec *funcs;
        jsval agent_val;
#endif
};
typedef struct template_session template_session_t;

extern solard_driver_t template_driver;

json_value_t *template_get_info(template_session_t *s);

#ifdef JS
int jsinit(template_session_t *s);
#endif

#endif
