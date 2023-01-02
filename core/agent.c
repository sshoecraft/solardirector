
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define DEBUG_AGENT 1
#define dlevel 2

#ifdef DEBUG
#undef DEBUG
#define DEBUG DEBUG_AGENT
#endif
#include "debug.h"

#include "agent.h"
#ifdef JS
#include "jsobj.h"
#include "jsjson.h"
#include "jsarray.h"
#include "jsstr.h"
#include "jsfun.h"
#include "jsatom.h"
#endif

#define DEBUG_STARTUP 0
#if DEBUG_STARTUP
#define _ST_DEBUG LOG_DEBUG
#else
#define _ST_DEBUG 0
#endif

#define AGENT_HASFLAG(f) check_bit(ap->flags,SOLARD_AGENT_FLAG_##f)

int agent_set_callback(solard_agent_t *ap, solard_agent_callback_t cb, void *ctx) {
	ap->callback.func = cb;
	ap->callback.ctx = ctx;
	return 0;
}

int agent_clear_callback(solard_agent_t *ap) {
	ap->callback.func = 0;
	ap->callback.ctx = 0;
	return 0;
}

#ifdef MQTT
void agent_mktopic(char *topic, int topicsz, char *name, char *func) {
	register char *p;

	dprintf(dlevel,"name: %s, func: %s\n", name, func);

	p = topic;
	p += snprintf(p,topicsz-strlen(topic),"%s/%s",SOLARD_TOPIC_ROOT,SOLARD_TOPIC_AGENTS);
	if (name) p += snprintf(p,topicsz-strlen(topic),"/%s",name);
	if (func) p += snprintf(p,topicsz-strlen(topic),"/%s",func);
}

int agent_sub(solard_agent_t *ap, char *name, char *func) {
	char topic[SOLARD_TOPIC_LEN];

	if (!ap->m) return 0;
	if (!ap->m->enabled) return 0;

	dprintf(dlevel,"name: %s, func: %s\n", name, func);

	*topic = 0;
	agent_mktopic(topic,sizeof(topic)-1,name,func);
        dprintf(dlevel,"topic: %s\n", topic);
	return mqtt_sub(ap->m,topic);
}

int agent_pub(solard_agent_t *ap, char *func, char *message, int retain) {
	char topic[SOLARD_TOPIC_LEN];
	char temp[32];

	if (!ap->m) return 0;
	if (!ap->m->enabled) return 0;

	temp[0] = 0;
	strncat(temp,message,sizeof(temp)-1);
	dprintf(dlevel,"func: %s, message: %s, retain: %d\n", func, temp, retain);

	*topic = 0;
	agent_mktopic(topic,sizeof(topic)-1,ap->instance_name,func);
        dprintf(dlevel,"topic: %s\n", topic);
	if (mqtt_pub(ap->m,topic,message,1,retain)) {
		log_error("agent_pub: mqtt_pub: %s\n", ap->m->errmsg);
		return 1;
	}
	return 0;
}

int agent_pubinfo(solard_agent_t *ap, int disp) {
	char *info;
	int r;

	if (!ap->m) return 0;
	if (!ap->info) return 1;
	if (!ap->m->enabled) return 0;

	info = json_dumps(ap->info, 1);
	dprintf(dlevel,"info: %p\n", info);
	if (!info) {
		log_syserror("agent_pubinfo: json_dump");
		return 1;
	}
	dprintf(dlevel,"disp: %d\n", disp);
	if (disp) {
		printf("%s\n",info);
		r = 0;
	} else {
		r = agent_pub(ap, SOLARD_FUNC_INFO, info, 1);
	}
	free(info);
	dprintf(dlevel,"r: %d\n", r);
	return r;
}

int agent_pubconfig(solard_agent_t *ap) {
	char *data;
	json_value_t *v;
	int r;

	if (!ap->m) return 0;
	if (!ap->m->enabled) return 0;

	dprintf(dlevel,"publishing config...\n");

	v = config_to_json(ap->cp,CONFIG_FLAG_NOPUB,true);
	dprintf(dlevel+1,"v: %p\n", v);
	if (!v) {
		if (ap->cp) log_error(config_get_errmsg(ap->cp));
		return 1;
	}
	data = json_dumps(v, 1);
	json_destroy_value(v);
	if (!data) return 1;

	r = agent_pub(ap, SOLARD_FUNC_CONFIG, data, 1);
	free(data);
	return r;
}

int agent_pubdata(solard_agent_t *ap, json_value_t *v) {
	char *data;
	int r;

	if (!ap->m->enabled) return 0;

	dprintf(dlevel,"publishing data...\n");

	data = json_dumps(v, 1);
	r = agent_pub(ap, SOLARD_FUNC_DATA, data, 0);
	free(data);
	return r;
}

static void agent_process_message(solard_agent_t *ap, solard_message_t *msg) {
	char topic[SOLARD_TOPIC_LEN],*data;
	json_object_t *status;

	status = json_create_object();

	config_process_request(ap->cp, msg->data, status);

	data = json_dumps(json_object_value(status), 1);
	dprintf(dlevel,"status: %s\n", data);
	json_destroy_object(status);
	if (!data) return;

	snprintf(topic,sizeof(topic)-1,"%s/%s/%s/%s",SOLARD_TOPIC_ROOT,SOLARD_TOPIC_CLIENTS,msg->replyto,ap->instance_name);
	dprintf(dlevel,"topic: %s\n", topic);
	mqtt_pub(ap->m,topic,data,1,0);
	free(data);
}

void agent_getmsg(void *ctx, char *topic, char *message, int msglen, char *replyto) {
	solard_agent_t *ap = ctx;
	solard_message_t newmsg;

	if (solard_getmsg(&newmsg,topic,message,msglen,replyto)) return;
	list_add(ap->mq,&newmsg,sizeof(newmsg));
}
#endif

void agent_event(solard_agent_t *ap, char *module, char *action) {
	json_object_t *o;

	dprintf(dlevel,"e: %p\n", ap->e);
	if (!ap->e) return;
	event(ap->e, ap->instance_name, module, action);

	o = json_create_object();
	json_object_add_string(o,"agent",ap->instance_name);
	json_object_add_string(o,"module",module);
	json_object_add_string(o,"action",action);

#ifdef MQTT
	if (ap->m && mqtt_connected(ap->m)) {
		char *event;

		event = json_dumps(json_object_value(o), 1);
		dprintf(dlevel,"event: %s\n", event);
		if (event) {
			char topic[SOLARD_TOPIC_SIZE];
			*topic = 0;
			agent_mktopic(topic,sizeof(topic)-1,ap->instance_name,"Event");
			mqtt_pub(ap->m,topic,event,1,0);
			free(event);
		}
	}
#endif
#ifdef INFLUX
	if (ap->i) {
		dprintf(dlevel,"influx_connected: %d\n", influx_connected(ap->i));
		if (influx_connected(ap->i)) influx_write_json(ap->i,"events",json_object_value(o));
	}
#endif
	json_destroy_object(o);
}

int agent_event_handler(solard_agent_t *ap, event_handler_t *func, void *ctx, char *name, char *module, char *action) {
	return event_handler(ap->e, func, ctx, name, module, action);
}

