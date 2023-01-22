
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define DEBUG_COMMON 1
#define dlevel 3

#ifdef DEBUG
#undef DEBUG
#define DEBUG DEBUG_COMMON
#endif
#include "debug.h"

#if DEBUG_COMMON_INIT
#define DPRINTF(format, args...) printf("%s(%d): " format,__FUNCTION__,__LINE__, ## args)
#else
#define DPRINTF(format, args...) /* noop */
#endif

#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include "common.h"
#include "driver.h"
#include "config.h"
#include "mqtt.h"
#include "influx.h"
#include "transports.h"
#include "battery.h"
#include "inverter.h"
#ifdef __WIN32
#include <winsock2.h>
#endif
#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

char SOLARD_BINDIR[SOLARD_PATH_MAX];
char SOLARD_ETCDIR[SOLARD_PATH_MAX];
char SOLARD_LIBDIR[SOLARD_PATH_MAX];
char SOLARD_LOGDIR[SOLARD_PATH_MAX];

#if defined(__WIN32) && !defined(__WIN64)
#include <windows.h>
BOOL InitOnceExecuteOnce(
	PINIT_ONCE    InitOnce,
	PINIT_ONCE_FN InitFn,
	PVOID         Parameter,
	LPVOID        *Context
) {
	InitFn(InitOnce,Parameter,Context);
	return TRUE;
}
#endif


static int _getcfg(cfg_info_t *cfg, char *section_name, char *dest, char *what, int ow) {
	char name[256];
	char *p;
	int r;

	dprintf(dlevel,"cfg: %p, section_name: %s, dest: %s, what: %s, ow: %d\n", cfg, section_name, dest, what, ow);
	sprintf(name,"%sdir",what);
	dprintf(dlevel,"name: %s\n", name);

	r = 1;
	p = cfg_get_item(cfg,section_name,name);
	dprintf(dlevel,"%s p: %p\n", name, p);
	if (!p) {
		p = cfg_get_item(cfg,section_name,"prefix");
		dprintf(dlevel,"prefix p: %p\n", p);
		if (!p) goto _getcfg_err;
		if (ow) {
			dprintf(dlevel,"prefix: %s\n", p);
			sprintf(dest,"%s/%s",p,what);
		}
	} else {
		strncpy(dest,p,SOLARD_PATH_MAX-1);
	}
	dprintf(dlevel,"dest: %s\n", dest);
	r = 0;
_getcfg_err:
	dprintf(dlevel,"returning: %d\n", r);
	return r;
}

static void _getpath(cfg_info_t *cfg, char *section_name, char *home, char *dest, char *what, int ow) {

	dprintf(dlevel,"cfg: %p, home: %s, what: %s, ow: %d\n", cfg, home, what, ow);

	if (cfg && _getcfg(cfg,section_name,dest,what,1) == 0) return;
	if (!ow) return;

#ifdef __WIN32
//	strcpy(dest,"%ProgramFiles%\\SolarDirector");
	strcpy(dest,"c:\\sd");
	return;
#else
	/* Am I root? */
	if (getuid() == 0) {
		if (strcmp(what,"bin") == 0)
			strcpy(dest,"/usr/local/bin");
		else if (strcmp(what,"etc") == 0)
			strcpy(dest,"/usr/local/etc");
		else if (strcmp(what,"lib") == 0)
			strcpy(dest,"/usr/local/lib");
		else if (strcmp(what,"log") == 0)
			strcpy(dest,"/var/log");
		else {
			printf("error: init _getpath: no path set for: %s\n", what);
			dest = ".";
		}
		return;

	/* Not root */
	} else {
		sprintf(dest,"%s/%s",home,what);
		return;
	}
	printf("error: init: _getpath: no path set for: %s, using /tmp\n", what);
	dest = "/tmp";
#endif
}

static void _setp(char *name,char *value) {
	int len=strlen(value);
	if (len > 1 && value[len-1] == '/') value[len-1] = 0;
	fixpath(value,SOLARD_PATH_MAX-1);
	os_setenv(name,value,1);
}

