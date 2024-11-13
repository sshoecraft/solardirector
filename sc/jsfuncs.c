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
#include "jsobj.h"
#include "jsarray.h"

#if 0
enum SC_PROPERTY_ID {
//	SC_PROPERTY_ID_NAME=1,
//	SC_PROPERTY_ID_AGENTS,
//	SC_PROPERTY_ID_STATE,
	SC_PROPERTY_ID_AGENT_WARNING,
	SC_PROPERTY_ID_AGENT_ERROR,
	SC_PROPERTY_ID_AGENT_NOTIFY,
//	SC_PROPERTY_ID_STATUS,
//	SC_PROPERTY_ID_ERRMSG,
//	SC_PROPERTY_ID_LAST_CHECK,
//	SC_PROPERTY_ID_START,
};
#endif

static JSBool js_sc_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	sc_session_t *sc;

	sc = JS_GetPrivate(cx,obj);
	dprintf(1,"sc: %p\n", sc);
	if (!sc) {
		JS_ReportError(cx, "js_sc_getprop: private is null!");
		return JS_FALSE;
	}
	dprintf(dlevel,"sc->ap: %p, sc->ap->cp: %p\n", sc->ap, sc->ap ? sc->ap->cp : 0);
	if (sc->ap && sc->ap->cp) return js_config_common_getprop(cx, obj, id, rval, sc->ap->cp, 0);
	return JS_TRUE;
}

static JSBool js_sc_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	sc_session_t *sc;

	sc = JS_GetPrivate(cx,obj);
	dprintf(1,"sc: %p\n", sc);
	if (!sc) {
		JS_ReportError(cx, "js_sc_setprop: private is null!");
		return JS_FALSE;
	}
	dprintf(dlevel,"sc->ap: %p, sc->ap->cp: %p\n", sc->ap, sc->ap ? sc->ap->cp : 0);
	if (sc->ap && sc->ap->cp) return js_config_common_setprop(cx, obj, id, rval, sc->ap->cp, 0);
	return JS_TRUE;
}

static JSClass sc_class = {
	"sc",
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

static int js_sc_exec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	char *prog;
	JSObject *arr;
	pid_t pid;
	int u,i;
	unsigned int acount;
	jsval val;
	bool wait;
#define _EXEC_MAX_ARGS 64
	char *args[_EXEC_MAX_ARGS+1];

	dprintf(dlevel,"argc: %d\n", argc);
	u = 1;
	if (argc >= 2 && JSVAL_IS_OBJECT(argv[1])) {
        	arr = JSVAL_TO_OBJECT(argv[1]);
		if (OBJ_IS_ARRAY(cx,arr)) u = 0;
	}
	if (u) {
                JS_ReportError(cx,"exec requires 2 arguments (program path: string, arguments: array of strings) and 1 optional argument (wait: boolean)\n");
                return JS_FALSE;
        }

	// Get the prog
	prog = JSVAL_TO_CHAR(cx,argv[0]);
	dprintf(dlevel,"prog: %s\n", prog);

	// Get the args
        if (!js_GetLengthProperty(cx, arr, &acount)) {
                JS_ReportError(cx,"add_props: unable to get array length");
                return JS_FALSE;
        }
	dprintf(dlevel,"acount: %d\n", acount);
        for(i=0; i < acount; i++) {
		dprintf(dlevel,"i: %d\n", i);
		if (i >= _EXEC_MAX_ARGS) {
			char temp[128];
			sprintf(temp,"exec: max number of arguments (%d) exceeded", _EXEC_MAX_ARGS);
			JS_ReportError(cx,temp);
			return JS_FALSE;
		}
		JS_GetElement(cx, arr, i, &val);
		dprintf(dlevel,"element %d type: %s\n", i, jstypestr(cx,val));
		if (JS_TypeOfValue(cx,val) != JSTYPE_STRING) {
			char temp[128];
			sprintf(temp,"exec: element %d is not a string", i);
			JS_ReportError(cx,temp);
			return JS_FALSE;
		}
		dprintf(dlevel,"getting chars...\n");
		args[i] = JSVAL_TO_CHAR(cx,val);
		dprintf(dlevel,"arg[%d]: %s\n", i, args[i]);
	}
	args[i] = 0;

	if (argc > 2) {
		wait = JSVAL_TO_BOOLEAN(argv[2]);
		dprintf(dlevel,"wait: %d\n", wait);
	} else {
		wait = false;
	}

	pid = solard_exec(prog,args,0,wait);
	dprintf(dlevel,"pid: %d\n", pid);

	/* Free the args */
        for(i=0; i < acount; i++) JS_free(cx,args[i]);

	JS_free(cx,prog);
	*rval = INT_TO_JSVAL(pid);
	return JS_TRUE;
}