int agent_get_info(solard_agent_t *ap, int info_flag, int pub) {

	/* If info flag, print info to stdout then exit */
	dprintf(dlevel,"info_flag: %d\n", info_flag);

	if (ap->info) json_destroy_value(ap->info);

	/* Get driver info */
	if (ap->driver && ap->driver->config && ap->driver->config(ap->handle,SOLARD_CONFIG_GET_INFO,&ap->info)) return 1;

#ifdef MQTT
	/* Publish the info */
	if (pub && agent_pubinfo(ap, info_flag)) return 1;
#endif
	if (info_flag) exit(0);
	return 0;
}

#ifdef JS
static int agent_script_path(char *path, int size, solard_agent_t *ap, char *name) {
	char temp[258];

	*path = 0;
	dprintf(dlevel+1,"script_dir: %s, name: %s\n", ap->js.script_dir, name);
	if (*name == NULLCH) return 1;

	if (*name != '.' && strlen(ap->js.script_dir)) {
		snprintf(temp,sizeof(temp)-1,"%s/%s",ap->js.script_dir,name);
		snprintf(path,size,"%s",temp);
	} else {
		snprintf(path,size,"%s",name);
	}
	dprintf(dlevel+1,"path: %s\n", path);
	return 0;
}

int agent_script_exists(solard_agent_t *ap, char *name) {
	char path[256];
	int r;

	if (!ap->js.e) return 0;
	agent_script_path(path,sizeof(path)-1,ap,name);
	r = (access(path,0) == 0);
	dprintf(dlevel+1,"r: %d\n", r);
	return r;
}

int agent_start_jsfunc(solard_agent_t *ap, char *name, char *func, int argc, jsval *argv) {
	char path[256];

	if (agent_script_path(path,sizeof(path)-1,ap,name)) return 1;
	dprintf(dlevel+1,"Running function: %s %s()\n", path, func);
	return JS_EngineExec(ap->js.e, path, func, argc, argv, 0);
}

int agent_start_script(solard_agent_t *ap, char *name) {
	char path[256];

	dprintf(dlevel+1,"e: %p\n", ap->js.e);
	if (!ap->js.e) return 1;
	dprintf(dlevel+1,"run_scripts: %d\n", ap->js.run_scripts);
	if (!ap->js.run_scripts) return 0;
	agent_script_path(path,sizeof(path)-1,ap,name);
	dprintf(dlevel,"Starting script: %s\n", path);
	return JS_EngineExec(ap->js.e, path, 0, 0, 0, 0);
}

int agent_jsexec(solard_agent_t *ap, char *string) {
	return JS_EngineExecString(ap->js.e, string);
}
#endif

int cf_agent_exit(void *ctx, list args, char *errmsg, json_object_t *results) {
	dprintf(dlevel,"exit: clearing RUN flag\n");
	clear_state((solard_agent_t *)ctx,SOLARD_AGENT_STATE_RUNNING);
	return 0;
}

int cf_agent_ping(void *ctx, list args, char *errmsg, json_object_t *results) {
	sprintf(errmsg,"pong");
	return 1;
}

int cf_agent_getinfo(void *ctx, list args, char *errmsg, json_object_t *results) {
	solard_agent_t *ap = ctx;
	char *j;
	json_value_t *v;

	/* empty info is not an error */
	dprintf(dlevel,"ap->info: %p\n", ap->info);
	if (!ap->info && agent_get_info(ap,false,false)) return 0;

	j = json_dumps(ap->info,0);
	if (j) {
		v = json_parse(j);
		free(j);
		if (v) json_object_set_value(results,"info",v);
	}
	return 0;
}

int cf_agent_getconfig(void *ctx, list args, char *errmsg, json_object_t *results) {
	solard_agent_t *ap = ctx;
	json_value_t *v;

	if (!ap->cp) {
		sprintf(errmsg,"config not initialized");
		return 1;
	}
	v = config_to_json(ap->cp,CONFIG_FLAG_NOPUB,true);
	dprintf(dlevel,"v: %p\n", v);
	if (!v) {
		sprintf(errmsg,"%s",config_get_errmsg(ap->cp));
		return 1;
	}
	json_object_set_object(results,"config",json_value_toobject(v));
	return 0;
}

int cf_agent_log_open(void *ctx, list args, char *errmsg, json_object_t *results) {
	solard_agent_t *ap = ctx;
	char *filename;
	extern int logopts;

	dprintf(dlevel,"count: %d\n", list_count(args));
	list_reset(args);
	filename = list_get_next(args);
	dprintf(dlevel,"filename: %s\n", filename);
	if (strcmp(filename,"stdout") == 0) filename = 0;
	log_open(ap->instance_name,filename,logopts);
	return 0;
}

#ifdef JS
#if 0
/* man this is dangerous */
int cf_agent_exec(void *ctx, list args, char *errmsg) {
	solard_agent_t *ap = ctx;
	char *string;
	extern int logopts;

	dprintf(dlevel,"count: %d\n", list_count(args));
	list_reset(args);
	string = list_get_next(args);
	dprintf(dlevel,"string: %s\n", string);
	if (JS_EngineExecString(ap->js.e, string)) {
		sprintf(errmsg,"unable to execute");
		return 1;
	}
	return 0;
}
#endif
#endif

static int agent_config_get_value(void *ctx, list args, char *errmsg, json_object_t *results) {
	solard_agent_t *ap = ctx;
	char *name;
	config_property_t *p;
	json_value_t *v;

	/* handle special cases of info/config */
	dprintf(dlevel,"args count: %d\n", list_count(args));
	list_reset(args);
	while((name = list_get_next(args)) != 0) {
		dprintf(dlevel,"name: %s\n", name);
		p = config_find_property(ap->cp, name);
		dprintf(dlevel,"p: %p\n", p);
		if (!p) {
			/* handle special cases of info/config */
			if (strcmp(name,"info") == 0) {
				cf_agent_getinfo(ap,args,errmsg,results);
				continue;
			} else if (strcmp(name,"config") == 0) {
				cf_agent_getconfig(ap,args,errmsg,results);
				continue;
			} else {
				sprintf(errmsg,"property %s not found",name);
				return 1;
			}
		}
		v = json_from_type(p->type,p->dest,p->len);
		if (v) json_object_set_value(results,name,v);
	}
	return 0;
}

static int agent_service_set(void *ctx, list args, char *errmsg, json_object_t *results) {
	solard_agent_t *ap = ctx;

	if (config_service_set_value(ap->cp, args, errmsg,results)) return 1;
	dprintf(dlevel,"publishing...\n");
	return agent_pubconfig(ap);
}

static int agent_service_clear(void *ctx, list args, char *errmsg, json_object_t *results) {
	solard_agent_t *ap = ctx;

	if (config_service_clear_value(ap->cp, args, errmsg,results)) return 1;
	dprintf(dlevel,"publishing...\n");
	return agent_pubconfig(ap);
}

static int agent_configfile_set(void *ctx, config_property_t *p) {
	solard_agent_t *ap = ctx;

	dprintf(dlevel,"configfile: %s\n", ap->configfile);
	dprintf(dlevel,"ap->cp: %p\n", ap->cp);
	if (ap->cp) config_set_filename(ap->cp,ap->configfile,CONFIG_FILE_FORMAT_AUTO);
	return 0;
}

