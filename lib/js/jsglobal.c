
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 4
#include "debug.h"

#if 0
#ifdef XP_WIN
#define USE_MALLINFO 0
#else
#define USE_MALLINFO 0
#endif

/* Malloc must appear first */
#include <malloc.h>
#endif

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
#include "jsarray.h"
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
#include "jswindow_defs.h"
#include "json.h"

static int version = 182;
#define VERSION_MAJOR 1
#define VERSION_MINOR 8
#define VERSION_RELEASE 2
#define XSTR(n) #n
#define STR(n) XSTR(n)
#define VERSION_STRING STR(VERSION_MAJOR)"."STR(VERSION_MINOR)"-"STR(VERSION_RELEASE)

enum GLOBAL_PROPERTY_ID {
	GLOBAL_PROPERTY_ID_SCRIPT_NAME=1,
	GLOBAL_PROPERTY_ID_MEMINFO,
#if defined(DEBUG) && DEBUG > 0
	GLOBAL_PROPERTY_ID_DEBUG,
#endif
	JSWINDOW_PROPIDS,
	JSWINDOW_PROPERTY_ID_LOCATION,
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

const char *get_script_name(JSContext *cx) {
	JSStackFrame *fp;

	/* Get the currently executing script's name. */
	fp = JS_GetScriptedCaller(cx, NULL);
	if (!fp || !fp->script || !fp->script->filename) {
		dprintf(dlevel,"unable to get current script!\n");
		return "unknown";
	}
	return(fp->script->filename);
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
		if (_JS_EngineExec(e, fixedname, cx, 0, 0, 0, 0)) {
//			JS_ReportError(cx, "load(%s) failed\n",fixedname);
			return JS_FALSE;
		}
	}
	return JS_TRUE;
}

static JSBool js_run(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	JSEngine *e;
	jsval *argv = JS_ARGV(cx,vp);
	char *filename, *func;
	char funcname[1024];
	int r,req;
	jsval fval,rval;
        JSBool ok;

	if (argc < 1) {
		JS_ReportError(cx, "run takes 2 arguments (script:string, optional-function:string)\n");
		return 1;
	}

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	e = JS_GetPrivate(cx, obj);

	filename = _getstr(cx,argv[0]);
	if (argc > 1) {
		func = _getstr(cx,argv[1]);
		if (!func) {
			JS_free(cx,filename);
			return _error(cx,"js_run: func is null!");
		}
	} else {
		func = 0;
	}
	dprintf(dlevel,"filename: %s, func: %s\n", filename, func);

	/* Load the file */
	r = _JS_EngineExec(e, filename, cx, 0, 0, 0, 0);
	dprintf(dlevel,"r: %d\n", r);
	if (r) {
//		JS_ReportError(cx, "load(%s) failed\n",fixedname);
		JS_free(cx,filename);
		if (func) JS_free(cx,func);
		return JS_FALSE;
	}

	if (!func) {
		register char *p;

		strncpy(funcname,basename(filename),sizeof(funcname)-1);
		while(1) {
			p = strrchr(funcname,'.');
			if (!p) break;
			*p = 0;
		}
		strcat(funcname,"_main");
		func = funcname;
		req = 0;
	} else {
		req = 1;
		func = func;
	} 
	dprintf(dlevel,"func: %s, req: %d\n", func, req);

	/* Get the function */
        ok = JS_GetProperty(cx, obj, func, &fval);
        dprintf(dlevel,"getprop ok: %d, req: %d\n", ok, req);
        if (!ok && req) return 1;
	dprintf(dlevel,"fval: %s\n", jstypestr(cx,fval));
	if (JSVAL_IS_VOID(fval)) {
		dprintf(dlevel,"req: %d\n", req);
		if (req) {
			JS_ReportError(cx,"run %s: function %s not found\n", filename, func);
			r = 1;
		}
	} else {
		/* Call the function */
		rval = JSVAL_VOID;
		ok = JS_CallFunctionValue(cx, JS_GetGlobalObject(cx), fval, 0, 0, &rval);
		dprintf(dlevel,"ok: %d, rval: %x (%s)\n", ok, rval, jstypestr(cx,rval));
		if (!ok) {
			r = 1;
		} else if (rval != JSVAL_VOID) {
			JS_ValueToInt32(cx,rval,&r);
		}
	}

	dprintf(dlevel,"r: %d\n", r);
	*vp = INT_TO_JSVAL(r);
	JS_free(cx,filename);
	if (req) JS_free(cx,func);
	return JS_TRUE;
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

#if 0
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
dprintf(-1,"JS_GetObjectName!\n");
	name = JS_GetObjectName(cx,obj);
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,name));
	JS_free(cx,name);

	return JS_TRUE;
}
#endif

