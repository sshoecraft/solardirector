#ifdef MQTT
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 4
#include "debug.h"

#include "client.h"
//#include <libgen.h>
//#include <pthread.h>

#ifdef JS
#include "jsobj.h"
#include "jsstr.h"
#include "jsarray.h"
#include "jsfun.h"
#include "jsatom.h"
#endif

#define DEBUG_STARTUP 0

#if DEBUG_STARTUP
#define _ST_DEBUG LOG_DEBUG
#else
#define _ST_DEBUG 0
#endif

static list clients = 0;

#if 0
#ifdef JS
struct js_client_rootinfo {
	JSContext *cx;
	void *vp;
	char *name;
};
#endif
#endif

static void client_getmsg(void *ctx, char *topic, char *message, int msglen, char *replyto) {
	solard_client_t *c = ctx;
	solard_message_t newmsg;

	dprintf(dlevel,"addmq: %d\n", c->addmq);
	if (!c->addmq) return;
	if (solard_getmsg(&newmsg,topic,message,msglen,replyto)) return;
	list_add(c->mq,&newmsg,sizeof(newmsg));
}

void client_mktopic(char *topic, int topicsz, char *name, char *func) {
	register char *p;

	dprintf(dlevel,"name: %s, func: %s\n", name, func);

	p = topic;
	p += snprintf(p,topicsz-strlen(topic),"%s/%s",SOLARD_TOPIC_ROOT,SOLARD_TOPIC_CLIENTS);
	if (name) p += snprintf(p,topicsz-strlen(topic),"/%s",name);
	if (func) p += snprintf(p,topicsz-strlen(topic),"/%s",func);
}

void client_event(solard_client_t *c, char *module, char *action) {
//	json_object_t *o;

	if (c->e) event_signal(c->e, c->name, module, action);

#if 0
	o = json_create_object();
	json_object_add_string(o,"name",c->name);
	json_object_add_string(o,"module",module);
	json_object_add_string(o,"action",action);

#ifdef MQTT
	if (mqtt_connected(c->m)) {
		char *event;

		event = json_dumps(json_object_value(o), 1);
		dprintf(dlevel,"event: %s\n", event);
		if (event) {
			char topic[SOLARD_TOPIC_SIZE];
			client_mktopic(topic,sizeof(topic)-1,c->name,"Event");
			mqtt_pub(c->m,topic,event,1,0);
			free(event);
		}
	}
#endif
#ifdef INFLUX
	dprintf(dlevel,"influx_connected: %d\n", influx_connected(c->i));
	if (influx_connected(c->i)) influx_write_json(c->i,"events",json_object_value(o));
#endif
	json_destroy_object(o);
#endif
}

int client_event_handler(solard_client_t *c, event_handler_t *func, void *ctx, char *name, char *module, char *action) {
	return event_handler(c->e, func, ctx, name, module, action);
}

static int client_startup(solard_client_t *c, char *configfile, char *mqtt_info,
		char *influx_info, config_property_t *prog_props) {
	config_property_t client_props[] = {
#if 0
#ifdef MQTT
		{ "addmq", DATA_TYPE_BOOL, &c->addmq, 0, "yes", 0 },
#endif
#ifdef JS
		{ "rtsize", DATA_TYPE_INT, &c->js.rtsize, 0, 0, 0 },
		{ "stacksize", DATA_TYPE_INT, &c->js.stacksize, 0, 0, 0 },
#endif
#endif
		{0}
	};
	config_property_t *props;
	void *eptr;
#ifdef MQTT
	void *mptr;
#endif
#ifdef INFLUX
	void *iptr;
#endif
#ifdef JS
	void *jptr;
#endif

	props = config_combine_props(prog_props,client_props);

	eptr = (c->flags & CLIENT_FLAG_NOEVENT ? 0 : &c->e);
#ifdef MQTT
	mptr = (c->flags & CLIENT_FLAG_NOMQTT ? 0 : &c->m);
#endif
#ifdef INFLUX
	iptr = (c->flags & CLIENT_FLAG_NOINFLUX ? 0 : &c->i);
#endif
#ifdef JS
	jptr = (c->flags & CLIENT_FLAG_NOJS ? 0 : &c->js);
#endif

        /* Call common startup */
	if (solard_common_startup(&c->cp, c->section_name, configfile, props, 0, eptr
#ifdef MQTT
		,mptr, 0, client_getmsg, c, mqtt_info, c->config_from_mqtt
#endif
#ifdef INFLUX
		,iptr, influx_info
#endif
#ifdef JS
		,jptr, c->js.rtsize, c->js.stacksize, (js_outputfunc_t *)log_info
#endif
	)) return 1;
        if (props) free(props);

#ifdef MQTT
	if (0) {
		char my_mqtt_info[256];
		mqtt_get_config(my_mqtt_info,sizeof(my_mqtt_info)-1,c->m,0);
		dprintf(0,"my_mqtt_info: %s\n", my_mqtt_info);
		exit(0);
	}
#endif
	config_add_props(c->cp, "client", client_props, CONFIG_FLAG_NOSAVE | CONFIG_FLAG_NOID);
	return 0;
}

