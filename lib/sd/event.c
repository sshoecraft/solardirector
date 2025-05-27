
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 4
#include "debug.h"

#include "common.h"

struct event_handler_info {
	event_handler_t *handler;
	void *ctx;
	event_info_t scope;
	char *name;			/* agent name */
	char *module;
	char *action;
};
typedef struct event_handler_info event_handler_info_t;

struct event_session {
	list handlers;
};
typedef struct event_session event_session_t;

event_session_t *event_init(void) {
	event_session_t *e;

	e = malloc(sizeof(*e));
	if (!e) return 0;
	memset(e,0,sizeof(*e));
	e->handlers = list_create(); 

	return e;
}

void event_destroy(event_session_t *e) {
	struct event_handler_info *info;

	dprintf(dlevel,"handler count: %d\n", list_count(e->handlers));
	list_reset(e->handlers);
	while((info = list_get_next(e->handlers)) != 0) {
		dprintf(dlevel,"destroying handler: %p\n", info->handler);
		info->handler(info->ctx,0,0,"__destroy__");
		if (info->name) free(info->name);
		if (info->module) free(info->module);
		if (info->action) free(info->action);
		free(info);
	}
	list_destroy(e->handlers);
	free(e);
}

int event_handler(event_session_t *e, event_handler_t handler, void *ctx, char *name, char *module, char *action) {
	struct event_handler_info *newinfo;

	newinfo = malloc(sizeof(*newinfo));
	if (!newinfo) return 1;
	memset(newinfo,0,sizeof(*newinfo));
	newinfo->handler = handler;
	newinfo->ctx = ctx;
	if (name) newinfo->name = strdup(name);
	if (module) newinfo->module = strdup(module);
	if (action) newinfo->action = strdup(action);
	list_add(e->handlers,newinfo,0);
	return 0;
}

void event_signal(event_session_t *e, char *name, char *module, char *action) {
	struct event_handler_info *info;

	int ldlevel = dlevel;

	dprintf(ldlevel,"e: %p\n", e);
	if (!e) return;

//	log_info("EVENT: name: %s, module: %s, action: %s\n", name, module, action);

	dprintf(ldlevel,"handler count: %d\n", list_count(e->handlers));
	list_reset(e->handlers);
	while((info = list_get_next(e->handlers)) != 0) {
		dprintf(ldlevel,"info: handler: %p, ctx: %p, name: %s, module: %s, action: %s\n",
			info->handler, info->ctx, info->name, info->module, info->action);
		dprintf(ldlevel,"calling handler: %p\n", info->handler);
		info->handler(info->ctx, name, module, action);
	}
	dprintf(dlevel,"done!\n");
}

#ifdef JS
#include "jsapi.h"
#include "jsobj.h"
#include "jsfun.h"
#include "jsatom.h"
#include "jsarray.h"

static JSBool js_event_signal(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	event_session_t *p;
	char *name, *module,*action;

	int ldlevel = dlevel;

	p = JS_GetPrivate(cx, obj);
	dprintf(ldlevel,"p: %p\n", p);
	if (!p) {
		JS_ReportError(cx,"event private is null!");
		return JS_FALSE;
	}
	name = module = action = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "s s s", &name, &module, &action)) return JS_FALSE;
	event_signal(p,name,module,action);
	if (name) JS_free(cx,name);
	if (module) JS_free(cx,module);
	if (action) JS_free(cx,action);
	return JS_TRUE;
}

struct js_event_info {
	JSContext *cx;
	jsval func;
	char *name;
};

static void _js_event_handler(void *ctx, char *name, char *module, char *action) {
	struct js_event_info *info = ctx;
	JSBool ok;
	jsval args[3];
	jsval rval;

	dprintf(dlevel,"name: %s, module: %s, action: %s\n", name, module, action);

	/* destroy context */
	if (!name && !module && action && strcmp(action,"__destroy__") == 0) {
		JS_free(info->cx,info);
		return;
	}

	args[0] = STRING_TO_JSVAL(JS_InternString(info->cx,name));
	args[1] = STRING_TO_JSVAL(JS_InternString(info->cx,module));
	args[2] = STRING_TO_JSVAL(JS_InternString(info->cx,action));
	ok = JS_CallFunctionValue(info->cx, JS_GetGlobalObject(info->cx), info->func, 3, args, &rval);
	dprintf(dlevel,"call ok: %d\n", ok);
	return;
}

