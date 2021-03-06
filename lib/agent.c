
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define DEBUG_STARTUP 0

#if DEBUG_STARTUP
#define _ST_DEBUG LOG_DEBUG
#else
#define _ST_DEBUG 0
#endif

#include <time.h>
#include <ctype.h>
#include "agent.h"
#include "uuid.h"

#include "controller.h"
#include "inverter.h"
#include "battery.h"

static int suspend_read = 0;
static int suspend_write = 0;
#ifndef __WIN32
#include <sys/signal.h>

void usr_handler(int signo) {
	if (signo == SIGUSR1) {
		log_write(LOG_INFO,"agent: %s reads\n", (suspend_read ? "resuming" : "suspending"));
		suspend_read = (suspend_read ? 0 : 1);
	} else if (signo == SIGUSR2) {
		log_write(LOG_INFO,"agent: %s writes\n", (suspend_write ? "resuming" : "suspending"));
		suspend_write = (suspend_write ? 0 : 1);
	}
}
#endif

int agent_set_callback(solard_agent_t *ap, solard_agent_callback_t cb) {
	ap->cb = cb;
	return 0;
}

/* Get driver handle from role */
void *agent_get_handle(solard_agent_t *ap) {
	return (ap->role ? ap->role->get_handle(ap->role_handle) : 0);
}

void agent_mktopic(char *topic, int topicsz, solard_agent_t *ap, char *name, char *func, char *action, char *id) {
	register char *p;

	dprintf(1,"name: %s, func: %s, action: %s, id: %s\n", name, func, action, id);

	/* /Solard/Battery/+/Config/Get/Status */
	p = topic;
	p += snprintf(p,topicsz-strlen(topic),"%s",SOLARD_TOPIC_ROOT);
	if (ap->role) p += snprintf(p,topicsz-strlen(topic),"/%s",ap->role->name);
	if (name) p += snprintf(p,topicsz-strlen(topic),"/%s",name);
	if (func) p += snprintf(p,topicsz-strlen(topic),"/%s",func);
	if (action) p += snprintf(p,topicsz-strlen(topic),"/%s",action);
	if (id) p += snprintf(p,topicsz-strlen(topic),"/%s",id);
}

int agent_sub(solard_agent_t *ap, char *name, char *func, char *action, char *id) {
	char topic[SOLARD_TOPIC_LEN];

	dprintf(1,"name: %s, func: %s, action: %s, id: %s\n", name, func, action, id);

	agent_mktopic(topic,sizeof(topic)-1,ap,name,func,action,id);
	log_write(LOG_INFO,"%s\n", topic);
	return mqtt_sub(ap->m,topic);
}

int agent_pub(solard_agent_t *ap, char *func, char *action, char *id, char *message, int retain) {
	char topic[SOLARD_TOPIC_LEN];

	dprintf(1,"func: %s, action: %s, id: %s, message: %s, retain: %d\n", func, action, id, message, retain);

	agent_mktopic(topic,sizeof(topic)-1,ap,ap->instance_name,func,action,id);
        dprintf(1,"topic: %s\n", topic);
        return mqtt_pub(ap->m,topic,message,retain);
}


int agent_send_status(solard_agent_t *ap, char *name, char *func, char *op, char *id, int status, char *message) {
	char msg[4096];
	json_value_t *o;

	dprintf(1,"op: %s, status: %d, message: %s\n", op, status, message);

	o = json_create_object();
	json_add_number(o,"status",status);
	json_add_string(o,"message",message);
	json_tostring(o,msg,sizeof(msg)-1,0);
	json_destroy(o);
//	return agent_pub(ap, func, op, id, message, 0);
#if 1
	{
		char topic[SOLARD_TOPIC_LEN];

		/* /root/role/name/func/op/Status/id */
		if (ap->role)
			sprintf(topic,"%s/%s/%s/%s/%s/Status/%s",SOLARD_TOPIC_ROOT,ap->role->name,name,func,op,id);
		else
			sprintf(topic,"%s/%s/%s/%s/Status/%s",SOLARD_TOPIC_ROOT,name,func,op,id);
		dprintf(1,"topic: %s\n", topic);
		mqtt_pub(ap->m,topic,msg,0);
		return 0;
	}
#endif
}