static JSBool js_isarray(JSContext *cx, uintN argc, jsval *vp) {
	jsval *argv = JS_ARGV(cx,vp);
	JSObject *obj;
	int val;

	dprintf(dlevel,"argc: %d\n", argc);
	if (argc != 1) {
		JS_ReportError(cx, "objname takes 1 argument: object\n");
		return JS_FALSE;
	}
//	dprintf(dlevel,"type: %s\n", jstypestr(cx,argv[0]));
	obj = JSVAL_TO_OBJECT(argv[0]);
	if (!obj) val = false;
	else val = (OBJ_IS_ARRAY(cx,obj) || OBJ_IS_DENSE_ARRAY(cx, obj)) ? true : false;
	dprintf(dlevel,"val: %d\n", val);
	*vp = BOOLEAN_TO_JSVAL(val);
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

static JSBool js_delay(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	int r,n;

	dprintf(dlevel,"argc: %d\n", argc);
	if (!argc) return JS_TRUE;
	n = JSVAL_TO_INT(argv[0]);
	dprintf(dlevel,"n: %d\n", n);

#ifdef XP_WIN
	Delay(n);
	r = 0;
#else
	r = (usleep(n*1000) != 0);
#endif
	*rval = BOOLEAN_TO_JSVAL(r);
	return JS_TRUE;
}

static JSBool js_time(JSContext *cx, uintN argc, jsval *vp){
	*vp = NUMBER_TO_JSVAL(time(0));
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
	JS_free(cx,path);
	return JS_TRUE;
}

static JSBool js_test_json(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	char *j;
	json_value_t *v;
	int ok;

	if (argc != 1) {
		JS_ReportError(cx,"test_json requires 1 argument (json:string)\n");
		return JS_FALSE;
	}

	j = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "s", &j)) return JS_FALSE;
	dprintf(dlevel+1,"j: %p\n", j);
	if (!j) {
		*rval = BOOLEAN_TO_JSVAL(false);
		return JS_TRUE;
	}
	dprintf(dlevel+1,"j: %s\n", j);
	v = json_parse(j);
	ok = (v ? true : false);
	json_destroy_value(v);
	*rval = BOOLEAN_TO_JSVAL(ok);
	ok = ok;
	JS_free(cx,j);
	return JS_TRUE;
}

static JSBool js_log_info(JSContext *cx, uintN argc, jsval *vp) {
	return js_log_write(LOG_INFO,cx,argc,vp);
}

static JSBool js_log_warning(JSContext *cx, uintN argc, jsval *vp) {
	return js_log_write(LOG_WARNING,cx,argc,vp);
}

static JSBool js_log_verbose(JSContext *cx, uintN argc, jsval *vp) {
	return js_log_write(LOG_VERBOSE,cx,argc,vp);
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
	dprintf(dlevel,"flags: %x\n", flags);
	return js_log_write(flags,cx,argc,vp);
}

#if 0
static JSBool js_setTimeout(JSContext *cx, uintN argc, jsval *rval) {
	char *s;
	int i;

	if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,rval), "s i", &s, &i)) return JS_FALSE;
	dprintf(-1,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx,"js_setTimer requires 2 arguments: command (string), interval (int)\n");
		return JS_FALSE;
	}
	dprintf(-1,"s: %s\n", s);
	return JS_TRUE;
}
#endif

static JSBool js_gc(JSContext *cx, uintN argc, jsval *vp) {
	JS_GC(cx);
	return JS_TRUE;
}

static JSBool js_memused(JSContext *cx, uintN argc, jsval *vp) {
//	JS_DumpArenaStats(stdout);
	*vp = INT_TO_JSVAL(JS_ArenaTotalBytes());
	return JS_TRUE;
}

