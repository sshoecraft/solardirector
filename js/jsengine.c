
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define DEBUG_JSENGINE 1
#define dlevel 7
#define SCRIPT_CACHE 1

#ifdef DEBUG
#undef DEBUG
#define DEBUG DEBUG_JSENGINE
#endif
#include "debug.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>

#include "jsengine.h"
#include "jsprf.h"
#include "jsstddef.h"
#include "jsinterp.h"
#include "jsscript.h"
#include "jsdbgapi.h"
#include "jsdtracef.h"
#include "jsobj.h"
#include "jscntxt.h"

#if SCRIPT_CACHE
struct _scriptinfo {
	JSEngine *e;
	char filename[256];
	int modtime;
	JSScript *script;
//	JSObject *obj;
	JSTempValueRooter tvr;
	int exitcode;
};
typedef struct _scriptinfo scriptinfo_t;

struct js_initfuncinfo {
//	char *name;
	char name[64];
	int type;
	union {
		js_initfunc_t *func;
		js_initclass_t *class;
	};
	void *priv;
};
typedef struct js_initfuncinfo js_initfuncinfo_t;

static time_t _getmodtime(char *filename) {
	struct stat sb;

	dprintf(dlevel,"filename: %s\n", filename);
	if (stat(filename,&sb) < 0) return 0;
	return sb.st_mtime;
}

static scriptinfo_t *_getsinfo(JSEngine *e, char *filename, int add) {
	scriptinfo_t *sp;

	dprintf(dlevel,"filename: %s\n", filename);
	list_reset(e->scripts);
	while((sp = list_get_next(e->scripts)) != 0) {
		dprintf(dlevel,"sp: %p, sp->filename: %s\n", sp, sp->filename);
		if (strcmp(sp->filename,filename) == 0) {
			dprintf(dlevel,"found\n");
			return sp;
		}
	}
	dprintf(dlevel,"NOT found\n");
	if (add) {
		scriptinfo_t newsinfo;

		memset(&newsinfo,0,sizeof(newsinfo));
		newsinfo.e = e;
		strcpy(newsinfo.filename,filename);
		sp = list_add(e->scripts,&newsinfo,sizeof(newsinfo));
	}
	dprintf(dlevel,"sp: %p\n", sp);
	return sp;
}

static int _chkscript(JSContext *cx, scriptinfo_t *sp) {
	time_t mt;

	dprintf(dlevel,"scriptinfo: sp: %p, e: %p, filename: %s, modtime: %d, script: %p\n", sp,
		sp->e, sp->filename, sp->modtime, sp->script);

	mt = _getmodtime(sp->filename);
	dprintf(dlevel,"mt: %ld, modtime: %ld\n", mt, sp->modtime);
//	if (!sp->script || sp->modtime != mt) {
	if (sp->script && sp->modtime == mt) {
		return 2;
	} else {
		dprintf(dlevel,"script: %p\n", sp->script);
		if (sp->script) {
			/* Removing the root will cause GC to destroy the script */
			JS_RemoveRoot(cx, &sp->script->object);
		}
		JS_BeginRequest(cx);
		sp->script = JS_CompileFile(cx, JS_GetGlobalObject(cx), sp->filename);
		dprintf(dlevel,"script: %p\n", sp->script);
		if (!sp->script) {
			JS_EndRequest(cx);
			return 1;
		}
		JS_NewScriptObject(cx, sp->script);
		dprintf(dlevel,"script->object: %p\n", sp->script->object);
		if (!sp->script->object) {
			JS_DestroyScript(cx, sp->script);
			sp->script = 0;
			JS_EndRequest(cx);
			return 1;
		}
		if (!JS_AddNamedRoot(cx, &sp->script->object, sp->filename)) return 1;
		JS_EndRequest(cx);
		sp->modtime = mt;
	}
	return 0;
}
#endif