static int agent_run_set(void *ctx, config_property_t *p) {
	solard_agent_t *ap = ctx;

	dprintf(dlevel,"run_scripts: %d\n", ap->js.run_scripts);
	if (!ap->js.run_scripts) return 0;
	// We dont know what the old value was...
	if (!ap->js.init_run) {
		dprintf(dlevel,"init_script: %s\n", ap->js.init_script);
		if (agent_script_exists(ap,ap->js.init_script) && agent_start_script(ap,ap->js.init_script)) {
			log_error("agent_run_set: init_script failed\n");
			sprintf(ap->errmsg,"init_script %s failed\n", ap->js.init_script);
			return 1;
		}
		ap->js.init_run = true;
	}
	if (!ap->js.start_run) {
		dprintf(dlevel,"start_script: %s\n", ap->js.start_script);
		if (agent_script_exists(ap,ap->js.start_script) && agent_start_script(ap,ap->js.start_script)) {
			log_error("agent_run_set: start_script failed\n");
			sprintf(ap->errmsg,"start_script %s failed\n", ap->js.start_script);
			return 1;
		}
		ap->js.start_run = true;
	}
	return 0;
}

#ifdef JS
static int set_script_dir(void *ctx, config_property_t *p) {
	solard_agent_t *ap = ctx;

	/* Set script_dir if empty */
	dprintf(dlevel,"script_dir(%d): %s\n", strlen(ap->js.script_dir), ap->js.script_dir);
	if (!strlen(ap->js.script_dir)) {
		strncpy(ap->js.script_dir,ap->agent_libdir,sizeof(ap->js.script_dir)-1);
		fixpath(ap->js.script_dir,sizeof(ap->js.script_dir));
		dprintf(dlevel,"NEW script_dir(%d): %s\n", strlen(ap->js.script_dir), ap->js.script_dir);
	}
	return 0;
}
#endif

config_property_t *agent_get_props(solard_agent_t *ap) {
	config_property_t agent_props[] = {
		/* name, type, dest, dsize, def, flags, scope, values, labels, units, scale, precision */
		{ "driver_name", DATA_TYPE_STRING, ap->name, sizeof(ap->name)-1, 0, CONFIG_FLAG_PRIVATE },
		{ "name", DATA_TYPE_STRING, ap->instance_name, sizeof(ap->instance_name)-1, 0 },
		{ "configfile", DATA_TYPE_STRING, ap->configfile, sizeof(ap->configfile)-1, 0, CONFIG_FLAG_NOSAVE, 0, 0, 0, 0, 0, 0, agent_configfile_set, ap },
		{ "interval", DATA_TYPE_INT, &ap->interval, 0, "30", 0, "range", "0, 99999, 1", 0, "S", 1, 0 },
		{ "debug_mem", DATA_TYPE_BOOL, &ap->debug_mem, 0, "no", CONFIG_FLAG_PRIVATE },
		{ "agent_libdir", DATA_TYPE_STRING, ap->agent_libdir, sizeof(ap->agent_libdir)-1, 0 },
#ifdef JS
		{ "rtsize", DATA_TYPE_INT, &ap->js.rtsize, 0, 0, CONFIG_FLAG_READONLY },
		{ "stacksize", DATA_TYPE_INT, &ap->js.stksize, 0, 0, CONFIG_FLAG_READONLY },
		{ "run_scripts", DATA_TYPE_BOOL, &ap->js.run_scripts, 0, 0, CONFIG_FLAG_NOSAVE, 0, 0, 0, 0, 0, 0, agent_run_set, ap },
		{ "script_dir", DATA_TYPE_STRING, &ap->js.script_dir, sizeof(ap->js.script_dir)-1, 0, 0, 0, 0, 0, 0, 0, 0, set_script_dir, ap },
		{ "init_script", DATA_TYPE_STRING, ap->js.init_script, sizeof(ap->js.init_script)-1, "init.js", 0 },
		{ "start_script", DATA_TYPE_STRING, ap->js.start_script, sizeof(ap->js.start_script)-1, "start.js", 0 },
		{ "read_script", DATA_TYPE_STRING, ap->js.read_script, sizeof(ap->js.read_script)-1, "read.js", 0 },
		{ "write_script", DATA_TYPE_STRING, ap->js.write_script, sizeof(ap->js.write_script)-1, "write.js", 0 },
		{ "run_script", DATA_TYPE_STRING, ap->js.run_script, sizeof(ap->js.run_script)-1, "run.js", 0 },
		{ "stop_script", DATA_TYPE_STRING, ap->js.stop_script, sizeof(ap->js.stop_script)-1, "stop.js", 0 },
		{ "gc_interval", DATA_TYPE_INT, &ap->js.gc_interval, 0, "60", 0 },
#endif
		{0}
	};
	return config_combine_props(agent_props,0);
}

static int agent_startup(solard_agent_t *ap, char *mqtt_info, char *influx_info,
		config_property_t *driver_props, config_function_t *driver_funcs) {
	config_property_t *agent_props,*props;
	config_function_t *funcs;
	config_function_t agent_funcs[] = {
		{ "ping", cf_agent_ping, ap, 0 },
		{ "exit", cf_agent_exit, ap, 0 },
		{ "get_info", cf_agent_getinfo, ap, 0 },
		{ "get_config", cf_agent_getconfig, ap, 0 },
		{ "get", agent_config_get_value, ap, 1 },
		{ "set", agent_service_set, ap, 2 },
		{ "clear", agent_service_clear, ap, 1 },
		{ "log_open", cf_agent_log_open, ap, 1 },
#ifdef JS
//		{ "exec", cf_agent_exec, ap, 1 },
#endif
		{ 0 }
	};
	void *eptr;
#ifdef MQTT
	void *mptr;
	char old_name[sizeof(ap->instance_name)+1];
	char lwt[SOLARD_TOPIC_SIZE];
#endif
#ifdef INFLUX
	void *iptr;
#endif
#ifdef JS
	void *jptr;
#endif

	agent_props = agent_get_props(ap);
	dprintf(dlevel,"agent_props: %p\n", agent_props);
	if (!agent_props) return 1;
//	dprintf(dlevel,"driver_props: %p\n", driver_props);
	props = config_combine_props(driver_props,agent_props);
	dprintf(dlevel,"props: %p\n", props);
	funcs = config_combine_funcs(driver_funcs,agent_funcs);
	dprintf(dlevel,"funcs: %p\n", funcs);

	eptr = (ap->flags & AGENT_FLAG_NOEVENT ? 0 : &ap->e);
#ifdef MQTT
	/* Create LWT topic */
	mptr = (ap->flags & AGENT_FLAG_NOMQTT ? 0 : &ap->m);
	*lwt = *old_name = 0;
	agent_mktopic(lwt,sizeof(lwt)-1,ap->instance_name,"Status");
	strcpy(old_name,ap->instance_name);
#endif
#ifdef INFLUX
	iptr = (ap->flags & AGENT_FLAG_NOINFLUX ? 0 : &ap->i);
#endif
#ifdef JS
	jptr = (ap->flags & AGENT_FLAG_NOJS ? 0 : &ap->js.e);
#endif

        /* Call common startup */
	if (solard_common_startup(&ap->cp, ap->section_name, ap->configfile, props, funcs, eptr
#ifdef MQTT
		,mptr, lwt, agent_getmsg, ap, mqtt_info, ap->config_from_mqtt
#endif
#ifdef INFLUX
		,iptr, influx_info
#endif
#ifdef JS
		,jptr, ap->js.rtsize, ap->js.stksize, (js_outputfunc_t *)log_info
#endif
	)) return 1;
//	if (props) free(props);
//	if (funcs) free(funcs);

#if 0
#ifdef JS
	dprintf(dlevel,"nojs: %d\n", check_bit(ap->flags, AGENT_FLAG_NOJS));
	if (!check_bit(ap->flags, AGENT_FLAG_NOJS)) {
		/* Add JS init funcs (define classes) */
		JS_EngineAddInitClass(ap->js.e, "js_InitAgentClass", js_InitAgentClass);
		JS_EngineAddInitClass(ap->js.e, "js_InitConfigClass", js_InitConfigClass);
		JS_EngineAddInitClass(ap->js.e, "js_InitMQTTClass", js_InitMQTTClass);
		JS_EngineAddInitClass(ap->js.e, "js_InitInfluxClass", js_InitInfluxClass);
	} else {
		/* See if the driver has an engine ptr */
//		if (ap->driver && ap->driver->config) ap->driver->config(ap->handle, AGENT_CONFIG_GETJS, &ap->js.e);
	}
#endif
#endif

#ifdef MQTT
	/* If name changed (from config), re-register new LWT */
	dprintf(dlevel,"name: %s, instance_name: %s\n", old_name, ap->instance_name);
	if (strcmp(ap->instance_name,old_name) != 0 && ap->m->enabled) {
		char new_topic[SOLARD_TOPIC_SIZE];

		agent_mktopic(new_topic,sizeof(new_topic)-1,ap->instance_name,"Status");
		mqtt_set_lwt(ap->m, new_topic);
	}
#endif

	/* Agent libdir */
	sprintf(ap->agent_libdir,"%s/agents/%s",SOLARD_LIBDIR,ap->name);

	set_script_dir(ap,0);

	config_add_props(ap->cp, "agent", agent_props, CONFIG_FLAG_NODEF | CONFIG_FLAG_PRIVATE);

	free(agent_props);
	return 0;
}