static int solard_get_dirs(cfg_info_t *cfg, char *section_name, char *home, int ow) {

	dprintf(dlevel,"cfg: %p, section_name: %s, home: %s\n", cfg, section_name, home);

	_getpath(cfg,section_name,home,SOLARD_BINDIR,"bin",ow);
	_getpath(cfg,section_name,home,SOLARD_ETCDIR,"etc",ow);
	_getpath(cfg,section_name,home,SOLARD_LIBDIR,"lib",ow);
	_getpath(cfg,section_name,home,SOLARD_LOGDIR,"log",ow);

	_setp("SOLARD_BINDIR",SOLARD_BINDIR);
	_setp("SOLARD_ETCDIR",SOLARD_ETCDIR);
	_setp("SOLARD_LIBDIR",SOLARD_LIBDIR);
	_setp("SOLARD_LOGDIR",SOLARD_LOGDIR);
	dprintf(dlevel-1,"BINDIR: %s, ETCDIR: %s, LIBDIR: %s, LOGDIR: %s\n", SOLARD_BINDIR, SOLARD_ETCDIR, SOLARD_LIBDIR, SOLARD_LOGDIR);

	return 0;
}

int solard_common_init(int argc,char **argv,char *version,opt_proctab_t *add_opts,int start_opts) {
	/* std command-line opts */
	static char logfile[256];
	char home[246],configfile[256];
	cfg_info_t *cfg;
	static int append_flag;
	static int back_flag,verb_flag,vers_flag;
	static int help_flag,err_flag;
	static opt_proctab_t std_opts[] = {
		{ "-b|run in background",&back_flag,DATA_TYPE_LOGICAL,0,0,"no" },
		{ "-d:#|set debugging level",&debug,DATA_TYPE_INT,0,0,"0" },
		{ "-e|redirect output to stderr",&err_flag,DATA_TYPE_LOGICAL,0,0,"N" },
		{ "-h|display program options",&help_flag,DATA_TYPE_LOGICAL,0,0,"N" },
		{ "-l:%|redirect output to logfile",&logfile,DATA_TYPE_STRING,sizeof(logfile)-1,0,"" },
		{ "-a|append to logfile",&append_flag,DATA_TYPE_LOGICAL,0,0,"N" },
		{ "-v|enable verbose output ",&verb_flag,DATA_TYPE_LOGICAL,0,0,"N" },
		{ "-V|program version ",&vers_flag,DATA_TYPE_LOGICAL,0,0,"N" },
		{ 0,0,0,0,0 }
	};
	opt_proctab_t *opts;
	char *file;
	volatile int error,log_opts;
	char *ident;
#ifdef __WIN32
	WSADATA wsaData;
	int iResult;
#endif

	append_flag = back_flag = verb_flag = help_flag = err_flag = 0;

        /* Open the startup log */
	ident = "startup";
	log_opts = start_opts;
	if (log_opts & LOG_WX) {
		if (argv) ident = argv[argc+1];
		else log_opts &= ~LOG_WX;
	}
	log_open(ident,0,log_opts);

	log_debug("common_init: argc: %d, argv: %p, add_opts: %p, log_opts: %x\n", argc, argv, add_opts, log_opts);

#ifdef __WIN32
	log_debug("initializng winsock...\n");
	/* Initialize Winsock */
	iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0) {
		log_write(LOG_SYSERR,"WSAStartup");
		return 1;
	}
#endif

	/* If add_opts, add to std */
	opts = opt_combine_opts(std_opts,add_opts);

	/* Init opts */
	log_debug("init'ing opts: %p\n",opts);
	opt_init(opts);

	/* Process the opts */
	log_debug("common_init: processing opts...\n");
//	dprintf(0,"argc: %d, argv: %p\n", argc, argv);
	error = opt_process(argc,argv,opts);
	if (vers_flag) {
		printf("%s\n",version);
		exit(0);
	}

	/* If help flag, display usage and exit */
	log_debug("common_init: error: %d, help_flag: %d\n",error,help_flag);
	if (!error && help_flag) {
		opt_usage((argv ? argv[0] : ""),opts);
		free(opts);
		/* XXX */
//		error = 1;
		exit(0);
	}

	/* If add_opts, free malloc'd opts */
	free(opts);
	if (error) return 1;

	/* Set the requested flags */
	log_opts &= ~(LOG_DEBUG|LOG_DEBUG2|LOG_DEBUG3|LOG_DEBUG4);