#if 0
static void _delsinfo(JSEngine *e, JSContext *cx, scriptinfo_t *sp) {
	scriptinfo_t *ssp;

	printf("********* XXXX\n");
	dprintf(dlevel,"sp: %p\n", sp);
	list_reset(e->scripts);
	while((ssp = list_get_next(e->scripts)) != 0) {
		dprintf(dlevel,"ssp: %p\n", ssp);
		if (ssp == sp) {
			dprintf(dlevel,"found\n");
			JS_RemoveRoot(cx, &sp->script->object);
			list_delete(e->scripts,ssp);
			return;
		}
	}
	dprintf(dlevel,"NOT found\n");
	return;
}
#endif

static void script_error(JSContext *cx, const char *message, JSErrorReport *report) {
	char line[1024], prefix[128], *p;
	JSEngine *e;
	js_outputfunc_t *output;
	int n,i,j,k;

	dprintf(dlevel,"cx: %p, message: %s, report: %p\n", cx, message, report);
	if (!report) return;

	if (debug >= dlevel) {
		log_info("%-20.20s: %s\n", "filename", report->filename);
		log_info("%-20.20s: %d\n", "lineno", report->lineno);
		log_info("%-20.20s: %s\n", "linebuf", report->linebuf);
		log_info("%-20.20s: %s\n", "linebuf", report->linebuf);
		log_info("%-20.20s: %s\n", "tokenptr", report->tokenptr);
		log_info("%-20.20s: %s\n", "uclinebuf", report->uclinebuf ? js_DeflateString(cx, report->uclinebuf, js_strlen(report->uclinebuf)) : "(null)");
		log_info("%-20.20s: %s\n", "uctokenptr", report->uctokenptr ? js_DeflateString(cx, report->uctokenptr, js_strlen(report->uctokenptr)) : "(null)");
		log_info("%-20.20s: %x\n", "flags", report->flags);
		log_info("%-20.20s: %d\n", "errorNumber", report->errorNumber);
		log_info("%-20.20s: %s\n", "ucmessage", report->ucmessage ? js_DeflateString(cx, report->ucmessage, js_strlen(report->ucmessage)) : "(null)");
	}

	/* Get output func */
	dprintf(dlevel,"cx: %p, global: %p\n", cx, JS_GetGlobalObject(cx));
	e = JS_GetPrivate(cx, JS_GetGlobalObject(cx));
	if (!e) {
		JS_ReportError(cx, "global private is null!");
		return;
	}
	dprintf(dlevel,"output: %p\n", e->output);
	output = e->output;
	if (!output) output = printf;

	if (report->filename) sprintf(prefix, "%s(%d): ", report->filename, report->lineno);
	else *prefix = 0;
	output("%s%s\n", prefix, message);
//	snprintf(e->errmsg,sizeof(e->errmsg),"%s%s", prefix, message);

	if (report->linebuf) {
		output("%s%s",prefix,(char *)report->linebuf);
		p = line;
		n = PTRDIFF(report->tokenptr, report->linebuf, char);
		for (i = j = 0; i < n; i++) {
			if (report->linebuf[i] == '\t') {
				for (k = (j + 8) & ~7; j < k; j++) {
					p += sprintf(p,".");
				}
				continue;
			}
			p += sprintf(p,".");
			j++;
		}
		p += sprintf(p,"^\n");
		output("%s%s",prefix,line);
	}
}

JSContext *JS_EngineNewContext(JSEngine *e) {
	JSContext *cx;
	js_initfuncinfo_t *f;
	JSObject *gobj;
	int r;

	dprintf(dlevel,"stacksize: %d\n",e->stacksize);
	cx = JS_NewContext(e->rt, e->stacksize);
	dprintf(dlevel,"cx: %p\n", cx);
	if (!cx) {
		dprintf(dlevel,"error creating context!\n");
		return 0;
	}

	JS_SetOptions(cx, JSOPTION_STRICT);
	JS_SetOptions(cx, JSOPTION_VAROBJFIX);

//	JS_SetContextCallback(sinfo->rt, ContextCallback);
	JS_SetErrorReporter(cx, script_error);

	gobj = JS_CreateGlobalObject(cx, e);
	if (!gobj) {
		dprintf(dlevel,"error creating global object\n");
		goto JS_EngineNewContext_error;
	}

	/* Add the initfuncs */
	dprintf(dlevel,"adding init funcs...\n");
	list_reset(e->initfuncs);
	while((f = list_get_next(e->initfuncs)) != 0) {
		dprintf(dlevel,"calling: init for %s\n",f->name);
		r = 0;
		if (f->type == 0) r = f->func(cx, gobj, f->priv);
		else if (f->type == 1) r = (f->class(cx, gobj) ? 0 : 1);
		if (r) {
			char msg[128];

			sprintf(msg,"%s init%s failed!\n",f->name,f->type ? "class" : "func");
			log_error(msg);
//			JS_ReportError(cx,msg);
			goto JS_EngineNewContext_error;
		}
	}
	return cx;

JS_EngineNewContext_error:
	dprintf(dlevel,"cx: %p\n", cx);
	if (cx) {
		dprintf(dlevel,"destroying CX!!\n");
//		JS_DestroyContext(cx);
	}
	return 0;
}

