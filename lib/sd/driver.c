
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 4
#include "debug.h"

#include <string.h>
#include "agent.h"
#include "driver.h"
#include "debug.h"
#include "transports.h"
#ifdef JS
#include "jsapi.h"
#include "jsfun.h"
#include "jsinterp.h"
#endif

solard_driver_t *find_driver(solard_driver_t **transports, char *name) {
	register int i;

	dprintf(dlevel,"name: %s\n", name);

	if (!transports || !name) return 0;

	if (strcmp(name,"null") == 0) return &null_driver;
	for(i=0; transports[i]; i++) {
		dprintf(dlevel,"transports[%d]: %p\n", i, transports[i]);
		dprintf(dlevel,"name[%d]: %s\n", i, transports[i]->name);
		if (strcmp(transports[i]->name,name)==0) break;
	}

	dprintf(dlevel,"tp: %p\n", transports[i]);
	return transports[i];
}

#ifdef JS
#include "common.h"
#include "config.h"
#include "jsapi.h"
#include "jsobj.h"
#include "jsstr.h"

typedef struct driver_private {
	char *name;
	solard_driver_t *dp;
	JSEngine *e;
	JSContext *cx;
	JSObject *obj;
	JSObject *agent;
	config_t *cp;
	list funcs;
} driver_private_t;

struct _funcinfo {
	char name[128];
	jsval fval;
};

#define CACHEFUNCS 1
#if CACHEFUNCS
/* maintain a "cache" of funcs */
static int _getfunc(driver_private_t *p, char *name, jsval *fval) {
	struct _funcinfo *infop, newinfo;
	JSBool ok;

	int ldlevel = dlevel;

	list_reset(p->funcs);
	while((infop = list_get_next(p->funcs)) != 0) {
		if (strcmp(infop->name,name) == 0) {
			*fval = infop->fval;
			return 0;
		}
	}
	ok = JS_GetProperty(p->cx, p->obj, name, fval);
	dprintf(ldlevel,"getprop ok: %d\n", ok);
	if (!ok) return 1;
	dprintf(ldlevel,"isfunc: %d\n", VALUE_IS_FUNCTION(p->cx,*fval));
	if (!VALUE_IS_FUNCTION(p->cx,*fval)) return -1;
	strncpy(newinfo.name,name,sizeof(newinfo.name));
	newinfo.fval = *fval;
	list_add(p->funcs,&newinfo,sizeof(newinfo));
	return 0;
}
#endif

static int _callfunc(driver_private_t *p, char *name, int argc, jsval *argv, jsval *rval) {
	JSBool ok;
#if CACHEFUNCS
	jsval fval;
#endif
	int r;

	int ldlevel = dlevel;

	dprintf(ldlevel,"name: %s, argc: %d, argv: %p, rval: %p\n", name, argc, argv, rval);

	*rval = JSVAL_VOID;
#if CACHEFUNCS
	r = _getfunc(p, name, &fval);
	dprintf(ldlevel,"r: %d\n", r);
	if (r < 0) return 0;
	if (r) return 1;
	ok = js_InternalCall(p->cx, p->obj, fval, argc, argv, rval);
#else
	ok = JS_CallFunctionName(p->cx, p->obj, name, argc, argv, rval);
#endif
	dprintf(ldlevel,"ok: %d\n", ok);
//	js_ReportUncaughtException(p->cx);
	JS_ReportPendingException(p->cx);
	dprintf(dlevel,"ok: %d\n", ok);
	return (ok ? 0 : 1);
}

static int _getrval(JSContext *cx, jsval rval) {
	int status;
	dprintf(dlevel,"isvoid: %d\n", JSVAL_IS_VOID(rval));
	if (!JSVAL_IS_VOID(rval)) {
		JS_ValueToInt32(cx,rval,&status);
		dprintf(dlevel,"status: %d\n", status);
		return status;
	}
	return 0;
}

static solard_driver_t js_driver;
static void *driver_new(void *transport, void *transport_handle) {
	driver_private_t *p;
	solard_driver_t *dp;

	p = calloc(1,sizeof(*p));
	if (!p) {
		log_syserr("driver_new: calloc");
		return 0;
	}
	dp = malloc(sizeof(*dp));
	if (!dp) return 0;
	memset(dp,0,sizeof(*dp));
	*dp = js_driver;
	dp->name = strdup((char *)transport);
	p->dp = dp;
	return p;
}

static int driver_free(void *handle) {
	driver_private_t *p = handle;

	if (!p) return 1;

	dprintf(dlevel,"destroying driver private...\n");
	if (p->dp) {
		if (p->dp->name) free(p->dp->name);
		free(p->dp);
	}
	free(p);

	return 0;
}

static int driver_open(void *h) {
	dprintf(dlevel,"h: %p\n",h);
	return 1;
}

static int driver_close(void *h) {
	dprintf(dlevel,"h: %p\n",h);
	return 1;
}

