
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "jsapi.h"
#include "jsengine.h"
 
static JSClass console_class = {
	"console",		/* Name */
	0,			/* Flags */
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


JSBool JS_Printf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool js_console_log(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	JSObject *gobj = JS_GetGlobalObject(cx);
	JSBool ok;

	ok = JS_Printf(cx, gobj, argc, argv, rval);
	if (ok) ((JSEngine *)JS_GetPrivate(cx,gobj))->output("\n");
	return ok;
}

#if 0
static JSBool console_constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
//	JSFile   *file;

	dprintf(1,"argc: %d\n", argc);
	if (!(cx->fp->flags & JSFRAME_CONSTRUCTING)) {
		/* Replace obj with a new File object. */
		obj = JS_NewObject(cx, &js_JSONClass, NULL, NULL);
		if (!obj) return JS_FALSE;
		*rval = OBJECT_TO_JSVAL(obj);
	}

//	file = file_init(cx, obj, js_GetStringBytes(cx,str));
//	if (!file)
//	return JS_FALSE;

//	SECURITY_CHECK(cx, NULL, "constructor", file);

	return JS_TRUE;
}
#endif

JSObject * js_InitConsoleClass(JSContext *cx, JSObject *global_object) {
	JSFunctionSpec console_funcs[] = {
		/* log()	Outputs a message to the console */
		JS_FS("log",		js_console_log,		0, 0, 0),
#if 0
		assert()	Writes an error message to the console if the assertion is false
		JS_FS("assert",		js_console_assert,	0, 0, 0),
		clear()	Clears the console
		JS_FS("clear",		js_console_clear,	0, 0, 0),
		count()	Logs the number of times that this particular call to count() has been called
		JS_FS("count",		js_console_count,	0, 0, 0),
		error()	Outputs an error message to the console
		JS_FS("error",		js_console_error,	0, 0, 0),
		group()	Creates a new inline group in the console. This indents following console messages by an additional level, until console.groupEnd() is called
		JS_FS("group",		js_console_group,	0, 0, 0),
		groupCollapsed()	Creates a new inline group in the console. However, the new group is created collapsed. The user will need to use the disclosure button to expand it
		JS_FS("groupCollapsed",	js_console_groupCollapsed, 0, 0, 0),
		groupEnd()	Exits the current inline group in the console
		JS_FS("groupEnd",	js_console_groupEnd,	0, 0, 0),
		info()	Outputs an informational message to the console
		JS_FS("info",		js_console_info,	0, 0, 0),
		table()	Displays tabular data as a table
		JS_FS("table",		js_console_table,	0, 0, 0),
		time()	Starts a timer (can track how long an operation takes)
		JS_FS("time",		js_console_time,	0, 0, 0),
		timeEnd()	Stops a timer that was previously started by console.time()
		JS_FS("timeEnd",	js_console_timeEnd,	0, 0, 0),
		trace()	Outputs a stack trace to the console
		JS_FS("trace",		js_console_trace,	0, 0, 0),
		warn()	Outputs a warning message to the console
		JS_FS("warn",		js_console_warn,	0, 0, 0),
#endif
		{ 0 }
	};
	JSObject *obj;

	obj = JS_InitClass(cx, global_object, 0, &console_class, 0, 0, 0, console_funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize console class");
		return 0;
	}
	return obj;
}

#if 0
static JSFunctionSpec console_static_methods[] = {
    {0,0,0,0,0}
};
 
	obj = JS_NewObject(cx, &console_class, NULL, JS_GetGlobalObject(obj));
	if (!obj) return NULL;

	if (!JS_DefineProperty(cx, obj, "JSON", OBJECT_TO_JSVAL(obj), JS_PropertyStub, JS_PropertyStub, JSPROP_ENUMERATE))
		return NULL;

	if (!JS_DefineFunctions(cx, obj, console_static_methods)) return NULL;

	return JSON;
}
#endif
