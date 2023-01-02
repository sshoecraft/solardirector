
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include <string.h>
#include "agent.h"
#include "driver.h"
#include "debug.h"
#include "transports.h"
#ifdef JS
#include "jsapi.h"
#include "jsfun.h"
#endif

#define dlevel 1

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
	JSEngine *e;
	jsval cpval;
	JSContext *cx;
	JSObject *parent;
	JSObject *obj;
	solard_agent_t *ap;
	jsval agent_val;
	config_t *cp;
	jsval init;
} driver_private_t;

static int _callfunc(driver_private_t *p, char *name, int argc, jsval *argv, jsval *rval) {
	JSBool ok;
	jsval fval;

	dprintf(dlevel,"name: %s\n", name);
	ok = JS_GetProperty(p->cx, p->obj, name, &fval);
	dprintf(dlevel,"ok: %d, isfunc: %d\n", ok, VALUE_IS_FUNCTION(p->cx,fval));
	if (ok && VALUE_IS_FUNCTION(p->cx,fval)) ok = JS_CallFunctionValue(p->cx, p->obj, fval, argc, argv, rval);
	dprintf(dlevel,"ok: %d\n", ok);
	return (ok != JS_TRUE);
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
	jsval rval,argv[2] = { INT_TO_JSVAL(control ? *control : 0), INT_TO_JSVAL(buflen) };

//	dprintf(dlevel,"h: %p\n",h);
	_callfunc(p,"read",2,argv,&rval);
	return 0;
}

static int driver_write(void *h, uint32_t *control, void *buf, int buflen) {
	driver_private_t *p = h;

	jsval rval,argv[2] = { INT_TO_JSVAL(control ? *control : 0), INT_TO_JSVAL(buflen) };
//	dprintf(dlevel,"h: %p\n",h);
	_callfunc(p,"write",2,argv,&rval);
	return 0;
}

static int driver_cb(void *h) {
	driver_private_t *p = h;
	jsval rval;

	_callfunc(p,"run",0,0,&rval);
	return 0;
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

			dprintf(dlevel,"**** CONFIG INIT *******\n");
			ap = va_arg(va,solard_agent_t *);
			p->agent_val = OBJECT_TO_JSVAL(js_agent_new(p->cx,p->parent,ap));
			dprintf(dlevel,"agent_val: %x\n", p->agent_val);
			if (p->agent_val) {
 				jsval argv[1] = { p->agent_val };

				p->ap = ap;
				_callfunc(p,"init",1,argv,&rval);
				agent_set_callback(p->ap,driver_cb,p);
			}
		}
		break;
exit(0);
	case SOLARD_CONFIG_GET_INFO:
		/* Call info func, get return val and turn into a json_object */
		dprintf(dlevel,"ap: %p\n", p->ap);
		if (p->ap) {
			json_value_t **vp = va_arg(va,json_value_t **);
			jsval rval;

			dprintf(dlevel,"vp: %p\n", vp);
			if (vp) {
				JSString *str;
				char *j;
				json_value_t *v;

				if (_callfunc(p,"get_info",0,0,&rval)) {
					str = JS_ValueToString(p->cx, rval);
					j = (char *)js_GetStringBytes(p->cx, str);
				} else {
					j = "{}";
				}
				dprintf(dlevel,"j: %s\n", j);
				v = json_parse(j);
				dprintf(dlevel,"v: %p\n", v);
				config_add_info(p->ap->cp,json_value_object(v));
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

//	driver_new,		/* New */
static solard_driver_t js_driver = {
	"__Driver__",		/* Name */
	0,			/* New */
	0,			/* Free */
	driver_open,		/* Open */
	driver_close,		/* Close */
	driver_read,		/* Read */
	driver_write,		/* Write */
	driver_config,		/* Config */
};

solard_driver_t *js_driver_get_driver(char *name) {
	solard_driver_t *dp;

	dp = malloc(sizeof(*dp));
	if (!dp) return 0;
	memset(dp,0,sizeof(*dp));
	*dp = js_driver;
	dp->name = strdup(name);
	return dp;
}

enum CLASS_PROPERTY_ID {
	CLASS_PROPERTY_ID_CP=1,
};

static JSBool driver_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	driver_private_t *p;

	p = JS_GetPrivate(cx,obj);
	if (!p) {
		JS_ReportError(cx,"internal error: driver_getprop: private is null!");
		return JS_FALSE;
	}
	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	if (JSVAL_IS_INT(id)) {
		int prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case CLASS_PROPERTY_ID_CP:
			dprintf(dlevel,"cpval: %x, cp: %p\n", p->cpval, p->cp);
			if (!p->cpval && p->cp) {
				JSObject *newobj = js_config_new(cx,obj,p->cp);
				if (newobj) p->cpval = OBJECT_TO_JSVAL(obj);
			}
			*rval = p->cpval;
			break;
		default:
			if (p->cp) return js_config_common_getprop(cx, obj, id, rval, p->cp, 0);
                        break;
		}
	} else {
#if 0
		if (JSVAL_IS_STRING(id)) {
			char *sname, *name;
			JSClass *driverp = OBJ_GET_CLASS(cx, obj);

			sname = (char *)driverp->name;
			name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
			dprintf(dlevel,"sname: %s, name: %s\n", sname, name);
			if (name) JS_free(cx, name);
		}
#endif
		dprintf(dlevel,"cp: %p\n", p->cp);
		if (p->cp) return js_config_common_getprop(cx, obj, id, rval, p->cp, 0);
	}
	return JS_TRUE;
}

