
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define DEBUG_GLOBAL 1
#define dlevel 6
#ifdef XP_WIN
#define USE_MALLINFO 0
#else
#define USE_MALLINFO 1
#endif

/* Malloc must appear first */
#include <malloc.h>

#ifdef DEBUG
#undef DEBUG
#define DEBUG DEBUG_GLOBAL
#endif
#include "debug.h"

#include <string.h>
#include <stdlib.h>
#ifdef WINDOWS
#include <winsock2.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <libgen.h>

#include "jsapi.h"
#include "jscntxt.h"
#include "jsengine.h"
#include "jsprintf.h"
#include "jsprf.h"
#include "jsstddef.h"
#include "jsinterp.h"
#include "jsscript.h"
#include "jsdbgapi.h"
#include "jsdtracef.h"
#include "jsengine.h"
#include "jsscan.h"
#include "jsarena.h"
#include "jsgc.h"
#include "jswindow.h"

static int version = 182;
#define VERSION_MAJOR 1
#define VERSION_MINOR 8
#define VERSION_RELEASE 2
#define XSTR(n) #n
#define STR(n) XSTR(n)
#define VERSION_STRING STR(VERSION_MAJOR)"."STR(VERSION_MINOR)"-"STR(VERSION_RELEASE)

enum GLOBAL_PROPERTY_ID {
	GLOBAL_PROPERTY_ID_SCRIPT_NAME=1,
};

static char *_getstr(JSContext *cx, jsval val) {
	JSString *str;

	str = JS_ValueToString(cx, val);
	if (!str) return 0;
	return JS_EncodeString(cx, str);
}

static JSBool _error(JSContext *cx, char *fmt, ...) {
	char msg[1024];
	va_list ap;
	va_start(ap,fmt);
	vsnprintf(msg,sizeof(msg)-1,fmt,ap);
	va_end(ap);
	JS_ReportError(cx, msg);
	return JS_FALSE;
}

static const char *get_script_name(JSContext *cx) {
	JSStackFrame *fp;

	/* Get the currently executing script's name. */
	fp = JS_GetScriptedCaller(cx, NULL);
	if (!fp || !fp->script || !fp->script->filename) {
		dprintf(dlevel,"unable to get current script!\n");
		return "unknown";
	}
	return(fp->script->filename);
}

static JSBool global_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;

//	dprintf(0,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case GLOBAL_PROPERTY_ID_SCRIPT_NAME:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,get_script_name(cx)));
			break;
		default:
			JS_ReportError(cx, "global_getprop: internal error: property %d not found", prop_id);
			return JS_FALSE;
		}
#if 0
        } else if (JSVAL_IS_STRING(id)) {
		char *name;
		jsval val;
		JSBool ok;

		name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
		dprintf(0,"name: %s\n", name);
//		ok = JS_GetProperty(cx,obj,name,&val);
//		if (ok) *rval = val;
		if (name) JS_free(cx,name);
#endif
        }
	return JS_TRUE;
}

static JSBool js_print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	JSEngine *e;
	uintN i;
	JSString *str;
	char *bytes;

	e = JS_GetPrivate(cx, obj);

	for (i = 0; i < argc; i++) {
//		dprintf(dlevel,"argv[%d]: %s\n", i, jstypestr(cx,argv[i]));
		str = JS_ValueToString(cx, argv[i]);
		if (!str) return JS_FALSE;
		bytes = JS_EncodeString(cx, str);
		if (bytes) e->output("%s",bytes);
		JS_free(cx, bytes);
	}

	return JS_TRUE;
}

static int js_error(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	char msg[1024];
	JSString *str;
	char *bytes;
	uintN i;

	msg[0] = 0;
	for (i = 0; i < argc; i++) {
//		dprintf(dlevel,"argv[%d]: %s\n", i, jstypestr(cx,argv[i]));
		str = JS_ValueToString(cx, argv[i]);
		if (!str) return JS_FALSE;
		bytes = JS_EncodeString(cx, str);
		if (bytes) strcat(msg,bytes);
		JS_free(cx, bytes);
	}
	JS_ReportError(cx,msg);
	return JS_TRUE;
}