void agent_destroy(solard_agent_t *ap) {
	if (!ap) return;
	if (ap->info) json_destroy_value(ap->info);
	if (ap->cp) config_destroy(ap->cp);
#ifdef MQTT
	if (ap->m) mqtt_destroy(ap->m);
	list_destroy(ap->mq);
#endif
#ifdef INFLUX
	if (ap->i) influx_destroy(ap->i);
#endif
	solard_common_shutdown();
#ifdef JS
	if (ap->js.props) free(ap->js.props);
	if (ap->js.funcs) free(ap->js.funcs);
	if (ap->js.roots) {
		struct js_agent_rootinfo *ri;

		list_reset(ap->js.roots);
		while((ri = list_get_next(ap->js.roots)) != 0) {
			dprintf(dlevel,"removing root: %s\n", ri->name);
			JS_RemoveRoot(ri->cx,ri->vp);
			JS_free(ri->cx,ri->name);
		}
		list_destroy(ap->js.roots);
        }
	if (ap->js.e) JS_EngineDestroy(ap->js.e);
#endif
	if (ap->e) event_destroy(ap->e);
	free(ap);
}

solard_agent_t *agent_init(int argc, char **argv, char *agent_version, opt_proctab_t *agent_opts,
				solard_driver_t *Cdriver, void *handle, int flags,
				config_property_t *driver_props, config_function_t *driver_funcs) {
	solard_agent_t *ap;
#ifdef JS
	char jsexec[4096];
	char script[256];
	char script_dir[256];
	int rtsize,stksize,noscript,nojs;
#endif
#ifdef MQTT
	char mqtt_topic[SOLARD_TOPIC_SIZE];
	int config_from_mqtt;
#endif
	char mqtt_info[200];			/* Needs to be here for agent startup */
	char influx_info[200];			/* Needs to be here for agent startup */
	char configfile[SOLARD_PATH_MAX];
	char name[SOLARD_NAME_LEN];
	char sname[64];
	int info_flag;
//	interval;
	opt_proctab_t std_opts[] = {
		/* Spec, dest, type len, reqd, default val, have */
#ifdef MQTT
		{ "-m::|mqtt host,clientid[,user[,pass]]",&mqtt_info,DATA_TYPE_STRING,sizeof(mqtt_info)-1,0,"" },
		{ "-M|get config from mqtt",&config_from_mqtt,DATA_TYPE_LOGICAL,0,0,"N" },
		{ "-T|override mqtt topic",&mqtt_topic,DATA_TYPE_STRING,sizeof(mqtt_topic)-1,0,"" },
#endif
		{ "-c:%|config file",&configfile,DATA_TYPE_STRING,sizeof(configfile)-1,0,"" },
//		{ "-F:%|config file format",&cformat,DATA_TYPE_INT,0,0,"-1" },
		{ "-s::|config section name",&sname,DATA_TYPE_STRING,sizeof(sname)-1,0,"" },
		{ "-n::|agent name",&name,DATA_TYPE_STRING,sizeof(name)-1,0,"" },
#ifdef INFLUX
		{ "-i::|influx host[,user[,pass]]",&influx_info,DATA_TYPE_STRING,sizeof(influx_info)-1,0,"" },
#endif
#ifdef JS
		{ "-e:%|exectute javascript",&jsexec,DATA_TYPE_STRING,sizeof(jsexec)-1,0,"" },
		{ "-U:#|javascript runtime size",&rtsize,DATA_TYPE_INT,0,0,"1048576" },
		{ "-K:#|javascript stack size",&stksize,DATA_TYPE_INT,0,0,"65536" },
		{ "-D::|set JS script dir",script_dir,DATA_TYPE_STRING,sizeof(script_dir)-1,0,"" },
		{ "-X::|execute JS script and exit",&script,DATA_TYPE_STRING,sizeof(script)-1,0,"" },
		{ "-N|dont exec any scripts",&noscript,DATA_TYPE_BOOL,0,0,"no" },
		{ "-Z|do not init js engine",&nojs,DATA_TYPE_BOOL,0,0,"no" },
#endif
		{ "-I|display agent info and exit",&info_flag,DATA_TYPE_LOGICAL,0,0,"0" },
//		{ "-r:#|read/write interval",&interval,DATA_TYPE_INT,0,0,"-1" },
		OPTS_END
	}, *opts;
	int logopts = LOG_INFO|LOG_WARNING|LOG_ERROR|LOG_SYSERR|_ST_DEBUG;

	opts = opt_combine_opts(std_opts,agent_opts);
	dprintf(dlevel,"opts: %p\n", opts);
	if (!opts) {
		printf("error initializing options processing\n");
		return 0;
	}
	*configfile = *name = *sname = 0;
#ifdef MQTT
	*mqtt_info = 0;
	config_from_mqtt = 0;
#endif
#ifdef INFLUX
	*influx_info = 0;
#endif
#ifdef JS
	*jsexec = *script = *script_dir = 0;
#endif
#if DEBUG_STARTUP
	printf("argc: %d, argv: %p\n", argc, argv);
#endif
	if (solard_common_init(argc,argv,agent_version,opts,logopts)) {
		dprintf(dlevel,"common init failed!\n");
		return 0;
	}
	free(opts);
	dprintf(dlevel,"configfile: %s\n", configfile);

	ap = calloc(sizeof(*ap),1);
	dprintf(dlevel,"ap: %p\n", ap);
	if (!ap) {
		log_write(LOG_SYSERR,"calloc agent config");
		return 0;
	}
	ap->driver = Cdriver;
	ap->handle = handle;
	ap->flags = flags;
	if (nojs) flags |= AGENT_FLAG_NOJS;
#ifdef MQTT
	ap->mq = list_create();
	ap->config_from_mqtt = config_from_mqtt;
	strcpy(ap->mqtt_topic,mqtt_topic);
#endif
#ifdef JS
	ap->js.rtsize = rtsize;
	ap->js.stksize = stksize;
	ap->js.run_scripts = (noscript == 0);
	ap->js.roots = list_create();
#endif
//	ap->interval = interval;

	/* Agent name is driver name */
	strncpy(ap->name,(Cdriver && Cdriver->name ? Cdriver->name : "agent"),sizeof(ap->name)-1);

	/* Instance name: if name specified on command line use that, else use driver name */
	if (strlen(name)) strcpy(ap->instance_name,name);
	else strncpy(ap->instance_name,ap->name,sizeof(ap->instance_name)-1);
	dprintf(dlevel,"instance_name: %s\n", ap->instance_name);

	/* (config)Section name: if sname specified on command line use that, else use driver name */
	if (strlen(sname)) strncpy(ap->section_name,sname,sizeof(ap->section_name)-1);
	else strncpy(ap->section_name,ap->name,sizeof(ap->section_name)-1);
	dprintf(dlevel,"section_name: %s\n", ap->section_name);

#if 0
	/* Auto generate configfile if not specified */
	if (!config_from_mqtt && !strlen(configfile)) {
		char *names[4];
		char *types[] = { "json", "conf", 0 };
		char path[256];
		int c,i,j,f;

		f = c = 0;
		if (strlen(name)) names[c++] = name;
		if (strlen(sname)) names[c++] = sname;
		names[c++] = ap->name;
		for(i=0; i < c; i++) {
			for(j=0; types[j]; j++) {
				sprintf(path,"%s/%s.%s",SOLARD_ETCDIR,names[i],types[j]);
				dprintf(dlevel,"trying: %s\n", path);
				if (access(path,0) == 0) {
					f = 1;
					break;
				}
			}
			if (f) break;
		}
		if (f) strcpy(configfile,path);
	}
#endif
	strcpy(ap->configfile,configfile);

	/* Call common startup */
	if (agent_startup(ap, mqtt_info, influx_info, driver_props, driver_funcs)) goto agent_init_error;

	/* If name was specified in config file, make sure it's "dirty" in config */
	if (strlen(name) && ap->cp) config_set_property(ap->cp,ap->section_name,"name",DATA_TYPE_STRING,name,strlen(name));

	/* Call the agent's config init func */
	if (ap->driver && ap->driver->config && ap->driver->config(ap->handle, SOLARD_CONFIG_INIT, ap)) goto agent_init_error;

#ifdef MQTT
	/* XXX req'd for sdconfig */
	if (ap->m) agent_sub(ap, ap->instance_name, 0);
#endif

	/* interval from commanline overrides config */
//	dprintf(dlevel,"interval: %d\n", interval);
//	if (interval > 0) ap->interval = interval;

#ifdef JS
	dprintf(dlevel,"e: %p\n", ap->js.e);
	if (ap->js.e) {
		/* script_dir from commanline overrides config */
		if (strlen(script_dir)) strcpy(ap->js.script_dir,script_dir);

		/* Execute any command-line javascript code */
		dprintf(dlevel,"jsexec(%d): %s\n", strlen(jsexec), jsexec);
		if (strlen(jsexec)) {
			if (JS_EngineExecString(ap->js.e, jsexec)) {
				log_error("unable to exec js expression\n");
				goto agent_init_error;
			}
		}

		/* Start the init script */
		dprintf(dlevel,"init_script: %s\n", ap->js.init_script);
		if (agent_script_exists(ap,ap->js.init_script) && agent_start_script(ap,ap->js.init_script)) {
			if (!ap->js.ignore_js_errors) {
				log_error("agent_init: init_script failed\n");
				goto agent_init_error;
			}
		}

		/* if specified, Execute javascript file then exit */
		dprintf(dlevel,"script: %s\n", script);
		if (strlen(script)) {
			if (agent_start_script(ap,script)) log_error("agent_init: unable to execute script: %s\n", script);
			goto agent_init_error;
		}
	}
#endif

	/*  Get and publish driver info */
	if (agent_get_info(ap,info_flag,true)) goto agent_init_error;

#ifdef MQTT
	/* Publish our config */
	if (ap->cp && agent_pubconfig(ap)) goto agent_init_error;
#endif

	dprintf(dlevel,"returning: %p\n",ap);
	return ap;

agent_init_error:
	agent_destroy(ap);
	dprintf(dlevel,"returning 0\n");
	return 0;
}