#ifdef DEBUG
	dprintf(dlevel,"debug: %d\n", debug);
	switch(debug) {
		case 4:
		default:
			log_opts |= LOG_DEBUG4;
			/* Fall-through */
		case 3:
			log_opts |= LOG_DEBUG3;
			/* Fall-through */
		case 2:
			log_opts |= LOG_DEBUG2;
			/* Fall-through */
		case 1:
			log_opts |= LOG_DEBUG;
			break;
	}
#endif
	dprintf(dlevel,"verb_flag: %d, err_flag: %d, append_flag: %d, log_opts: %x\n",verb_flag,err_flag,append_flag,log_opts);
	if (verb_flag) log_opts |= LOG_VERBOSE;
	if (err_flag) log_opts |= LOG_STDERR;
	if (append_flag) log_opts &= ~LOG_CREATE;
	else log_opts |= LOG_CREATE;

	/* If back_flag is set, become a daemon */
	if (back_flag) become_daemon();

	/* Re-open the startup log */
	log_open(ident, 0 ,log_opts);

	/* Log to a file? */
	dprintf(dlevel,"logfile: %s\n", logfile);
	if (strlen(logfile)) {
//		strcpy(logfile,os_fnparse(logfile,".log",0));
//		dprintf(dlevel,"logfile: %s\n", logfile);
//		log_lock_id = os_lock_file(logfile,0);
//		if (!log_lock_id) return 1;
		log_opts |= LOG_TIME;
		file = logfile;
		log_opts &= ~LOG_WX;
		ident = "logfile";
	} else {
		file = 0;
	}

	/* Re-open log */
	dprintf(dlevel,"file: %s\n", file);
	log_open(ident,file,log_opts);

	/* Get paths */
	*SOLARD_BINDIR = *SOLARD_ETCDIR = *SOLARD_LIBDIR = *SOLARD_LOGDIR = 0;

	/* Look for configfile in ~/.sd.conf, and /etc/sd.conf, /usr/local/etc/sd.conf */
	*configfile = 0;
	if (gethomedir(home,sizeof(home)-1) == 0) {
		char path[256];

		sprintf(path,"%s/.sd.conf",home);
		fixpath(path,sizeof(path)-1);
		if (access(path,0) == 0) strcpy(configfile,path);
	}
#ifndef __WIN32
	if (access("/etc/sd.conf",R_OK) == 0) strcpy(configfile,"/etc/sd.conf");
	if (access("/usr/local/etc/sd.conf",R_OK) == 0) strcpy(configfile,"/usr/local/etc/sd.conf");
#endif

	cfg = 0;
	dprintf(dlevel,"configfile: %s\n", configfile);
	if (strlen(configfile)) {
		cfg = cfg_read(configfile);
		if (!cfg) log_write(LOG_SYSERR,"cfg_read %s",configfile);
	}
	solard_get_dirs(cfg,"",home,1);

	dprintf(dlevel,"cfg: %p\n", cfg);
	if (cfg) cfg_destroy(cfg);

	dprintf(dlevel,"done!\n");
	return 0;
}

#ifdef MQTT
static int common_config_from_mqtt(config_t *cp, mqtt_session_t *m) {
	return 1;
}
#endif

//static int _rc = 0;

