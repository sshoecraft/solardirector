

/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
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

static list agents = 0;

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

	dprintf(dlevel,"m: %p, info: %p\n", ap->m, ap->info);
	if (!ap->m) return 0;
	if (!ap->info) return 1;
	dprintf(dlevel,"enabled: %d\n",ap->m->enabled);
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

static int agent_process_message(solard_agent_t *ap, solard_message_t *msg) {
	char topic[SOLARD_TOPIC_LEN],*data,*name;
	json_object_t *status;
	int mdata;

	/* XXX only process messages sent to our top level topic */
	*topic = 0;
	agent_mktopic(topic,sizeof(topic)-1,ap->instance_name,0);
	dprintf(dlevel,"my topic: %s, msg topic: %s\n", topic, msg->topic);
	if (strcmp(msg->topic,topic) != 0) return 1;

	status = json_create_object();

	config_process_request(ap->cp, msg->data, status);

	/* if replyto is set, send the response */
	if (strlen(msg->replyto)) {
		data = json_dumps(json_object_value(status), 1);
		dprintf(dlevel,"data: %s\n", data);
		if (!data) {
			data = "{ \"status\": 1, \"message\": \"internal error\", \"results\": {} }";
			mdata = 0;
		} else {
			mdata = 1;
		}

		name = strlen(ap->instance_name) ? ap->instance_name : ap->m->clientid;
		dprintf(dlevel,"name: %s\n", name);
		if (!strlen(name)) name = "agent";
		snprintf(topic,sizeof(topic)-1,"%s/%s/%s/%s",SOLARD_TOPIC_ROOT,SOLARD_TOPIC_CLIENTS,msg->replyto,name);
		dprintf(dlevel,"topic: %s\n", topic);
		mqtt_pub(ap->m,topic,data,1,0);
		if (mdata) free(data);
	}
	json_destroy_object(status);
	return 0;
}

static void agent_getmsg(void *ctx, char *topic, char *message, int msglen, char *replyto) {
	solard_agent_t *ap = ctx;
	solard_message_t newmsg;

	if (solard_getmsg(&newmsg,topic,message,msglen,replyto)) return;
	list_add(ap->mq,&newmsg,sizeof(newmsg));
}
#else
int agent_pubconfig(solard_agent_t *ap) { return 0; }
#endif

void agent_event(solard_agent_t *ap, char *module, char *action) {
	json_object_t *o;

	dprintf(dlevel,"e: %p\n", ap->e);
	if (!ap->e) return;
	dprintf(dlevel,"calling event...\n");
	event_signal(ap->e, ap->instance_name, module, action);

	dprintf(dlevel,"creating json object...\n");
	o = json_create_object();
	json_object_add_string(o,"agent",ap->instance_name);
	json_object_add_string(o,"module",module);
	json_object_add_string(o,"action",action);

#ifdef MQTT
	dprintf(dlevel,"m: %p, connected: %d\n", ap->m, ap->m ? mqtt_connected(ap->m) : 0);
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
	dprintf(dlevel,"i: %p, connected: %d\n", ap->i, ap->i ? influx_connected(ap->i) : 0);
	if (ap->i && influx_connected(ap->i)) influx_write_json(ap->i,"events",json_object_value(o));
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
	dprintf(dlevel,"pub: %d\n", pub);
	if (pub && agent_pubinfo(ap, info_flag)) return 1;
#endif
	if (info_flag) exit(0);
	dprintf(dlevel,"done!\n");
	return 0;
}