static JSContext *_getcx(JSEngine *e, int lock, int mknew) {
	if (!e) return 0;
	if (lock) {
		dprintf(dlevel,"locking global cx\n");
		pthread_mutex_lock(&e->lockcx);
	}
	if (!e->cx && mknew) e->cx = JS_EngineNewContext(e);
	dprintf(dlevel,"returning cx: %p\n", e->cx);
	return e->cx;
}

static void _relcx(JSEngine *e) {
	dprintf(dlevel,"unlocking global cx\n");
	pthread_mutex_unlock(&e->lockcx);
}

JSEngine *JS_EngineInit(int rtsize, int stacksize, js_outputfunc_t *output) {
	JSEngine *e;

	dprintf(dlevel,"rtsize: %d, stacksize: %d, output: %p\n", rtsize, stacksize, output);

	e = calloc(sizeof(*e),1);
	if (!e) return 0;

	dprintf(dlevel,"rtsize: %d\n", rtsize);
	e->rt = JS_NewRuntime(rtsize);
	if (!e->rt) {
		dprintf(dlevel,"error creating runtime\n");
		goto JS_EngineInit_error;
	}
	e->rtsize = rtsize;
	e->stacksize = stacksize;
	e->output = output;
	e->scripts = list_create();
	e->initfuncs = list_create();
	pthread_mutex_init(&e->lockcx, 0);

	dprintf(dlevel,"e: %p\n", e);
	return e;

JS_EngineInit_error:
	if (e->rt) JS_DestroyRuntime(e->rt);
	free(e);
	return 0;
}

JSEngine *JS_DupEngine(JSEngine *old) {
	JSEngine *e;

	dprintf(dlevel,"OLD: rtsize: %d, stacksize: %d, output: %p\n", old->rtsize, old->stacksize, old->output);
	e = JS_EngineInit(old->rtsize, old->stacksize, old->output);
	if (!e) return 0;
	list_destroy(e->initfuncs);
	e->initfuncs = list_dup(old->initfuncs);
	e->stacksize = old->stacksize;

	return e;
}

int JS_EngineDestroy(JSEngine *e) {
	scriptinfo_t *sp;
//	js_initfuncinfo_t *fp;

	dprintf(dlevel,"e: %p\n", e);
	if (!e) return 1;

	list_reset(e->scripts);
	while((sp = list_get_next(e->scripts)) != 0) {
		if (sp->script)
			JS_RemoveRoot(e->cx, &sp->script->object);
	}
	if (e->cx) JS_GC(e->cx);
	if (e->cx) JS_DestroyContext(e->cx);
	JS_DestroyRuntime(e->rt);
	list_destroy(e->scripts);
	list_destroy(e->initfuncs);
	free(e);
	return 1;
}

int JS_EngineAddObject(JSEngine *e, jsobjinit_t *func, void *priv) {
	JSContext *cx;

	dprintf(dlevel,"e: %p\n", e);
	if (!e) return 1;
	cx = _getcx(e,1,1);
	if (cx) func(cx, priv);
	_relcx(e);
	return (!cx);
}