int agent_run(solard_agent_t *ap) {
#ifdef MQTT
	solard_message_t *msg;
#endif
	int read_status,write_status;
	time_t last_read,cur,diff;
	int used,last_used,peak,last_peak;

	dprintf(dlevel,"ap: %p\n", ap);
#ifdef JS
	if (agent_script_exists(ap,ap->js.start_script)) agent_start_script(ap,ap->js.start_script);
#endif

	last_read = 0;
#ifdef JS
	/* Do a GC before we start */
	JS_EngineCleanup(ap->js.e);
#endif
	peak = last_peak = last_used = 0;
	set_state(ap,SOLARD_AGENT_STATE_RUNNING);
	ap->run_count = 0;
	agent_event(ap,"Agent","Start");
	while(check_state(ap,SOLARD_AGENT_STATE_RUNNING)) {
		/* Call read func */
		time(&cur);
		diff = cur - last_read;
		/* only write if read is successful */
		dprintf(dlevel,"diff: %d, interval: %d\n", (int)diff, ap->interval);
		read_status = 0;
		if (diff >= ap->interval) {
			if (ap->driver) {
				dprintf(dlevel,"open_before_read: %d, open: %p\n", AGENT_HASFLAG(OBREAD), ap->driver->open);
				if (AGENT_HASFLAG(OBREAD) && ap->driver->open) {
					if (ap->driver->open(ap->handle)) {
						log_error("agent_run: open for read failed\n");
						/* make sure we wait the correct amount */
						time(&last_read);
						continue;
					}
				}
				dprintf(dlevel,"read: %p\n", ap->driver->read);
				if (ap->driver->read) read_status = ap->driver->read(ap->handle,0,0,0);
				dprintf(dlevel,"driver read_status: %d\n", read_status);
			}
#ifdef JS
			/* Only call script if driver didnt error */
			if (read_status == 0) {
				dprintf(dlevel,"read_script: %s\n", ap->js.read_script);
				if (agent_script_exists(ap,ap->js.read_script)) {
					read_status = agent_start_script(ap,ap->js.read_script);
					dprintf(dlevel,"script read_status: %d\n", read_status);
					if (ap->js.ignore_js_errors) read_status = 0;
				}
			}
#endif
			if (ap->driver) {
				dprintf(dlevel,"close_after_read: %d, close: %p\n", AGENT_HASFLAG(CAREAD), ap->driver->close);
				if (AGENT_HASFLAG(CAREAD) && ap->driver->close) ap->driver->close(ap->handle);
			}
			time(&last_read);
		}

#ifdef MQTT
		/* Process messages */
		dprintf(dlevel+1,"mq count: %d\n", list_count(ap->mq));
		list_reset(ap->mq);
		while((msg = list_get_next(ap->mq)) != 0) {
//			solard_message_dump(msg,0);
			agent_process_message(ap,msg);
			list_delete(ap->mq,msg);
		}
#endif

		/* Call cb */
		dprintf(dlevel+1,"func: %p\n", ap->callback.func);
		if (ap->callback.func) ap->callback.func(ap->callback.ctx);

#ifdef JS
		/* Call run script */
		if (agent_script_exists(ap,ap->js.run_script)) agent_start_script(ap,ap->js.run_script);
#endif

		/* Call write func */
		if (read_status == 0 && diff >= ap->interval) {
			if (ap->driver) {
				dprintf(dlevel,"flags: 0x%04x\n", ap->flags);
				dprintf(dlevel,"open_before_write: %d, open: %p\n",
					AGENT_HASFLAG(OBWRITE), ap->driver->open);
				if (AGENT_HASFLAG(OBWRITE) && ap->driver->open) {
					if (ap->driver->open(ap->handle)) {
						log_error("agent_run: open for write failed\n");
						continue;
					}
				}
				write_status = 0;
				dprintf(dlevel,"write: %p\n", ap->driver->write);
				if (ap->driver->write) write_status = ap->driver->write(ap->handle,0,0,0);
				dprintf(dlevel,"driver write_status: %d\n", write_status);
			}
#ifdef JS
			/* Only call script if driver didnt error */
			if (write_status == 0) {
				dprintf(dlevel,"write_script: %s\n", ap->js.write_script);
				if (agent_script_exists(ap,ap->js.write_script)) {
					write_status = agent_start_script(ap,ap->js.write_script);
					dprintf(dlevel,"script write_status: %d\n", write_status);
					if (ap->js.ignore_js_errors) write_status = 0;
				}
			}
#endif
			if (ap->driver) {
				dprintf(dlevel,"close_after_write: %d, close: %p\n",
					AGENT_HASFLAG(CAWRITE), ap->driver->close);
				if (AGENT_HASFLAG(CAWRITE) && ap->driver->close) ap->driver->close(ap->handle);
			}
			dprintf(dlevel,"write_status: %d\n", write_status);
			if (write_status) dprintf(dlevel,"write failed!\n");

			used = mem_used();
			if (used > peak) peak = used;
//			if (ap->debug_mem && peak != last_peak) {
			dprintf(dlevel,"debug_mem: %d, used: %d, last_used: %d\n", ap->debug_mem, used, last_used);
			if (ap->debug_mem && used != last_used) {
				int udiff,pdiff;

				udiff = used - last_used;
				pdiff = peak - last_peak;
				log_info("used: %d (%s%d), peak: %d (%s%d)\n", used, (udiff > 0 ? "+" : ""), udiff, peak, (pdiff > 0 ? "+" : ""), pdiff);
			}
			last_used = used;
			last_peak = peak;
		}
#ifdef JS
		/* At gc_interval, do a cleanup */
		dprintf(dlevel,"run_count: %d, gc_interval: %d\n", ap->run_count, ap->js.gc_interval);
		if (ap->js.e && ap->js.gc_interval > 0 && ((ap->run_count % ap->js.gc_interval) == 0))
			JS_EngineCleanup(ap->js.e);
#endif
		ap->run_count++;
		sleep(1);
//		break;
	}
	agent_event(ap,"Agent","Stop");
#ifdef JS
	if (agent_script_exists(ap,ap->js.stop_script)) agent_start_script(ap,ap->js.stop_script);
#endif
	return 0;
}