list agent_config_payload2list(solard_agent_t *ap, char *name, char *func, char *op, char *id, char *payload) {
	json_value_t *v;
	list lp;
	int i;
	solard_confreq_t newreq;

	dprintf(1,"name: %s, op: %s, payload: %s\n", name, op, payload);

	/* Payload must either be an array of param names for get or param name/value pairs for set */

	lp = list_create();
	if (!lp) {
		agent_send_status(ap,name,func,op,id,1,"internal error");
		return 0;
	}

	/* Parse payload */
	v = json_parse(payload);
	if (!v) {
		agent_send_status(ap,name,func,op,id,1,"error parsing json string");
		goto agent_config_payload_error;
	}

	/* If Get, must be array of strings */
	if (strcmp(op,"Get")== 0) {
		json_array_t *a;
		json_value_t *av;
		char *p;

		dprintf(1,"type: %s(%d)\n", json_typestr(v->type), v->type);
		if (v->type != JSONArray) {
			agent_send_status(ap,name,func,op,id,1,"invalid format");
			goto agent_config_payload_error;
		}
		/* Process items */
		a = v->value.array;
		dprintf(1,"a->count: %d\n", (int)a->count);
		for(i=0; i < a->count; i++) {
			av = a->items[i];
			dprintf(1,"av type: %d(%s)\n", av->type, json_typestr(av->type));
			p = (char *)json_string(av);
			dprintf(1,"p: %s\n", p);
			list_add(lp,p,strlen(p)+1);
		}
	/* If Set, must be object of key/value pairs */
	} else if (strcmp(op,"Set")== 0) {
		json_object_t *o;
		json_value_t *ov;

		dprintf(1,"type: %s(%d)\n", json_typestr(v->type), v->type);
		if (v->type != JSONObject) {
			agent_send_status(ap,name,func,op,id,1,"invalid format");
			goto agent_config_payload_error;
		}
		o = v->value.object;
		dprintf(1,"o->count: %d\n", (int)o->count);
		for(i=0; i < o->count; i++) {
			ov = o->values[i];
			dprintf(1,"ov name: %s, type: %s(%d)\n", o->names[i], json_typestr(ov->type),ov->type);
			memset(&newreq,0,sizeof(newreq));
			strncat(newreq.name,o->names[i],sizeof(newreq.name)-1);
			if (ov->type == JSONNumber) {
				newreq.dval = ov->value.number;
				newreq.type = DATA_TYPE_F64;
			} else if (ov->type == JSONString) {
				strncat(newreq.sval,json_string(ov),sizeof(newreq.sval)-1);
				newreq.type = DATA_TYPE_STRING;
			} else if (ov->type == JSONBoolean) {
				newreq.bval = ov->value.boolean;
				newreq.type = DATA_TYPE_BOOL;
			}
			list_add(lp,&newreq,sizeof(newreq));
		}
	} else {
		agent_send_status(ap,name,func,op,id,1,"unsupported action");
		goto agent_config_payload_error;
	}

	json_destroy(v);
	dprintf(1,"returning: %p\n", lp);
	return lp;
agent_config_payload_error:
	json_destroy(v);
	dprintf(1,"returning: 0\n");
	return 0;
}

int agent_config(solard_agent_t *ap, list lp) {
	return 0;
}

void agent_process(solard_agent_t *ap, solard_message_t *msg) {
	char *message;
	list lp;
	long start;

	dprintf(1,"msg: name: %s, func: %s, action: %s, id: %s\n", msg->name, msg->func, msg->action, msg->id);

	start = mem_used();
	if (strcmp(msg->func,"Config")==0) {
		lp = agent_config_payload2list(ap,msg->name,msg->func,msg->action,msg->id,msg->data);
		if (!lp) {
			message = "internal error";
			goto agent_process_error;
		}
		if (ap->role && ap->role->config) ap->role->config(ap->role_handle,msg->action,msg->id,lp);
		list_destroy(lp);
	} else if (strcmp(msg->func,"Control")==0) {
		json_value_t *v;

		v = json_parse(msg->data);
		dprintf(1,"v: %p\n", v);
		if (!v) {
			message = "invalid json format";
			goto agent_process_error;
		}
		if (ap->role && ap->role->control) ap->role->control(ap->role_handle,msg->action,msg->id,v);
		json_destroy(v);
	}
	dprintf(1,"used: %ld\n", mem_used() - start);
	return;

agent_process_error:
	agent_send_status(ap,msg->name,msg->func,msg->action,msg->id,1,message);
	return;
}

