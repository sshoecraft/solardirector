
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define DEBUG_LOG 0

#ifdef DEBUG
#undef DEBUG
#endif
#if DEBUG_LOG
#define DPRINTF(format, args...) printf("%s(%d): " format,__FUNCTION__,__LINE__, ## args)
#else
#define DPRINTF(format, args...) /* noop */
#endif
#include "debug.h"
#include "common.h"

FILE *logfp = (FILE *) 0;
int logopts;

#if DEBUG
static struct _opt_info {
	int type;
	char *name;
} optlist[] = {
	{ LOG_CREATE, "CREATE" },
	{ LOG_TIME, "TIME" },
	{ LOG_STDERR, "STDERR" },
	{ LOG_INFO, "INFO" },
	{ LOG_VERBOSE, "VERBOSE" },
	{ LOG_WARNING, "WARNING" },
	{ LOG_ERROR, "ERROR" },
	{ LOG_SYSERR, "SYSERR" },
	{ LOG_DEBUG, "DEBUG" },
	{ LOG_DEBUG2, "DEBUG2" },
	{ LOG_DEBUG3, "DEBUG3" },
	{ LOG_DEBUG4, "DEBUG4" },
	{ 0,0 }
};

static __inline void _dispopts(int type) {
	register struct _opt_info *opt;
	char message[128];
	register char *ptr;

	message[0] = 0;
	sprintf(message,"log options:");
	ptr = message + strlen(message);
	for(opt = optlist; opt->name; opt++) {
		if (type & opt->type) {
			sprintf(ptr," %s",opt->name);
			ptr += strlen(ptr);
		}
	}
	if (logfp) fprintf(logfp,"%s\n",message);
	return;
}
#endif

int log_open(char *ident,char *filename,int opts) {

#if DEBUG
	DPRINTF("ident: %p, filename: %p,opts: %x\n",ident,filename,opts);
	_dispopts(opts);
#endif

	if (filename) {
		char *op;

		/* Open the file */
		op = (opts & LOG_CREATE ? "w+" : "a+");
		DPRINTF("op: %s\n", op);
		logfp = fopen(filename,op);
		if (!logfp) {
			perror("log_open: unable to create logfile");
			return 1;
		}
	} else if (opts & LOG_STDERR) {
		DPRINTF("logging to stderr\n");
		logfp = stderr;
	} else {
		DPRINTF("logging to stdout\n");
		logfp = stdout;
	}
	logopts = opts;

	DPRINTF("log is opened.\n");
	return 0;
}

//FILE *log_getfp(void) { return(logfp); }

static int _log_write(int type,char *format,va_list ap) {
	char dt[32],*errstr;

	/* get the error text now before it's gone */
	if (type & LOG_SYSERR) errstr = strerror(errno);

	/* Make sure log_open was called */
	if (!logfp) log_open("",0,LOG_INFO|LOG_WARNING|LOG_ERROR|LOG_SYSERR|LOG_DEBUG);

	/* Do we even log this type? */
	DPRINTF("logopts: %0x, type: %0x\n",logopts,type);
	if ( (logopts | type) != logopts) return 0;

	/* Prepend the time? */
	if (logopts & LOG_TIME || type & LOG_TIME) {
		DPRINTF("prepending time...\n");
		get_timestamp(dt,sizeof(dt),1);
		fprintf(logfp,"%s  ",dt);
	}

	/* If it's a warning, prepend warning: */
	if (type & LOG_WARNING) fprintf(logfp,"warning: ");

	/* If it's an error, prepend error: */
	else if ((type & LOG_ERROR) || (type & LOG_SYSERR)) {
		DPRINTF("prepending error...\n");
		fprintf(logfp,"error: ");
	}

	/* Build the rest of the message */
	DPRINTF("adding message...\n");
//	fprintf(logfp,"format: %s",format);
	vfprintf(logfp,format,ap);
	va_end(ap);

	/* If it's a system error, concat the system message */
	if (type & LOG_SYSERR && errstr) {
		DPRINTF("adding error text...\n");
		fprintf(logfp,": %s\n",errstr);
	}
	fflush(logfp);

	return 0;
}

int log_write(int type, char *format, ...) {
	va_list ap;

	va_start(ap,format);
	return _log_write(type,format,ap);
}

#define LOGDEF(n,t) int n(char *format,...) { va_list ap; va_start(ap,format); return _log_write(t,format,ap); }

LOGDEF(log_info,LOG_INFO);
LOGDEF(log_warning,LOG_WARNING);
LOGDEF(log_error,LOG_ERROR);
LOGDEF(log_syserr,LOG_SYSERR);
LOGDEF(log_syserror,LOG_SYSERR);
LOGDEF(log_debug,LOG_DEBUG);

#ifdef JS
#include "jsapi.h"
#include "jsengine.h"
#include "jsstr.h"
static int js_log_init(JSContext *cx, JSObject *parent, void *priv) {
#if 0
	JSObject *obj = JS_GetGlobalObject(cx);
	char temp[64];
	int i,count;
	JSFunctionSpec funcs[] = {
		JS_FN("typestr",jstypes_typestr,1,1,0),
		{ 0 }
	};
	JSConstantSpec *consts;

	if (!JS_DefineFunctions(cx, obj, funcs)) {
		dprintf(1,"error defining types funcs\n");
		return 1;
	}

	for(i=count=0; type_info[i].name; i++) count++;
	consts = malloc((count+1)*sizeof(JSConstantSpec));
	if (!consts) {
		log_syserror("JSTypes: malloc");
		return 1;
	}
#define PREFIX "LOG_"
	for(i=0; i < count; i++) {
		snprintf(temp,sizeof(temp)-1,"%s%s",PREFIX,type_info[i].name);
//		consts[i].name = js_GetStringBytes(cx,JS_NewStringCopyZ(cx,temp));
		consts[i].name = JS_EncodeString(cx, JS_InternString(cx,temp));
		consts[i].value = NUMBER_TO_JSVAL(type_info[i].type);
	}
	consts[i].name = 0;
	if(!JS_DefineConstants(cx, obj, consts)) {
		dprintf(1,"error defining types constants\n");
		return 1;
	}
	for(i=0; i < count; i++) JS_free(cx,(void *)consts[i].name);
	free(consts);
#endif
	JSConstantSpec log_consts[] = {
		JS_NUMCONST(LOG_CREATE),
		JS_NUMCONST(LOG_TIME),
		JS_NUMCONST(LOG_STDERR),
		JS_NUMCONST(LOG_INFO),
		JS_NUMCONST(LOG_VERBOSE),
		JS_NUMCONST(LOG_WARNING),
		JS_NUMCONST(LOG_ERROR),
		JS_NUMCONST(LOG_SYSERR),
		JS_NUMCONST(LOG_DEBUG),
		{0}
	};

	if(!JS_DefineConstants(cx, parent, log_consts)) {
		dprintf(1,"error defining log constants\n");
		return 1;
	}

	return 0;
}

int log_jsinit(void *e) {
	return JS_EngineAddInitFunc((JSEngine *)e, "log", js_log_init, 0);
}
#endif
