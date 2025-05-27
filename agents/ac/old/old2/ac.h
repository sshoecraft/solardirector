
#ifndef __AC_H
#define __AC_H

#include "agent.h"
#include "can.h"
#include "wpi.h"

struct ac_session {
	solard_agent_t *ap;
#if 0
	uint16_t state;
	solard_agent_t *ap;
#ifdef JS
        JSPropertySpec *props;
        JSFunctionSpec *funcs;
        jsval agent_val;
#endif
//	char errmsg[128];
	pthread_t th;
	/* Can stuff */
        solard_driver_t *can;
        void *can_handle;
	time_t times[16];
        char can_target[SOLARD_TARGET_LEN];
        char can_topts[SOLARD_TOPTS_LEN];
        struct can_frame frames[16];
#endif
};
typedef struct ac_session ac_session_t;

#define AC_CAN_OPEN		0x0001
#define AC_CAN_RUN		0x0002

extern solard_driver_t ac_driver;

int ac_can_init(ac_session_t *s);
int can_read(ac_session_t *s, uint32_t id, uint8_t *data, int data_len);
int can_write(ac_session_t *s, uint32_t id, uint8_t *data, int data_len);

#ifdef JS
int jsinit(ac_session_t *s);
JSBool js_can_read(JSContext *cx, uintN argc, jsval *vp);
JSBool js_can_write(JSContext *cx, uintN argc, jsval *vp);
#endif

#endif