#if 0
static JSBool js_meminfo(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *global,*obj;
	JSEngine *e;

	global = JS_GetGlobalObject(cx);
	e = JS_GetPrivate(cx,global);
	obj = JS_NewObject(cx,0,0,0);
	JS_DefineProperty(cx, obj, "allocated", INT_TO_JSVAL((int)e->rt->gcBytes), 0, 0, 0);
	JS_DefineProperty(cx, obj, "total", INT_TO_JSVAL((int)e->rt->gcMaxBytes), 0, 0, 0);

	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}
#endif

static JSBool js_dumpstats(JSContext *cx, uintN argc, jsval *vp) {
	JSEngine *e = JS_GetPrivate(cx,JS_GetGlobalObject(cx));
	js_DumpGCStats(e->rt,stdout);
	return JS_TRUE;
}

static JSBool js_DumpArenaStats(JSContext *cx, uintN argc, jsval *vp) {
	JS_DumpArenaStats(stdout);
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

JSBool js_window_setInterval(JSContext *cx, uintN argc, jsval *rval) {
	dprintf(dlevel,"setInterval called\n");
	return JS_TRUE;
}

#if 0
static JSClass meminfo_class = {
	"meminfo",		/* Name */
	JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	global_getprop,		/* getProperty */
	global_setprop,		/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};
#endif

int js_mem_used_pct(JSContext *cx) {
	JSEngine *e;
	int allocated, total, pct;

	e = JS_GetPrivate(cx,JS_GetGlobalObject(cx));
//	dprintf(dlevel,"e: %p\n", e);
	if (!e) return 0;
	allocated = (int)e->rt->gcBytes;
	total = (int)e->rt->gcMaxBytes;
//	dprintf(-1,"allocated: %d, total: %d\n", allocated, total);
	pct = ((float)allocated / (float)total) * 100.0;
//	dprintf(-1,"pct: %d\n", pct);
	return pct;
}

JSBool global_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	JSEngine *s;

	s = JS_GetPrivate(cx,obj);

//	dprintf(0,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case GLOBAL_PROPERTY_ID_SCRIPT_NAME:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,get_script_name(cx)));
			break;
		case GLOBAL_PROPERTY_ID_MEMINFO:
			{
			JSObject *global,*newobj;
			JSEngine *e;

			global = JS_GetGlobalObject(cx);
			e = JS_GetPrivate(cx,global);
			newobj = JS_NewObject(cx,0,0,0);
			JS_DefineProperty(cx, newobj, "allocated", INT_TO_JSVAL((int)e->rt->gcBytes), 0, 0, 0);
			JS_DefineProperty(cx, newobj, "total", INT_TO_JSVAL((int)e->rt->gcMaxBytes), 0, 0, 0);

			*rval = OBJECT_TO_JSVAL(newobj);
			}
			break;
#if defined(DEBUG) && DEBUG > 0
		case GLOBAL_PROPERTY_ID_DEBUG:
			*rval = INT_TO_JSVAL(debug);
			break;
#endif
		case JSWINDOW_PROPERTY_ID_LOCATION:
			dprintf(dlevel,"returning location...\n");
			if (!((jswindow_t *)s->private)->location_val) {
				jslocation_t *loc = malloc(sizeof(*loc));
				if (loc) {
					memset(loc,0,sizeof(*loc));
					loc->protocol = "file:";
					JSObject *locobj = js_location_new(cx,obj,loc);
					if (locobj) ((jswindow_t *)s->private)->location_val = OBJECT_TO_JSVAL(locobj);
				}
			}
			*rval = ((jswindow_t *)s->private)->location_val;
			break;
		JSWINDOW_GETPROP
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

JSBool global_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
//	JSEngine *e;

//	e = JS_GetPrivate(cx,obj);

//	dprintf(0,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
#if defined(DEBUG) && DEBUG > 0
		case GLOBAL_PROPERTY_ID_DEBUG:
			jsval_to_type(DATA_TYPE_INT,&debug,0,cx,*rval);
			break;
#endif
		}
        }
	return JS_TRUE;
}