static JSBool js_load(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	JSEngine *e;
	jsval *argv = vp + 2;
	JSString *str;
	int i;
	char *filename;
	char fixedname[1024];

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	e = JS_GetPrivate(cx, obj);
//printf("argc: %d\n", argc);

	for (i = 0; i < argc; i++) {
		str = JS_ValueToString(cx, argv[i]);
//printf("str: %p\n", str);
		if (!str) return JS_FALSE;
		filename = JS_EncodeString(cx, str);
//printf("filename: %s\n", filename);
		strncpy(fixedname,filename,sizeof(fixedname)-1);
		JS_free(cx,filename);
		fixpath(fixedname,sizeof(fixedname)-1);
		if (_JS_EngineExec(e, fixedname, cx, 0, 0, 0, 0)) {
//			JS_ReportError(cx, "load(%s) failed\n",fixedname);
			return JS_FALSE;
		}
	}
	return JS_TRUE;
}

static JSBool js_run(JSContext *cx, uintN argc, jsval *vp) {
	char *name, *func;
	JSObject *obj;
	JSEngine *e;
	jsval *argv = JS_ARGV(cx,vp);
	char fixedname[256],fname[256];
	int req,r;

//dprintf(0,"start: %ld\n", mem_used());
	if (argc < 1) {
		JS_ReportError(cx, "run takes 2 arguments (script:string, optional-function:string)\n");
		return 1;
	}
	name = func = 0;
	// Memory leak in JS_ConvertArguments?
//	if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "s / s", &name, &func)) return JS_FALSE;
	name = _getstr(cx,argv[0]);
	if (!name) return _error(cx,"js_run: name is null!");
	if (argc > 1) {
		func = _getstr(cx,argv[1]);
		if (!func) {
			JS_free(cx,name);
			return _error(cx,"js_run: func is null!");
		}
	}
	dprintf(dlevel,"name: %s, func: %s\n", name, func);

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	e = JS_GetPrivate(cx, obj);
	strncpy(fixedname,name,sizeof(fixedname)-1);
	fixpath(fixedname,sizeof(fixedname)-1);
	req = 1;
	if (!func) {
		register char *p;

		strncpy(fname,basename(name),sizeof(fname)-1);
		while(1) {
			p = strrchr(fname,'.');
			if (!p) break;
			*p = 0;
		}
		strcat(fname,"_main");
		func = fname;
		req = 0;
	} 
//	*vp = INT_TO_JSVAL(_JS_EngineExec(e, fixedname, cx, func, req));
	r =  _JS_EngineExec(e, fixedname, cx, func, 0, 0, req);
	JS_free(cx,name);
	if (argc > 1) JS_free(cx,func);
        return (r ? JS_FALSE : JS_TRUE);
}

static JSBool js_system(JSContext *cx, uintN argc, jsval *vp) {
	char *cmd;
	int r;

	if (argc < 1) {
		JS_ReportError(cx, "run takes 1 argument (script:string)\n");
		return JS_FALSE;
	}
	cmd = 0;
	if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "s", &cmd)) return JS_FALSE;
	dprintf(dlevel,"cmd: %s\n", cmd);
	if (!cmd) {
		JS_ReportError(cx, "run takes 1 argument (script:string)\n");
		return JS_FALSE;
	}

	r = system(cmd);
	*vp = INT_TO_JSVAL(r);
        return JS_TRUE;
}

static JSBool js_exit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {

	dprintf(dlevel,"argc: %d\n", argc);

	if (argc != 0) {
		dprintf(dlevel,"defining prop...\n");
		JS_DefineProperty(cx, JS_GetGlobalObject(cx), "exit_code", argv[0] ,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
	} else {
		*rval = JSVAL_VOID;
	}

        return JS_FALSE;
}

static JSBool js_abort(JSContext *cx, uintN argc, jsval *vp) {
	jsval *argv = JS_ARGV(cx,vp);

	dprintf(dlevel,"argc: %d\n", argc);
	if (argc && JSVAL_IS_INT(argv[0])) exit(JSVAL_TO_INT(argv[0]));
	else exit(0);

}

static JSBool js_objname(JSContext *cx, uintN argc, jsval *vp) {
	jsval *argv = JS_ARGV(cx,vp);
	JSObject *obj;
	char *name;

	dprintf(dlevel,"argc: %d\n", argc);
	if (argc != 1) {
		JS_ReportError(cx, "objname takes 1 argument: object\n");
		return JS_FALSE;
	}
	dprintf(dlevel,"type: %s\n", jstypestr(cx,argv[0]));
	obj = JSVAL_TO_OBJECT(argv[0]);
	name = JS_GetObjectName(cx,obj);
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,name));
	JS_free(cx,name);

	return JS_TRUE;
}

