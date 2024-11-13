
#ifndef __TEMPLATE_H
#define __TEMPLATE_H

#include "agent.h"

struct template_session {
	int control_1;
	char *control_2;
	uint16_t state;
        char transport[SOLARD_TRANSPORT_LEN];
        char target[SOLARD_TARGET_LEN];
        char topts[SOLARD_TOPTS_LEN];
        solard_driver_t *tp;
        void *tp_handle;
	solard_agent_t *ap;
#ifdef JS
        JSPropertySpec *props;
        JSFunctionSpec *funcs;
        jsval agent_val;
#endif
	char errmsg[128];
};
typedef struct template_session template_session_t;

#define TEMPLATE_STATE_OPEN 0x0001

extern solard_driver_t template_driver;

int template_agent_init(int argc,char **argv,opt_proctab_t *opts,template_session_t *s);
json_value_t *template_get_info(template_session_t *s);
int template_config(void *,int,...);

#ifdef JS
int jsinit(template_session_t *s);
#endif

#endif