static int driver_read(void *h, uint32_t *control, void *buf, int buflen) {
	driver_private_t *p = h;
	jsval rval,argv[3] = { OBJECT_TO_JSVAL(p->agent), INT_TO_JSVAL(control ? *control : 0), INT_TO_JSVAL(buflen) };
	int r;

	dprintf(dlevel,"h: %p\n",h);
	r = _callfunc(p,"read",3,argv,&rval);
	if (!r) r = _getrval(p->cx,rval);
	return r;
}

static int driver_write(void *h, uint32_t *control, void *buf, int buflen) {
	driver_private_t *p = h;
	jsval rval,argv[3] = { OBJECT_TO_JSVAL(p->agent), INT_TO_JSVAL(control ? *control : 0), INT_TO_JSVAL(buflen) };
	int r;

	dprintf(dlevel,"h: %p\n",h);
	r = _callfunc(p,"write",3,argv,&rval);
	if (!r) r = _getrval(p->cx,rval);
	return r;
}

static int driver_cb(void *h) {
	driver_private_t *p = h;
	jsval argv[1] = { OBJECT_TO_JSVAL(p->agent) };
	jsval rval;
	int r;

	r = _callfunc(p,"run",1,argv,&rval);
//	if (!r) r = _getrval(p->cx,rval);
	return r;
}

int driver_config(void *h, int req, ...) {
	driver_private_t *p = h;
	va_list va;
//	void **vp;
	jsval rval;
	int r;

	r = 0;
	va_start(va,req);
	dprintf(dlevel,"req: %d\n", req);
	switch(req) {
	case SOLARD_CONFIG_INIT:
		{
			solard_agent_t *ap;
			jsval argv[1];

			dprintf(dlevel,"**** CONFIG INIT *******\n");
			ap = va_arg(va,solard_agent_t *);
			/* get cp from agent instance */
			if (ap->cp) p->cp = ap->cp;
			agent_set_callback(ap,driver_cb,p);

			/* create the new agent instance */
			dprintf(dlevel,"cx: %p, obj: %p, ap: %p\n", p->cx, p->obj, ap);
			p->agent = js_agent_new(p->cx, p->obj, ap);
			if (!p->agent) r = 1;
			argv[0] = OBJECT_TO_JSVAL(p->agent);
			r = _callfunc(p,"init",1,argv,&rval);
			dprintf(dlevel,"init r: %d\n", r);
			if (!r) r = _getrval(p->cx,rval);
			dprintf(dlevel,"init r: %d\n", r);
		}
		break;
	case SOLARD_CONFIG_GET_INFO:
		/* Call info func, get return val and turn into a json_object */
		dprintf(dlevel,"cp: %p\n", p->cp);
		if (p->cp) {
			json_value_t **vp = va_arg(va,json_value_t **);
			jsval rval;

			dprintf(dlevel,"vp: %p\n", vp);
			if (vp) {
				char *j;
				json_value_t *v;
				jsval argv[1] = { OBJECT_TO_JSVAL(p->agent) };

				if (_callfunc(p,"get_info",1,argv,&rval)) {
					jsval_to_type(DATA_TYPE_STRINGP,&j,sizeof(j),p->cx,rval);
				} else {
					j = "{}";
				}
				dprintf(dlevel,"j: %s\n", j);
				v = json_parse(j);
				dprintf(dlevel,"v: %p\n", v);
				config_add_info(p->cp,json_value_object(v));
				*vp = v;
			}
		}
		break;
	case AGENT_CONFIG_GETJS:
		{
			JSEngine **ep = va_arg(va,JSEngine **);
			dprintf(dlevel,"ep: %p, p->e: %p\n", ep, p->e);
			if (ep) *ep = p->e;
		}
		break;
	default:
		r = 1;
		break;
	}
	dprintf(dlevel,"returning: %d\n", r);
	va_end(va);
	return r;
}

static solard_driver_t js_driver = {
	"__Driver__",		/* Name */
	driver_new,		/* New */
	driver_free,		/* Free */
	driver_open,		/* Open */
	driver_close,		/* Close */
	driver_read,		/* Read */
	driver_write,		/* Write */
	driver_config,		/* Config */
};

solard_driver_t *js_driver_get_driver(void *handle) {
	driver_private_t *p = handle;
	return p->dp;
}

JSObject *js_driver_get_agent(void *handle) {
	return (((driver_private_t *)handle)->agent);
}

#if 0
solard_driver_t *js_driver_get_driver(char *name) {
	solard_driver_t *dp;

	dp = malloc(sizeof(*dp));
	if (!dp) return 0;
	memset(dp,0,sizeof(*dp));
	*dp = js_driver;
	dp->name = strdup(name);
	return dp;
}
#endif

enum DRIVER_PROPERTY_ID {
	DRIVER_PROPERTY_ID_CP=1,
	DRIVER_PROPERTY_ID_NAME,
};

static JSBool driver_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	driver_private_t *p;
	int prop_id;

	p = JS_GetPrivate(cx,obj);
	if (!p) {
		JS_ReportError(cx,"internal error: driver_getprop: private is null!");
		return JS_FALSE;
	}
	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case DRIVER_PROPERTY_ID_NAME:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,p->name,strlen(p->name));
			break;
		default:
			dprintf(dlevel,"p->cp: %p\n", p->cp);
			if (p->cp) *rval = js_config_common_getprop(cx, obj, id, rval, p->cp, 0);
			break;
		}
	} else {
		if (p->cp) return js_config_common_getprop(cx, obj, id, rval, p->cp, 0);
	}
	return JS_TRUE;
}