static JSBool js_sleep(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	int r,n;

	dprintf(dlevel,"argc: %d\n", argc);
	if (!argc) return JS_TRUE;
	n = JSVAL_TO_INT(argv[0]);
	dprintf(dlevel,"n: %d\n", n);

#ifdef XP_WIN
	Sleep(n);
	r = 0;
#else
	r = (sleep(n) != 0);
#endif
	*rval = BOOLEAN_TO_JSVAL(r);
	return JS_TRUE;
}

static JSBool js_time(JSContext *cx, uintN argc, jsval *vp){
	*vp = INT_TO_JSVAL(time(0));
	return JS_TRUE;
}

/*
 * function readline()
 * Provides a hook for scripts to read a line from stdin.
 */
static JSBool
ReadLine(JSContext *cx, uintN argc, jsval *vp)
{
#define BUFSIZE 256
    FILE *from;
    char *buf, *tmp;
    size_t bufsize, buflength, gotlength;
    JSBool sawNewline;
    JSString *str;

    from = stdin;
    buflength = 0;
    bufsize = BUFSIZE;
    buf = (char *) JS_malloc(cx, bufsize);
    if (!buf)
        return JS_FALSE;

    sawNewline = JS_FALSE;
    while ((gotlength = js_fgets(buf + buflength, bufsize - buflength, from)) > 0) {
        buflength += gotlength;

        /* Are we done? */
        if (buf[buflength - 1] == '\n') {
            buf[buflength - 1] = '\0';
            sawNewline = JS_TRUE;
            break;
        } else if (buflength < bufsize - 1) {
            break;
        }

        /* Else, grow our buffer for another pass. */
        bufsize *= 2;
        if (bufsize > buflength) {
            tmp = (char *) JS_realloc(cx, buf, bufsize);
        } else {
            JS_ReportOutOfMemory(cx);
            tmp = NULL;
        }

        if (!tmp) {
            JS_free(cx, buf);
            return JS_FALSE;
        }

        buf = tmp;
    }

    /* Treat the empty string specially. */
    if (buflength == 0) {
        *vp = feof(from) ? JSVAL_NULL : JS_GetEmptyStringValue(cx);
        JS_free(cx, buf);
        return JS_TRUE;
    }

    /* Shrink the buffer to the real size. */
    tmp = (char *) JS_realloc(cx, buf, buflength);
    if (!tmp) {
        JS_free(cx, buf);
        return JS_FALSE;
    }

    buf = tmp;

    /*
     * Turn buf into a JSString. Note that buflength includes the trailing null
     * character.
     */
    str = JS_NewStringCopyN(cx, buf, sawNewline ? buflength - 1 : buflength);
    JS_free(cx, buf);
    if (!str)
        return JS_FALSE;

    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool js_pround(JSContext *cx, uintN argc, jsval *rval) {
	char value[32], format[8];
	double d;
	int p;

	if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,rval), "d i", &d, &p)) return JS_FALSE;
	if (p < 0) p *= (-1);
//	dprintf(0,"num: %lf, places: %d\n", d, p);
	snprintf(format,sizeof(format),"%%.%df",p);
//	dprintf(0,"format: %s\n", format);
	sprintf(value,format,d);
//	dprintf(0,"value: %s\n", value);
	JS_NewDoubleValue(cx, strtod(value,0), rval);
	return JS_TRUE;
}

