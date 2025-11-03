#ifdef JS

/*
Copyright (c) 2024, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/


#define dlevel 4
#include "debug.h"

#include "ac.h"
#include "can.h"
#include "jsobj.h"
#include "jsfun.h"
#include "jsstr.h"
#include "jsprintf.h"

/*************************************************************************/

#if 0
enum AC_PROPERTY_IDS {
	AC_PROPERTY_ID_INFO=1024,
	AC_PROPERTY_ID_AGENT,
	AC_PROPERTY_ID_CAN,
};
#endif

static JSBool getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	ac_session_t *s;
	config_property_t *p;

	dprintf(dlevel,"obj: %p\n", obj);
	s = JS_GetPrivate(cx,obj);
	dprintf(dlevel,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx,"private is null!");
		return JS_FALSE;
	}

	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	p = 0;
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
#if 0
		case AC_PROPERTY_ID_INFO:
			dprintf(dlevel,"getting info..\n");
			*rval = json2jsval(s->ap->info, cx);
			break;
		case AC_PROPERTY_ID_AGENT:
			dprintf(dlevel,"getting agent..\n");
			*rval = s->agent_val;
			break;
		case AC_PROPERTY_ID_CAN:
			*rval = s->can_val;
			break;
#endif
		default:
			p = CONFIG_GETMAP(s->ap->cp,prop_id);
			dprintf(dlevel,"p: %p\n", p);
			if (!p) p = config_get_propbyid(s->ap->cp,prop_id);
			dprintf(dlevel,"p: %p\n", p);
			if (!p) {
				JS_ReportError(cx, "internal error: property %d not found", prop_id);
				return JS_FALSE;
			}
			break;
		}
	} else if (JSVAL_IS_STRING(id)) {
		char *name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
		dprintf(dlevel,"name: %s\n", name);
		if (name) {
			p = config_get_property(s->ap->cp, s->ap->driver->name, name);
			JS_free(cx,name);
		}
	}
	dprintf(dlevel,"p: %p\n", p);
	if (p && p->dest) {
//		config_dump_property(p,0);
		dprintf(dlevel,"p: type: %d(%s), name: %s\n", p->type, typestr(p->type), p->name);
		*rval = type_to_jsval(cx,p->type,p->dest,p->len);
	}
	return JS_TRUE;
}

static JSBool setprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	ac_session_t *s;

	s = JS_GetPrivate(cx,obj);
	if (!s) {
		JS_ReportError(cx,"setprop: private is null!");
		return JS_FALSE;
	}
	return js_config_common_setprop(cx, obj, id, rval, s->ap->cp, 0);
}

//	JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,	/* Flags */
static JSClass myclass = {
	"ac",			/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	getprop,		/* getProperty */
	setprop,		/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool callfunc(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	ac_session_t *s;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) {
		JS_ReportError(cx, "callfunc: internal error: object is null!\n");
		return JS_FALSE;
	}
	s = JS_GetPrivate(cx, obj);
	if (!s) {
		JS_ReportError(cx, "callfunc: internal error: private is null!\n");
		return JS_FALSE;
	}
	if (!s->ap) return JS_TRUE;
	if (!s->ap->cp) return JS_TRUE;

	return js_config_callfunc(s->ap->cp, cx, argc, vp);
}

static JSBool js_ac_event(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	ac_session_t *s;
	char *module,*action;

	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx,"ac private is null!\n");
		return JS_FALSE;
	}
	module = action = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "s s", &module, &action)) return JS_FALSE;
	agent_event(s->ap,module,action);
	if (module) JS_free(cx,module);
	if (action) JS_free(cx,action);
	return JS_TRUE;
}

/* in case of testing with no wpi */
#ifndef HIGH
#define HIGH 1
#endif
#ifndef LOW
#define LOW 0
#endif