int solard_common_startup(config_t **cp, char *sname, char *configfile, config_property_t *props, config_function_t *funcs,
		event_session_t **e
#ifdef MQTT
		,mqtt_session_t **m, char *lwt, mqtt_callback_t *getmsg, void *mctx, char *mqtt_info, int config_from_mqtt
#endif
#ifdef INFLUX
		,influx_session_t **i, char *influx_info
#endif
#ifdef JS
		,JSEngine **js, int rtsize, int stksize, js_outputfunc_t *jsout
#endif
	)
{
#ifdef MQTT
	int mqtt_init_done;
#endif

	dprintf(dlevel,"Starting up...\n");
//	{ char *p  = 0; if (_rc++ > 0) *p = 0; };

#ifdef MQTT
	if (m) {

		/* Create MQTT session */
		*m = mqtt_new(false, getmsg, mctx);
		if (!*m) {
			log_syserror("unable to create MQTT session\n");
			return 1;
		}
		dprintf(dlevel,"m: %p\n", *m);
		dprintf(dlevel,"mqtt_info: %s\n", mqtt_info);
		if (strlen(mqtt_info)) mqtt_parse_config(*m,mqtt_info);
		if (lwt && strlen(lwt)) strncpy((*m)->lwt_topic,lwt,sizeof((*m)->lwt_topic)-1);
	}
#endif
#ifdef HAVE_CURL
	curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
#ifdef INFLUX
	if (i) {
		/* Create InfluxDB session */
		*i = influx_new();
		if (!*i) {
			log_syserror("unable to create InfluxDB session\n");
			return 1;
		}
		dprintf(dlevel,"i: %p\n", *i);
		dprintf(dlevel,"influx_info: %s\n", influx_info);
		if (strlen(influx_info)) influx_parse_config(*i, influx_info);
	}
#endif

	/* New event session */
	if (e) *e = event_init();

	/* Init the config */
	dprintf(dlevel,"sname: %s\n", sname);
	*cp = config_init(sname, props, funcs);
	if (!*cp) return 0;
	common_add_props(*cp, sname);
#ifdef MQTT
	if (m) mqtt_add_props(*m, *cp, sname);
#endif
#ifdef INFLUX
	if (i) influx_add_props(*i, *cp, sname);
#endif

#ifdef MQTT
	if (m) {
		dprintf(dlevel,"config_from_mqtt: %d\n", config_from_mqtt);
		mqtt_init_done = 0;
		if (config_from_mqtt) {
			/* init mqtt */
			if (mqtt_init(*m)) {
				log_error("mqtt_init: %s\n",(*m)->errmsg);
				return 1;
			}
			mqtt_init_done = 1;

			/* read the config */
			if (common_config_from_mqtt(*cp,*m)) return 1;
		}
	}
#endif
	dprintf(dlevel,"configfile: %s\n", configfile);
	if (strlen(configfile)) {
		if (config_read_file(*cp,configfile)) {
			log_error(config_get_errmsg(*cp));
			return 1;
		}
	}
#ifdef MQTT
	/* re-apply mqtt_info if specified as commandline takes precedence */
	if (m && strlen(mqtt_info)) mqtt_parse_config(*m,mqtt_info);
#endif

#ifdef INFLUX
	/* re-apply influx_info if specified as commandline takes precedence */
	if (i && strlen(influx_info)) influx_parse_config(*i,influx_info);
#endif

#ifdef MQTT
//	if (m) mqtt_get_config(*m,*cp);
#endif
#ifdef INFLUX
//	if (i) influx_get_config(*i,*cp);
#endif

#ifdef MQTT
	if (m) {
		/* If MQTT not init, do it now */
		if (!mqtt_init_done && mqtt_init(*m)) {
			log_error("mqtt_init: %s\n",(*m)->errmsg);
			return 1;
		}

		/* Subscribe to our clientid */
		sprintf(mqtt_info,"%s/%s/%s/#",SOLARD_TOPIC_ROOT,SOLARD_TOPIC_CLIENTS,(*m)->clientid);
		mqtt_sub(*m,mqtt_info);
	}
#endif

#ifdef INFLUX
	/* Connect to the influx server */
	if (i) influx_connect(*i);
#endif

#ifdef JS
	if (js) {
		*js = common_jsinit(rtsize, stksize, jsout);
		if (!*js) return 1;
	}
#endif

	return 0;
}

void common_shutdown(void) {
#ifdef HAVE_CURL
        curl_global_cleanup();
#endif
}

void common_add_props(config_t *cp, char *name) {
	config_property_t common_props[] = {
		{ "BINDIR", DATA_TYPE_STRING, SOLARD_BINDIR, sizeof(SOLARD_BINDIR)-1, 0 },
		{ "ETCDIR", DATA_TYPE_STRING, SOLARD_ETCDIR, sizeof(SOLARD_ETCDIR)-1, 0 },
		{ "LIBDIR", DATA_TYPE_STRING, SOLARD_LIBDIR, sizeof(SOLARD_LIBDIR)-1, 0 },
		{ "LOGDIR", DATA_TYPE_STRING, SOLARD_LOGDIR, sizeof(SOLARD_LOGDIR)-1, 0 },
		{ "debug", DATA_TYPE_INT, &debug, 0, 0, 0, "range", "0,99,1", "debug level", 0, 1, 0 },
		{ 0 }
	};
	config_add_props(cp, name, common_props, 0);
}

