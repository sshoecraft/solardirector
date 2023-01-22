
#ifdef JS

#include <libgen.h>

#include "client.h"
#include "agent.h"
#include "smanet.h"
#include "jsengine.h"

#ifdef MQTTx
#undef MQTT
#endif

#define dlevel 2

#ifdef WINDOWS
#include "wineditline/editline.c"
#include "wineditline/history.c"
#include "wineditline/fn_complete.c"
#else
#define HAVE_READLINE 1
#if HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#else
#include "editline/editline.c"
#include "editline/sysunix.c"
#endif
#endif

#define TESTING 0

struct sdjs_private {
        int argc;
        char **argv;
#ifdef MQTT
        solard_client_t *c;
#else
        solard_agent_t *ap;
#endif
	JSEngine *e;
};
typedef struct sdjs_private sdjs_private_t;

static JSBool GetLine(JSContext *cx, char *bufp, FILE *file, const char *prompt) {
    /*
     * Use readline only if file is stdin, because there's no way to specify
     * another handle.  Are other filehandles interactive?
     */
#ifndef WINDOWS
    if (file == stdin) {
        char *linep = readline(prompt);
        if (!linep) return JS_FALSE;
        if (linep[0] != '\0') add_history(linep);
        strcpy(bufp, linep);
//      DISPOSE(linep);
#ifdef free
#undef free
#endif
	free(linep);
        bufp += strlen(bufp);
        *bufp++ = '\n';
        *bufp = '\0';
   } else
#endif
   {
        char line[256];
        printf(prompt);
        fflush(stdout);
        if (!fgets(line, sizeof line, file)) return JS_FALSE;
        strcpy(bufp, line);
   }
   return JS_TRUE;
}

int shell(JSContext *cx) {
    JSBool ok, hitEOF;
    JSScript *script;
    jsval result;
    JSString *str;
    char buffer[4096];
    char *bufp;
    int lineno;
    int startline;

    /* It's an interactive filehandle; drop into read-eval-print loop. */
    lineno = 1;
    hitEOF = JS_FALSE;
    do {
        bufp = buffer;
        *bufp = '\0';

        /*
         * Accumulate lines until we get a 'compilable unit' - one that either
         * generates an error (before running out of source) or that compiles
         * cleanly.  This should be whenever we get a complete statement that
         * coincides with the end of a line.
         */
        startline = lineno;
        do {
            if (!GetLine(cx, bufp, stdin, startline == lineno ? "js> " : "")) {
                hitEOF = JS_TRUE;
                break;
            }
            bufp += strlen(bufp);
            lineno++;
        } while (!JS_BufferIsCompilableUnit(cx, JS_GetGlobalObject(cx), buffer, strlen(buffer)));
	dprintf(dlevel,"buffer: %s\n", buffer);
	if (strncmp(buffer,"quit",4)==0) break;

        /* Clear any pending exception from previous failed compiles.  */
        JS_ClearPendingException(cx);
        script = JS_CompileScript(cx, JS_GetGlobalObject(cx), buffer, strlen(buffer), "typein", startline);
        if (script) {
                ok = JS_ExecuteScript(cx, JS_GetGlobalObject(cx), script, &result);
		dprintf(dlevel,"result: %x\n", result);
                if (ok && result != JSVAL_VOID) {
                    str = JS_ValueToString(cx, result);
                    if (str)
                        printf("%s\n", JS_GetStringBytes(str));
                    else
                        ok = JS_FALSE;
                }
            JS_DestroyScript(cx, script);
        }
    } while (!hitEOF);
	return 0;
}

typedef JSObject *(js_newobj_t)(JSContext *cx, JSObject *parent, void *priv);
static void newgobj(JSContext *cx, char *name, js_newobj_t func, void *priv) {
	JSObject *newobj, *global = JS_GetGlobalObject(cx);
	jsval newval;

	dprintf(dlevel,"name: %s\n", name);
	newobj = func(cx, global, priv);
	dprintf(dlevel,"%s newobj: %p\n", name, newobj);
	if (newobj) {
		newval = OBJECT_TO_JSVAL(newobj);
		JS_DefineProperty(cx, global, name, newval, 0, 0, JSPROP_ENUMERATE);
	}
}

JSObject *mkargv(JSContext *cx, JSObject *parent, void *ctx) {
	sdjs_private_t *p = ctx;
	JSObject *obj;
	int count,i,ind;
	jsval val;
	char *arg;

	dprintf(dlevel,"argc: %d, optind: %d, argv: %p\n", p->argc, optind, p->argv);
	count = p->argc - optind;
	if (count < 0) count = 0;
	dprintf(dlevel,"argc: %d, optind: %d, count: %d\n", p->argc, optind, count);
 	obj = JS_NewArrayObject(cx, count, NULL);
	ind = optind;
	for(i=0; i < count; i++) {
		arg = p->argv[ind++];
		val = type_to_jsval(cx,DATA_TYPE_STRING,arg,strlen(arg));
		JS_SetElement(cx, obj, i, &val);
	}
	val = OBJECT_TO_JSVAL(obj);
	JS_DefineProperty(cx, parent, "argv", val, 0, 0, JSPROP_READONLY | JSPROP_ENUMERATE);

	return obj;
}

#ifdef MQTT
int sdjs_jsinit(JSContext *cx, JSObject *parent, void *ctx) {
	sdjs_private_t *p = ctx;

	newgobj(cx,"argv",mkargv,p);

	/* Create global convienience objects */
	JS_DefineProperty(cx, parent, "client", OBJECT_TO_JSVAL(js_client_new(cx,parent,p->c)), 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "config", p->c->config_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "mqtt", p->c->mqtt_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "influx", p->c->influx_val, 0, 0, JSPROP_ENUMERATE);
	smanet_jsinit(p->c->js);
	return 0;
}

