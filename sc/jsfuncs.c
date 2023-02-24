#ifdef JS

/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "sc.h"
#include "jsstr.h"
#include "jsprintf.h"

static JSBool sc_agent_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	sc_agent_t *info;
	int prop_id;

	info = JS_GetPrivate(cx,obj);
	dprintf(1,"info: %p\n", info);
	if (!info) {
		JS_ReportError(cx, "private is null!");
		return JS_FALSE;
	}

	dprintf(1,"is_int: %d\n", JSVAL_IS_INT(id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(1,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		default:
			*rval = JSVAL_NULL;
			break;
		}
	}
	return JS_TRUE;
}

static JSBool sc_agent_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	sc_agent_t *info;
	int prop_id;

	info = JS_GetPrivate(cx,obj);
	dprintf(1,"info: %p\n", info);
	if (!info) {
		JS_ReportError(cx, "private is null!");
		return JS_FALSE;
	}

	dprintf(1,"is_int: %d\n", JSVAL_IS_INT(id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(1,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		default:
			*vp = JSVAL_NULL;
			break;
		}
	}
	return JS_TRUE;
}

static JSClass sc_agent_class = {
	"sc_agent",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,
	JS_PropertyStub,
	sc_agent_getprop,
	sc_agent_setprop,
	JS_EnumerateStub,
	JS_ResolveStub,
	JS_ConvertStub,
	JS_FinalizeStub,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *JSAgentInfo(JSContext *cx, sc_agent_t *info) {
	JSPropertySpec sc_agent_props[] = { 
		{0}
	};
	JSFunctionSpec sc_agent_funcs[] = {
		{ 0 }
	};
	JSObject *obj;

	dprintf(1,"defining %s object\n",sc_agent_class.name);
	obj = JS_InitClass(cx, JS_GetGlobalObject(cx), 0, &sc_agent_class, 0, 0, sc_agent_props, sc_agent_funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize si class");
		return 0;
	}
	JS_SetPrivate(cx,obj,info);
	dprintf(1,"done!\n");
	return obj;
}

enum SOLARD_PROPERTY_ID {
	SOLARD_PROPERTY_ID_NAME=1,
	SOLARD_PROPERTY_ID_AGENTS,
	SOLARD_PROPERTY_ID_STATE,
	SOLARD_PROPERTY_ID_AGENT_WARNING,
	SOLARD_PROPERTY_ID_AGENT_ERROR,
	SOLARD_PROPERTY_ID_AGENT_NOTIFY,
	SOLARD_PROPERTY_ID_STATUS,
	SOLARD_PROPERTY_ID_ERRMSG,
	SOLARD_PROPERTY_ID_LAST_CHECK,
	SOLARD_PROPERTY_ID_START,
};

static JSBool js_sc_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	sc_session_t *sd;
	register int i;

	sd = JS_GetPrivate(cx,obj);
	dprintf(1,"sd: %p\n", sd);
	if (!sd) {
		JS_ReportError(cx, "private is null!");
		return JS_FALSE;
	}

	dprintf(1,"is_int: %d\n", JSVAL_IS_INT(id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(1,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case SOLARD_PROPERTY_ID_AGENTS:
			{
				sc_agent_t *info;
				JSObject *rows;
				jsval node;

				rows = JS_NewArrayObject(cx, list_count(sd->agents), NULL);
				i = 0;
				list_reset(sd->agents);
				while( (info = list_next(sd->agents)) != 0) {
					node = OBJECT_TO_JSVAL(JSAgentInfo(cx,info));
					JS_SetElement(cx, rows, i++, &node);
				}
				*rval = OBJECT_TO_JSVAL(rows);
			}
			break;
#if 0
		case SOLARD_PROPERTY_ID_BATTERIES:
			{
				solard_battery_t *bp;
				JSObject *rows;
				jsval node;

				rows = JS_NewArrayObject(cx, list_count(sd->batteries), NULL);
				i = 0;
				list_reset(sd->batteries);
				while( (bp = list_next(sd->batteries)) != 0) {
					node = OBJECT_TO_JSVAL(JSBattery(cx,bp));
					JS_SetElement(cx, rows, i++, &node);
				}
				*rval = OBJECT_TO_JSVAL(rows);
			}
			break;
#endif
		default:
			*rval = JSVAL_NULL;
			break;
		}
	}
	return JS_TRUE;
}

static JSBool js_sc_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	sc_session_t *sd;

	sd = JS_GetPrivate(cx,obj);
	dprintf(1,"sd: %p\n", sd);
	if (!sd) {
		JS_ReportError(cx, "private is null!");
		return JS_FALSE;
	}

	dprintf(1,"is_int: %d\n", JSVAL_IS_INT(id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(1,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		default:
			*rval = JSVAL_NULL;
			break;
		}
	}
	return JS_TRUE;
}

static JSClass sc_class = {
	"MCU",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,
	JS_PropertyStub,
	js_sc_getprop,
	js_sc_setprop,
	JS_EnumerateStub,
	JS_ResolveStub,
	JS_ConvertStub,
	JS_FinalizeStub,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool jssd_notify(JSContext *cx, uintN argc, jsval *vp) {
	jsval val;
	char *str;
//	sc_session_t *sd;
	JSObject *obj;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
//	sd = JS_GetPrivate(cx, obj);

	JS_SPrintf(cx, JS_GetGlobalObject(cx), argc, JS_ARGV(cx, vp), &val);
	str = (char *)js_GetStringBytes(cx, JSVAL_TO_STRING(val));
	dprintf(dlevel,"str: %s\n", str);
//	sd_notify(sd,"%s",str);
	return JS_TRUE;
}

JSBool sd_callfunc(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	sc_session_t *s;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) {
		JS_ReportError(cx, "sd_callfunc: internal error: object is null!\n");
		return JS_FALSE;
	}
	s = JS_GetPrivate(cx, obj);
	if (!s) {
		JS_ReportError(cx, "sd_callfunc: internal error: private is null!\n");
		return JS_FALSE;
	}
	if (!s->ap) return JS_TRUE;
	if (!s->ap->cp) return JS_TRUE;

	return js_config_callfunc(s->ap->cp, cx, argc, vp);
}

static int jssd_init(JSContext *cx, JSObject *parent, void *priv) {
	sc_session_t *sd = priv;
	JSPropertySpec sd_props[] = {
		{ "name",		SOLARD_PROPERTY_ID_NAME,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "agents",		SOLARD_PROPERTY_ID_AGENTS,	JSPROP_ENUMERATE },
		{ 0 }
	};
	JSFunctionSpec sd_funcs[] = {
		JS_FN("notify",jssd_notify,0,0,0),
		{ 0 }
	};
	JSAliasSpec sd_aliases[] = {
		{ 0 }
	};
	JSConstantSpec sd_constants[] = {
		{ 0 }
	};
	JSObject *obj,*global = JS_GetGlobalObject(cx);

	dprintf(dlevel,"sd->props: %p, cp: %p\n",sd->props,sd->ap->cp);
	if (sd->ap && sd->ap->cp) {
		if (!sd->props) {
			sd->props = js_config_to_props(sd->ap->cp, cx, "sc", sd_props);
			dprintf(dlevel,"sd->props: %p\n",sd->props);
			if (!sd->props) {
				log_error("unable to create props: %s\n", config_get_errmsg(sd->ap->cp));
				return 0;
			}
		}
		if (!sd->funcs) {
			sd->funcs = js_config_to_funcs(sd->ap->cp, cx, sd_callfunc, sd_funcs);
			dprintf(dlevel,"sd->funcs: %p\n",sd->funcs);
			if (!sd->funcs) {
				log_error("unable to create funcs: %s\n", config_get_errmsg(sd->ap->cp));
				return 0;
			}
		}
	}
	dprintf(dlevel,"sd->props: %p, sd->funcs: %p\n", sd->props, sd->funcs);
	if (!sd->props) sd->props = sd_props;
	if (!sd->funcs) sd->funcs = sd_funcs;

	dprintf(dlevel,"Defining %s object\n",sc_class.name);
	obj = JS_InitClass(cx, parent, 0, &sc_class, 0, 0, sd->props, sd_funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s class", sc_class.name);
		return 1;
	}
	dprintf(dlevel,"Defining %s aliases\n",sc_class.name);
	if (!JS_DefineAliases(cx, obj, sd_aliases)) {
		JS_ReportError(cx,"unable to define aliases");
		return 1;
	}
	dprintf(dlevel,"Defining %s constants\n",sc_class.name);
	if (!JS_DefineConstants(cx, global, sd_constants)) {
		JS_ReportError(cx,"unable to define constants");
		return 1;
	}
	dprintf(dlevel,"done!\n");
	JS_SetPrivate(cx,obj,sd);

//	sd->agents_val = OBJECT_TO_JSVAL(jsagent_new(cx,obj,sd->ap));

	/* Create the global convenience objects */
	JS_DefineProperty(cx, global, "sd", OBJECT_TO_JSVAL(obj), 0, 0, 0);
//	JS_DefineProperty(cx, global, "data", sd->data_val, 0, 0, 0);
	return 0;
}

int sc_jsinit(sc_session_t *sd) {
	return JS_EngineAddInitFunc(sd->ap->js.e, "solard", jssd_init, sd);
}
#endif