static int js_sc_checkpid(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	int pid,status,r;
int od;

	if (argc != 1) {
                JS_ReportError(cx,"exec requires 1 argument (pid: number)\n");
                return JS_FALSE;
        }
	jsval_to_type(DATA_TYPE_INT, &pid, sizeof(pid), cx, argv[0]);
	dprintf(dlevel,"pid: %d\n", pid);

od = debug;
debug = 2;
	status = 0;
	r = solard_checkpid(pid,&status);
debug = od;
	dprintf(dlevel,"r: %d, status: %d\n", r, status);
	if (r) status = 1;
	else if (status) status = 1;
	dprintf(dlevel,"status: %d\n", status);

	*rval = INT_TO_JSVAL(status);
	return JS_TRUE;
}

JSBool sc_callfunc(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	sc_session_t *sc;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) {
		JS_ReportError(cx, "sd_callfunc: internal error: object is null!\n");
		return JS_FALSE;
	}
	sc = JS_GetPrivate(cx, obj);
	if (!sc) {
		JS_ReportError(cx, "sc_callfunc: internal error: private is null!\n");
		return JS_FALSE;
	}
	if (!sc->ap) return JS_TRUE;
	if (!sc->ap->cp) return JS_TRUE;

	return js_config_callfunc(sc->ap->cp, cx, argc, vp);
}

static int jssc_init(JSContext *cx, JSObject *parent, void *priv) {
	sc_session_t *sc = priv;
	JSPropertySpec sc_props[] = {
		{ 0 }
	};
	JSFunctionSpec sc_funcs[] = {
		JS_FN("notify",jssd_notify,0,0,0),
		JS_FS("exec",js_sc_exec,2,2,0),
		JS_FS("checkpid",js_sc_checkpid,1,1,0),
		{ 0 }
	};
#if 0
	JSAliasSpec sc_aliases[] = {
		{ 0 }
	};
	JSConstantSpec sc_constants[] = {
		{ 0 }
	};
#endif
	JSObject *obj;

	dprintf(dlevel,"sc->props: %p, cp: %p\n",sc->props,sc->ap->cp);
	if (sc->ap && sc->ap->cp) {
		if (!sc->props) {
			sc->props = js_config_to_props(sc->ap->cp, cx, "sc", sc_props);
			dprintf(dlevel,"sc->props: %p\n",sc->props);
			if (!sc->props) {
				log_error("unable to create props: %s\n", config_get_errmsg(sc->ap->cp));
				return 0;
			}
		}
		if (!sc->funcs) {
			sc->funcs = js_config_to_funcs(sc->ap->cp, cx, sc_callfunc, sc_funcs);
			dprintf(dlevel,"sc->funcs: %p\n",sc->funcs);
			if (!sc->funcs) {
				log_error("unable to create funcs: %s\n", config_get_errmsg(sc->ap->cp));
				return 0;
			}
		}
	}
	dprintf(dlevel,"sc->props: %p, sc->funcs: %p\n", sc->props, sc->funcs);
	if (!sc->props) sc->props = sc_props;
	if (!sc->funcs) sc->funcs = sc_funcs;

	dprintf(dlevel,"Defining %s object\n",sc_class.name);
	obj = JS_InitClass(cx, parent, 0, &sc_class, 0, 0, sc->props, sc_funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s class", sc_class.name);
		return 1;
	}
#if 0
	dprintf(dlevel,"Defining %s aliases\n",sc_class.name);
	if (!JS_DefineAliases(cx, obj, sc_aliases)) {
		JS_ReportError(cx,"unable to define aliases");
		return 1;
	}
	dprintf(dlevel,"Defining %s constants\n",sc_class.name);
	if (!JS_DefineConstants(cx, parent, sc_constants)) {
		JS_ReportError(cx,"unable to define constants");
		return 1;
	}
#endif
	dprintf(dlevel,"done!\n");
	JS_SetPrivate(cx,obj,sc);

	sc->agent_val = OBJECT_TO_JSVAL(js_agent_new(cx,obj,sc->ap));

	/* Create the global convenience objects */
	JS_DefineProperty(cx, parent, "sc", OBJECT_TO_JSVAL(obj), 0, 0, 0);
	JS_DefineProperty(cx, parent, "agent", sc->agent_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "config", sc->ap->js.config_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "mqtt", sc->ap->js.mqtt_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "influx", sc->ap->js.influx_val, 0, 0, JSPROP_ENUMERATE);
	return 0;
}

int sc_jsinit(sc_session_t *sc) {
	return JS_EngineAddInitFunc(sc->ap->js.e, "solard", jssc_init, sc);
}
#endif
