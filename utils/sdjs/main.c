#ifdef JS

/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#ifdef DEBUG_MEM
#undef DEBUG_MEM
#endif

// f it
// #define DEFAULT_RUNTIME_SIZE 64M
// #define DEFAULT_STACK_SIZE 2M

// no problem on any scripts tested
//#define DEFAULT_RUNTIME_SIZE 1M
//#define DEFAULT_STACK_SIZE 32K

// runs most stuff
// #define DEFAULT_RUNTIME_SIZE 512K
// #define DEFAULT_STACK_SIZE 16K

// small scripts
#define DEFAULT_RUNTIME_SIZE 256K
#define DEFAULT_STACK_SIZE 8K

// Wont run very much
//#define DEFAULT_RUNTIME_SIZE 128K
//#define DEFAULT_STACK_SIZE 4K

// Doesnt even start up anymore
//#define DEFAULT_RUNTIME_SIZE 64K
//#define DEFAULT_STACK_SIZE 2K

#include "common.h"
#include "config.h"
#include "jsengine.h"

//#include "agent.h"
//#include "smanet.h"
//#include <libgen.h>
#include "wpi.h"

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

//#define DEBUG_STARTUP 1
#if DEBUG_STARTUP
#define _ST_DEBUG LOG_DEBUG
#else
#define _ST_DEBUG 0
#endif

#ifndef STRINGIFY2
#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)
#endif

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
//		DISPOSE(linep);
#undef free
		free(linep);
		bufp += strlen(bufp);
		*bufp++ = '\n';
		*bufp = '\0';
	} else