static JSBool driver_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	driver_private_t *p;

	dprintf(dlevel,"setprop called!\n");
	p = JS_GetPrivate(cx,obj);
	if (!p) {
		JS_ReportError(cx,"internal error: driver_setprop: private is null!");
		return JS_FALSE;
	}
	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	if (JSVAL_IS_INT(id)) {
		int prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case CLASS_PROPERTY_ID_CP:
			p->cpval = *vp;
			p->cp = JS_GetPrivate(cx,JSVAL_TO_OBJECT(*vp));
			break;
		default:
			if (p->cp) return js_config_common_setprop(cx, obj, id, vp, p->cp, 0);
                        break;
		}
	} else {
#if 0
		if (JSVAL_IS_STRING(id)) {
			char *sname, *name;
			JSClass *driverp = OBJ_GET_CLASS(cx, obj);

			sname = (char *)driverp->name;
			name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
			dprintf(dlevel,"sname: %s, name: %s\n", sname, name);
			if (name) JS_free(cx, name);
		}
#endif
		dprintf(dlevel,"cp: %p\n", p->cp);
		if (p->cp) return js_config_common_setprop(cx, obj, id, vp, p->cp, 0);
	}
	return JS_TRUE;
}

static JSClass driver_class = {
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
	JSObject *newobj;
	JSClass *newdriverp,*ccp = &driver_class;
	char *namep;
	JSPropertySpec props[] = {
		{ DRIVER_CP,CLASS_PROPERTY_ID_CP,JSPROP_ENUMERATE },
		{0}
	};
	JSFunctionSpec funcs[] = {
		{0}
	};

	if (argc < 1) {
		JS_ReportError(cx, "Class requires 1 argument: (name:string)");
		return JS_FALSE;
	}
	namep =0;
	if (!JS_ConvertArguments(cx, argc, argv, "s", &namep)) return JS_FALSE;
	dprintf(dlevel,"name: %s\n", namep);

	p = JS_malloc(cx,sizeof(*p));
	if (!p) {
		JS_ReportError(cx,"error allocating memory");
		return JS_FALSE;
	}
	memset(p,0,sizeof(*p));
	p->name = JS_strdup(cx,namep);
//	strncpy(p->name,namep,sizeof(p->name)-1);
	p->e = JS_GetPrivate(cx,JS_GetGlobalObject(cx));

	/* Create a new instance of Class */
	newdriverp = JS_malloc(cx,sizeof(JSClass));
//	memcpy(newdriverp,&driver_class,sizeof(driver_class));
	*newdriverp = *ccp;
	newdriverp->name = p->name;

	newobj = JS_InitClass(cx, parent, 0, newdriverp, 0, 0, props, funcs, 0, 0);
	if (!newobj) {
		JS_ReportError(cx,"error allocating memory");
		return JS_FALSE;
	}
	JS_SetPrivate(cx,newobj,p);
	
	/* make sure private has cx and obj */
	p->cx = cx;
	p->parent = parent;
	p->obj = newobj;
	*rval = OBJECT_TO_JSVAL(newobj);
	return JS_TRUE;
}

JSObject *js_InitDriverClass(JSContext *cx, JSObject *parent) {
	JSObject *obj;

	dprintf(dlevel,"Creating %s class\n",driver_class.name);
	obj = JS_InitClass(cx, parent, 0, &driver_class, driver_ctor, 2, 0, 0, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize driver Class");
		return 0;
	}
	dprintf(dlevel,"done!\n");
	return obj;
}

#if 0
int driver_jsinit(void *e) {
        return JS_EngineAddInitClass((JSEngine *)e, "Driver", js_InitDriverClass);
}
#endif
#endif