solard_client_t *client_init(int argc,char **argv,char *Cname, char *version,opt_proctab_t *client_opts, int flags,
			config_property_t *props) {
	solard_client_t *c;
	char mqtt_info[200],influx_info[200];
	char configfile[256];
	char name[64],sname[CFG_SECTION_NAME_SIZE];
#ifdef MQTT
	int config_from_mqtt;
	int addmq_flag;
#endif
#ifdef JS
	char jsexec[4096];
	int rtsize,stksize;
	char script[256];
#endif
	opt_proctab_t std_opts[] = {
		/* Spec, dest, type len, reqd, default val, have */
#ifdef MQTT
		{ "-m::|mqtt host,clientid[,user[,pass]]",&mqtt_info,DATA_TYPE_STRING,sizeof(mqtt_info)-1,0,"" },
		{ "-M|get config from mqtt",&config_from_mqtt,DATA_TYPE_LOGICAL,0,0,"N" },
		{ "-A|Add to message que",&addmq_flag,DATA_TYPE_LOGICAL,0,0,"no" },
#endif
		{ "-c:%|config file",&configfile,DATA_TYPE_STRING,sizeof(configfile)-1,0,"" },
		{ "-s::|config section name",&sname,DATA_TYPE_STRING,sizeof(sname)-1,0,"" },
		{ "-n::|client name",&name,DATA_TYPE_STRING,sizeof(name)-1,0,"" },
#ifdef INFLUX
		{ "-i::|influx host[,user[,pass]]",&influx_info,DATA_TYPE_STRING,sizeof(influx_info)-1,0,"" },
#endif
#ifdef JS
//		{ "-e:%|exectute javascript",&jsexec,DATA_TYPE_STRING,sizeof(jsexec)-1,0,"" },
                { "-U:#|javascript runtime size",&rtsize,DATA_TYPE_INT,0,0,"1M" },
                { "-K:#|javascript stack size",&stksize,DATA_TYPE_INT,0,0,"64K" },
		{ "-X::|execute JS script and exit",&script,DATA_TYPE_STRING,sizeof(script)-1,0,"" },
#endif
		OPTS_END
	}, *opts;
	int logopts = LOG_INFO|LOG_WARNING|LOG_ERROR|LOG_SYSERR|_ST_DEBUG;

	int ldlevel = dlevel;

	dprintf(ldlevel,"init'ing\n");
	opts = opt_combine_opts(std_opts,client_opts);
	dprintf(ldlevel,"opts: %p\n", opts);
	if (!opts) {
		printf("error initializing options processing\n");
		return 0;
	}
	*configfile = 0;
#ifdef MQTT
	*mqtt_info = 0;
#endif
#ifdef INFLUX
	*influx_info = 0;
#endif
#ifdef JS
	*script = *jsexec = 0;
#endif
	dprintf(ldlevel,"argv: %p\n", argv);
	if (!Cname && argv) {
		dprintf(ldlevel,"NEW Cname: %s\n", Cname);
		Cname = argv[0];
	}
#if DEBUG_STARTUP
	printf("argc: %d, argv: %p\n", argc, argv);
#endif
	/* still need to call common init even with no args (so defaults get set), bonehead! */
	if (solard_common_init(argc,argv,version,opts,logopts)) {
		printf("client_init: error calling common_init (bad option table?)\n");
		return 0;
	}
	if (opts) free(opts);

	dprintf(ldlevel,"creating session...\n");
	c = calloc(1,sizeof(*c));
	if (!c) {
		log_syserror("client_init: malloc(%d)",sizeof(*c));
		return 0;
	}
	dprintf(ldlevel,"name: %s\n", name);
	if (strlen(name)) {
		strncpy(c->name,name,sizeof(c->name)-1);
	} else if (Cname) {
		strncpy(c->name,Cname,sizeof(c->name)-1);
	} else {
		strncpy(c->name,"client",sizeof(c->name)-1);
	}
	dprintf(ldlevel,"c->name: %s\n", c->name);
	dprintf(ldlevel,"sname: %s\n", sname);
	if (strlen(sname)) strncpy(c->section_name,sname,sizeof(c->section_name)-1);
	else strncpy(c->section_name,c->name,sizeof(c->section_name)-1);
	dprintf(ldlevel,"c->section_name: %s\n", c->section_name);
#ifdef MQTT
	c->config_from_mqtt = config_from_mqtt;
	c->mq = list_create();
	c->addmq = addmq_flag;
#endif
	c->flags = flags;
#ifdef JS
	c->js.rtsize = rtsize;
	c->js.stacksize = stksize;
	c->js.roots = list_create();
#endif

	/* Auto generate configfile if not specified */
	if (!strlen(configfile)) {
		char *types[] = { "json", "conf", 0 };
		char path[300];
		int j,f;

		f = 0;
		for(j=0; types[j]; j++) {
			sprintf(path,"%s/%s.%s",SOLARD_ETCDIR,c->name,types[j]);
			dprintf(ldlevel,"trying: %s\n", path);
			if (access(path,0) == 0) {
				f = 1;
				break;
			}
		}
		if (f) strcpy(configfile,path);
	} else if (strcmp(configfile,"none") == 0) {
		configfile[0] = 0;
	}
        dprintf(ldlevel,"configfile: %s\n", configfile);

	if (client_startup(c,configfile,mqtt_info,influx_info,props)) goto client_init_error;

	/* Call initcb if specified */
//	dprintf(ldlevel,"initcb: %p\n",initcb);
//	if (initcb && initcb(ctx,c)) goto client_init_error;

#ifdef JS
	/* Execute any command-line javascript code */
	dprintf(ldlevel,"jsexec(%d): %s\n", strlen(jsexec), jsexec);
	if (strlen(jsexec)) {
		if (JS_EngineExecString(c->js.e, jsexec))
			log_warning("error executing js expression\n");
	}

#if 0
	/* Start the init script */
	snprintf(jsexec,sizeof(jsexec)-1,"%s/%s",c->script_dir,c->init_script);
	if (access(jsexec,0)) JS_EngineExec(c->js,jsexec,0,0,0,0);

	/* if specified, Execute javascript file then exit */
	dprintf(ldlevel,"script: %s\n", script);
	if (strlen(script)) {
		JS_EngineExec(c->js,script,0,0,0,0);
		exit(0);
	}
#endif
#endif

	if (!clients) clients = list_create();
	list_add(clients,c,0);

	dprintf(ldlevel,"returning: %p\n", c);
	return c;
client_init_error:
	free(c);
	return 0;
}