static int js_init(JSContext *cx, JSObject *parent, void *priv) {
	ac_session_t *s = priv;
	JSPropertySpec props[] = {
#if 0
		{ "info", AC_PROPERTY_ID_INFO, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "agent", AC_PROPERTY_ID_AGENT, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "can", AC_PROPERTY_ID_CAN, JSPROP_ENUMERATE | JSPROP_READONLY },
#endif
		{ 0 }
	};
	JSFunctionSpec funcs[] = {
		JS_FN("can_read",js_can_read,1,1,0),
		JS_FN("can_write",js_can_write,2,2,0),
		JS_FS("signal",js_ac_event,2,2,0),
		{ 0 }
	};
	JSAliasSpec aliases[] = {
		{ 0 }
	};
	JSConstantSpec constants[] = {
		JS_NUMCONST(HIGH),
		JS_NUMCONST(LOW),
		{ 0 }
	};
	JSObject *newobj;

	dprintf(dlevel,"parent: %p\n", parent);
	dprintf(dlevel,"s->props: %p, cp: %p\n",s->props,s->ap->cp);
	if (s->ap && s->ap->cp) {
		if (!s->props) {
			s->props = js_config_to_props(s->ap->cp, cx, ac_driver.name, props);
			dprintf(dlevel,"s->props: %p\n",s->props);
			if (!s->props) {
				log_error("unable to create props: %s\n", config_get_errmsg(s->ap->cp));
				return 0;
			}
		}
		if (!s->funcs) {
			s->funcs = js_config_to_funcs(s->ap->cp, cx, callfunc, funcs);
			dprintf(dlevel,"s->funcs: %p\n",s->funcs);
			if (!s->funcs) {
				log_error("unable to create funcs: %s\n", config_get_errmsg(s->ap->cp));
				return 0;
			}
		}
	}
	dprintf(dlevel,"s->props: %p, s->funcs: %p\n", s->props, s->funcs);
	if (!s->props) s->props = props;
	if (!s->funcs) s->funcs = funcs;

	dprintf(dlevel,"Defining %s object\n",myclass.name);
	newobj = JS_InitClass(cx, parent, 0, &myclass, 0, 0, s->props, s->funcs, 0, 0);
	if (!newobj) {
		JS_ReportError(cx,"unable to initialize %s class", myclass.name);
		return 1;
	}
	JS_SetPrivate(cx,newobj,s);

	dprintf(dlevel,"Defining %s aliases\n",myclass.name);
	if (!JS_DefineAliases(cx, newobj, aliases)) {
		JS_ReportError(cx,"unable to define aliases");
		return 1;
	}
	dprintf(dlevel,"Defining global constants\n");
	if (!JS_DefineConstants(cx, parent, constants)) {
		JS_ReportError(cx,"unable to define constants");
		return 1;
	}

//	JS_DefineProperty(cx, parent, ac_driver.name, OBJECT_TO_JSVAL(newobj), 0, 0, JSPROP_ENUMERATE);

	s->agent_val = OBJECT_TO_JSVAL(js_agent_new(cx,newobj,s->ap));
	s->can_val = OBJECT_TO_JSVAL(js_can_new(cx,newobj,s->can_handle));

	/* Create the standard child objects */
	JS_DefineProperty(cx, newobj, "agent", s->agent_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, newobj, "config", s->ap->js.config_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, newobj, "mqtt", s->ap->js.mqtt_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, newobj, "influx", s->ap->js.influx_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, newobj, "event", s->ap->js.event_val, 0, 0, JSPROP_ENUMERATE);

	/* Create the global convenience objects */
	JS_DefineProperty(cx, parent, "ac", OBJECT_TO_JSVAL(newobj), 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "can", s->can_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "agent", s->agent_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "config", s->ap->js.config_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "mqtt", s->ap->js.mqtt_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "influx", s->ap->js.influx_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "event", s->ap->js.event_val, 0, 0, JSPROP_ENUMERATE);


#if 0
	dprintf(dlevel,"defining global objects\n");
	s->agent_val = OBJECT_TO_JSVAL(js_agent_new(cx,newobj,s->ap));
	JS_DefineProperty(cx, parent, "agent", s->agent_val, 0, 0, JSPROP_ENUMERATE);

	dprintf(dlevel,"defining std objects\n");
	JS_DefineProperty(cx, parent, "config", s->ap->js.config_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "mqtt", s->ap->js.mqtt_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "influx", s->ap->js.influx_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "event", s->ap->js.event_val, 0, 0, JSPROP_ENUMERATE);
#endif

	dprintf(dlevel,"done!\n");
	return 0;
}

int jsinit(ac_session_t *s) {
	int r;

	dprintf(dlevel,"s->ap->js.e: %p\n", s->ap->js.e);

	/* Initialize WiringPi JavaScript bindings */
	dprintf(dlevel,"Calling wpi_init...\n");
	r = wpi_init(s->ap->js.e);
	dprintf(dlevel,"wpi_init returned: %d\n", r);

	JS_EngineAddInitFunc(s->ap->js.e, ac_driver.name, js_init, s);
	return 0;
}
#endif