static JSBool js_timestamp(JSContext *cx, uintN argc, jsval *vp) {
	char ts[32];
	int local;

	if (argc) {
		if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "d", &local)) return JS_FALSE;
	} else {
		local = 1;
	}
	if (get_timestamp(ts,sizeof(ts),local)) return JS_FALSE;
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,ts));
	return JS_TRUE;
}

static JSBool js_dirname(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	char *path;

	if (argc != 1) {
		JS_ReportError(cx,"dirname requires 1 argument (path:string)\n");
		return JS_FALSE;
	}
	if (!JS_ConvertArguments(cx, argc, argv, "s", &path)) return JS_FALSE;
	if (!path) {
		JS_ReportError(cx,"path is null!");
		return JS_FALSE;
	}
//	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,dirname(path)));
	*rval = STRING_TO_JSVAL(JS_InternString(cx,dirname(path)));
	JS_free(cx,path);
	return JS_TRUE;
}

static JSBool js_basename(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	char *path;

	if (argc != 1) {
		JS_ReportError(cx,"basename requires 1 argument (path:string)\n");
		return JS_FALSE;
	}
	if (!JS_ConvertArguments(cx, argc, argv, "s", &path)) return JS_FALSE;
	if (!path) {
		JS_ReportError(cx,"path is null!");
		return JS_FALSE;
	}
	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,basename(path)));
	return JS_TRUE;
}

static JSBool js_log_info(JSContext *cx, uintN argc, jsval *vp) {
	return js_log_write(LOG_INFO,cx,argc,vp);
}

static JSBool js_log_warning(JSContext *cx, uintN argc, jsval *vp) {
	return js_log_write(LOG_WARNING,cx,argc,vp);
}

static JSBool js_log_error(JSContext *cx, uintN argc, jsval *vp) {
	return js_log_write(LOG_ERROR,cx,argc,vp);
}

static JSBool js_log_syserror(JSContext *cx, uintN argc, jsval *vp) {
	return js_log_write(LOG_SYSERR,cx,argc,vp);
}

static JSBool js_global_log_write(JSContext *cx, uintN argc, jsval *vp) {
	jsval *argv = vp + 2;
	int flags;

	if (argc < 2) {
		JS_ReportError(cx,"log_write requires 2 arguments: flags (number), message (string)\n");
		return JS_FALSE;
	}
	jsval_to_type(DATA_TYPE_INT,&flags,sizeof(flags),cx,argv[0]);
	dprintf(0,"flags: %x\n", flags);
	return js_log_write(flags,cx,argc,vp);
}

static JSBool js_gc(JSContext *cx, uintN argc, jsval *vp) {
	JS_GC(cx);
	return JS_TRUE;
}

static JSBool js_memused(JSContext *cx, uintN argc, jsval *vp) {
//	JS_DumpArenaStats(stdout);
	*vp = INT_TO_JSVAL(JS_ArenaTotalBytes());
	return JS_TRUE;
}

static JSBool js_sysmemused(JSContext *cx, uintN argc, jsval *vp) {
#if USE_MALLINFO
	struct mallinfo mi = mallinfo();
	*vp = INT_TO_JSVAL(mi.uordblks);
#else
	*vp = INT_TO_JSVAL(mem_used());
#endif
	return JS_TRUE;
}

static JSBool js_version(JSContext *cx, uintN argc, jsval *vp) {
	*vp = INT_TO_JSVAL(version);
	return JS_TRUE;
}

