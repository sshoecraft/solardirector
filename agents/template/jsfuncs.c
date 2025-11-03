#ifdef JS

/*
Copyright (c) 2024, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/


#define dlevel 4
#include "debug.h"

#include "template.h"
#include "jsobj.h"
#include "jsstr.h"
#include "jsprintf.h"

/*************************************************************************/

enum PROPERTY_IDS {
	PROPERTY_ID_INFO=1024,
};

static JSBool js_template_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	template_session_t *s;
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
		case PROPERTY_ID_INFO:
		    {
			json_value_t *v;

			dprintf(dlevel,"getting info..\n");
			/* Convert from C JSON type to JS JSON type */
			v = template_get_info(s);
			dprintf(dlevel,"v: %p\n", v);
			if (!v) {
				JS_ReportError(cx, "unable to create info\n");
				return JS_FALSE;
			}
			*rval = json2jsval(v, cx);
		    }
		    break;
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
		char *sname, *name;
		JSClass *classp = OBJ_GET_CLASS(cx, obj);

		sname = (char *)classp->name;
		name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
		dprintf(dlevel,"sname: %s, name: %s\n", sname, name);
		if (sname && name) p = config_get_property(s->ap->cp, sname, name);
//		if (!p) dprintf(0,"prop not found: %s\n", name);
		if (name) JS_free(cx,name);
	}
	dprintf(dlevel,"p: %p\n", p);
	if (p && p->dest) {
//		config_dump_property(p,0);
		dprintf(dlevel,"p: type: %d(%s), name: %s\n", p->type, typestr(p->type), p->name);
		*rval = type_to_jsval(cx,p->type,p->dest,p->len);
	}
	return JS_TRUE;
}

static JSBool js_template_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	template_session_t *s;

	s = JS_GetPrivate(cx,obj);
	if (!s) {
		JS_ReportError(cx,"setprop: private is null!");
		return JS_FALSE;
	}
	return js_config_common_setprop(cx, obj, id, rval, s->ap->cp, 0);
}

static JSClass template_class = {
	"template",		/* Name */
	JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	js_template_getprop,		/* getProperty */
	js_template_setprop,		/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool js_template_callfunc(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	template_session_t *s;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) {
		JS_ReportError(cx, "js_template_callfunc: internal error: object is null!\n");
		return JS_FALSE;
	}
	s = JS_GetPrivate(cx, obj);
	if (!s) {
		JS_ReportError(cx, "js_template_callfunc: internal error: private is null!\n");
		return JS_FALSE;
	}
	if (!s->ap) return JS_TRUE;
	if (!s->ap->cp) return JS_TRUE;

	return js_config_callfunc(s->ap->cp, cx, argc, vp);
}

static JSBool js_template_signal(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	template_session_t *s;
	char *module,*action;

	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx,"template private is null!\n");
		return JS_FALSE;
	}
	module = action = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "s s", &module, &action)) return JS_FALSE;
	agent_event(s->ap,module,action);
	if (module) JS_free(cx,module);
	if (action) JS_free(cx,action);
	return JS_TRUE;
}

static int js_init(JSContext *cx, JSObject *parent, void *priv) {
	template_session_t *s = priv;
	JSPropertySpec props[] = {
		{ "info", PROPERTY_ID_INFO, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ 0 }
	};
	JSFunctionSpec funcs[] = {
		JS_FS("signal",js_template_signal,2,2,0),
		{ 0 }
	};
	JSAliasSpec aliases[] = {
		{ 0 }
	};
	JSConstantSpec constants[] = {
		{ 0 }
	};
	JSObject *obj;

	dprintf(dlevel,"parent: %p\n", parent);
	dprintf(dlevel,"s->props: %p, cp: %p\n",s->props,s->ap->cp);
	if (s->ap && s->ap->cp) {
		if (!s->props) {
			s->props = js_config_to_props(s->ap->cp, cx, template_driver.name, props);
			dprintf(dlevel,"s->props: %p\n",s->props);
			if (!s->props) {
				log_error("unable to create props: %s\n", config_get_errmsg(s->ap->cp));
				return 0;
			}
		}
		if (!s->funcs) {
			s->funcs = js_config_to_funcs(s->ap->cp, cx, js_template_callfunc, funcs);
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

	dprintf(dlevel,"Defining %s object\n",template_class.name);
	obj = JS_InitClass(cx, parent, 0, &template_class, 0, 0, s->props, s->funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s class", template_class.name);
		return 1;
	}

	dprintf(dlevel,"Defining %s aliases\n",template_class.name);
	if (!JS_DefineAliases(cx, obj, aliases)) {
		JS_ReportError(cx,"unable to define aliases");
		return 1;
	}
	dprintf(dlevel,"Defining global constants\n");
	if (!JS_DefineConstants(cx, parent, constants)) {
		JS_ReportError(cx,"unable to define constants");
		return 1;
	}
	dprintf(dlevel,"done!\n");
	JS_SetPrivate(cx,obj,s);

	s->agent_val = OBJECT_TO_JSVAL(js_agent_new(cx,obj,s->ap));

	/* Create the global convenience objects */
	JS_DefineProperty(cx, parent, template_driver.name, OBJECT_TO_JSVAL(obj), 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "agent", s->agent_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "config", s->ap->js.config_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "mqtt", s->ap->js.mqtt_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "influx", s->ap->js.influx_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "event", s->ap->js.event_val, 0, 0, JSPROP_ENUMERATE);
	return 0;
}

int jsinit(template_session_t *s) {
	JS_EngineAddInitFunc(s->ap->js.e, template_driver.name, js_init, s);
	return 0;
}
#endif
