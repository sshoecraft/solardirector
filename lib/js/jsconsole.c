
#define dlevel 4
#include "debug.h"

#include "jsapi.h"
#include "jsinterp.h"
#include "jsprintf.h"

#if 0
assert()	Writes an error message to the console if the assertion is false
clear()	Clears the console
count()	Logs the number of times that this particular call to count() has been called
error()	Outputs an error message to the console
group()	Creates a new inline group in the console. This indents following console messages by an additional level, until console.groupEnd() is called
groupCollapsed()	Creates a new inline group in the console. However, the new group is created collapsed. The user will need to use the disclosure button to expand it
groupEnd()	Exits the current inline group in the console
info()	Outputs an informational message to the console
log()	Outputs a message to the console
table()	Displays tabular data as a table
time()	Starts a timer (can track how long an operation takes)
timeEnd()	Stops a timer that was previously started by console.time()
trace()	Outputs a stack trace to the console
warn()	Outputs a warning message to the console
#endif

//	JSCLASS_HAS_PRIVATE,
static JSClass js_console_class = {
	"console",
	0,
	JS_PropertyStub,
	JS_PropertyStub,
	JS_PropertyStub,
	JS_PropertyStub,
	JS_EnumerateStub,
	JS_ResolveStub,
	JS_ConvertStub,
	JS_FinalizeStub,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static jsval *_mkvp(JSContext *cx, void *mark, uintN argc, jsval *vp) {
	char format[96],*p;
	jsval *argv,*newvp;
	int i;

	dprintf(4,"argc: %d\n", argc);
	p = format;
	*p = 0;
	newvp = js_AllocStack(cx, 3 + argc, mark);
	if (!newvp) return 0;

	newvp[0] = vp[0];
	newvp[1] = vp[1];
	for(i = 0; i < argc; i++) {
		if (i > 31) break;
		p += sprintf(p,"%%s ");
	}
	dprintf(4,"format: %s\n", format);
	newvp[2] = type_to_jsval(cx,DATA_TYPE_STRING,format,strlen(format));
    	argv = vp + 2;
    	memcpy(newvp + 3, argv, argc * sizeof *argv);
	return newvp;
}

static JSBool js_console_log(JSContext *cx, uintN argc, jsval *vp) {
	jsval *newvp;
	void *mark;
	int r;

	newvp = _mkvp(cx,&mark,argc,vp);
	r = js_log_write(LOG_INFO|LOG_NEWLINE,cx,argc+1,newvp);
	js_FreeStack(cx, mark);
	return r;
}

static JSBool js_console_error(JSContext *cx, uintN argc, jsval *vp) {
	jsval *newvp;
	void *mark;
	int r;

	newvp = _mkvp(cx,&mark,argc,vp);
	r = js_log_write(LOG_ERROR|LOG_NEWLINE,cx,argc+1,newvp);
	js_FreeStack(cx, mark);
	return r;
}

JSObject * js_InitConsoleClass(JSContext *cx, JSObject *parent) {
	JSObject *obj;
	JSPropertySpec props[] = {
		{ 0 }
	};
	JSFunctionSpec funcs[] = {
		JS_FN("log",js_console_log,1,1,0),
		JS_FN("error",js_console_error,1,1,0),
		{ 0 }
	};

	obj = JS_InitClass(cx, parent, 0, &js_console_class, 0, 0, props, funcs, 0, 0);
        if (!obj) {
                JS_ReportError(cx,"unable to initialize %s class", js_console_class.name);
                return 0;
        }
//	if (!JS_DefineProperty(cx, obj, "console", OBJECT_TO_JSVAL(obj), JS_PropertyStub, JS_PropertyStub, 0)) return NULL;
//	if (!JS_DefineFunctions(cx, obj, console_static_methods)) return NULL;

	return obj;
}