solard_agent_t *agent_init(int argc, char **argv, opt_proctab_t *agent_opts, solard_module_t *driver) {
	solard_agent_t *ap;
	char info[65536];
	char tp_info[128],mqtt_info[200],*p;
	char configfile[256];
	char name[SOLARD_NAME_LEN];
	char transport[SOLARD_TRANSPORT_LEN];
	char target[SOLARD_TARGET_LEN];
	char topts[SOLARD_TOPTS_LEN];
	char sname[64];
	int info_flag,pretty_flag,read_interval,write_interval;
	opt_proctab_t std_opts[] = {
		/* Spec, dest, type len, reqd, default val, have */
		{ "-t::|transport,target,opts",&tp_info,DATA_TYPE_STRING,sizeof(tp_info)-1,0,"" },
		{ "-m::|mqtt host,clientid[,user[,pass]]",&mqtt_info,DATA_TYPE_STRING,sizeof(mqtt_info)-1,0,"" },
		{ "-c:%|config file",&configfile,DATA_TYPE_STRING,sizeof(configfile)-1,0,"" },
		{ "-s::|config section name",&sname,DATA_TYPE_STRING,sizeof(sname)-1,0,"" },
		{ "-n::|agent name",&name,DATA_TYPE_STRING,sizeof(name)-1,0,"" },
//		{ "-i:#|instance ID",&id,DATA_TYPE_STRING,sizeof(id),0,"" },
		{ "-I|agent info",&info_flag,DATA_TYPE_LOGICAL,0,0,"0" },
		{ "-P|pretty print json output",&pretty_flag,DATA_TYPE_LOGICAL,0,0,0 },
		{ "-r:#|reporting interval",&read_interval,DATA_TYPE_INT,0,0,"30" },
		{ "-w:#|update (write) interval",&write_interval,DATA_TYPE_INT,0,0,"30" },
		OPTS_END
	}, *opts;
	int logopts = LOG_INFO|LOG_WARNING|LOG_ERROR|LOG_SYSERR|_ST_DEBUG;
	solard_module_t *tp;
	void *tp_handle,*driver_handle;
	list names;

	if (!driver) {
		printf("driver is null, aborting\n");
		exit(0);
	}

	opts = opt_addopts(std_opts,agent_opts);
	if (!opts) return 0;
	if (solard_common_init(argc,argv,opts,logopts)) return 0;

	ap = calloc(1,sizeof(*ap));
	if (!ap) {
		log_write(LOG_SYSERR,"calloc agent config");
		return 0;
	}
	ap->modules = list_create();
	ap->mq = list_create();
	ap->pretty = pretty_flag;
	ap->read_interval = read_interval;
	ap->write_interval = write_interval;

	/* Agent name is driver name */
	strncat(ap->name,driver->name,sizeof(ap->name)-1);

	/* Could load these as modules, but not really needed here */
	dprintf(1,"driver->type: %d\n", driver->type);
	switch(driver->type) {
	case SOLARD_MODTYPE_CONTROLLER:
		ap->role = &controller;
		break;
	case SOLARD_MODTYPE_INVERTER:
		ap->role = &inverter;
		break;
	case SOLARD_MODTYPE_BATTERY:
		ap->role = &battery;
		break;
	default:
		break;
	}

	strncpy(ap->section_name,strlen(sname) ? sname : (strlen(name) ? name : driver->name), sizeof(ap->section_name)-1);
	dprintf(1,"section_name: %s\n", ap->section_name);
	dprintf(1,"configfile: %s\n", configfile);
	*transport = *target = *topts = 0;
	if (strlen(configfile)) {
		cfg_proctab_t agent_tab[] = {
			{ ap->section_name, "name", 0, DATA_TYPE_STRING,&ap->instance_name,sizeof(ap->instance_name)-1, 0 },
			{ ap->section_name, "debug", 0, DATA_TYPE_INT,&debug,0, 0 },
			{ ap->section_name, "transport", "Transport", DATA_TYPE_STRING,&transport,sizeof(transport), 0 },
			{ ap->section_name, "target", "Transport address/interface/device", DATA_TYPE_STRING,&target,sizeof(target), 0 },
			{ ap->section_name, "topts", "Transport specific options", DATA_TYPE_STRING,&topts,sizeof(topts), 0 },
			CFG_PROCTAB_END
		};
		*transport = *target = *topts = 0;
		ap->cfg = cfg_read(configfile);
		if (!ap->cfg) goto agent_init_error;
		cfg_get_tab(ap->cfg,agent_tab);
		if (debug) cfg_disp_tab(agent_tab,"agent",0);
		if (mqtt_get_config(ap->cfg,&ap->mqtt_config)) goto agent_init_error;
	} 

	/* Name specified on command line overrides configfile */
	if (strlen(name)) strcpy(ap->instance_name,name);

	/* If still no instance name, set it to driver name */
	if (!strlen(ap->instance_name)) strncpy(ap->instance_name,driver->name,sizeof(ap->instance_name)-1);

	dprintf(1,"tp_info: %s\n", tp_info);
	if (strlen(tp_info)) {
		strncat(transport,strele(0,",",tp_info),sizeof(transport)-1);
		strncat(target,strele(1,",",tp_info),sizeof(target)-1);
		strncat(topts,strele(2,",",tp_info),sizeof(topts)-1);
	}

	/* If no transport and target specified, use null */
	dprintf(1,"transport: %s, target: %s, topts: %s\n", transport, target, topts);
	if (!strlen(transport) || !strlen(target)) {
		tp = &null_transport;
	} else {
		/* Load the transport driver */
		tp = load_module(ap->modules,transport,SOLARD_MODTYPE_TRANSPORT);
		if (!tp) goto agent_init_error;
	}

	/* Create an instance of the transport driver */
	tp_handle = tp->new(ap,target,topts);
	dprintf(1,"tp_handle: %p\n", tp_handle);
	if (!tp_handle) goto agent_init_error;

	/* Create an instance of the agent driver */
	driver_handle = driver->new(ap,tp,tp_handle);
	if (!driver_handle) goto agent_init_error;

	/* Create an instance of the role driver */
	ap->role_handle = ap->role->new(ap,driver,driver_handle);

	/* Get info */
	ap->info = ap->role->info(ap->role_handle);
	if (!ap->info) goto agent_init_error;
	json_dumps_r(ap->info,info,sizeof(info));

	/* If info flag, dump info then exit */
	dprintf(1,"info_flag: %d\n", info_flag);
	if (info_flag) {
		printf("%s\n",info);
//		free(ap);
		exit(0);
	}

	dprintf(1,"mqtt_info: %s\n", mqtt_info);
	if (strlen(mqtt_info) && mqtt_parse_config(&ap->mqtt_config,mqtt_info)) goto agent_init_error;

	/* MQTT Init */
	dprintf(1,"host: %s, clientid: %s, user: %s, pass: %s\n",
		ap->mqtt_config.host, ap->mqtt_config.clientid, ap->mqtt_config.user, ap->mqtt_config.pass);
	if (!strlen(ap->mqtt_config.host)) {
		log_write(LOG_WARNING,"mqtt host not specified, using localhost\n");
		strcpy(ap->mqtt_config.host,"localhost");
	}

	/* MQTT requires a unique ClientID for connections */
	if (!strlen(ap->mqtt_config.clientid)) {
		uint8_t uuid[16];

		dprintf(1,"gen'ing MQTT ClientID...\n");
		uuid_generate_random(uuid);
		my_uuid_unparse(uuid, ap->mqtt_config.clientid);
		dprintf(4,"clientid: %s\n",ap->mqtt_config.clientid);
	}

	/* Create LWT topic */
	agent_mktopic(ap->mqtt_config.lwt_topic,sizeof(ap->mqtt_config.lwt_topic)-1,ap,ap->instance_name,
		"Status",0,0);
//	sprintf(ap->mqtt_config.lwt_topic,"%s/%s/%s/%s",SOLARD_TOPIC_ROOT,ap->role->name,ap->instance_name,SOLARD_FUNC_STATUS);

	/* Create MQTT session */
	ap->m = mqtt_new(&ap->mqtt_config, solard_getmsg, ap->mq);

	/* Callback - must be done before connect */
//	if (mqtt_setcb(ap->m,ap,0,agent_callback0)) return 0;

	/* Connect to the server */
	if (mqtt_connect(ap->m,20)) goto agent_init_error;

	/* Publish our Info */
	sprintf(mqtt_info,"%s/%s/%s/%s",SOLARD_TOPIC_ROOT,ap->role->name,ap->instance_name,SOLARD_FUNC_INFO);
	mqtt_pub(ap->m,mqtt_info,info,1);

	names = list_create();
	list_add(names,"all",4);
	list_add(names,"All",4);
	list_add(names,ap->name,strlen(ap->name)+1);
	if (strcmp(ap->name,ap->instance_name) != 0) list_add(names,ap->instance_name,strlen(ap->instance_name)+1);
	list_add(names,ap->mqtt_config.clientid,strlen(ap->mqtt_config.clientid)+1);
	p = json_get_string(ap->info,"device_model");
	dprintf(1,"p: %s\n", p);
	if (p && strlen(p)) list_add(names,p,strlen(p)+1);

	/* Sub to all messages for the instance */
	list_reset(names);
	log_write(LOG_INFO,"Subscribing to the following topics:\n");
	while((p = list_get_next(names)) != 0) {
		agent_sub(ap,p,SOLARD_FUNC_CONFIG,"+","+");
		agent_sub(ap,p,SOLARD_FUNC_CONTROL,"Set","+");
	}

#ifndef __WIN32
	/* Set the rw toggle handler */
//	signal(SIGUSR1, usr_handler);
//	signal(SIGUSR2, usr_handler);
#endif

	dprintf(1,"returning: %p\n",ap);
	return ap;
agent_init_error:
	free(ap);
	return 0;
}