void client_shutdown(void) {
#if 0
	if (clients) {
		solard_client_t *c;
		while(true) {
			list_reset(clients);
			c = list_get_next(clients);
			if (!c) break;
			client_destroy_client(c);
		}
		list_destroy(clients);
		clients = 0;
	}
#endif
	config_shutdown();
	mqtt_shutdown();
#ifdef INFLUX
	influx_shutdown();
#endif
	common_shutdown();
}

void client_destroy_client(solard_client_t *c) {

	dprintf(dlevel,"c: %p\n", c);
	if (!c) return;

#if 0
#ifdef JS
	/* XXX roots must be destroyed first because finalize is called on CTOR sessions */
	if (c->js.roots) {
		struct js_agent_rootinfo *ri;

		list_reset(c->js.roots);
		while((ri = list_get_next(c->js.roots)) != 0) {
			dprintf(dlevel,"removing root: %s\n", ri->name);
			JS_RemoveRoot(ri->cx,ri->vp);
			JS_free(ri->cx,ri->name);
		}
		list_destroy(c->js.roots);
        }
#endif
#endif
	dprintf(dlevel,"cp: %p\n", c->cp);
	config_destroy_config(c->cp);
#ifdef MQTT
	if (c->m) mqtt_destroy_session(c->m);
	list_destroy(c->mq);
#endif
#ifdef INFLUX
	if (c->i) influx_destroy_session(c->i);
#endif
	if (c->e) event_destroy(c->e);
#ifdef JS
	if (c->js.props) free(c->js.props);
	if (c->js.e) JS_EngineDestroy(c->js.e);
#endif
	if (clients) list_delete(clients,c);
	free(c);
}