int JS_EngineAddInitFunc(JSEngine *e, char *name, js_initfunc_t *func, void *priv) {
	js_initfuncinfo_t newfunc;

	dprintf(dlevel,"e: %p\n", e);
	if (!e) return 1;

	dprintf(dlevel,"adding: name: %s, func: %p, priv: %p\n", name, func, priv);
	memset(&newfunc,0,sizeof(newfunc));
	strncpy(newfunc.name,name,sizeof(newfunc.name)-1);
	newfunc.type = 0;
	newfunc.func = func;
	newfunc.priv = priv;
	list_add(e->initfuncs, &newfunc, sizeof(newfunc));
	return 0;
}

int JS_EngineAddInitClass(JSEngine *e, char *name, js_initclass_t *class) {
	js_initfuncinfo_t newfunc;

	dprintf(dlevel,"e: %p\n", e);
	if (!e) return 1;

	dprintf(dlevel,"adding: name: %s, class: %p\n", name, class);
	memset(&newfunc,0,sizeof(newfunc));
	strncpy(newfunc.name,name,sizeof(newfunc.name)-1);
	newfunc.type = 1;
	newfunc.class = class;
	list_add(e->initfuncs, &newfunc, sizeof(newfunc));
	return 0;
}

static jsval js_get_function(JSContext *cx, JSObject *obj, char *name) {
	jsval fval;
	JSBool ok;

	/* See if the func exists */
	ok = JS_GetProperty(cx, obj, name, &fval);
	dprintf(dlevel,"getprop ok: %d\n", ok);
	if (ok) {
		dprintf(dlevel,"fval type: %s\n", jstypestr(cx,fval));
		if (strcmp(jstypestr(cx,fval),"function") == 0) {
			return fval;
		}
	}
	return JSVAL_NULL;
}

int _JS_EngineExec(JSEngine *e, char *filename, JSContext *cx, char *function_name, int argc, jsval *argv, int req) {
#if SCRIPT_CACHE
	scriptinfo_t *sinfo;
#endif
	JSScript *script;
	int status,ok,r;
	jsval rval,exit_rval;

	dprintf(dlevel,"filename: %s, cx: %p, function_name: %s, argc: %d, argv: %p, req: %d\n", filename, cx, function_name, argc, argv, req);

#if SCRIPT_CACHE
	/* If script previously executed, get it */
	sinfo = _getsinfo(e,filename,1);
	dprintf(dlevel,"sinfo: %p\n", sinfo);
	if (!sinfo) {
		log_error("_JS_EngineExec: error getting script info\n");
		return 1;
	}
	r = _chkscript(cx, sinfo);
	if (r == 1) return 1;
	script = sinfo->script;
#else
	script = JS_CompileFile(cx, JS_GetGlobalObject(cx), filename);
	if (!script) {
		log_error("_JS_EngineExec: error compiling\n");
		return 1;
	}
	r = 0;
#endif

	if (r == 0) {
		dprintf(dlevel,"cx: %p, global: %p, script: %p\n", cx, JS_GetGlobalObject(cx), script);
		ok = JS_ExecuteScript(cx, JS_GetGlobalObject(cx), script, &rval);
		dprintf(dlevel,"%s: exec ok: %d\n", filename, ok);
	} else {
		ok = 1;
	}

	if (ok && function_name) {
		jsval fval;

		dprintf(dlevel,"function_name: %s\n", function_name);

		/* Get the function */
		fval = js_get_function(cx,JS_GetGlobalObject(cx),function_name);
		dprintf(dlevel,"fval: %s\n", jstypestr(cx,fval));
		if (fval == JSVAL_NULL) {
			dprintf(dlevel,"req: %d\n", req);
			if (req) {
				status = 1;
				goto _JS_EngineExec_done;
			}
		} else {
			/* Call the function */
			ok = JS_CallFunctionValue(cx, JS_GetGlobalObject(cx), fval, argc, argv, &rval);
			dprintf(dlevel,"ok: %d, rval: %x (%s)\n", ok, rval, jstypestr(cx,rval));
			if (ok && rval != JSVAL_VOID) {
				JS_ValueToInt32(cx,rval,&status);
				dprintf(dlevel,"status: %d\n", status);
				return status;
			}
		}
	}
#if !SCRIPT_CACHE
	JS_DestroyScript(cx, script);
#endif
	dprintf(dlevel,"ok: %d\n", ok);
	status = (ok ? 0 : 1);

	/* If script called exit(), it will return !ok and exit code in prop */
	dprintf(dlevel,"ok: %d\n", ok);
	if (!ok) {
		status = 1;
		dprintf(dlevel,"getting exit code...\n");
		ok = JS_GetProperty(cx, JS_GetGlobalObject(cx), "exit_code", &exit_rval);
		dprintf(dlevel,"ok: %d, exit_rval: %x (%s)\n", ok, exit_rval, jstypestr(cx,exit_rval));
		if (ok && exit_rval != JSVAL_VOID) {
			JS_ValueToInt32(cx,exit_rval,&status);
			dprintf(dlevel,"new status: %d\n", status);
			JS_DeleteProperty(cx, JS_GetGlobalObject(cx), "exit_code");
		}
	}

_JS_EngineExec_done:
	dprintf(dlevel,"returning status: %d\n", status);
	return status;

}