int sdjs_init(void *ctx, solard_client_t *c) {
	sdjs_private_t *p = ctx;

	JS_EngineAddInitFunc(p->e, "sdjs_jsinit", sdjs_jsinit, p);
	return 0;

}
#else
int sdjs_jsinit(JSContext *cx, JSObject *parent, void *ctx) {
	sdjs_private_t *p = ctx;

	newgobj(cx,"argv",mkargv,p);

	/* Create global convienience objects */
	JS_DefineProperty(cx, parent, "agent", OBJECT_TO_JSVAL(js_agent_new(cx,parent,p->ap)), 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "config", p->ap->js.config_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "mqtt", p->ap->js.mqtt_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "influx", p->ap->js.influx_val, 0, 0, JSPROP_ENUMERATE);
	return 0;
}

int sdjs_config(void *h, int req, ...) {
	sdjs_private_t *p = h;
	va_list va;

	dprintf(dlevel,"p: %p\n", p);
	va_start(va,req);
	dprintf(1,"req: %d\n", req);
	switch(req) {
	case SOLARD_CONFIG_INIT:
		p->ap = va_arg(va,solard_agent_t *);
		if (p->ap->js.e) JS_EngineAddInitFunc(p->ap->js.e,"sdjs_jsinit",sdjs_jsinit,p);
		break;
	case SOLARD_CONFIG_GET_INFO:
		{
			json_value_t **vpp = va_arg(va,json_value_t **);
			dprintf(1,"vpp: %p\n", vpp);
			if (vpp) {
				json_object_t *o;
				o = json_create_object();
				if (p->ap->cp && o) config_add_info(p->ap->cp,o);
				*vpp = json_object_value(o);
			}
		}
		break;
	}
	return 0;
}
#endif

JSBool js_load(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

int main(int argc, char **argv) {
	sdjs_private_t *p;
	char script[256],func[128],load[4096];
	opt_proctab_t opts[] = {
		{ "%|script",&script,DATA_TYPE_STRING,sizeof(script)-1,0,"" },
		{ "-f:|function name (default: <script_name>_main)",&func,DATA_TYPE_STRING,sizeof(func)-1,0,"" },
		{ 0 }
	};
	config_property_t sdjs_props[] = {
		/* name, type, dest, dsize, def, flags, scope, values, labels, units, scale, precision, trigger, ctx */
		{ "load", DATA_TYPE_STRING, load, sizeof(load)-1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0 },
		{ 0 }
	};
#ifndef MQTT
	solard_driver_t driver;
#endif

#if TESTING
	char *args[] = { "js", "-d", "7", "json.js" };
	argc = (sizeof(args)/sizeof(char *));
	argv = args;
#endif


	p = malloc(sizeof(*p));
	dprintf(dlevel,"p: %p\n", p);
	if (!p) {
		log_syserror("malloc private");
		return 1;
	}
	memset(p,0,sizeof(*p));
	p->argc = argc;
	p->argv = argv;

	*script = *func = *load = 0;
#ifdef MQTT
	p->c = client_init(argc,argv,"1.0",opts,"sdjs",CLIENT_FLAG_JSGLOBAL,sdjs_props,0,sdjs_init,p);
	if (!p->c) {
		log_error("unable to initialize client\n");
		return 1;
	}
	p->e = p->c->js;
#else
	memset(&driver,0,sizeof(driver));
	driver.name = "sdjs";
	driver.config = sdjs_config;
	p->ap = agent_init(argc,argv,"1.0",opts,&driver,p,0,sdjs_props,0);
	if (!p->ap) {
		log_error("unable to initialize agent\n");
		return 1;
	}
	p->e = p->ap->js.e;
#endif

	JS_EngineExecString(p->e, "");

	/* load any scripts specified in load */
	dprintf(dlevel,"load: %s\n", load);
	if (strlen(load)) {
//		char cmd[1024];
		char *name;
		int i = 0;
		JSBool ok;
		dprintf(dlevel,"load: %s\n", load);
		while(1) {
			char fixedname[1024];

			name = strele(i++,",",load);
			dprintf(dlevel,"name: %s\n", name);
			if (!strlen(name)) break;
			strncpy(fixedname,name,sizeof(fixedname)-1);
	                fixpath(fixedname,sizeof(fixedname)-1);
			ok = JS_EngineExec(p->e,fixedname,0,0,0,0);
			dprintf(dlevel,"ok: %d\n", ok);
		}
	}

	dprintf(dlevel,"script: %s, func: %s\n", script, func);
	if (*script) {
		if (access(script,0) < 0) {
			printf("%s: %s\n",script,strerror(errno));
		} else {
#ifdef MQTT
			strcpy(p->c->name,basename(script));
#endif
			dprintf(dlevel,"executing script: %s\n",script);
			JS_EngineExec(p->e,script,func,0,0,0);
		}
	} else {
		shell(JS_EngineGetCX(p->e));
	}
	dprintf(dlevel,"shutting down...\n");
#ifdef MQTT
	client_shutdown();
#else
	agent_shutdown();
#endif
//	free(p);
#ifdef DEBUG_MEM
	log_info("final mem used: %ld\n", mem_used());
#endif
	return 0;
}
#else
int main(void) { return 1; }
#endif