#ifdef JS
enum CLIENT_PROPERTY_ID {
	CLIENT_PROPERTY_ID_NAME=2048,
#ifdef MQTT
	CLIENT_PROPERTY_ID_MQ,
	CLIENT_PROPERTY_ID_ADDMQ,
	CLIENT_PROPERTY_ID_PURGE,
#endif
};

static JSBool js_client_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	solard_client_t *c;
	config_property_t *p;

	c = JS_GetPrivate(cx,obj);
	dprintf(dlevel,"c: %p\n", c);
	if (!c) {
		JS_ReportError(cx, "client_getprop: private is null!");
		return JS_FALSE;
	}
	p = 0;
	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case CLIENT_PROPERTY_ID_NAME:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,c->name));
			break;
#ifdef MQTT
		case CLIENT_PROPERTY_ID_MQ:
			*rval = OBJECT_TO_JSVAL(js_create_messages_array(cx,obj,c->mq));
			break;
		case CLIENT_PROPERTY_ID_ADDMQ:
			*rval = type_to_jsval(cx,DATA_TYPE_BOOL,&c->addmq,0);
			break;
#endif
		default:
			p = CONFIG_GETMAP(c->cp,prop_id);
			dprintf(dlevel,"p: %p\n", p);
			if (!p) p = config_get_propbyid(c->cp,prop_id);
			dprintf(dlevel,"p: %p\n", p);
			if (!p) {
				JS_ReportError(cx, "js_client_getprop: internal error: property %d not found", prop_id);
				*rval = JSVAL_VOID;
				return JS_FALSE;
			}
			break;
		}
	} else if (JSVAL_IS_STRING(id)) {
		char *sname, *name;
		JSClass *classp = OBJ_GET_CLASS(cx, obj);

		sname = (char *)classp->name;
		name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
		dprintf(dlevel,"sname: %s, name: %s\n", sname, name);
		if (sname && name) p = config_get_property(c->cp, sname, name);
		if (name) JS_free(cx, name);
	}
	dprintf(dlevel,"p: %p\n", p);
	if (p && p->dest) {
		dprintf(dlevel,"p: type: %d(%s), name: %s\n", p->type, typestr(p->type), p->name);
		*rval = type_to_jsval(cx,p->type,p->dest,p->len);
	}
	return JS_TRUE;
}

