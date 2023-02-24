
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "agent.h"

#define TESTING 0
#define TESTLVL 3

struct dummy_session {
	solard_agent_t *ap;
};
typedef struct dummy_session dummy_session_t;

#ifdef JS
int dummy_jsinit(JSContext *cx, JSObject *parent, void *ctx);
#endif

int dummy_config(void *h, int req, ...) {
	dummy_session_t *s = h;
	va_list va;

	va_start(va,req);
	dprintf(1,"req: %d\n", req);
	switch(req) {
	case SOLARD_CONFIG_INIT:
		s->ap = va_arg(va,solard_agent_t *);
#ifdef JS
		if (s->ap->js.e) JS_EngineAddInitFunc(s->ap->js.e,"dummy_jsinit",dummy_jsinit,s);
#endif
		break;
	case SOLARD_CONFIG_GET_INFO:
		{
			json_value_t **vpp = va_arg(va,json_value_t **);
			dprintf(1,"vpp: %p\n", vpp);
			if (vpp) {
				json_object_t *o;
				o = json_create_object();
				if (s->ap->cp && o) config_add_info(s->ap->cp,o);
				*vpp = json_object_value(o);
			}
		}
		break;
	}
	return 0;
}

int set_parm1(void *ctx, config_property_t *p, void *old_value) {
	printf("set!\n");
	return 0;
}

int main(int argc, char **argv) {
	solard_driver_t dummy;
	dummy_session_t *s;
#if TESTING
        char *args[] = { "dummy", "-d", STRINGIFY(TESTLVL)  };
        argc = (sizeof(args)/sizeof(char *));
        argv = args;
#endif
	char parm1[256];
	int parm2;
	float parm3;
	config_property_t dummy_props[] = {
		{ "parm1", DATA_TYPE_STRING, parm1, sizeof(parm1)-1, 0, 0,
                        0, 0, 0, 0, 0, 1, set_parm1, 0 },
		{ "parm2", DATA_TYPE_INT, &parm2, 0, "2" },
		{ "parm3", DATA_TYPE_FLOAT, &parm3, 0, "4.5" },
		{ 0 }
	};

	memset(&dummy,0,sizeof(dummy));
	dummy.name = "dummy";
	dummy.config = dummy_config;
	s = calloc(sizeof(*s),1);
	s->ap = agent_init(argc,argv,"1.0",0,&dummy,s,0,dummy_props,0);
	if (!s->ap) return 1;
#ifdef JS
	strcpy(s->ap->js.script_dir,".");
#endif
	s->ap->interval = 5;
	agent_run(s->ap);
	agent_shutdown();
	free(s);
	return 0;
}

#ifdef JS
#include "jsobj.h"
int dummy_jsinit(JSContext *cx, JSObject *parent, void *ctx) {
	dummy_session_t *s = ctx;

	JS_DefineProperty(cx, parent, "agent", OBJECT_TO_JSVAL(js_agent_new(cx,parent,s->ap)), 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "config", s->ap->js.config_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "mqtt", s->ap->js.mqtt_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "influx", s->ap->js.influx_val, 0, 0, JSPROP_ENUMERATE);
	return 0;
}
#endif
