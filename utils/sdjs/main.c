
#ifdef JS

#include <libgen.h>

#include "client.h"
#include "smanet.h"
#include "jsengine.h"

#define dlevel 3

#ifdef WINDOWS
#include "wineditline/editline.c"
#include "wineditline/history.c"
#include "wineditline/fn_complete.c"
#else
#define HAVE_READLINE 1
#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#else
#include "editline/editline.c"
#include "editline/sysunix.c"
#endif
#endif

#define TESTING 0

static JSBool
GetLine(JSContext *cx, char *bufp, FILE *file, const char *prompt) {
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
        JS_free(cx, linep);
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
//		dprintf(dlevel,"result type: %s\n", jstypestr(cx,result));
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

static int sdjs_argc = 0;
static char **sdjs_argv = 0;

JSObject *mkargv(JSContext *cx, JSObject *parent, void *ctx) {
	JSObject *obj;
	int count,i,ind;
	jsval val;
	char *p;

	dprintf(dlevel,"argc: %d, optind: %d\n", sdjs_argc, optind);
	count = sdjs_argc - optind;
	if (count < 0) count = 0;
	dprintf(dlevel,"argc: %d, optind: %d, count: %d\n", sdjs_argc, optind, count);
 	obj = JS_NewArrayObject(cx, count, NULL);
	ind = optind;
	for(i=0; i < count; i++) {
		p = sdjs_argv[ind++];
		val = type_to_jsval(cx,DATA_TYPE_STRING,p,strlen(p));
		JS_SetElement(cx, obj, i, &val);
	}
	val = OBJECT_TO_JSVAL(obj);
	JS_DefineProperty(cx, parent, "argv", val, 0, 0, JSPROP_READONLY | JSPROP_ENUMERATE);

	return obj;
}

int sdjs_jsinit(JSContext *cx, JSObject *parent, void *ctx) {
	solard_client_t *c = ctx;

	newgobj(cx,"argv",mkargv,c);
	newgobj(cx,"client",js_client_new,c);
	/* Create global convienience objects */
	JS_DefineProperty(cx, parent, "config", c->config_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "mqtt", c->mqtt_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "influx", c->influx_val, 0, 0, JSPROP_ENUMERATE);
	smanet_jsinit(c->js);
	return 0;
}

int sdjs_init(solard_client_t *c) {
	JS_EngineAddInitFunc(c->js, "sdjs_jsinit", sdjs_jsinit, c);
	return 0;

}

int main(int argc, char **argv) {
	char script[256],func[128],load[4096];
	opt_proctab_t opts[] = {
		{ "%|script",&script,DATA_TYPE_STRING,sizeof(script)-1,0,"" },
		{ "-f:|function name (default: <script_name>_main)",&func,DATA_TYPE_STRING,sizeof(func)-1,0,"" },
		{ 0 }
	};
	config_property_t sdjs_props[] = {
		/* name, type, dest, dsize, def, flags, scope, values, labels, units, scale, precision, trigger, ctx */
		{ "load", DATA_TYPE_STRING, load, sizeof(load)-1, "", 0, 0, 0, 0, 0, 0, 1, 0, 0 },
		{ 0 }
	};

#if TESTING
	char *args[] = { "js", "-d", "7", "json.js" };
	argc = (sizeof(args)/sizeof(char *));
	argv = args;
#endif
	solard_client_t *c;

	sdjs_argc = argc;
	sdjs_argv = argv;

	*script = *func = 0;
	c = client_init(argc,argv,"1.0",opts,"sdjs",CLIENT_FLAG_JSGLOBAL,sdjs_props,0,sdjs_init);
	if (!c) {
		log_error("unable to initialize client\n");
		return 1;
	}

	/* load any scripts specified in load */
	if (strlen(load)) {
		char cmd[1024], *name;
		int i = 0;
		dprintf(dlevel,"load: %s\n", load);
		while(1) {
			name = strele(i++,",",load);
			dprintf(dlevel,"name: %s\n", name);
			if (!strlen(name)) break;
			snprintf(cmd,sizeof(cmd)-1,"load(\"%s\");",name);
			dprintf(dlevel,"cmd: %s\n", cmd);
			if (JS_EngineExecString(c->js, cmd))
				log_error("unable to load %s\n",name);
		}
	}

	dprintf(dlevel,"script: %s, func: %s\n", script, func);
	if (*script) {
		if (access(script,0) < 0) {
			printf("%s: %s\n",script,strerror(errno));
		} else {
			strcpy(c->name,basename(script));
			dprintf(dlevel,"executing script: %s\n",script);
			if (JS_EngineExec(c->js,script,func,0,0,1)) {
//				char *msg = JS_EngineGetErrmsg(c->js);
//				printf("%s: %s\n",script,strlen(msg) ? msg : "error executing script");
				return 1;
			}
		}
	} else {
		JS_EngineExecString(c->js,"");
		shell(JS_EngineGetCX(c->js));
	}
	return 0;
}
#else
int main(void) { return 1; }
#endif