static JSBool js_client_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	solard_client_t *c;
	int prop_id;

	int ldlevel = dlevel;

	c = JS_GetPrivate(cx,obj);
	dprintf(ldlevel,"c: %p\n", c);
	if (!c) {
		JS_ReportError(cx, "client_getprop: private is null!");
		return JS_FALSE;
	}
	dprintf(ldlevel,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(ldlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
#ifdef MQTT
		case CLIENT_PROPERTY_ID_ADDMQ:
			jsval_to_type(DATA_TYPE_BOOL,&c->addmq,0,cx,*vp);
			break;
#endif
		default:
			return js_config_common_setprop(cx, obj, id, vp, c->cp, c->name);
		}
	} else if (JSVAL_IS_STRING(id)) {
		char *name;
		name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
		if (name) {
			config_property_t *p;

			dprintf(ldlevel,"value type: %s\n", jstypestr(cx,*vp));
			p = config_get_property(c->cp, c->name, name);
			JS_free(cx, name);
			dprintf(ldlevel,"p: %p\n", p);
			if (p) return js_config_property_set_value(p,cx,*vp,c->cp->triggers);
		}
	}
	return JS_TRUE;
}

static JSClass js_client_class = {
	"Client",		/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	js_client_getprop,	/* getProperty */
	js_client_setprop,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool js_client_mktopic(JSContext *cx, uintN argc, jsval *vp) {
	solard_client_t *c;
	char topic[SOLARD_TOPIC_SIZE], *name, *func;
	int topicsz = SOLARD_TOPIC_SIZE;
	JSObject *obj;
	register char *p;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	c = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"c: %p\n", c);

	if (argc != 1) {
		JS_ReportError(cx,"mktopic requires 1 arguments (func:string)\n");
		return JS_FALSE;
	}

        func = 0;
        if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "s s", &name, &func)) return JS_FALSE;
	dprintf(dlevel,"func: %s\n", func);

//	client_mktopic(topic,sizeof(topic)-1,c,c->instance_name,func);

	dprintf(dlevel,"name: %s, func: %s\n", name, func);

	p = topic;
	p += snprintf(p,topicsz-strlen(topic),"%s/%s/%s/%s",SOLARD_TOPIC_ROOT,SOLARD_TOPIC_AGENTS,name,func);
	p += snprintf(p,topicsz-strlen(topic),"/%s",name);
	p += snprintf(p,topicsz-strlen(topic),"/%s",func);
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,topic));
	return JS_TRUE;
}

static JSBool js_client_event(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	solard_client_t *c;
	char *module,*action;

	c = JS_GetPrivate(cx, obj);
	if (!c) {
		JS_ReportError(cx,"client private is null!\n");
		return JS_FALSE;
	}
	module = action = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "s s", &module, &action)) return JS_FALSE;
	client_event(c,module,action);
	if (module) JS_free(cx,module);
	if (action) JS_free(cx,action);
	return JS_TRUE;
}

struct js_client_event_info {
	JSContext *cx;
	jsval func;
	char *name;
};

static void _js_client_event_handler(void *ctx, char *name, char *module, char *action) {
	struct js_client_event_info *info = ctx;
	JSBool ok;
	jsval args[3];
	jsval rval;

	dprintf(dlevel,"name: %s, module: %s, action: %s\n", name, module, action);

	/* destroy context */
	if (!name && !module && action && strcmp(action,"__destroy__") == 0) {
		free(info);
		return;
	}

	args[0] = STRING_TO_JSVAL(JS_InternString(info->cx,name));
	args[1] = STRING_TO_JSVAL(JS_InternString(info->cx,module));
	args[2] = STRING_TO_JSVAL(JS_InternString(info->cx,action));
	ok = JS_CallFunctionValue(info->cx, JS_GetGlobalObject(info->cx), info->func, 3, args, &rval);
	dprintf(dlevel,"call ok: %d\n", ok);
	return;
}