int agent_run(solard_agent_t *ap) {
	solard_message_t *msg;
	int read_status;
	uint32_t mem_start;
	time_t last_read,last_write,cur,diff;

	dprintf(1,"ap: %p\n", ap);

//	pthread_mutex_init(&ap->lock, 0);

	last_read = last_write = 0;
	mem_start = mem_used();
	read_status = 0;
	solard_set_state(ap,SOLARD_AGENT_RUNNING);
	while(solard_check_state(ap,SOLARD_AGENT_RUNNING)) {
		/* Call read func */
		time(&cur);
		diff = cur - last_read;
		dprintf(5,"diff: %d, read_interval: %d\n", (int)diff, ap->read_interval);
		if (suspend_read == 0 && diff >= ap->read_interval) {
			dprintf(5,"reading...\n");
			read_status = ap->role->read(ap->role_handle,0,0);
			time(&last_read);
			dprintf(5,"used: %ld\n", mem_used() - mem_start);
		}

		/* Process messages */
		list_reset(ap->mq);
		while((msg = list_get_next(ap->mq)) != 0) {
			agent_process(ap,msg);
			list_delete(ap->mq,msg);
		}

		/* Skip rest if failed to read */
		if (read_status != 0) {
			sleep(1);
			continue;
		}

		/* Call cb */
		if (ap->cb) ap->cb(ap);

		/* Call write func */
		time(&cur);
		diff = cur - last_write;
		dprintf(5,"diff: %d, write_interval: %d\n", (int)diff, ap->write_interval);
		if (read_status == 0 && suspend_write == 0 && diff >= ap->write_interval) {
			dprintf(5,"writing...\n");
			ap->role->write(ap->role_handle,0,0);
			time(&last_write);
			dprintf(5,"used: %ld\n", mem_used() - mem_start);
		}
		sleep(1);
	}
	mqtt_disconnect(ap->m,10);
	mqtt_destroy(ap->m);
	return 0;
}