#ifdef JS
enum AGENT_PROPERTY_ID {
	AGENT_PROPERTY_ID_CONFIG=2048,
	AGENT_PROPERTY_ID_MQTT,
	AGENT_PROPERTY_ID_INFLUX,
	AGENT_PROPERTY_ID_MSG,
	AGENT_PROPERTY_ID_INFO,
	AGENT_PROPERTY_ID_ADDMQ,
};

static JSBool js_agent_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	solard_agent_t *ap;
	config_property_t *p;

	ap = JS_GetPrivate(cx,obj);
	dprintf(dlevel,"ap: %p\n", ap);
	if (!ap) {
		JS_ReportError(cx, "agent_getprop: private is null!");
		return JS_FALSE;
	}
	p = 0;
	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case AGENT_PROPERTY_ID_INFO:
		    {
			char *j;
			JSString *str;
			jsval rootVal;
			JSONParser *jp;
			jsval reviver = JSVAL_NULL;
			JSBool ok;

			/* Convert from C JSON type to JS JSON type */
			dprintf(dlevel,"ap->info: %p\n", ap->info);
			j = json_dumps(ap->info,0);
			if (!j) j = strdup("{}");
			dprintf(dlevel,"j: %s\n", j);
			jp = js_BeginJSONParse(cx, &rootVal);
			dprintf(dlevel,"jp: %p\n", jp);
			if (!jp) {
				JS_ReportError(cx, "unable init JSON parser\n");
				free(j);
				return JS_FALSE;
			}
			str = JS_NewStringCopyZ(cx,j);
        		ok = js_ConsumeJSONText(cx, jp, JS_GetStringChars(str), JS_GetStringLength(str));
			dprintf(dlevel,"ok: %d\n", ok);
			ok = js_FinishJSONParse(cx, jp, reviver);
			dprintf(dlevel,"ok: %d\n", ok);
			free(j);
			*rval = rootVal;
		    }
		    break;
		case AGENT_PROPERTY_ID_CONFIG:
			*rval = ap->js.config_val;
			break;
		case AGENT_PROPERTY_ID_MQTT:
			*rval = ap->js.mqtt_val;
			break;
		case AGENT_PROPERTY_ID_INFLUX:
			*rval = ap->js.influx_val;
			break;
		case AGENT_PROPERTY_ID_MSG:
			*rval = OBJECT_TO_JSVAL(js_create_messages_array(cx,obj,ap->mq));
			break;
		case AGENT_PROPERTY_ID_ADDMQ:
			*rval = type_to_jsval(cx,DATA_TYPE_BOOL,&ap->addmq,0);
			break;
		default:
			p = CONFIG_GETMAP(ap->cp,prop_id);
			dprintf(dlevel,"p: %p\n", p);
			if (!p) p = config_get_propbyid(ap->cp,prop_id);
			dprintf(dlevel,"p: %p\n", p);
			if (!p) {
				JS_ReportError(cx, "js_agent_getprop: internal error: property %d not found", prop_id);
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
		if (sname && name) p = config_get_property(ap->cp, sname, name);
		if (name) JS_free(cx, name);
	}
	dprintf(dlevel,"p: %p\n", p);
	if (p && p->dest) {
		dprintf(dlevel,"p: type: %d(%s), name: %s\n", p->type, typestr(p->type), p->name);
		*rval = type_to_jsval(cx,p->type,p->dest,p->len);
	}
	return JS_TRUE;
}