static JSBool js_client_event_handler(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	solard_client_t *c;
	char *fname, *name, *module,*action;
	JSFunction *fun;
	jsval func;
	struct js_client_event_info *ctx;
//	struct js_client_rootinfo ri;

	c = JS_GetPrivate(cx, obj);
	if (!c) {
		JS_ReportError(cx,"client private is null!\n");
		return JS_FALSE;
	}
	func = 0;
	name = module = action = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "v / s s s", &func, &name, &module, &action)) return JS_FALSE;
	dprintf(dlevel,"VALUE_IS_FUNCTION: %d\n", VALUE_IS_FUNCTION(cx, func));
	if (!VALUE_IS_FUNCTION(cx, func)) {
                JS_ReportError(cx, "client_event_handler: arguments: required: function(function), optional: name(string), module(string), action(string)");
                return JS_FALSE;
        }
	fun = JS_ValueToFunction(cx, argv[0]);
	dprintf(dlevel,"fun: %p\n", fun);
	if (!fun) {
		JS_ReportError(cx, "js_client_event_handler: internal error: funcptr is null!");
		return JS_FALSE;
	}
	fname = JS_EncodeString(cx, ATOM_TO_STRING(fun->atom));
	dprintf(dlevel,"fname: %s\n", fname);
	if (!fname) {
		JS_ReportError(cx, "js_client_event_handler: internal error: fname is null!");
		return JS_FALSE;
	}
	ctx = malloc(sizeof(*ctx));
	if (!ctx) {
		JS_ReportError(cx, "js_client_event_handler: internal error: malloc ctx");
		return JS_FALSE;
	}
	memset(ctx,0,sizeof(*ctx));
	ctx->cx = cx;
	ctx->func = func;
	ctx->name = fname;
	client_event_handler(c,_js_client_event_handler,ctx,name,module,action);

	JS_EngineAddRoot(cx,fname,&ctx->func);
#if 0
	/* root it */
        ri.name = fname;
	printf("===> ADDING ROOT: %s\n", fname);
        JS_AddNamedRoot(cx,&ctx->func,fname);
        ri.cx = cx;
        ri.vp = &ctx->func;
        list_add(c->js.roots,&ri,sizeof(ri));
#endif

	if (name) JS_free(cx,name);
	if (module) JS_free(cx,action);
	if (action) JS_free(cx,action);
	return JS_TRUE;
}

#ifdef MQTT
static JSBool js_client_purgemq(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	solard_client_t *c;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	c = JS_GetPrivate(cx, obj);
	if (!c) {
		JS_ReportError(cx,"client private is null!\n");
		return JS_FALSE;
	}
	list_purge(c->mq);
	return JS_TRUE;
}

static JSBool js_client_deletemsg(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	solard_client_t *c;
	JSObject *mobj;

	c = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"c: %p\n", c);
	if (!c) {
		JS_ReportError(cx,"internal error: client private is null!\n");
		return JS_FALSE;
	}
	mobj = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "o", &mobj)) return JS_FALSE;
	dprintf(dlevel,"mobj: %p\n", mobj);
	if (mobj) {
		solard_message_t *msg = JS_GetPrivate(cx, mobj);
		dprintf(dlevel,"msg: %p\n", msg);
		list_delete(c->mq, msg);
	}
	return JS_TRUE;
}
#endif

static JSBool js_client_callfunc(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	solard_client_t *c;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) {
		JS_ReportError(cx, "js_client_callfunc: internal error: object is null!\n");
		return JS_FALSE;
	}
	c = JS_GetPrivate(cx, obj);
	if (!c) {
		JS_ReportError(cx, "js_client_callfunc: internal error: private is null!\n");
		return JS_FALSE;
	}
	dprintf(dlevel,"c: %p\n", c);
	if (!c) return JS_TRUE;
	dprintf(dlevel,"c->cp: %p\n", c->cp);
	if (!c->cp) return JS_TRUE;

	dprintf(dlevel,"argc: %d\n", argc);
	return js_config_callfunc(c->cp, cx, argc, vp);
}