static JSClass global_class = {
	"global",		/* Name */
	JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	global_getprop,		/* getProperty */
	global_setprop,		/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

void JS_GlobalShutdown(JSContext *cx) {
	JSEngine *e;
	jswindow_t *w;

	e = JS_GetPrivate(cx,JS_GetGlobalObject(cx));
	if (!e) return;
	w = e->private;
	dprintf(dlevel,"location_val: %x\n", w->location_val);
	if (w->location_val) {
		JSObject *lobj;
		void *p;

		lobj = JSVAL_TO_OBJECT(w->location_val);
		dprintf(dlevel,"lobj: %p\n", lobj);
		p = JS_GetPrivate(cx,lobj);
		dprintf(dlevel,"p: %p\n", p);
		free(p);
	}
}

/******************************** WINDOW ***********************************/

static jsnavigator_t nav = { };

static jsdocument_t doc = { };

static jswindow_t win = { };

#define EWINDOW_FUNCSPEC \

//mkstdclass(window,EWINDOW,JSEngine);

typedef JSObject *(js_newobj_t)(JSContext *cx, JSObject *parent, void *priv);
static void newgobj(JSContext *cx, char *name, js_newobj_t func, void *priv) {
	JSObject *newobj, *global = JS_GetGlobalObject(cx);
	jsval newval;

	newobj = func(cx, global, priv);
	dprintf(dlevel,"%s newobj: %p\n", name, newobj);
	if (newobj) {
		newval = OBJECT_TO_JSVAL(newobj);
		JS_DefineProperty(cx, global, name, newval, 0, 0, JSPROP_ENUMERATE);
	}
}

/* Add the window props/funcs to the global object */
char *js_define_window(JSContext *cx, JSObject *obj, JSEngine *e) {
	JSPropertySpec props[] = {
		JSWINDOW_PROPSPEC,
		{ "location",JSWINDOW_PROPERTY_ID_LOCATION,JSPROP_ENUMERATE| JSPROP_READONLY },
		{ 0 }
	};
	JSFunctionSpec funcs[] = {
		EWINDOW_FUNCSPEC
		JS_FN("setInterval",js_window_setInterval,0,0,0),
		{ 0 }
	};

//	nav.userAgent = "Mozilla/5.0 (X11; Linux x86_64; rv:102.0) Gecko/20100101 Firefox/102.0 debugger eval code:1:9";
	nav.userAgent = "Spidermonkey 1.8.2";
	newgobj(cx,"navigator",js_navigator_new,&nav);
	doc.compatMode = "BackCompat";
	newgobj(cx,"document",js_document_new,&doc);

	/* Set engine private to window private */
	e->private = &win;

	dprintf(dlevel,"Defining window props...\n");
	if(!JS_DefineProperties(cx, obj, props)) return "defining window properties";

	dprintf(dlevel,"Defining window funcs...\n");
	if(!JS_DefineFunctions(cx, obj, funcs)) return "defining window functions";

	dprintf(dlevel,"Defining window property...\n");
	if (!JS_DefineProperty(cx, obj, "window", OBJECT_TO_JSVAL(obj), 0, 0, 0)) 
		return "defining window property";

	return 0;
}

/******************************* init ****************************************/
JSObject *JS_CreateGlobalObject(JSContext *cx, void *priv) {
	JSPropertySpec global_props[] = {
		{ "script_name",GLOBAL_PROPERTY_ID_SCRIPT_NAME, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "meminfo",GLOBAL_PROPERTY_ID_MEMINFO, JSPROP_ENUMERATE | JSPROP_READONLY },
#if defined(DEBUG) && DEBUG > 0
		{ "debug",GLOBAL_PROPERTY_ID_DEBUG, JSPROP_ENUMERATE },
#endif
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
		JS_FS("delay",js_delay,1,0,0),
		JS_FS("exit",js_exit,1,1,0),
		JS_FN("abort",js_abort,1,1,0),
		JS_FN("quit",js_abort,0,0,0),
		JS_FN("pround",js_pround,2,2,0),
		JS_FN("readline",ReadLine,0,0,0),
//		JS_FN("objname",js_objname,1,1,0),
		JS_FN("isarray",js_isarray,1,1,0),
		JS_FN("timestamp",js_timestamp,0,1,0),
		JS_FN("time",js_time,0,0,0),
		JS_FS("dirname",js_dirname,1,0,0),
		JS_FS("basename",js_basename,1,0,0),
		JS_FS("test_json",js_test_json,1,0,0),
		JS_FN("log_info",js_log_info,0,0,0),
		JS_FN("log_warning",js_log_warning,0,0,0),
		JS_FN("log_verbose",js_log_verbose,0,0,0),
		JS_FN("log_error",js_log_error,0,0,0),
		JS_FN("log_syserror",js_log_syserror,0,0,0),
		JS_FN("log_write",js_global_log_write,1,1,0),
//		JS_FN("setTimeout",js_setTimeout,2,2,0),
		JS_FN("gc",js_gc,0,0,0),
		JS_FN("version",js_version,1,1,0),
		JS_FN("memused",js_memused,0,0,0),
		JS_FN("DumpArenaStats",js_DumpArenaStats,0,0,0),
		JS_FN("sysmemused",js_sysmemused,0,0,0),
//		JS_FN("meminfo",js_meminfo,0,0,0),
		JS_FN("dumpstats",js_dumpstats,0,0,0),
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
//	JS_DefineProperty(cx,obj,"debug",INT_TO_JSVAL(debug),0,0,JSPROP_ENUMERATE);

	dprintf(dlevel,"Defining global funcs...\n");
	if(!JS_DefineFunctions(cx, obj, global_funcs)) {
		msg = "defining global functions";
		goto _create_global_error;
	}

	dprintf(dlevel,"Defining global constants...\n");
	if(!JS_DefineConstants(cx, obj, global_const)) {
		msg = "defining global constants";
		goto _create_global_error;
	}

	dprintf(dlevel,"Defining global aliases...\n");
	if(!JS_DefineAliases(cx, obj, global_aliases)) {
		msg = "defining global aliases";
		goto _create_global_error;
	}

	dprintf(dlevel,"Setting global private...\n");
	JS_SetPrivate(cx,obj,priv);
	JS_DefineProperty(cx, obj, "global", OBJECT_TO_JSVAL(obj), 0, 0, JSPROP_ENUMERATE);

	msg = js_define_window(cx, obj, priv);
	if (msg) goto _create_global_error;

	/* Add standard classes */
	dprintf(dlevel,"Adding standard classes...\n");
	if(!JS_InitStandardClasses(cx, obj)) {
		msg = "initializing standard classes";
		goto _create_global_error;
	}

	dprintf(dlevel,"done!\n");
	return obj;

_create_global_error:
	if (msg) log_error("%s\n",msg);
	return 0;
}
#if 0
Window Object Methods
Method	Description
alert()	Displays an alert box with a message and an OK button
atob()	Decodes a base-64 encoded string
blur()	Removes focus from the current window
btoa()	Encodes a string in base-64
clearInterval()	Clears a timer set with setInterval()
clearTimeout()	Clears a timer set with setTimeout()
close()	Closes the current window
confirm()	Displays a dialog box with a message and an OK and a Cancel button
focus()	Sets focus to the current window
getComputedStyle()	Gets the current computed CSS styles applied to an element
getSelection()	Returns a Selection object representing the range of text selected by the user
matchMedia()	Returns a MediaQueryList object representing the specified CSS media query string
moveBy()	Moves a window relative to its current position
moveTo()	Moves a window to the specified position
open()	Opens a new browser window
print()	Prints the content of the current window
prompt()	Displays a dialog box that prompts the visitor for input
requestAnimationFrame()	Requests the browser to call a function to update an animation before the next repaint
resizeBy()	Resizes the window by the specified pixels
resizeTo()	Resizes the window to the specified width and height
scroll()	Deprecated. This method has been replaced by the scrollTo() method.
scrollBy()	Scrolls the document by the specified number of pixels
scrollTo()	Scrolls the document to the specified coordinates
setInterval()	Calls a function or evaluates an expression at specified intervals (in milliseconds)
setTimeout()	Calls a function or evaluates an expression after a specified number of milliseconds
stop()	Stops the window from loading
#endif