static JSBool js_event_handler(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	event_session_t *p;
	char *fname, *name, *module,*action;
	JSFunction *fun;
	jsval func;
	struct js_event_info *ctx;

	p = JS_GetPrivate(cx, obj);
	if (!p) {
		JS_ReportError(cx,"event private is null!\n");
		return JS_FALSE;
	}
	func = 0;
	name = module = action = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "v / s s s", &func, &name, &module, &action)) return JS_FALSE;
	dprintf(dlevel,"VALUE_IS_FUNCTION: %d\n", VALUE_IS_FUNCTION(cx, func));
	if (!VALUE_IS_FUNCTION(cx, func)) {
		JS_ReportError(cx, "event_handler: arguments: required: function(function), optional: name(string), module(string), action(string)");
		return JS_FALSE;
	}
	fun = JS_ValueToFunction(cx, argv[0]);
	dprintf(dlevel,"fun: %p\n", fun);
	if (!fun) {
		JS_ReportError(cx, "js_event_handler: internal error: funcptr is null!");
		return JS_FALSE;
	}
	fname = JS_EncodeString(cx, ATOM_TO_STRING(fun->atom));
	dprintf(dlevel,"fname: %s\n", fname);
	if (!fname) {
		JS_ReportError(cx, "js_event_handler: internal error: fname is null!");
		return JS_FALSE;
	}
	ctx = JS_malloc(cx,sizeof(*ctx));
	if (!ctx) {
		JS_free(cx,fname);
		JS_ReportError(cx, "js_event_handler: internal error: malloc ctx");
		return JS_FALSE;
	}
	memset(ctx,0,sizeof(*ctx));
	ctx->cx = cx;
	ctx->func = func;
	ctx->name = fname;
	event_handler(p,_js_event_handler,ctx,name,module,action);

	/* root it */
	JS_EngineAddRoot(cx,fname,&ctx->func);

	dprintf(dlevel,"name: %p, module: %p, action: %p\n", name, module, action);
	if (name) JS_free(cx,name);
	if (module) JS_free(cx,module);
	if (action) JS_free(cx,action);
	JS_free(cx,fname);
	return JS_TRUE;
}

static JSClass js_event_class = {
	"Event",		/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	JS_PropertyStub,	/* getProperty */
	JS_PropertyStub,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *js_event_new(JSContext *cx, JSObject *parent, void *priv) {
        JSObject *newobj;

        //int ldlevel = dlevel;

        /* Create the new object */
        dprintf(2,"Creating %s object...\n", js_event_class.name);
        newobj = JS_NewObject(cx, &js_event_class, 0, parent);
        if (!newobj) return 0;

	JS_SetPrivate(cx, newobj, priv);
	return newobj;
}

static JSBool js_event_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	event_session_t *e;
	JSObject *newobj;

	int ldlevel = dlevel;

	dprintf(ldlevel,"cx: %p, obj: %p, argc: %d\n", cx, obj, argc);

	/* init */
	e = event_init();
	dprintf(ldlevel,"e: %p\n", e);
	if (!e) {
		JS_ReportError(cx, "event_init failed");
		return JS_FALSE;
	}

	/* create the new event instance */
	newobj = js_event_new(cx, obj, e);
	if (!newobj) {
		JS_ReportError(cx, "internal error: unable to create new event instance");
		return JS_FALSE;
	}

	*rval = OBJECT_TO_JSVAL(newobj);
	return JS_TRUE;
}

JSObject *js_InitEventClass(JSContext *cx, JSObject *parent) {
	JSFunctionSpec event_funcs[] = {
		JS_FS("signal",js_event_signal,2,2,0),
		JS_FS("handler",js_event_handler,1,4,0),
		{ 0 }
	};

        JSObject *newobj;

        dprintf(2,"creating %s class\n",js_event_class.name);
        newobj = JS_InitClass(cx, parent, 0, &js_event_class, js_event_ctor, 0, 0, event_funcs, 0, 0);
        if (!newobj) {
                JS_ReportError(cx,"unable to initialize %s class", js_event_class.name);
                return 0;
        }
        dprintf(dlevel,"newobj: %p\n", newobj);
        return newobj;
}
#endif