static JSBool js_agent_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	solard_agent_t *ap;
	int prop_id;

	ap = JS_GetPrivate(cx,obj);
	dprintf(dlevel,"ap: %p\n", ap);
	if (!ap) {
		JS_ReportError(cx, "agent_getprop: private is null!");
		return JS_FALSE;
	}
	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case AGENT_PROPERTY_ID_ADDMQ:
			jsval_to_type(DATA_TYPE_BOOL,&ap->addmq,0,cx,*vp);
			break;
		default:
			return js_config_common_setprop(cx, obj, id, vp, ap->cp, ap->js.props);
		}
	} else {
		return js_config_common_setprop(cx, obj, id, vp, ap->cp, ap->js.props);
	}
	return JS_TRUE;
}

static JSClass agent_class = {
	"Agent",		/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	js_agent_getprop,	/* getProperty */
	js_agent_setprop,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

#ifdef MQTT
static JSBool js_agent_run(JSContext *cx, uintN argc, jsval *vp) {
	solard_agent_t *ap;
	JSObject *obj;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	ap = JS_GetPrivate(cx, obj);

	agent_run(ap);
	return JS_TRUE;
}

static JSBool js_agent_mktopic(JSContext *cx, uintN argc, jsval *vp) {
	solard_agent_t *ap;
	char topic[SOLARD_TOPIC_SIZE], *func;
	JSObject *obj;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	ap = JS_GetPrivate(cx, obj);

	if (argc != 1) {
		JS_ReportError(cx,"mktopic requires 1 arguments (func:string)\n");
		return JS_FALSE;
	}

        func = 0;
        if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "s", &func)) return JS_FALSE;
	dprintf(dlevel,"func: %s\n", func);

	agent_mktopic(topic,sizeof(topic)-1,ap->instance_name,func);
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,topic));
	return JS_TRUE;
}

static JSBool js_agent_purgemq(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	solard_agent_t *ap;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	ap = JS_GetPrivate(cx, obj);
	if (!ap) {
		JS_ReportError(cx,"agent private is null!\n");
		return JS_FALSE;
	}
	list_purge(ap->mq);
	return JS_TRUE;
}
#endif

static JSBool js_agent_pubinfo(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	solard_agent_t *ap;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	ap = JS_GetPrivate(cx, obj);
	if (!ap) {
		JS_ReportError(cx,"agent private is null!\n");
		return JS_FALSE;
	}
	agent_get_info(ap,false,true);
	return JS_TRUE;
}

static JSBool js_agent_pubconfig(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	solard_agent_t *ap;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	ap = JS_GetPrivate(cx, obj);
	if (!ap) {
		JS_ReportError(cx,"agent private is null!\n");
		return JS_FALSE;
	}
	agent_pubconfig(ap);
	return JS_TRUE;
}

static JSBool js_agent_event(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	solard_agent_t *ap;
	char *module,*action;

	ap = JS_GetPrivate(cx, obj);
	if (!ap) {
		JS_ReportError(cx,"agent private is null!\n");
		return JS_FALSE;
	}
	module = action = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "s s", &module, &action)) return JS_FALSE;
	agent_event(ap,module,action);
	if (module) JS_free(cx,module);
	if (action) JS_free(cx,action);
	return JS_TRUE;
}

struct js_agent_event_info {
	JSContext *cx;
	jsval func;
	char *name;
};

static void _js_agent_event_handler(void *ctx, char *name, char *module, char *action) {
	struct js_agent_event_info *info = ctx;
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

static JSBool js_agent_event_handler(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	solard_agent_t *ap;
	char *fname, *name, *module,*action;
	JSFunction *fun;
	jsval func;
	struct js_agent_event_info *ctx;
        struct js_agent_rootinfo ri;

	ap = JS_GetPrivate(cx, obj);
	if (!ap) {
		JS_ReportError(cx,"agent private is null!\n");
		return JS_FALSE;
	}
	func = 0;
	name = module = action = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "v / s s s", &func, &name, &module, &action)) return JS_FALSE;
	dprintf(dlevel,"VALUE_IS_FUNCTION: %d\n", VALUE_IS_FUNCTION(cx, func));
	if (!VALUE_IS_FUNCTION(cx, func)) {
                JS_ReportError(cx, "agent_event_handler: arguments: required: function(function), optional: name(string), module(string), action(string)");
                return JS_FALSE;
        }
	fun = JS_ValueToFunction(cx, argv[0]);
	dprintf(dlevel,"fun: %p\n", fun);
	if (!fun) {
		JS_ReportError(cx, "js_agent_event_handler: internal error: funcptr is null!");
		return JS_FALSE;
	}
	fname = JS_EncodeString(cx, ATOM_TO_STRING(fun->atom));
	dprintf(dlevel,"fname: %s\n", fname);
	if (!fname) {
		JS_ReportError(cx, "js_agent_event_handler: internal error: fname is null!");
		return JS_FALSE;
	}
	ctx = malloc(sizeof(*ctx));
	if (!ctx) {
		JS_ReportError(cx, "js_agent_event_handler: internal error: malloc ctx");
		return JS_FALSE;
	}
	memset(ctx,0,sizeof(*ctx));
	ctx->cx = cx;
	ctx->func = func;
	ctx->name = fname;
	agent_event_handler(ap,_js_agent_event_handler,ctx,name,module,action);

	/* root it */
        ri.name = fname;
        JS_AddNamedRoot(cx,&ctx->func,fname);
        ri.cx = cx;
        ri.vp = &ctx->func;
        list_add(ap->js.roots,&ri,sizeof(ri));

//	if (name) JS_free(cx,name);
//	if (module) JS_free(cx,action);
//	if (action) JS_free(cx,action);
	return JS_TRUE;
}

static JSBool js_agent_callfunc(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	solard_agent_t *ap;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) {
		JS_ReportError(cx, "js_agent_callfunc: internal error: object is null!\n");
		return JS_FALSE;
	}
	ap = JS_GetPrivate(cx, obj);
	if (!ap) {
		JS_ReportError(cx, "js_agent_callfunc: internal error: private is null!\n");
		return JS_FALSE;
	}
	dprintf(dlevel,"ap: %p\n", ap);
	if (!ap) return JS_TRUE;
	dprintf(dlevel,"ap->cp: %p\n", ap->cp);
	if (!ap->cp) return JS_TRUE;

	dprintf(dlevel,"argc: %d\n", argc);
	return js_config_callfunc(ap->cp, cx, argc, vp);
}