int agent_repub(solard_agent_t *ap) {
	agent_get_info(ap, false, true);
	agent_pubconfig(ap);
//	agent_event(ap,"Agent","repub");
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
	dprintf(dlevel,"j: %p\n", j);
	if (j) {
		v = json_parse(j);
		free(j);
		dprintf(dlevel,"v: %p\n", v);
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

static int cf_agent_repub(void *ctx, list args, char *errmsg, json_object_t *results) {
	return agent_repub(ctx);
}


int cf_agent_log_open(void *ctx, list args, char *errmsg, json_object_t *results) {
	solard_agent_t *ap = ctx;
	config_arg_t *arg;
	char *filename;
	extern int logopts;

	dprintf(dlevel,"count: %d\n", list_count(args));
	arg = list_get_first(args);
	filename = arg->argv[0];
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
	config_arg_t *arg;
	char *string;
	extern int logopts;

	dprintf(dlevel,"count: %d\n", list_count(args));
	arg = list_get_first(args);
	string = arg->argv[0];
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
	config_arg_t *arg;
	char *name;
	config_property_t *p;
	json_value_t *v;

	/* handle special cases of info/config */
	dprintf(dlevel,"args count: %d\n", list_count(args));
	list_reset(args);
	while((arg = list_get_next(args)) != 0) {
		name = arg->argv[0];
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

static int agent_configfile_set(void *ctx, config_property_t *p, void *old_value) {
	solard_agent_t *ap = ctx;

	dprintf(dlevel,"configfile: %s\n", ap->configfile);
	dprintf(dlevel,"ap->cp: %p\n", ap->cp);
	if (ap->cp) config_set_filename(ap->cp,ap->configfile,CONFIG_FILE_FORMAT_AUTO);
	return 0;
}

#ifdef JS
static int agent_run_set(void *ctx, config_property_t *p, void *old_value) {
	solard_agent_t *ap = ctx;

	dprintf(dlevel,"run_scripts: %d\n", ap->js.run_scripts);
	if (!ap->js.run_scripts) return 0;
	if (!ap->js.init_run) {
		dprintf(dlevel,"init_script: %s\n", ap->js.init_script);
		if (agent_script_exists(ap,ap->js.init_script) && agent_start_script(ap,ap->js.init_script)) {
			log_error("agent_run_set: init_script failed\n");
			sprintf(ap->errmsg,"init_script %s failed\n", ap->js.init_script);
			return 1;
		}
		ap->js.init_run = true;
	}
	if (!ap->js.start_run && check_state(ap,SOLARD_AGENT_STATE_RUNNING)) {
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

static int set_script_dir(void *ctx, config_property_t *p, void *old_value) {
	solard_agent_t *ap = ctx;
	char *old_dir = (old_value ? old_value : "");

	/* Set script_dir if empty */
	dprintf(dlevel,"script_dir(%d): %s\n", strlen(ap->js.script_dir), ap->js.script_dir);
	if (!strlen(ap->js.script_dir)) {
		strncpy(ap->js.script_dir,ap->agent_libdir,sizeof(ap->js.script_dir)-1);
		fixpath(ap->js.script_dir,sizeof(ap->js.script_dir));
		dprintf(dlevel,"NEW script_dir(%d): %s\n", strlen(ap->js.script_dir), ap->js.script_dir);
	}
	dprintf(dlevel,"ap->js.e: %p\n", ap->js.e);
	if (strcmp(ap->js.script_dir,old_dir) != 0) {
		/* [re]run init+start scripts */
		ap->js.init_run = ap->js.start_run = false;
		agent_run_set(ap,p,0);
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
#ifdef DEBUG_MEM
		{ "debug_mem", DATA_TYPE_BOOL, &ap->debug_mem, 0, "no", 0 },
		{ "debug_mem_always", DATA_TYPE_BOOL, &ap->debug_mem_always, 0, "no", 0 },
#endif
		{ "agent_libdir", DATA_TYPE_STRING, ap->agent_libdir, sizeof(ap->agent_libdir)-1, 0 },
		{ "run_count", DATA_TYPE_INT, &ap->run_count, 0, 0, CONFIG_FLAG_READONLY },
		{ "read_count", DATA_TYPE_INT, &ap->read_count, 0, 0, CONFIG_FLAG_READONLY },
		{ "write_count", DATA_TYPE_INT, &ap->write_count, 0, 0, CONFIG_FLAG_READONLY },
#ifdef MQTT
		{ "purge", DATA_TYPE_BOOL, &ap->purge, 0, "true", 0 },
#endif
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
	config_property_t *agent_props;
	config_function_t agent_funcs[] = {
		{ "ping", cf_agent_ping, ap, 0 },
		{ "exit", cf_agent_exit, ap, 0 },
		{ "repub", cf_agent_repub, ap, 0 },
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
	dprintf(dlevel,"driver_props: %p\n", driver_props);
	/* XXX combine keeps the 2nd one in case of dupes!!!, driver overrides agent */
//	ap->props = config_combine_props(driver_props,agent_props);
	ap->props = config_combine_props(agent_props,driver_props);
	dprintf(dlevel,"props: %p\n", ap->props);
//	ap->funcs = config_combine_funcs(driver_funcs,agent_funcs);
	ap->funcs = config_combine_funcs(agent_funcs,driver_funcs);
	dprintf(dlevel,"funcs: %p\n", ap->funcs);

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
	if (solard_common_startup(&ap->cp, ap->section_name, ap->configfile, ap->props, ap->funcs, eptr
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

#ifdef JS
	/* See if the driver has an engine ptr */
	if (ap->driver && ap->driver->config) ap->driver->config(ap->handle, AGENT_CONFIG_GETJS, &ap->js.e);
	dprintf(dlevel, "ap->js.e: %p\n", ap->js.e);
#endif

#ifdef MQTT
	/* If name changed (from config), re-register new LWT */
	dprintf(dlevel,"name: %s, instance_name: %s\n", old_name, ap->instance_name);
	if (strcmp(ap->instance_name,old_name) != 0 && ap->m->enabled) {
		char new_topic[SOLARD_TOPIC_SIZE];

		*new_topic = 0;
		agent_mktopic(new_topic,sizeof(new_topic)-1,ap->instance_name,"Status");
		mqtt_set_lwt(ap->m, new_topic);
	}
#endif

	/* Agent libdir */
	sprintf(ap->agent_libdir,"%s/agents/%s",SOLARD_LIBDIR,ap->name);

#ifdef JS
	set_script_dir(ap,0,0);
#endif

	config_add_props(ap->cp, "agent", agent_props, CONFIG_FLAG_NODEF | CONFIG_FLAG_PRIVATE);
//	config_add_props(ap->cp, "agent", ap->props, CONFIG_FLAG_NODEF | CONFIG_FLAG_PRIVATE);

	free(agent_props);
	return 0;
}

void agent_destroy_agent(solard_agent_t *ap) {

	int ldlevel = dlevel;

	dprintf(ldlevel,"ap: %p\n", ap);
	if (!ap) return;

	if (ap->info) json_destroy_value(ap->info);
	/* XXX JS must come first because finialize is called on CTOR sessions */
#ifdef JS
	dprintf(ldlevel,"roots: %p\n", ap->js.roots);
	if (ap->js.roots) {
		struct js_agent_rootinfo *ri;

		dprintf(ldlevel,"roots count: %d\n",list_count(ap->js.roots));
		list_reset(ap->js.roots);
		while((ri = list_get_next(ap->js.roots)) != 0) {
			dprintf(ldlevel,"removing root: %s\n", ri->name);
			JS_RemoveRoot(ri->cx,ri->vp);
			JS_free(ri->cx,ri->name);
		}
		list_destroy(ap->js.roots);
	}
#endif
	dprintf(ldlevel,"cp: %p\n", ap->cp);
	if (ap->cp) config_destroy_config(ap->cp);
#ifdef MQTT
	dprintf(ldlevel,"m: %p\n", ap->m);
	if (ap->m) mqtt_destroy_session(ap->m);
	list_destroy(ap->mq);
#endif
#ifdef INFLUX
	dprintf(ldlevel,"i: %p\n", ap->i);
	if (ap->i) influx_destroy_session(ap->i);
#endif
	dprintf(ldlevel,"e: %p\n", ap->e);
	if (ap->e) event_destroy(ap->e);
	if (ap->aliases) list_destroy(ap->aliases);

#ifdef JS
	if (ap->js.e) {
		JSContext *cx = JS_EngineGetCX(ap->js.e);
		if (ap->js.props) JS_free(cx,ap->js.props);
		if (ap->js.funcs) JS_free(cx,ap->js.funcs);
		/* XXX destroy engine last */
		/* XXX dont destroy engine if NOJS flag is set */
		dprintf(dlevel,"ap->js.e: %p, NOJS: %d\n", ap->js.e, (ap->flags & AGENT_FLAG_NOJS) != 0);
		if ((ap->flags & AGENT_FLAG_NOJS) == 0) JS_EngineDestroy(ap->js.e);
	}
#endif

	if (agents) list_delete(agents,ap);
	free(ap);
}

void agent_shutdown(void) {
	solard_agent_t *ap;

	int ldlevel = dlevel;

	dprintf(ldlevel,"agents: %p\n", agents);
	if (agents) {
		dprintf(ldlevel,"agents count: %d\n", list_count(agents));
		while(true) {
			list_reset(agents);
			ap = list_get_next(agents);
			if (!ap) break;
			agent_destroy_agent(ap);
		}
        	list_destroy(agents);
        	agents = 0;
	}
	dprintf(ldlevel,"Shutting down config...\n");
	config_shutdown();
#ifdef MQTT
	dprintf(ldlevel,"Shutting down mqtt...\n");
	mqtt_shutdown();
#endif
#ifdef INFLUX
	dprintf(ldlevel,"Shutting down influx...\n");
	influx_shutdown();
#endif
	dprintf(ldlevel,"Shutting down common...\n");
	common_shutdown();
}

solard_agent_t *agent_init(int argc, char **argv, char *agent_version, opt_proctab_t *agent_opts,
				solard_driver_t *Cdriver, void *handle, int flags,
				config_property_t *driver_props, config_function_t *driver_funcs) {
	solard_agent_t *ap;
#ifdef JS
	char jsexec[4096];
	char script[256];
	char script_dir[256];
	int run_scripts;
	int rtsize,stksize,noscript,nojs;
#endif
#ifdef MQTT
	char mqtt_topic[SOLARD_TOPIC_SIZE];
#endif
	int config_from_mqtt;
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
		{ "-U:#|javascript runtime size",&rtsize,DATA_TYPE_INT,0,0,"256K" },
		{ "-K:#|javascript stack size",&stksize,DATA_TYPE_INT,0,0,"8K" },
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
#endif
	config_from_mqtt = 0;
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
#ifdef MQTT
	ap->mq = list_create();
	ap->config_from_mqtt = config_from_mqtt;
	strcpy(ap->mqtt_topic,mqtt_topic);
#endif
#ifdef JS
	if (nojs) flags |= AGENT_FLAG_NOJS;
	ap->js.rtsize = rtsize;
	ap->js.stksize = stksize;
	run_scripts = (noscript == 0);
	ap->js.run_scripts = false;
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

	/* Auto generate configfile if not specified */
	if (ap->flags & AGENT_FLAG_AUTOCONFIG && !config_from_mqtt && !strlen(configfile)) {
		char *names[4];
		char *types[] = { "json", "conf", 0 };
		char path[512];
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
	ap->js.run_scripts = run_scripts;
	dprintf(dlevel,"ap->js.e: %p\n", ap->js.e);
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
			dprintf(dlevel,"ignore_js_errors: %d\n", ap->js.ignore_js_errors);
			if (!ap->js.ignore_js_errors) {
				log_error("agent_init: init_script failed\n");
				goto agent_init_error;
			}
			ap->js.init_run = true;
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

	if (!agents) agents = list_create();
	list_add(agents,ap,0);

	dprintf(dlevel,"returning: %p\n",ap);
	return ap;

agent_init_error:
	agent_destroy_agent(ap);
	dprintf(dlevel,"returning 0\n");
	return 0;
}

int agent_run(solard_agent_t *ap) {
#ifdef MQTT
	solard_message_t *msg;
#endif
	int read_status,write_status;
	time_t last_read,cur,diff;
#ifdef DEBUG_MEM
	int used,last_used,peak,last_peak;
#endif

	dprintf(dlevel,"ap: %p\n", ap);
#ifdef JS
	if (agent_script_exists(ap,ap->js.start_script)) agent_start_script(ap,ap->js.start_script);
#endif

	last_read = 0;
#ifdef JS
	/* Do a GC before we start */
	dprintf(dlevel,"Cleaning up...\n");
	JS_EngineCleanup(ap->js.e);
	dprintf(dlevel,"back...\n");
#endif
#ifdef DEBUG_MEM
	peak = last_peak = last_used = 0;
#endif
	set_state(ap,SOLARD_AGENT_STATE_RUNNING);
	ap->run_count = ap->read_count = ap->write_count = 0;
	dprintf(dlevel,"Starting...\n");
	agent_event(ap,"Agent","Start");
	dprintf(dlevel,"state: %d\n", ap->state);
	while(check_state(ap,SOLARD_AGENT_STATE_RUNNING)) {
#ifdef JS
		/* Check every 10s if any of the JS loaded scripts have been updated and if so reload them */
		if (ap->js.e && (ap->run_count % 10) == 0) {
			dprintf(dlevel,"Checking if loaded JS scripts have been updated....\n");
			JS_EngineCheckLoaded(ap->js.e);
		}
#endif

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
			ap->read_count++;
		}

		/* Call cb */
		dprintf(dlevel+1,"func: %p\n", ap->callback.func);
		if (ap->callback.func) ap->callback.func(ap->callback.ctx);

#ifdef JS
		/* Call run script */
		if (agent_script_exists(ap,ap->js.run_script)) agent_start_script(ap,ap->js.run_script);
#endif

#ifdef MQTT
		/* Process messages */
		dprintf(dlevel+1,"mq count: %d\n", list_count(ap->mq));
		list_reset(ap->mq);
		while((msg = list_get_next(ap->mq)) != 0) {
//			solard_message_dump(msg,0);
			if (agent_process_message(ap,msg) == 0 || ap->purge) {
				dprintf(dlevel,"deleting message...\n");
				list_delete(ap->mq,msg);
			}
		}
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
			ap->write_count++;

#ifdef DEBUG_MEM
            if (ap->debug_mem) {
                used = mem_used();
                if (used > peak) peak = used;
                dprintf(dlevel,"debug_mem: %d, used: %d, last_used: %d\n", ap->debug_mem, used, last_used);
                if ((ap->debug_mem_always == 1) || (ap->debug_mem && used != last_used)) {
                    int udiff,pdiff;

                    udiff = used - last_used;
                    pdiff = peak - last_peak;
                    log_info("used: %d (%s%d), peak: %d (%s%d)\n", used, (udiff > 0 ? "+" : ""), udiff, peak, (pdiff > 0 ? "+" : ""), pdiff);
                }
                last_used = used;
                last_peak = peak;
            }
#endif
		}
#ifdef JS
		/* At gc_interval, do a cleanup */
		dprintf(dlevel,"e: %p, run_count: %d, gc_interval: %d\n", ap->js.e, ap->run_count, ap->js.gc_interval);
		if (ap->js.e && ap->js.gc_interval > 0 && ((ap->run_count % ap->js.gc_interval) == 0)) {
			if (ap->debug_mem) dprintf(dlevel,"running gc...\n");
			JS_EngineCleanup(ap->js.e);
		}
#endif
		ap->run_count++;
		sleep(1);

		if (ap->refresh) {
			last_read -= ap->interval;
			ap->refresh = false;
		}
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
	AGENT_PROPERTY_ID_START=2048,
#ifdef MQTT
	AGENT_PROPERTY_ID_MSG,
	AGENT_PROPERTY_ID_ADDMQ,
	AGENT_PROPERTY_ID_PURGE,
#endif
#if 0
	AGENT_PROPERTY_ID_CONFIG=2048,
#ifdef MQTT
	AGENT_PROPERTY_ID_MQTT,
	AGENT_PROPERTY_ID_MSG,
	AGENT_PROPERTY_ID_ADDMQ,
	AGENT_PROPERTY_ID_PURGE,
#endif
#ifdef INFLUX
	AGENT_PROPERTY_ID_INFLUX,
#endif
	AGENT_PROPERTY_ID_INFO,
#endif
};

static JSBool js_agent_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	solard_agent_t *ap;
	config_property_t *p;

	int ldlevel = dlevel+2;

	ap = JS_GetPrivate(cx,obj);
	dprintf(ldlevel,"ap: %p\n", ap);
	if (!ap) {
		JS_ReportError(cx, "agent_getprop: private is null!");
		return JS_FALSE;
	}
	p = 0;
	dprintf(ldlevel,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(ldlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
#ifdef MQTT
		case AGENT_PROPERTY_ID_MSG:
			*rval = OBJECT_TO_JSVAL(js_create_messages_array(cx,obj,ap->mq));
			break;
		case AGENT_PROPERTY_ID_ADDMQ:
			*rval = type_to_jsval(cx,DATA_TYPE_BOOL,&ap->addmq,0);
			break;
		case AGENT_PROPERTY_ID_PURGE:
			*rval = type_to_jsval(cx,DATA_TYPE_BOOL,&ap->purge,0);
			break;
#endif

#if 0
		case AGENT_PROPERTY_ID_INFO:
		    {
			char *j;
			JSString *str;
			jsval rootVal;
			JSONParser *jp;
			jsval reviver = JSVAL_NULL;
			JSBool ok;

			/* Convert from C JSON type to JS JSON type */
			dprintf(ldlevel,"ap->info: %p\n", ap->info);
			j = json_dumps(ap->info,0);
			if (!j) j = JS_strdup(cx,"{}");
			dprintf(ldlevel,"j: %s\n", j);
			jp = js_BeginJSONParse(cx, &rootVal);
			dprintf(ldlevel,"jp: %p\n", jp);
			if (!jp) {
				JS_ReportError(cx, "unable init JSON parser\n");
				JS_free(cx,j);
				return JS_FALSE;
			}
			str = JS_NewStringCopyZ(cx,j);
        		ok = js_ConsumeJSONText(cx, jp, JS_GetStringChars(str), JS_GetStringLength(str));
			dprintf(ldlevel,"ok: %d\n", ok);
			ok = js_FinishJSONParse(cx, jp, reviver);
			dprintf(ldlevel,"ok: %d\n", ok);
			JS_free(cx,j);
			*rval = rootVal;
		    }
		    break;
		case AGENT_PROPERTY_ID_CONFIG:
			*rval = ap->js.config_val;
			break;
#ifdef MQTT
		case AGENT_PROPERTY_ID_MQTT:
			*rval = ap->js.mqtt_val;
			break;
		case AGENT_PROPERTY_ID_MSG:
			*rval = OBJECT_TO_JSVAL(js_create_messages_array(cx,obj,ap->mq));
			break;
		case AGENT_PROPERTY_ID_ADDMQ:
			*rval = type_to_jsval(cx,DATA_TYPE_BOOL,&ap->addmq,0);
			break;
		case AGENT_PROPERTY_ID_PURGE:
			*rval = type_to_jsval(cx,DATA_TYPE_BOOL,&ap->purge,0);
			break;
#endif
#ifdef INFLUX
		case AGENT_PROPERTY_ID_INFLUX:
			*rval = ap->js.influx_val;
			break;
#endif
#endif

                        break;
		default:
			p = CONFIG_GETMAP(ap->cp,prop_id);
			dprintf(ldlevel,"p: %p\n", p);
			if (!p) p = config_get_propbyid(ap->cp,prop_id);
			dprintf(ldlevel,"p: %p\n", p);
			if (!p) {
				JS_ReportError(cx, "agent_getprop: internal error: property %d not found", prop_id);
				return JS_FALSE;
			}
			break;
		}
	} else if (JSVAL_IS_STRING(id)) {
		char *name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
		dprintf(ldlevel,"name: %s\n", name);
		if (name) {
			p = config_get_property(ap->cp, ap->driver->name, name);
			JS_free(cx,name);
		}
	}
	dprintf(ldlevel,"p: %p\n", p);
	if (p && p->dest) {
		dprintf(ldlevel,"p: type: %d(%s), name: %s\n", p->type, typestr(p->type), p->name);
		*rval = type_to_jsval(cx,p->type,p->dest,p->len);
	}
        return JS_TRUE;
}

static JSBool js_agent_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	solard_agent_t *ap;

	int ldlevel = dlevel+2;

	ap = JS_GetPrivate(cx,obj);
	dprintf(ldlevel,"ap: %p\n", ap);
	if (!ap) {
		JS_ReportError(cx, "agent_getprop: private is null!");
		return JS_FALSE;
	}
	dprintf(ldlevel,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		int prop_id = JSVAL_TO_INT(id);
		dprintf(ldlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
#ifdef MQTT
		case AGENT_PROPERTY_ID_ADDMQ:
			jsval_to_type(DATA_TYPE_BOOL,&ap->addmq,0,cx,*vp);
			break;
		case AGENT_PROPERTY_ID_PURGE:
			jsval_to_type(DATA_TYPE_BOOL,&ap->purge,0,cx,*vp);
			break;
#endif
		default:
			return js_config_common_setprop(cx, obj, id, vp, ap->cp, ap->driver->name);
			break;
		}
	} else if (JSVAL_IS_STRING(id)) {
		return js_config_common_setprop(cx, obj, id, vp, ap->cp, ap->driver->name);
	}
#if 0
		default:
			dprintf(-1,"id: %d\n", id);
			return js_config_common_setprop(cx, obj, id, vp, ap->cp, 0);
		}
	} else if (JSVAL_IS_STRING(id)) {
		char *name;
		name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
		if (name) {
			config_property_t *p;

			dprintf(ldlevel,"value type: %s\n", jstypestr(cx,*vp));
			p = config_get_property(ap->cp, ap->driver->name, name);
			JS_free(cx, name);
			dprintf(ldlevel,"p: %p\n", p);
			if (p) return js_config_property_set_value(p,cx,*vp,ap->cp->triggers);
		}
	}
#endif
	return JS_TRUE;
}

static JSClass js_agent_class = {
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

static JSBool js_agent_run(JSContext *cx, uintN argc, jsval *vp) {
	solard_agent_t *ap;
	JSObject *obj;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	ap = JS_GetPrivate(cx, obj);

	agent_run(ap);
	return JS_TRUE;
}

static JSBool js_agent_destroy(JSContext *cx, uintN argc, jsval *vp) {
	solard_agent_t *ap;
	JSObject *obj;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	ap = JS_GetPrivate(cx, obj);

	dprintf(dlevel,"destroying...\n");
	agent_destroy_agent(ap);
	return JS_TRUE;
}

#ifdef MQTT
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

	*topic = 0;
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

static JSBool js_agent_deletemsg(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	solard_agent_t *ap;
	JSObject *mobj;

	ap = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"ap: %p\n", ap);
	if (!ap) {
		JS_ReportError(cx,"internal error: agent private is null!\n");
		return JS_FALSE;
	}
	mobj = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "o", &mobj)) return JS_FALSE;
	dprintf(dlevel,"mobj: %p\n", mobj);
	if (mobj) {
		solard_message_t *msg = JS_GetPrivate(cx, mobj);
		dprintf(dlevel,"msg: %p\n", msg);
		list_delete(ap->mq, msg);
	}
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

static JSBool js_agent_callfunc(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	solard_agent_t *ap;

	int ldlevel = dlevel;

	obj = JS_THIS_OBJECT(cx, vp);
	dprintf(ldlevel,"obj: %p\n", obj);
	if (!obj) {
		JS_ReportError(cx, "js_agent_callfunc: internal error: object is null!\n");
		return JS_FALSE;
	}
	ap = JS_GetPrivate(cx, obj);
	dprintf(ldlevel,"ap: %p\n", ap);
	if (!ap) {
		JS_ReportError(cx, "js_agent_callfunc: internal error: private is null!\n");
		return JS_FALSE;
	}
	dprintf(ldlevel,"ap->cp: %p\n", ap->cp);
	if (!ap->cp) {
		JS_ReportError(cx, "js_agent_callfunc: internal error: config ptr is null!\n");
		return JS_FALSE;
	}

	dprintf(ldlevel,"argc: %d\n", argc);
	return js_config_callfunc(ap->cp, cx, argc, vp);
}

static JSBool js_agent_event(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	solard_agent_t *ap;
	char *module,*action;

	ap = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"ap: %p\n", ap);
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

static JSBool js_agent_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	solard_agent_t *ap;
	char **args, *vers;
        unsigned int count;
	JSObject *aobj, *dobj, *oobj, *pobj, *fobj;
	opt_proctab_t *opts;
	int i,sz;
	jsval val;
	JSString *jstr;
	solard_driver_t *driver;
	void *driver_handle;
	JSObject *newobj;
	config_property_t *props;
	config_function_t *funcs;
	JSFunctionSpec agent_ctor_funcs[] = {
		/* thse 2 only to be called from CTOR-created agent */
		JS_FN("run",js_agent_run,0,0,0),
		JS_FN("destroy",js_agent_destroy,0,0,0),
		{ 0 }
	};

	int ldlevel = dlevel;

	dprintf(ldlevel,"cx: %p, obj: %p, argc: %d\n", cx, obj, argc);

	vers = 0;
	aobj = dobj = pobj = fobj = 0;
        if (!JS_ConvertArguments(cx, argc, argv, "o o s / o o o", &aobj, &dobj, &vers, &oobj, &pobj, &fobj)) return JS_FALSE;
        dprintf(ldlevel,"aobj: %p, dobj: %p, vers: %s, oobj: %p, pobj: %p, fobj: %p\n", aobj, dobj, vers, oobj, pobj, fobj);
//	if (!JSVAL_IS_OBJECT(aobj) || !OBJ_IS_ARRAY(cx,aobj)) return 0;

	if (!dobj) {
		JS_ReportError(cx,"Agent: driver object is required\n");
		return JS_FALSE;
	}

	driver_handle = JS_GetPrivate(cx,dobj);
	if (!driver_handle) {
		JS_ReportError(cx,"internal error: unable to get driver handle");
		return JS_FALSE;
	}
	driver = js_driver_get_driver(driver_handle);
	if (!driver) {
		JS_ReportError(cx,"internal error: unable to get driver");
		return JS_FALSE;
	}
	dprintf(ldlevel,"driver name: %s\n", driver->name);

	/* get arg array count and alloc props */
	if (!js_GetLengthProperty(cx, aobj, &count)) {
		JS_ReportError(cx,"unable to get argv array length");
		return JS_FALSE;
	}
	dprintf(ldlevel,"count: %d\n", count);
	if (count) {
		/* prepend program name (agent name) */
		count++;
		sz = sizeof(char *) * count;
		dprintf(ldlevel,"sz: %d\n", sz);
		args = JS_malloc(cx,sz);
		if (!args) {
			JS_ReportError(cx,"unable to malloc args");
			return JS_FALSE;
		}
		memset(args,0,sz);

		args[0] = JS_strdup(cx,driver->name);
		for(i=1; i < count; i++) {
			JS_GetElement(cx, aobj, i-1, &val);
			dprintf(ldlevel,"aobj[%d] type: %s\n", i, jstypestr(cx,val));
			jstr = JS_ValueToString(cx, val);
			args[i] = (char *)JS_EncodeString(cx, jstr);
		}
//		for(i=0; i < count; i++) dprintf(ldlevel,"args[%d]: %s\n", i, args[i]);
	} else {
		args = 0;
	}

        /* Convert opts object to opt_property_t array */
        if (oobj) {
                opts = js_opt_arr2opts(cx, oobj);
                dprintf(ldlevel,"opts: %p\n", opts);
                if (!opts) {
                        JS_ReportError(cx, "agent_ctor: internal error: opts is null!");
                        return JS_FALSE;
                }
        } else {
                opts = 0;
        }

        /* Convert props object to config_property_t array */
	if (pobj) {
		props = js_config_obj2props(cx, pobj);
		if (!props) {
			JS_ReportError(cx, "agent_ctor: internal error: props is null!");
			return JS_FALSE;
		}
	} else {
		props = 0;
	}

        /* Convert funcs object to config_function_t array */
	if (fobj) {
		funcs = js_config_obj2funcs(cx, fobj);
		if (!funcs) {
			JS_ReportError(cx, "agent_ctor: internal error: funcs is null!");
			return JS_FALSE;
		}
	} else {
		funcs = 0;
	}

	/* init */
	ap = agent_init(count,args,vers,opts,driver,driver_handle,AGENT_FLAG_NOJS,props,funcs);
	if (args) {
		for(i=0; i < count; i++) JS_free(cx, args[i]);
		JS_free(cx,args);
	}
	if (!ap) {
		JS_ReportError(cx, "agent_init failed");
		return JS_FALSE;
	}

#if 0
	/* create the new agent instance */
	newobj = js_agent_new(cx, obj, ap);
	if (!newobj) {
		JS_ReportError(cx, "internal error: unable to create new agent instance");
		return JS_FALSE;
	}
#endif

	/* get the agent object from the driver */
	newobj = js_driver_get_agent(driver_handle);
	if (!newobj) {
		JS_ReportError(cx,"unable to initialize %s class", js_agent_class.name);
		return JS_FALSE;
	}

	/* add props to newly created object */
	if (props) {
		config_property_t *pp, *p;
		JSBool ok;
		int flags;

		for(pp = props; pp->name; pp++) {
			p = config_get_property(ap->cp, ap->driver->name, pp->name);
			dprintf(ldlevel,"p: %p\n", p);
			if (!p) continue;
			if (p->dest) val = type_to_jsval(cx,p->type,p->dest,p->len);
			else val = JSVAL_VOID;
			dprintf(ldlevel,"val: %x, void: %x\n", val, JSVAL_VOID);
			flags = JSPROP_ENUMERATE;
			if (p->flags & CONFIG_FLAG_READONLY) flags |= JSPROP_READONLY;
			ok = JS_DefinePropertyWithTinyId(cx,newobj,p->name,p->id,val,0,0,flags);
			dprintf(ldlevel,"define ok: %d\n", ok);
		}
	}
#if 0
	config_dump(ap->cp);
	dprintf(-1,"interval: %d\n", ap->interval);
	{
		config_property_t *p;
printf("==> getting prop\n");
		p = config_get_property(ap->cp, ap->driver->name, "interval");
		dprintf(-1,"p: %p\n",p);
		if (p) {
			dprintf(-1,"p->dest: %p\n", p->dest);
			dprintf(-1,"interval: %p\n", &ap->interval);
		}
	}
#endif

	dprintf(dlevel,"Defining agent ctor funcs\n");
	if (!JS_DefineFunctions(cx, newobj, agent_ctor_funcs)) {
		log_error("js_agent_ctor: unable to define funcs\n");
	}

	/* XXX delay this? */
	if (props) JS_free(cx,props);
	if (funcs) JS_free(cx,funcs);

	*rval = OBJECT_TO_JSVAL(newobj);
	return JS_TRUE;
}

JSObject *js_InitAgentClass(JSContext *cx, JSObject *parent) {
	JSPropertySpec agent_props[] = {
#ifdef MQTT
		{ "messages", AGENT_PROPERTY_ID_MSG, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "addmq", AGENT_PROPERTY_ID_ADDMQ, JSPROP_ENUMERATE },
		{ "purge", AGENT_PROPERTY_ID_PURGE, JSPROP_ENUMERATE },
#endif

#if 0
		{ "config", AGENT_PROPERTY_ID_CONFIG, JSPROP_ENUMERATE },
#ifdef MQTT
		{ "mqtt", AGENT_PROPERTY_ID_MQTT, JSPROP_ENUMERATE },
		{ "messages", AGENT_PROPERTY_ID_MSG, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "addmq", AGENT_PROPERTY_ID_ADDMQ, JSPROP_ENUMERATE },
		{ "purge", AGENT_PROPERTY_ID_PURGE, JSPROP_ENUMERATE },
#endif
#ifdef INFLUX
		{ "influx", AGENT_PROPERTY_ID_INFLUX, JSPROP_ENUMERATE },
#endif
		{ "info", AGENT_PROPERTY_ID_INFO, JSPROP_ENUMERATE | JSPROP_READONLY },
#endif
		{ 0 }
	};

	JSFunctionSpec agent_funcs[] = {
#ifdef MQTT
		JS_FN("mktopic",js_agent_mktopic,1,1,0),
		JS_FN("purgemq",js_agent_purgemq,0,0,0),
		JS_FS("delete_message",js_agent_deletemsg,1,1,0),
#endif
		JS_FS("signal",js_agent_event,2,2,0),
		JS_FN("pubinfo",js_agent_pubinfo,0,0,0),
		JS_FN("pubconfig",js_agent_pubconfig,0,0,0),
		{ 0 }
	};
	JSObject *newobj;


	dprintf(2,"creating %s class\n",js_agent_class.name);
	newobj = JS_InitClass(cx, parent, 0, &js_agent_class, js_agent_ctor, 3, agent_props, agent_funcs, 0, 0);
	if (!newobj) {
		JS_ReportError(cx,"unable to initialize %s class", js_agent_class.name);
		return 0;
	}
	dprintf(2,"newobj: %p\n", newobj);

	return newobj;
}

JSObject *js_agent_new(JSContext *cx, JSObject *parent, void *priv) {
	solard_agent_t *ap = priv;
	JSObject *newobj;

	int ldlevel = dlevel;

//	dprintf(ldlevel,"parent name: %s\n", JS_GetObjectName(cx,parent));

	/* Create the new object */
	dprintf(2,"Creating %s object...\n", js_agent_class.name);
	newobj = JS_NewObject(cx, &js_agent_class, 0, parent);
	if (!newobj) return 0;

	/* Add config props/funcs */
	dprintf(ldlevel,"ap->cp: %p\n", ap->cp);
	if (ap->cp) {
		/* Create agent config props */
		JSPropertySpec *props = js_config_to_props(ap->cp, cx, "agent", 0);
		if (!props) {
			log_error("js_agent_new: unable to create config props: %s\n", config_get_errmsg(ap->cp));
			return 0;
		}

		dprintf(ldlevel,"Defining agent config props\n");
		if (!JS_DefineProperties(cx, newobj, props)) {
			JS_ReportError(cx,"unable to define props");
			return 0;
		}
		JS_free(cx,props);

		/* Create agent config funcs */
		JSFunctionSpec *funcs = js_config_to_funcs(ap->cp, cx, js_agent_callfunc, 0);
		if (!funcs) {
			log_error("unable to create funcs: %s\n", config_get_errmsg(ap->cp));
			return 0;
		}
		dprintf(ldlevel,"Defining agent config funcs\n");
		if (!JS_DefineFunctions(cx, newobj, funcs)) {
			JS_ReportError(cx,"unable to define funcs");
			return 0;
		}
		JS_free(cx,funcs);

		/* Create the config object */
		dprintf(ldlevel,"Creating config_val...\n");
		ap->js.config_val = OBJECT_TO_JSVAL(js_config_new(cx,newobj,ap->cp));
		JS_DefineProperty(cx, newobj, "config", ap->js.config_val, 0, 0, JSPROP_ENUMERATE);
	}

	dprintf(ldlevel,"Setting private to: %p\n", ap);
	JS_SetPrivate(cx,newobj,ap);

	/* Only used by driver atm */
	dprintf(ldlevel,"Creating agent_val...\n");
	ap->js.agent_val = OBJECT_TO_JSVAL(newobj);
	dprintf(ldlevel,"agent_val: %x\n", ap->js.agent_val);

#ifdef MQTT
	if (ap->m) {
		/* Create MQTT child object */
		dprintf(ldlevel,"Creating mqtt_val...\n");
		ap->js.mqtt_val = OBJECT_TO_JSVAL(js_mqtt_new(cx,newobj,ap->m));
		JS_DefineProperty(cx, newobj, "mqtt", ap->js.mqtt_val, 0, 0, JSPROP_ENUMERATE);
	}
#endif
#ifdef INFLUX
	if (ap->i) {
		/* Create Influx child object */
		dprintf(ldlevel,"Creating influx_val...\n");
		ap->js.influx_val = OBJECT_TO_JSVAL(js_influx_new(cx,newobj,ap->i));
		JS_DefineProperty(cx, newobj, "influx", ap->js.influx_val, 0, 0, JSPROP_ENUMERATE);
	}
#endif
	dprintf(dlevel,"e: %p\n", ap->e);
	if (ap->e) {
		ap->js.event_val = OBJECT_TO_JSVAL(js_event_new(cx,newobj,ap->e));
		JS_DefineProperty(cx, newobj, "event", ap->js.event_val, 0, 0, JSPROP_ENUMERATE);
	}

	return newobj;
}
#endif