#endif
	{
		char line[256];
		printf("%s", prompt);
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
		dprintf(dlevel,"buffer: %s", buffer);
		if (strncmp(buffer,"quit",4)==0) break;

		/* Clear any pending exception from previous failed compiles.  */
		JS_ClearPendingException(cx);
		script = JS_CompileScript(cx, JS_GetGlobalObject(cx), buffer, strlen(buffer), "typein", startline);
		if (script) {
			ok = JS_ExecuteScript(cx, JS_GetGlobalObject(cx), script, &result);
			dprintf(dlevel,"result: %x", result);
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

static int find_configfile(char *name,int sz) {
	char temp[1024],path[2048];

	dprintf(dlevel,"name: %s\n", name);

	/* look for our configfile in current loc, home, home/etc, SOLARD_ETC, /etc, /usr/local/etc */

	/* get cwd */
	getcwd(temp,sizeof(temp));
	dprintf(dlevel,"cwd: %s\n", temp);
	snprintf(path,sizeof(path),"%s/%s",temp,name);
	dprintf(dlevel,"path: %s\n", path);
	if (access(path,R_OK) == 0) {
		strncpy(name,path,sz);
		return 1;
	}
	/* get home */
        if (gethomedir(temp,sizeof(temp)) == 0) {
		dprintf(dlevel,"home: %s\n", temp);
		snprintf(path,sizeof(path),"%s/%s",temp,name);
		dprintf(dlevel,"path: %s\n", path);
		if (access(path,R_OK) == 0) {
			strncpy(name,path,sz);
			return 1;
		}
		snprintf(path,sizeof(path),"%s/etc/%s",temp,name);
		dprintf(dlevel,"path: %s\n", path);
		if (access(path,R_OK) == 0) {
			strncpy(name,path,sz);
			return 1;
		}
	}
	dprintf(dlevel,"ETCDIR: %s\n", SOLARD_ETCDIR);
	snprintf(path,sizeof(path),"%s/%s",SOLARD_ETCDIR,name);
	dprintf(dlevel,"path: %s\n", path);
	if (access(path,R_OK) == 0) {
		strncpy(name,path,sz);
		return 1;
	}
	snprintf(path,sizeof(path),"/etc/%s",name);
	dprintf(dlevel,"path: %s\n", path);
	if (access(path,R_OK) == 0) {
		strncpy(name,path,sz);
		return 1;
	}
	snprintf(path,sizeof(path),"/usr/local/etc/%s",name);
	dprintf(dlevel,"path: %s\n", path);
	if (access(path,R_OK) == 0) {
		strncpy(name,path,sz);
		return 1;
	}
	return 0;
}

int main(int argc, char **argv) {
//	sdjs_private_t *p;
	char configfile[256];
	char script[256],func[128],load[4096];
	char jsexec[4096];
	int rtsize,stacksize;
	int rtsize_flag,stacksize_flag;
	opt_proctab_t opts[] = {
		{ "-c:%|config file",&configfile,DATA_TYPE_STRING,sizeof(configfile)-1,0,"sdjs.conf" },
		{ "-e:%|execute javascript",&jsexec,DATA_TYPE_STRING,sizeof(jsexec)-1,0,"" },
		{ "-f:|function name (default: <script_name>_main)",&func,DATA_TYPE_STRING,sizeof(func)-1,0,"" },
		{ "-U:#|javascript runtime size",&rtsize_flag,DATA_TYPE_INT,0,0,0 },
		{ "-K:#|javascript stack size",&stacksize_flag,DATA_TYPE_INT,0,0,0 },
		{ "%|script",&script,DATA_TYPE_STRING,sizeof(script)-1,0,"" },
		{ 0 }
	};
	config_property_t props[] = {
		/* name, type, dest, dsize, def, flags, scope, values, labels, units, scale, precision, trigger, ctx */
		/* comma-seperated list of files/scripts to load */
		{ "load", DATA_TYPE_STRING, load, sizeof(load)-1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0 },
		{ "rtsize", DATA_TYPE_INT, &rtsize, 0, STRINGIFY(DEFAULT_RUNTIME_SIZE), CONFIG_FLAG_READONLY },
		{ "stacksize", DATA_TYPE_INT, &stacksize, 0, STRINGIFY(DEFAULT_STACK_SIZE), CONFIG_FLAG_READONLY },
		{ 0 }
	};
//	solard_driver_t driver;
	config_t *cp;
	JSEngine *e;
	JSContext *cx;

#if TESTING
	char *args[] = { "sdjs", "-d", "7", "json.js" };
	argc = (sizeof(args)/sizeof(char *));
	argv = args;
#endif

//	atexit(agent_shutdown);
//	atexit(client_shutdown);

	*configfile = *script = *func = *jsexec = 0;
	rtsize_flag = stacksize_flag = 0xDEADBEEF;
	if (solard_common_init(argc,argv,"2.0",opts,LOG_DEFAULT|_ST_DEBUG)) return 1;

	/* Create the config instance, find and read configfile */
	cp = config_init("sdjs", props, 0);
	if (!cp) return 1;
	common_add_props(cp, "sdjs");
	common_read_config(cp, "sdjs");
	if (find_configfile(configfile,sizeof(configfile))) config_read_file(cp,configfile);

	/* command line overrides rt and stack sizes */
	dprintf(dlevel,"rtsize_flag: %X, stacksize_flag: %X\n", rtsize_flag, stacksize_flag);
	if (rtsize_flag != 0xDEADBEEF) rtsize = rtsize_flag;
	if (stacksize_flag != 0xDEADBEEF) stacksize = stacksize_flag;
	dprintf(dlevel,"rtsize: %d, stacksize: %d\n", rtsize, stacksize);

	/* init the js engine */
	e = common_jsinit(rtsize, stacksize, (js_outputfunc_t *)log_info);
	dprintf(dlevel,"e: %p\n", e);
	if (!e) return 1;
	wpi_init(e);

	/* one liner? */
	dprintf(dlevel,"jsexec: %s\n", jsexec);
	if (*jsexec) {
		JS_EngineExecString(e,jsexec);
		goto done;
	}

	// Kick start the engine by exec'ing nothing
	if (JS_EngineExecString(e, "")) goto done;
	cx = JS_EngineGetCX(e);
	dprintf(dlevel,"cx: %p\n", cx);
	if (!cx) goto done;

	dprintf(dlevel,"script: %s, func: %s\n", script, func);
	if (*script) {
		JSObject *obj;
		int count,i,ind;
		jsval val;
		char *arg;

		if (access(script,0) < 0) {
			printf("%s: %s\n",script,strerror(errno));
			goto done;
		}

		/* turn C argv into JS argv */
		dprintf(dlevel,"argc: %d, optind: %d, argv: %p\n", argc, optind, argv);
		count = argc - optind;
		if (count < 0) count = 0;
		dprintf(dlevel,"argc: %d, optind: %d, count: %d\n",argc, optind, count);
		obj = JS_NewArrayObject(cx, 0, NULL);
		ind = optind;
		for(i=0; i < count; i++) {
			arg = argv[ind++];
			val = type_to_jsval(cx,DATA_TYPE_STRING,arg,strlen(arg));
			JS_SetElement(cx, obj, i, &val);
		}
		val = OBJECT_TO_JSVAL(obj);

		dprintf(dlevel,"func: %s\n", func);

		/* create global argv? */
//		JS_DefineProperty(cx, parent, "argv", val, 0, 0, JSPROP_READONLY | JSPROP_ENUMERATE);

		/* argc is only 1 as argv is an array */
		dprintf(dlevel,"executing script: %s\n",script);
		JS_EngineExec(e,script,func,1,&val,0);
	} else {
		shell(JS_EngineGetCX(e));
	}

done:
	JS_EngineDestroy(e);
#ifdef DEBUG_MEM
	log_info("final mem used: %ld\n", mem_used());
#endif
	return 0;
}
#else
int main(void) { return 1; }
#endif