static JSBool js_client_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	solard_client_t *c;
	char **args, *name, *vers;
        unsigned int count;
	JSObject *aobj, *oobj, *pobj, *newobj;
	int i,sz;
	jsval val;
	JSString *jstr;
	opt_proctab_t *opts;
	config_property_t *props;

	dprintf(dlevel,"cx: %p, obj: %p, argc: %d\n", cx, obj, argc);

	name = vers = 0;
	aobj = oobj = pobj = 0;
        if (!JS_ConvertArguments(cx, argc, argv, "o s s / o o", &aobj, &name, &vers, &oobj, &pobj)) return JS_FALSE;
        dprintf(dlevel,"aobj: %p, name: %s, vers: %s, oobj: %p, pobj: %p\n", aobj, name, vers, oobj, pobj);

	/* get arg array count and alloc props */
	if (!js_GetLengthProperty(cx, aobj, &count)) {
		JS_ReportError(cx,"unable to get argv array length");
		return JS_FALSE;
	}
	dprintf(dlevel,"count: %d\n", count);
	if (count) {
		/* prepend program name (client name) */
		count++;
		sz = sizeof(char *) * count;
		dprintf(dlevel,"sz: %d\n", sz);
		args = JS_malloc(cx,sz);
		if (!args) {
			JS_ReportError(cx,"client_ctor: internal error: unable to malloc args");
			return JS_FALSE;
		}
		memset(args,0,sz);

		args[0] = name;
		for(i=1; i < count; i++) {
			JS_GetElement(cx, aobj, i-1, &val);
			dprintf(dlevel,"aobj[%d] type: %s\n", i, jstypestr(cx,val));
			jstr = JS_ValueToString(cx, val);
			args[i] = (char *)JS_EncodeString(cx, jstr);
		}
//		for(i=0; i < count; i++) dprintf(dlevel,"args[%d]: %s\n", i, args[i]);
	} else {
		args = 0;
	}

	/* Convert opts object to opt_property_t array */
	if (oobj) {
		opts = js_opt_arr2opts(cx, oobj);
		dprintf(dlevel,"opts: %p\n", opts);
		if (!opts) {
			JS_ReportError(cx, "client_ctor: internal error: opts is null!");
			return JS_FALSE;
		}
	} else {
		opts = 0;
	}

	/* Convert props object to config_property_t array */
	if (pobj) {
		props = js_config_obj2props(cx, pobj);
		if (!props) {
			JS_ReportError(cx, "client_ctor: internal error: props is null!");
			return JS_FALSE;
		}
	} else {
		props = 0;
	}

	/* init */
	c = client_init(count,args,name,vers,opts,CLIENT_FLAG_NOJS,props);
	if (args) {
		for(i=0; i < count; i++) JS_free(cx, args[i]);
		JS_free(cx,args);
	}
	dprintf(dlevel,"c: %p\n", c);
	if (!c) {
//		JS_ReportError(cx, "client_init failed");
		*rval = JSVAL_NULL;
		return JS_FALSE;
	}

	/* create a new client instance */
	newobj = js_client_new(cx,obj,c);
	if (!newobj) {
		JS_ReportError(cx, "client_ctor: internal error: unable to create new client object!");
		return JS_FALSE;
	}

	/* Create an object with the option names and values and attach to the new instance */
	js_opt_add_opts(cx,newobj,opts);

	*rval = OBJECT_TO_JSVAL(newobj);
	return JS_TRUE;
}

JSObject *js_InitClientClass(JSContext *cx, JSObject *parent) {
	JSPropertySpec client_props[] = {
                { "name", CLIENT_PROPERTY_ID_NAME, JSPROP_ENUMERATE | JSPROP_READONLY },
#ifdef MQTT
                { "mq", CLIENT_PROPERTY_ID_MQ, JSPROP_ENUMERATE | JSPROP_READONLY },
                { "addmq", CLIENT_PROPERTY_ID_ADDMQ, JSPROP_ENUMERATE },
#endif
		{ 0 }
	};
	JSFunctionSpec client_funcs[] = {
#ifdef MQTT
		JS_FN("mktopic",js_client_mktopic,1,1,0),
		JS_FN("purgemq",js_client_purgemq,0,0,0),
		JS_FS("delete_message",js_client_deletemsg,1,1,0),
#endif
		JS_FS("event",js_client_event,2,2,0),
		JS_FS("event_handler",js_client_event_handler,1,4,0),
		{ 0 }
	};
	JSObject *obj;

	dprintf(dlevel,"creating %s class\n",js_client_class.name);
	obj = JS_InitClass(cx, parent, 0, &js_client_class, js_client_ctor, 3, client_props, client_funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s class", js_client_class.name);
		return 0;
	}

	return obj;
}

