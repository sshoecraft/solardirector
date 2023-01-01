
#ifndef __JSENGINE_H
#define __JSENGINE_H

#include "jsapi.h"
#include "list.h"
#include <pthread.h>

typedef int (js_initfunc_t)(JSContext *cx,JSObject *parent,void *private);
typedef JSObject *(js_initclass_t)(JSContext *, JSObject *);

#if 0
struct js_initfuncinfo {
	char *name;
	int type;
	union {
		js_initfunc_t *func;
		js_initclass_t *class;
	};
	void *priv;
};
typedef struct js_initfuncinfo js_initfuncinfo_t;
#endif

typedef int (js_outputfunc_t)(const char *format, ...);
struct JSEngine {
	JSRuntime *rt;
	JSContext *cx;
	pthread_mutex_t lockcx;
	int rtsize;
	int stacksize;
	list initfuncs;
	js_outputfunc_t *output;
	list scripts;
//	char errmsg[1024];
	void *private;
};
typedef struct JSEngine JSEngine;

JSEngine *JS_EngineInit(int rtsize, int stksize, js_outputfunc_t *);
JSEngine *JS_DupEngine(JSEngine *e);
int JS_EngineDestroy(JSEngine *);
int JS_EngineAddInitFunc(JSEngine *, char *name, js_initfunc_t *func, void *priv);
int JS_EngineAddInitClass(JSEngine *, char *name, js_initclass_t *func);
int _JS_EngineExec(JSEngine *e, char *filename, JSContext *cx, char *fname, int argc, jsval *argv, int freq);
int JS_EngineExec(JSEngine *e, char *filename, char *function_name, int argc, jsval *argv, int newcx);
int JS_EngineSetStacksize(JSEngine *,int);
JSBool JS_EngineExecString(JSEngine *e, char *string);
JSContext *JS_EngineNewContext(JSEngine *e);
//int JS_EngineExecFunc(JSEngine *e, char *filename, char *funcname, int argc, jsval **argv);
typedef JSObject *(jsobjinit_t)(JSContext *cx, void *priv);
int JS_EngineAddObject(JSEngine *e, jsobjinit_t *func, void *priv);
//char *JS_EngineGetErrmsg(JSEngine *e);
void JS_EngineCleanup(JSEngine *e);
JSContext *JS_EngineGetCX(JSEngine *e);

#endif