static JSClass global_class = {
	"global",		/* Name */
	JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	global_getprop,		/* getProperty */
	JS_PropertyStub,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *JS_CreateGlobalObject(JSContext *cx, void *priv) {
	JSPropertySpec global_props[] = {
		{ "script_name",GLOBAL_PROPERTY_ID_SCRIPT_NAME, JSPROP_ENUMERATE | JSPROP_READONLY },
		{0}
	};
	JSFunctionSpec global_funcs[] = {
		JS_FS("printf",JS_Printf,0,0,0),
		JS_FS("sprintf",JS_SPrintf,0,0,0),
		JS_FS("dprintf",JS_DPrintf,1,1,0),
		JS_FS("putstr",js_print,0,0,0),
		JS_FS("print",js_print,0,0,0),
		JS_FS("error",js_error,0,0,0),
		JS_FN("load",js_load,1,1,0),
		JS_FN("include",js_load,1,1,0),
		JS_FN("system",js_system,1,1,0),
		JS_FN("run",js_run,1,1,0),
		JS_FS("sleep",js_sleep,1,0,0),
		JS_FS("exit",js_exit,1,1,0),
		JS_FN("abort",js_abort,1,1,0),
		JS_FN("quit",js_abort,0,0,0),
		JS_FN("pround",js_pround,2,2,0),
		JS_FN("readline",ReadLine,0,0,0),
		JS_FN("objname",js_objname,1,1,0),
		JS_FN("timestamp",js_timestamp,0,1,0),
		JS_FN("systime",js_time,0,0,0),
		JS_FS("dirname",js_dirname,1,0,0),
		JS_FS("basename",js_basename,1,0,0),
		JS_FN("log_info",js_log_info,0,0,0),
		JS_FN("log_warning",js_log_warning,0,0,0),
		JS_FN("log_error",js_log_error,0,0,0),
		JS_FN("log_syserror",js_log_syserror,0,0,0),
		JS_FN("log_write",js_global_log_write,1,1,0),
		JS_FN("gc",js_gc,0,0,0),
		JS_FN("version",js_version,1,1,0),
		JS_FN("memused",js_memused,0,0,0),
		JS_FN("sysmemused",js_sysmemused,0,0,0),
		JS_FS_END
	};
	JSConstantSpec global_const[] = {
		JS_NUMCONST(VERSION_MAJOR),
		JS_NUMCONST(VERSION_MINOR),
		JS_NUMCONST(VERSION_RELEASE),
		JS_STRCONST(VERSION_STRING),
		{0}
	};
	JSAliasSpec global_aliases[] = {
		{ 0 },
	};
	JSObject *obj;
	char *msg = 0;

	/* Create the global object */
	dprintf(dlevel,"Creating global object...\n");
	obj = JS_NewObject(cx, &global_class, 0, 0);
	if (!obj) {
		msg = "creating global object";
		goto _create_global_error;
	}
	dprintf(dlevel,"Global object: %p\n", obj);
	JS_SetParent(cx, obj, NULL);
	JS_SetGlobalObject(cx, obj);

	dprintf(dlevel,"Defining global props...\n");
	if(!JS_DefineProperties(cx, obj, global_props)) {
		msg = "defining global properties";
		goto _create_global_error;
	}

	dprintf(dlevel,"Defining global funcs...\n");
	if(!JS_DefineFunctions(cx, obj, global_funcs)) {
		msg = "defining global functions";
		goto _create_global_error;
	}

	dprintf(dlevel,"Defining global constants...\n");
	if(!JS_DefineConstants(cx, obj, global_const)) {
		msg = "error defining global constants";
		goto _create_global_error;
	}

	dprintf(dlevel,"Defining global aliases...\n");
	if(!JS_DefineAliases(cx, obj, global_aliases)) {
		msg = "error defining global aliases";
		goto _create_global_error;
	}

#if 0
	dprintf(dlevel,"Defining window property...\n");
	if (!JS_DefineProperty(cx, obj, "window", OBJECT_TO_JSVAL(obj), 0, 0, JSPROP_ENUMERATE)) {
		msg = "defining window property";
		goto _create_global_error;
	}
#endif

	dprintf(dlevel,"Setting global private...\n");
	JS_SetPrivate(cx,obj,priv);
	JS_DefineProperty(cx, obj, "global", OBJECT_TO_JSVAL(obj), 0, 0, JSPROP_ENUMERATE);

	dprintf(dlevel,"Adding window props/funcs\n");
	if(!JS_InitWindow(cx, obj, priv)) {
		msg = "initializing root window";
		goto _create_global_error;
	}

	/* Add standard classes */
	dprintf(dlevel,"Adding standard classes...\n");
	if(!JS_InitStandardClasses(cx, obj)) {
		msg = "initializing standard classes";
		goto _create_global_error;
	}

	dprintf(dlevel,"done!\n");
	return obj;

_create_global_error:
	if (msg) log_error(msg);
	return 0;
}