JSObject *js_client_new(JSContext *cx, JSObject *parent, void *priv) {
	solard_client_t *c = priv;
	JSObject *newobj;

//	dprintf(dlevel,"parent name: %s\n", JS_GetObjectName(cx,parent));

	/* Create the new object */
	dprintf(2,"creating %s object\n",js_client_class.name);
	newobj = JS_NewObject(cx, &js_client_class, 0, parent);
	if (!newobj) return 0;

	/* Add config props/funcs */
	dprintf(dlevel,"c->cp: %p\n", c->cp);
	if (!c->cp) {
		dprintf(-1,"CP IS NULL!\n");
	}
	/* Create client config props */
	JSPropertySpec *props = js_config_to_props(c->cp, cx, "client", 0);
	if (!props) {
		log_error("js_client_new: unable to create config props: %s\n", config_get_errmsg(c->cp));
		return 0;
	}

	dprintf(dlevel,"Defining client config props\n");
	if (!JS_DefineProperties(cx, newobj, props)) {
		JS_ReportError(cx,"unable to define props");
		return 0;
	}
	free(props);

	/* Create client config funcs */
	JSFunctionSpec *funcs = js_config_to_funcs(c->cp, cx, js_client_callfunc, 0);
	if (!funcs) {
		log_error("unable to create funcs: %s\n", config_get_errmsg(c->cp));
		return 0;
	}
	dprintf(dlevel,"Defining client config funcs\n");
	if (!JS_DefineFunctions(cx, newobj, funcs)) {
		JS_ReportError(cx,"unable to define funcs");
		return 0;
	}
	free(funcs);

	dprintf(dlevel,"Setting private to: %p\n", c);
	JS_SetPrivate(cx,newobj,c);

	if (c->cp) JS_DefineProperty(cx, newobj, "config", OBJECT_TO_JSVAL(js_config_new(cx,newobj,c->cp)), 0, 0, JSPROP_ENUMERATE);
#ifdef MQTT
	if (c->m) JS_DefineProperty(cx, newobj, "mqtt", OBJECT_TO_JSVAL(js_mqtt_new(cx,newobj,c->m)), 0, 0, JSPROP_ENUMERATE);
#endif
#ifdef INFLUX
	if (c->i) JS_DefineProperty(cx, newobj, "influx", OBJECT_TO_JSVAL(js_influx_new(cx,newobj,c->i)), 0, 0, JSPROP_ENUMERATE);
#endif

#if 0
	/* Only used by driver atm */
	dprintf(dlevel,"Creating client_val...\n");
	c->js.client_val = OBJECT_TO_JSVAL(newobj);
	dprintf(dlevel,"client_val: %x\n", c->js.client_val);

#ifdef MQTT
	if (c->m) {
		/* Create MQTT child object */
		dprintf(dlevel,"Creating mqtt_val...\n");
		if (c->m) c->js.mqtt_val = OBJECT_TO_JSVAL(js_mqtt_new(cx,newobj,c->m));
		JS_DefineProperty(cx, newobj, "mqtt", c->js.mqtt_val, 0, 0, JSPROP_ENUMERATE);
	}
#endif
#ifdef INFLUX
	if (c->i) {
		/* Create Influx child object */
		dprintf(dlevel,"Creating influx_val...\n");
		if (c->i) c->js.influx_val = OBJECT_TO_JSVAL(js_influx_new(cx,newobj,c->i));
		JS_DefineProperty(cx, newobj, "influx", c->js.influx_val, 0, 0, JSPROP_ENUMERATE);
	}
#endif
#endif

	return newobj;
}

#endif
#endif