int JS_EngineExec(JSEngine *e, char *filename, char *function_name, int argc, jsval *argv, int newcx) {
	JSContext *cx;
	char local_filename[256],fname[256],*p;
	int r,req;

	if (!e) return 1;

	dprintf(dlevel,"engine: %p, filename: %s, newcx: %d\n", e, filename, newcx);
	if (!strlen(filename)) return 1;
	if (access(filename,0) != 0) {
		dprintf(dlevel,"filename: %s does not exist!\n",filename);
		return 1;
	}
	r = 1;
	strncpy(local_filename,filename,sizeof(local_filename)-1);
	fixpath(local_filename,sizeof(local_filename)-1);
	if (newcx) cx = JS_EngineNewContext(e);
	else cx = _getcx(e,1,1);
	dprintf(dlevel,"cx: %p\n", cx);
	if (!cx) goto JS_EngineExec_error;
	if (!function_name || !strlen(function_name)) {
		strncpy(fname,basename(local_filename),sizeof(fname)-1);
		while(1) {
			p = strrchr(fname,'.');
			if (!p) break;
			*p = 0;
		}
		strcat(fname,"_main");
		req = 0;
	} else {
		/* if a func name was specified, mark is as required */
		strncpy(fname,function_name,sizeof(fname)-1);
		req = 1;
	}
	r = _JS_EngineExec(e, filename, cx, fname, argc, argv, req);

JS_EngineExec_error:
	dprintf(dlevel,"cx: %p, newcx: %d\n", cx, newcx);
	if (newcx && cx) {
		dprintf(dlevel,"destroying CX!!\n");
		JS_DestroyContext(cx);
	} else _relcx(e);
	return r;
}

JSContext *JS_EngineGetCX(JSEngine *e) {
	return e->cx;
}

int JS_EngineExecString(JSEngine *e, char *string) {
	jsval rval;
	JSContext *cx;
	int ok;

	dprintf(dlevel,"string: %s\n", string);
	cx = _getcx(e,1,1);
	if (cx) ok = JS_EvaluateScript(cx, JS_GetGlobalObject(cx), string, strlen(string),"-e", 1, &rval);
	else ok = 0;
	_relcx(e);
	dprintf(dlevel,"ok: %d\n", ok);
	return (ok ? 0 : 1);
}

#if 0
char *JS_EngineGetErrmsg(JSEngine *e) {
	if (!e) return "";
	return e->errmsg;
}
#endif

void JS_EngineCleanup(JSEngine *e) {
	JSContext *cx;

	if (!e) return;

	cx = _getcx(e,1,0);
	if (cx) JS_GC(cx);
	_relcx(e);
	return;
}

int JS_GetValue(JSEngine *e, char *name, int type, void *dest, int dsize) {
        JSContext *cx;
        JSBool ok;
        jsval val;

	cx = _getcx(e,1,0);
	if (cx) {
		ok = JS_GetProperty(cx, JS_GetGlobalObject(cx), name, &val);
		dprintf(dlevel,"ok: %d\n", ok);
		if (ok) jsval_to_type(type,dest,dsize,cx,val);
	} else {
		ok = 0;
	}
	_relcx(e);
	return (ok != JS_TRUE);
}