json_object_t *solard_create_results(int status, char *fmt, ...) {
	char errmsg[1024];
	json_object_t *o;
	va_list ap;

	va_start(ap,fmt);
	vsnprintf(errmsg,sizeof(errmsg),fmt,ap);
	va_end(ap);
	o = json_create_object();
	if (!o) return 0;
	json_object_set_number(o,"status",status);
	json_object_set_string(o,"errmsg",errmsg);
	return o;
}

#ifdef JS
enum COMMON_PROPERTY_ID {
	COMMON_PROPERTY_ID_BINDIR=1,
	COMMON_PROPERTY_ID_ETCDIR,
	COMMON_PROPERTY_ID_LIBDIR,
	COMMON_PROPERTY_ID_LOGDIR,
};

static JSBool common_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;

	dprintf(dlevel,"is_int: %d\n", JSVAL_IS_INT(id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case COMMON_PROPERTY_ID_BINDIR:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,SOLARD_BINDIR,strlen(SOLARD_BINDIR));
			break;
		case COMMON_PROPERTY_ID_ETCDIR:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,SOLARD_ETCDIR,strlen(SOLARD_ETCDIR));
			break;
		case COMMON_PROPERTY_ID_LIBDIR:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,SOLARD_LIBDIR,strlen(SOLARD_LIBDIR));
			break;
		case COMMON_PROPERTY_ID_LOGDIR:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,SOLARD_LOGDIR,strlen(SOLARD_LOGDIR));
			break;
		default:
			dprintf(1,"not found\n");
			JS_ReportError(cx, "property not found");
			return JS_FALSE;
			break;
		}
	}
	return JS_TRUE;
}

static JSBool common_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	int prop_id;

	dprintf(dlevel,"is_int: %d\n", JSVAL_IS_INT(id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case COMMON_PROPERTY_ID_BINDIR:
			jsval_to_type(DATA_TYPE_STRING,SOLARD_BINDIR,SOLARD_PATH_MAX,cx,*vp);
			break;
		case COMMON_PROPERTY_ID_ETCDIR:
			jsval_to_type(DATA_TYPE_STRING,SOLARD_ETCDIR,SOLARD_PATH_MAX,cx,*vp);
			break;
		case COMMON_PROPERTY_ID_LIBDIR:
			jsval_to_type(DATA_TYPE_STRING,SOLARD_LIBDIR,SOLARD_PATH_MAX,cx,*vp);
			break;
		case COMMON_PROPERTY_ID_LOGDIR:
			jsval_to_type(DATA_TYPE_STRING,SOLARD_LOGDIR,SOLARD_PATH_MAX,cx,*vp);
			break;
		default:
			dprintf(1,"not found\n");
			JS_ReportError(cx, "property not found");
			return JS_FALSE;
			break;
		}
	}
	return JS_TRUE;
}

static JSBool js_common_check_bit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	int value, mask;

	value = mask = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "i i", &value, &mask)) return JS_FALSE;
	dprintf(dlevel,"value: %x, mask: %x\n", value, mask);

	*rval = BOOLEAN_TO_JSVAL(check_bit(value,mask));
	return JS_TRUE;
}

static JSBool js_common_set_bit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	int value, mask;

	value = mask = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "i i", &value, &mask)) return JS_FALSE;
	dprintf(dlevel,"value: %x, mask: %x\n", value, mask);

	set_bit(value,mask);
	*rval = INT_TO_JSVAL(value);
	return JS_TRUE;
}

static JSBool js_common_clear_bit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	int value, mask;

	value = mask = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "i i", &value, &mask)) return JS_FALSE;
	dprintf(dlevel,"value: %x, mask: %x\n", value, mask);

	clear_bit(value,mask);
	*rval = INT_TO_JSVAL(value);
	return JS_TRUE;
}