static JSBool js_agent_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	solard_agent_t *ap;
	char **args, *name, *vers;
        unsigned int count;
	JSObject *aobj, *dobj, *pobj,*fobj;
	int i,sz;
	jsval val;
	JSString *jstr;
	solard_driver_t *driver;
	void *driver_handle;
	config_property_t *props;
	config_function_t *funcs;

	dprintf(0,"cx: %p, obj: %p, argc: %d\n", cx, obj, argc);

	name = vers = 0;
	aobj = dobj = pobj = fobj = 0;
        if (!JS_ConvertArguments(cx, argc, argv, "o o s / o o", &aobj, &dobj, &vers, &pobj, &fobj)) return JS_FALSE;
        dprintf(dlevel,"aobj: %p, dobj: %p, vers: %s, pobj: %p, fobj: %p\n", aobj, dobj, vers, pobj, fobj);

	/* get array count and alloc props */
	if (!js_GetLengthProperty(cx, aobj, &count)) {
		JS_ReportError(cx,"unable to get argv array length");
		return JS_FALSE;
	}
	dprintf(dlevel,"count: %d\n", count);
	if (count) {
		/* prepend program name (agent name) */
		count++;
		sz = sizeof(char *) * count;
		dprintf(dlevel,"sz: %d\n", sz);
		args = JS_malloc(cx,sz);
		if (!args) {
			JS_ReportError(cx,"unable to malloc args");
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

	name = JS_GetObjectName(cx,dobj);
	if (!name) {
		JS_ReportError(cx, "unable to get object name");
		return JS_FALSE;
	}
	dprintf(dlevel,"name: %s\n", name);

	/* Get the driver info from the driver object */
	driver = js_driver_get_driver(name);
	driver_handle = JS_GetPrivate(cx,dobj);

	/* Convert props array to config_property_t array */
	if (pobj) {
		props = js_config_obj2props(cx, pobj, 0);
		dprintf(dlevel,"props: %p\n", props);
		if (!props) return JS_FALSE;
	} else {
		props = 0;
	}

	/* Convert funcs array to config_function_t array */
	if (0 && fobj) {
		funcs = js_config_obj2funcs(cx, fobj);
		dprintf(dlevel,"funcs: %p\n", funcs);
		if (!funcs) return JS_FALSE;
	} else {
		funcs = 0;
	}

	/* init */
	ap = agent_init(count,args,vers,0,driver,driver_handle,AGENT_FLAG_NOJS,props,funcs);
	if (args) {
		for(i=0; i < count; i++) JS_free(cx, args[i]);
		JS_free(cx,args);
	}
	JS_free(cx,name);
	dprintf(dlevel,"ap: %p\n", ap);
	if (!ap) {
		JS_ReportError(cx, "agent_init returned null");
		return JS_FALSE;
	}

	/* Driver's init func must create agent object */
	dprintf(0,"agent_val: %x\n", ap->js.agent_val);
	*rval = ap->js.agent_val;
	return JS_TRUE;
}

JSObject *js_InitAgentClass(JSContext *cx, JSObject *global_object) {
	JSObject *obj;
	static JSPropertySpec agent_props[] = {
		{ "config", AGENT_PROPERTY_ID_CONFIG, JSPROP_ENUMERATE },
#ifdef MQTT
		{ "mqtt", AGENT_PROPERTY_ID_MQTT, JSPROP_ENUMERATE },
		{ "messages", AGENT_PROPERTY_ID_MSG, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "addmq", AGENT_PROPERTY_ID_ADDMQ, JSPROP_ENUMERATE },
#endif
#ifdef INFLUX
		{ "influx", AGENT_PROPERTY_ID_INFLUX, JSPROP_ENUMERATE },
#endif
		{ "info", AGENT_PROPERTY_ID_INFO, JSPROP_ENUMERATE },
		{ 0 }
	};

	static JSFunctionSpec agent_funcs[] = {
		JS_FN("run",js_agent_run,0,0,0),
#ifdef MQTT
		JS_FN("mktopic",js_agent_mktopic,1,1,0),
		JS_FN("purgemq",js_agent_purgemq,0,0,0),
#endif
		JS_FN("pubinfo",js_agent_pubinfo,0,0,0),
		JS_FN("pubconfig",js_agent_pubconfig,0,0,0),
		JS_FS("event",js_agent_event,2,2,0),
		JS_FS("event_handler",js_agent_event_handler,1,4,0),
		JS_FN("ftest",js_agent_callfunc,1,1,0),
		{ 0 }
	};

	dprintf(dlevel,"creating %s class\n",agent_class.name);
	obj = JS_InitClass(cx, global_object, 0, &agent_class, js_agent_ctor, 1, agent_props, agent_funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s class", agent_class.name);
		return 0;
	}
	dprintf(dlevel,"obj: %p\n", obj);
	return obj;
}

JSObject *js_agent_new(JSContext *cx, JSObject *parent, void *priv) {
	solard_agent_t *ap = priv;
	JSObject *newobj;

//	dprintf(dlevel,"parent name: %s\n", JS_GetObjectName(cx,parent));

	/* Create the new object */
	newobj = JS_NewObject(cx, &agent_class, 0, parent);
	if (!newobj) return 0;

	/* Add config props/funcs */
	dprintf(dlevel,"ap->cp: %p\n", ap->cp);
	if (ap->cp) {
		/* Create agent config props */
		JSPropertySpec *props = js_config_to_props(ap->cp, cx, "agent", 0);
		if (!props) {
			log_error("js_agent_new: unable to create config props: %s\n", config_get_errmsg(ap->cp));
			return 0;
		}

		dprintf(dlevel,"Defining agent config props\n");
		if (!JS_DefineProperties(cx, newobj, props)) {
			JS_ReportError(cx,"unable to define props");
			return 0;
		}
		free(props);

		/* Create agent config funcs */
		JSFunctionSpec *funcs = js_config_to_funcs(ap->cp, cx, js_agent_callfunc, 0);
		if (!funcs) {
			log_error("unable to create funcs: %s\n", config_get_errmsg(ap->cp));
			return 0;
		}
		dprintf(dlevel,"Defining agent config funcs\n");
		if (!JS_DefineFunctions(cx, newobj, funcs)) {
			JS_ReportError(cx,"unable to define funcs");
			return 0;
		}
		free(funcs);

		/* Create the config object */
		dprintf(dlevel,"Creating config_val...\n");
		ap->js.config_val = OBJECT_TO_JSVAL(js_config_new(cx,newobj,ap->cp));
		JS_DefineProperty(cx, newobj, "config", ap->js.config_val, 0, 0, JSPROP_ENUMERATE);
	}

	dprintf(dlevel,"Setting private to: %p\n", ap);
	JS_SetPrivate(cx,newobj,ap);

	/* Only used by driver atm */
	dprintf(dlevel,"Creating agent_val...\n");
	ap->js.agent_val = OBJECT_TO_JSVAL(newobj);
	dprintf(dlevel,"agent_val: %x\n", ap->js.agent_val);

#ifdef MQTT
	if (ap->m) {
		/* Create MQTT child object */
		dprintf(dlevel,"Creating mqtt_val...\n");
		if (ap->m) ap->js.mqtt_val = OBJECT_TO_JSVAL(js_mqtt_new(cx,newobj,ap->m));
		JS_DefineProperty(cx, newobj, "mqtt", ap->js.mqtt_val, 0, 0, JSPROP_ENUMERATE);
	}
#endif
#ifdef INFLUX
	if (ap->i) {
		/* Create Influx child object */
		dprintf(dlevel,"Creating influx_val...\n");
		if (ap->i) ap->js.influx_val = OBJECT_TO_JSVAL(js_influx_new(cx,newobj,ap->i));
		JS_DefineProperty(cx, newobj, "influx", ap->js.influx_val, 0, 0, JSPROP_ENUMERATE);
	}
#endif

	return newobj;
}
#endif