static JSBool driver_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	driver_private_t *p;

	p = JS_GetPrivate(cx,obj);
	dprintf(dlevel,"p: %p\n", p);
	if (!p) {
		JS_ReportError(cx,"setprop: private is null!");
		return JS_FALSE;
	}
	dprintf(dlevel,"p->cp: %p\n", p->cp);
	if (JSVAL_IS_STRING(id)) {
		char *name;
		name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
		if (name) {
			config_property_t *prop;

			dprintf(dlevel,"value type: %s\n", jstypestr(cx,*vp));
			prop = config_get_property(p->cp, p->name, name);
			JS_free(cx, name);
			dprintf(dlevel,"prop: %p\n", prop);
			if (prop) return js_config_property_set_value(prop,cx,*vp,p->cp->triggers);
		}
	}
	return js_config_common_setprop(cx, obj, id, vp, p->cp, 0);
}

static JSClass js_driver_class = {
	"Driver",		/* Name */
	JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	driver_getprop,		/* getProperty */
	driver_setprop,		/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool driver_ctor(JSContext *cx, JSObject *parent, uintN argc, jsval *argv, jsval *rval) {
	driver_private_t *p;
	char *namep;
#if 0
	JSClass *newdriverp,*ccp = &driver_class;
	JSPropertySpec driver_props[] = {
		{ DRIVER_CP,DRIVER_PROPERTY_ID_CP,0 },
		{ "name", DRIVER_PROPERTY_ID_NAME, JSPROP_ENUMERATE | JSPROP_READONLY },
		{0}
	};
#endif

	namep =0;
	if (!JS_ConvertArguments(cx, argc, argv, "s", &namep)) return JS_FALSE;
	dprintf(dlevel,"name: %s\n", namep);
	if (!namep) {
		JS_ReportError(cx, "driver requires 1 argument: (name:string)");
		return JS_FALSE;
	}

	p = js_driver.new(namep,0);
	if (!p) {
		JS_ReportError(cx,"internal error: unable to create driver private");
		return JS_FALSE;
	}
	p->e = JS_GetPrivate(cx,JS_GetGlobalObject(cx));
	p->cx = cx;
	p->obj = js_driver_new(cx, parent, p);
	p->name = p->dp->name;
//	p->parent = parent;
	p->funcs = list_create();

#if 0
	/* create our private storage */
	p = JS_malloc(cx,sizeof(*p));
	if (!p) {
		JS_ReportError(cx,"error allocating memory");
		return JS_FALSE;
	}
	memset(p,0,sizeof(*p));
	p->name = JS_strdup(cx,namep);
//	strncpy(p->name,namep,sizeof(p->name)-1);
	/* get the engine pointer */
	p->e = JS_GetPrivate(cx,JS_GetGlobalObject(cx));
	dprintf(dlevel,"e: %p\n", p->e);

	/* Create a new instance of JSClass */
	newdriverp = JS_malloc(cx,sizeof(JSClass));
	*newdriverp = *ccp;
	newdriverp->name = p->name;

	newobj = JS_InitClass(cx, parent, 0, newdriverp, 0, 0, driver_props, driver_funcs, 0, 0);
	if (!newobj) {
		JS_ReportError(cx,"error allocating memory");
		return JS_FALSE;
	}
	JS_SetPrivate(cx,newobj,p);
#endif
	
	*rval = OBJECT_TO_JSVAL(p->obj);
	return JS_TRUE;
}

JSObject *js_InitDriverClass(JSContext *cx, JSObject *parent) {
	JSPropertySpec props[] = {
//		{ DRIVER_CP,DRIVER_PROPERTY_ID_CP,0 },
		{ "name", DRIVER_PROPERTY_ID_NAME, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ 0 }
	};
	JSObject *newobj;

	dprintf(dlevel,"Creating %s class\n",js_driver_class.name);
	newobj = JS_InitClass(cx, parent, 0, &js_driver_class, driver_ctor, 1, props, 0, 0, 0);
	if (!newobj) {
		JS_ReportError(cx,"unable to initialize driver Class");
		return 0;
	}
//	dprintf(dlevel,"done!\n");
	return newobj;
}

JSObject *js_driver_new(JSContext *cx, JSObject *parent, void *priv) {
	solard_driver_t *p = priv;
	JSObject *newobj;

	int ldlevel = dlevel;

//	dprintf(ldlevel,"parent name: %s\n", JS_GetObjectName(cx,parent));

	/* Create the new object */
	dprintf(ldlevel,"creating %s object\n",js_driver_class.name);
	newobj = JS_NewObject(cx, &js_driver_class, 0, parent);
	if (!newobj) return 0;

	dprintf(dlevel,"Setting private to: %p\n", p);
	JS_SetPrivate(cx,newobj,p);
	return newobj;
}
#endif