static int js_common_init(JSContext *cx, JSObject *parent, void *priv) {
#define COMMON_FLAGS JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_EXPORTED
	JSPropertySpec common_props[] = {
		{ "SOLARD_BINDIR", COMMON_PROPERTY_ID_BINDIR, COMMON_FLAGS, common_getprop, common_setprop },
		{ "SOLARD_ETCDIR", COMMON_PROPERTY_ID_ETCDIR, COMMON_FLAGS, common_getprop, common_setprop },
		{ "SOLARD_LIBDIR", COMMON_PROPERTY_ID_LIBDIR, COMMON_FLAGS, common_getprop, common_setprop },
		{ "SOLARD_LOGDIR", COMMON_PROPERTY_ID_LOGDIR, COMMON_FLAGS, common_getprop, common_setprop },
		{0}
	};
	JSFunctionSpec common_funcs[] = {
		JS_FS("check_bit",js_common_check_bit,2,2,0),
		JS_FS("set_bit",js_common_set_bit,2,2,0),
		JS_FS("clear_bit",js_common_clear_bit,2,2,0),
		{0}
	};

	JSConstantSpec common_consts[] = {
		JS_STRCONST(SOLARD_TOPIC_ROOT),
		JS_STRCONST(SOLARD_TOPIC_AGENTS),
		JS_STRCONST(SOLARD_TOPIC_CLIENTS),
		JS_STRCONST(SOLARD_ROLE_CONTROLLER),
		JS_STRCONST(SOLARD_ROLE_STORAGE),
		JS_STRCONST(SOLARD_ROLE_BATTERY),
		JS_STRCONST(SOLARD_ROLE_INVERTER),
		JS_STRCONST(SOLARD_FUNC_STATUS),
		JS_STRCONST(SOLARD_FUNC_INFO),
		JS_STRCONST(SOLARD_FUNC_CONFIG),
		JS_STRCONST(SOLARD_FUNC_DATA),
		JS_STRCONST(SOLARD_FUNC_EVENT),
#ifdef DEBUG
		{ "DEBUG", INT_TO_JSVAL(1) },
#else
		{ "DEBUG", INT_TO_JSVAL(0) },
#endif
#ifdef DEBUG_MEM
		{ "DEBUG_MEM", INT_TO_JSVAL(1) },
#else
		{ "DEBUG_MEM", INT_TO_JSVAL(0) },
#endif
		{0}
	};

	dprintf(1,"Defining common properties...\n");
	if(!JS_DefineProperties(cx, parent, common_props)) {
		dprintf(1,"error defining common properties\n");
		return 1;
	}
	dprintf(1,"Defining common functions...\n");
	if(!JS_DefineFunctions(cx, parent, common_funcs)) {
		dprintf(1,"error defining common functions\n");
		return 1;
	}
	if(!JS_DefineConstants(cx, parent, common_consts)) {
		dprintf(1,"error defining common constants\n");
		return 1;
	}
	return 0;
}

JSEngine *common_jsinit(int rtsize, int stksize, js_outputfunc_t *jsout) {
	JSEngine *e;

	/* Init engine */
	dprintf(2,"calling JS_EngineInit...\n");
	e = JS_EngineInit(rtsize, stksize, jsout);
	if (!e) {
		log_error("unable to initialize JS Engine!\n");
		return 0;
	}

	/* Add Init functions (non classes) */
	JS_EngineAddInitFunc(e, "js_common_init", js_common_init, 0);
	JS_EngineAddInitFunc(e, "js_types_init", js_types_init, 0);
	JS_EngineAddInitFunc(e, "js_log_init", js_log_init, 0);
//	JS_EngineAddInitFunc(e, "js_json_init", js_json_init, 0);

	if (config_jsinit(e)) return 0;
	if (transports_jsinit(e)) return 0;

	/* Add Init classes */
	JS_EngineAddInitClass(e, "js_InitmyJSONClass", js_InitmyJSONClass);
	JS_EngineAddInitClass(e, "js_InitDriverClass", js_InitDriverClass);
	JS_EngineAddInitClass(e, "js_InitAgentClass", js_InitAgentClass);
	JS_EngineAddInitClass(e, "js_InitClientClass", js_InitClientClass);
	JS_EngineAddInitClass(e, "js_InitMQTTClass", js_InitMQTTClass);
	JS_EngineAddInitClass(e, "js_InitMessageClass", js_InitMessageClass);
	if (influx_jsinit(e)) return 0;
	JS_EngineAddInitClass(e, "js_InitBatteryClass", js_InitBatteryClass);
//	JS_EngineAddInitClass(e, "js_InitInverterClass", js_InitInverterClass);
	return e;
}
#endif
