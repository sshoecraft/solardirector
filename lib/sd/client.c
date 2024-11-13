#ifdef MQTT
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 3
#include "debug.h"

#include "client.h"
#include <libgen.h>
#include <pthread.h>

#ifdef JS
#include "jsobj.h"
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

static list clients = 0;

#ifdef JS
struct js_client_rootinfo {
	JSContext *cx;
	void *vp;
	char *name;
};
#endif

client_agentinfo_t *client_getagentbyname(solard_client_t *c, char *name) {
	client_agentinfo_t *info;

	dprintf(dlevel+1,"name: %s\n", name);

	list_reset(c->agents);
	while((info = list_get_next(c->agents)) != 0) {
		dprintf(dlevel,"info->name: %s\n", info->name);
		if (strcmp(info->name,name) == 0) {
			dprintf(dlevel+1,"found\n");
			return info;
		}
	}
	dprintf(dlevel+1,"NOT found\n");
	return 0;
}

client_agentinfo_t *client_getagentbyid(solard_client_t *c, char *id) {
	client_agentinfo_t *info;

	dprintf(dlevel+1,"id: %s\n", id);

	list_reset(c->agents);
	while((info = list_get_next(c->agents)) != 0) {
		dprintf(dlevel,"info->id: %s\n", info->id);
		if (strcmp(info->id,id) == 0) {
			dprintf(dlevel+1,"found\n");
			return info;
		}
	}
	dprintf(dlevel+1,"NOT found\n");
	return 0;
}

client_agentinfo_t *client_getagentbyrole(solard_client_t *c, char *role) {
	client_agentinfo_t *info;

	dprintf(dlevel+1,"role: %s\n", role);

	list_reset(c->agents);
	while((info = list_get_next(c->agents)) != 0) {
		dprintf(dlevel+1,"info->role: %s\n", info->role);
		if (strcmp(info->role,role) == 0) {
			dprintf(dlevel+1,"found\n");
			return info;
		}
	}
	dprintf(dlevel+1,"NOT found\n");
	return 0;
}

char *client_getagentrole(client_agentinfo_t *info) {
	register char *p;

	dprintf(dlevel+1,"name: %s\n", info->name);

	if (*info->role) p = info->role;
	else if (info->info) p = json_object_dotget_string(json_value_object(info->info),"agent_role");
	else p = 0;
	dprintf(dlevel+1,"role: %s\n", p);
	return p;
}

int client_matchagent(client_agentinfo_t *info, char *target, bool exact) {
	char temp[SOLARD_ROLE_LEN+SOLARD_NAME_LEN+2];
	char role[SOLARD_ROLE_LEN];
	char name[SOLARD_NAME_LEN];
	register char *p;

	dprintf(dlevel,"name: %s, target: %s, exact: %d\n", info->name, target, exact);

	if (strcasecmp(target,"all") == 0) return 1;

	if (exact) return (strcmp(info->name, target) == 0);

	strncpy(temp,target,sizeof(temp)-1);
	*role = *name = 0;
	p = strchr(temp,'/');
	if (p) {
		*p = 0;
		strncpy(role,temp,sizeof(role)-1);
		dprintf(dlevel+2,"role: %s, info->role: %s\n", role, info->role);
		if (*role && *info->role && strcmp(info->role,role) != 0) {
			dprintf(dlevel+1,"role NOT matched\n");
			return 0;
		}
		strncpy(name,p+1,sizeof(name)-1);
	} else {
		strncpy(name,temp,sizeof(name)-1);
	}
	dprintf(dlevel+1,"name: %s, info->name: %s\n", name, info->name);
	if (*name && *info->name && strcmp(info->name,name) == 0) {
		dprintf(dlevel+1,"name matched\n");
		return 1;
	}

	dprintf(dlevel+1,"checking aliases...\n");
	list_reset(info->aliases);
	while((p = list_get_next(info->aliases)) != 0) {
		dprintf(dlevel+1,"alias: %s\n", p);
		if (strcmp(p,name) == 0) {
			dprintf(dlevel+1,"alias found\n");
			return 1;
		}
	}
	dprintf(dlevel+1,"alias NOT found\n");
	return 0;
}

#ifdef MQTT
static void parse_funcs(client_agentinfo_t *info) {
	json_object_t *o;
	json_array_t *a;
	config_function_t newfunc;
	list funcs;
	char *p;
	int i;

	/* Functions is an array of objects */
	/* each object has name and args */
	a = json_object_get_array(json_value_object(info->info), "functions");
	dprintf(dlevel+2,"a: %p\n", a);
	if (!a) return;
	funcs = config_get_funcs(info->cp);
	for(i=0; i < a->count; i++) {
		if (json_value_get_type(a->items[i]) != JSON_TYPE_OBJECT) continue;
		o = json_value_object(a->items[i]);
		p = json_object_get_string(o,"name");
		newfunc.name = strdup(p);
		if (!newfunc.name) {
			log_syserror("parse_funcs: strdup");
			return;
		}
		newfunc.flags = CONFIG_FUNCTION_FLAG_ALLOCNAME;
		newfunc.nargs = json_object_get_number(o,"nargs");
		dprintf(dlevel+2,"adding: %s\n", newfunc.name);
		list_add(funcs,&newfunc,sizeof(newfunc));
	}
}

static void add_alias(client_agentinfo_t *info, char *name) {
	register char *p;

	if (!name) return;
	dprintf(dlevel+2,"name: %s\n", name);

	list_reset(info->aliases);
	while((p = list_get_next(info->aliases)) != 0) {
		if (strcmp(p,name)==0) {
			dprintf(dlevel+2,"NOT adding\n");
			return;
		}
	}
	dprintf(dlevel+2,"adding: %s\n",name);
	list_add(info->aliases,name,strlen(name)+1);
}

static int process_info(client_agentinfo_t *info, char *data) {
	register char *p;

	/* parse the info */
//	printf("data: %s\n", data);
	if (info->info) json_destroy_value(info->info);
	info->info = json_parse(data);
	if (info->info) {
		if (json_value_get_type(info->info) != JSON_TYPE_OBJECT) {
			log_error("invalid info data from: %s\n", info->name);
			return 1;
		}
		if (!strlen(info->role)) {
			p = json_object_get_string(json_value_object(info->info),"agent_role");
			if (p) strncpy(info->role,p,sizeof(info->role)-1);
			sprintf(info->target,"%s/%s",info->role,info->name);
		}
		p = json_object_get_string(json_value_object(info->info),"device_aliases");
		dprintf(dlevel,"aliasses: %p\n", info->aliases);
		if (p) conv_type(DATA_TYPE_STRING_LIST,info->aliases,0,DATA_TYPE_STRING,p,strlen(p));
		add_alias(info,json_object_get_string(json_value_object(info->info),"agent_name"));
		add_alias(info,json_object_get_string(json_value_object(info->info),"agent_role"));
		add_alias(info,json_object_get_string(json_value_object(info->info),"device_model"));
#if 0
		/* XXX could be stale - cant depend on this */
		/* If the AP doesnt have the id, grab it from info */
		if (!strlen(info->id)) {
			str = json_object_get_string(json_value_object(info->info),"agent_id");
			if (str) strncpy(info->id,str,sizeof(info->id)-1);
		}
#endif
		/* Parse the funcs */
		parse_funcs(info);
	} else {
		log_error("unable to parse info from: %s\n", info->name);
		return 1;
	}

	return 0;
}

static int process_config(client_agentinfo_t *info, char *data) {
	json_value_t *v;

	dprintf(dlevel+2,"config data: %s\n", data);
	v = json_parse(data);
	dprintf(dlevel,"v: %p\n", v);
	if (v && json_value_get_type(v) == JSON_TYPE_OBJECT) config_parse_json(info->cp,v);
	json_destroy_value(v);

	return 0;
}

#if 0
static int process_data(client_agentinfo_t *info, char *data) {
	json_value_t *v;

	v = json_parse(data);
	if (v) {
		if (info->private) json_destroy_value((json_value_t *)info->private);
		info->private = v;
	}
	return 0;
}
#endif

static int process_event(solard_client_t *c, client_agentinfo_t *info, char *data) {
	json_value_t *v;
	event_info_t einfo;

	dprintf(dlevel,"event data: %s\n", data);
	v = json_parse(data);
	dprintf(dlevel,"v: %p\n", v);
	if (v) {
		if (json_value_get_type(v) == JSON_TYPE_OBJECT) {
			json_proctab_t tab[] = {
//				{ "name",DATA_TYPE_STRING,&einfo.name,sizeof(einfo.name)-1,0 },
				{ "module",DATA_TYPE_STRING,&einfo.module,sizeof(einfo.module)-1,0 },
				{ "action",DATA_TYPE_STRING,&einfo.action,sizeof(einfo.action)-1,0 },
                		JSON_PROCTAB_END
			};

			dprintf(dlevel,"processing event...\n");
			json_to_tab(tab,v);
			strncpy(einfo.name, info->name, sizeof(einfo.name)-1);
			dprintf(dlevel,"name: %s, module: %s, action: %s\n", einfo.name, einfo.module, einfo.action);
			event(c->e, einfo.name, einfo.module, einfo.action);
//			list_add(c->eq,&einfo,sizeof(einfo));
		}
	}
	json_destroy_value(v);

	return 0;
}

void agentinfo_dump(client_agentinfo_t *info, int level) {
	char aliases[128];
	char funcs[1024];
	list cpfuncs;
	int r,n;
#define SFORMAT "%15.15s: %s\n"
#define UFORMAT "%15.15s: %04x\n"
#define IFORMAT "%15.15s: %d\n"

	dprintf(level,SFORMAT,"id",info->id);
	dprintf(level,SFORMAT,"role",info->role);
	dprintf(level,SFORMAT,"name",info->name);
	dprintf(level,SFORMAT,"target",info->target);
	dprintf(level,SFORMAT,"online",check_state(info,SOLARD_AGENT_STATE_ONLINE) ? "yes" : "no");
	if (list_count(info->aliases)) {
		dprintf(dlevel,"aliases: %p\n", info->aliases)
		conv_type(DATA_TYPE_STRING,aliases,sizeof(aliases),DATA_TYPE_STRING_LIST,info->aliases,list_count(info->aliases));
		dprintf(level,SFORMAT,"aliases",aliases);
	}
	cpfuncs = config_get_funcs(info->cp);
	if (list_count(cpfuncs)) {
	//	conv_type(DATA_TYPE_STRING,line,sizeof(line),DATA_TYPE_STRING_LIST,funcs,list_count(funcs));
		config_function_t *f;
		int i;
		register char *p;

		i = 0;
		p = funcs;
		r = sizeof(funcs);
		list_reset(cpfuncs);
		while((f = list_get_next(cpfuncs)) != 0) {
			if (!f->name) continue;
			if (i++) p += sprintf(p,", ");
//			dprintf(dlevel,"p: %p, funcs: %p, r: %d\n", p, funcs, r);
			n = snprintf(p,r,"%s(%d)",f->name,f->nargs);
			p += n;
			r -= n;
			if (r <= 0) break;
		}
		dprintf(level,SFORMAT,"functions",funcs);
	}
	dprintf(level,UFORMAT,"state",info->state);
	if (check_state(info,CLIENT_AGENTINFO_STATUS)) {
		dprintf(level,IFORMAT,"status",info->status);
		dprintf(level,SFORMAT,"errmsg",info->errmsg);
	}
}

static void _fillinfo(client_agentinfo_t *info, solard_message_t *msg) {
	char *p;

	if (strlen(msg->replyto) && (!strlen(info->id) || strcmp(msg->func,SOLARD_FUNC_CONFIG) == 0))
		strncpy(info->id,msg->replyto,sizeof(info->id)-1);
	if (msg->type == SOLARD_MESSAGE_TYPE_CLIENT) return;
	strncpy(info->target,msg->id,sizeof(info->target)-1);
	dprintf(dlevel,"info->target: %s\n", info->target);
	p = strchr(info->target,'/');
	if (p) {
		*p = 0;
		strncpy(info->role,info->target,sizeof(info->role)-1);
		strncpy(info->name,p+1,sizeof(info->name)-1);
		sprintf(info->target,"%s/%s",info->role,info->name);
	} else {
		strncpy(info->name,info->target,sizeof(info->name)-1);
	}
	if (*info->name && !*info->instance_name) strcpy(info->instance_name,info->name);
}

static client_agentinfo_t *agentinfo_new(solard_client_t *c, solard_message_t *msg) {
	solard_agent_t *ap;
	config_property_t *props;

	dprintf(dlevel,"agentinfo_new: topic: %s, id: %s, len: %d, replyto: %s\n", msg->topic, msg->id, (int)strlen(msg->data), msg->replyto);

	if (strcmp(basename(msg->topic),SOLARD_FUNC_STATUS) == 0) return 0;

	dprintf(dlevel,"************* NEWINFO ******************\n");
	ap = malloc(sizeof(*ap));
	if (!ap) return 0;
	dprintf(dlevel,"ap: %p\n", ap);
	memset(ap,0,sizeof(*ap));
	ap->aliases = list_create();
	ap->m = c->m;
	ap->mq = list_create();
	ap->addmq = false;
	_fillinfo(ap,msg);
	ap->cp = config_init(ap->name,0,0);
	props = agent_get_props(ap);
	config_add_props(ap->cp, "agent", props, CONFIG_FLAG_PRIVATE);
	if (props) free(props);
//	agentinfo_dump(ap,0);

	dprintf(dlevel,"adding ap: id: %s, role: %s, name: %s\n", ap->id, ap->role, ap->name);
	list_add(c->agents,ap,0);
	dprintf(dlevel,"new agent count: %d\n", list_count(c->agents));
	return ap;
}

static void client_getmsg(void *ctx, char *topic, char *message, int msglen, char *replyto) {
	solard_client_t *c = ctx;
	solard_message_t newmsg;
	client_agentinfo_t *info;

	if (solard_getmsg(&newmsg,topic,message,msglen,replyto)) return;
	dprintf(dlevel,"newmsg: topic: %s, id: %s, len: %d, replyto: %s\n", newmsg.topic, newmsg.id, (int)strlen(newmsg.data), newmsg.replyto);
	/* If it's a client message, it's a reply */
	if (newmsg.type == SOLARD_MESSAGE_TYPE_CLIENT) {
		info = client_getagentbyid(c,newmsg.replyto);
		if (!info) info = agentinfo_new(c,&newmsg);
		if (info) {
			agentinfo_dump(info,dlevel+1);
			dprintf(dlevel,"c->addmq: %d, info->addmq: %d\n", c->addmq, info->addmq);
			if (c->addmq || info->addmq) list_add(info->mq,&newmsg,sizeof(newmsg));
			if (c->cb) c->cb(info,&newmsg);
		}
		return;
	}
	info = 0;
	if (strlen(newmsg.replyto)) info = client_getagentbyid(c,newmsg.replyto);
	dprintf(dlevel,"info: %p\n", info);
	if (!info) info = client_getagentbyname(c,newmsg.id);
	dprintf(dlevel,"info: %p\n", info);
	if (!info) info = agentinfo_new(c,&newmsg);
	dprintf(dlevel,"info: %p\n", info);
	if (!info) return;
	_fillinfo(info,&newmsg);
	if (strcmp(newmsg.func,SOLARD_FUNC_STATUS)==0) {
		if (strcmp(newmsg.data,"Online")==0) set_state(info,SOLARD_AGENT_STATE_ONLINE);
		else clear_state(info,SOLARD_AGENT_STATE_ONLINE);
	} else if (strcmp(newmsg.func,SOLARD_FUNC_INFO)==0) {
		process_info(info,newmsg.data);
	} else if (strcmp(newmsg.func,SOLARD_FUNC_CONFIG)==0) {
		process_config(info,newmsg.data);
#if 0
	} else if (strcmp(newmsg.func,SOLARD_FUNC_DATA)==0) {
		process_data(info,newmsg.data);
#endif
	} else if (strcmp(newmsg.func,SOLARD_FUNC_EVENT)==0) {
		process_event(c,info,newmsg.data);
	} else {
		/* We did not handle the message */
		dprintf(dlevel,"c->addmq: %d, info->addmq: %d\n", c->addmq, info->addmq);
		if (c->addmq || info->addmq) list_add(info->mq,&newmsg,sizeof(newmsg));
		if (c->cb) c->cb(info,&newmsg);
	}
//	agent_dump(info,1);
	return;
}
#endif

config_function_t *client_getagentfunc(client_agentinfo_t *info, char *name) {
	config_function_t *f;
	list funcs;

	dprintf(dlevel,"name: %s\n", name);

	funcs = config_get_funcs(info->cp);
	list_reset(funcs);
	while((f = list_get_next(funcs)) != 0) {
		dprintf(dlevel,"f->name: %s\n", f->name);
		if (strcmp(f->name,name)==0) {
			dprintf(dlevel,"found!\n");
			return f;
		}
	}
	dprintf(dlevel,"NOT found!\n");
	return 0;
}

static json_value_t *client_mkargs(int nargs,char **args,config_function_t *f) {
	json_object_t *o;
	json_array_t *a;
	int i;

	/* nargs = 1: Object with function name and array of arguments */
	/* { "func": [ "arg", "argn", ... ] } */
	/* nargs > 1: Object with function name and array of argument arrays */
	/* { "func": [ [ "arg", "argn", ... ], [ "arg", "argn" ], ... ] } */
	o = json_create_object();
	a = json_create_array();
	dprintf(dlevel,"f->nargs: %d\n", f->nargs);
	if (f->nargs == 1) {
		for(i=0; i < nargs; i++) json_array_add_string(a,args[i]);
	} else if (f->nargs > 1) {
		json_array_t *aa;
		register int j;

		i = 0;
		while(i < nargs) {
			aa = json_create_array();
			for(j=0; j < f->nargs; j++) json_array_add_string(aa,args[i++]);
			json_array_add_array(a,aa);
		}
	}
	json_object_set_array(o,f->name,a);
	return json_object_value(o);
}

int client_callagentfunc(client_agentinfo_t *info, config_function_t *f, int nargs, char **args) {
#ifdef MQTT
	char topic[SOLARD_TOPIC_LEN],*p;
#endif
	json_value_t *v;
	char *j;

	dprintf(dlevel,"f: %p\n", f);
	if (!f) return 1;

	dprintf(dlevel,"nargs: %d, f->nargs: %d\n", nargs, f->nargs);

	/* Args passed must be divisible by # of args the function takes */
	if (f->nargs && (nargs % f->nargs) != 0) {
		set_state(info,CLIENT_AGENTINFO_STATUS);
		info->status = 1;
		sprintf(info->errmsg, "%s: function %s requires %d arguments but %d given",
			info->target, f->name, f->nargs, nargs);
		return 1;
	}

	/* create the args according to function */
	v = client_mkargs(nargs,args,f);
	j = json_dumps(v,0);
	if (!j) {
		set_state(info,CLIENT_AGENTINFO_STATUS);
		info->status = 1;
		sprintf(info->errmsg, "solard_callagentfunc: memory allocation error: %s",strerror(errno));
		return 1;
	}
	json_destroy_value(v);

#ifdef MQTT
	/* send the req */
	p = topic;
	p += sprintf(p,"%s/%s",SOLARD_TOPIC_ROOT,SOLARD_TOPIC_AGENTS);
//	if (strlen(info->role)) p += sprintf(p,"/%s", info->role);
	p += sprintf(p,"/%s",info->name);
	dprintf(dlevel,"m: %p\n",info->m);
//	if (mqtt_pub(info->c->m,topic,j,1,0)) {
	if (mqtt_pub(info->m,topic,j,1,0)) {
		free(j);
		log_error("mqtt_pub");
		return 1;
	}
	dprintf(dlevel,"done!\n");
#endif
	free(j);
	set_state(info,CLIENT_AGENTINFO_REPLY);
	clear_state(info,CLIENT_AGENTINFO_STATUS);
	return 0;
}

int client_callagentfuncbyname(client_agentinfo_t *info, char *name, int nargs, char **args) {
	config_function_t *f;

	f = client_getagentfunc(info,name);
	if (!f) return 1;
	return client_callagentfunc(info, f, nargs, args);
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

	if (c->e) event(c->e, c->name, module, action);

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

static int client_startup(solard_client_t *c, char *configfile, char *mqtt_info, char *influx_info,
		config_property_t *prog_props, config_function_t *prog_funcs) {
	config_property_t client_props[] = {
		{ "name", DATA_TYPE_STRING, c->name, sizeof(c->name)-1, 0 },
#ifdef JS
		{ "rtsize", DATA_TYPE_INT, &c->rtsize, 0, 0, 0 },
		{ "stacksize", DATA_TYPE_INT, &c->stacksize, 0, 0, 0 },
		{ "script_dir", DATA_TYPE_STRING, c->script_dir, sizeof(c->script_dir)-1, 0 },
		{ "init_script", DATA_TYPE_STRING, c->init_script, sizeof(c->init_script)-1, "init.js", 0 },
		{ "start_script", DATA_TYPE_STRING, c->start_script, sizeof(c->start_script)-1, "start.js", 0 },
		{ "stop_script", DATA_TYPE_STRING, c->stop_script, sizeof(c->stop_script)-1, "stop.js", 0 },
#endif
		{0}
	};
	config_property_t *props;
	config_function_t *funcs;
	config_function_t client_funcs[] = {
		{ 0 }
	};
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
	funcs = config_combine_funcs(prog_funcs,client_funcs);

	eptr = (c->flags & AGENT_FLAG_NOEVENT ? 0 : &c->e);
#ifdef MQTT
	mptr = (c->flags & AGENT_FLAG_NOMQTT ? 0 : &c->m);
#endif
#ifdef INFLUX
	iptr = (c->flags & AGENT_FLAG_NOINFLUX ? 0 : &c->i);
#endif
#ifdef JS
	jptr = (c->flags & AGENT_FLAG_NOJS ? 0 : &c->js);
#endif

        /* Call common startup */
	if (solard_common_startup(&c->cp, c->section_name, configfile, props, funcs, eptr
#ifdef MQTT
		,mptr, 0, client_getmsg, c, mqtt_info, c->config_from_mqtt
#endif
#ifdef INFLUX
		,iptr, influx_info
#endif
#ifdef JS
		,jptr, c->rtsize, c->stacksize, (js_outputfunc_t *)log_info
#endif
	)) return 1;
        if (props) free(props);
        if (funcs) free(funcs);

#ifdef MQTT
	if (0) {
		char my_mqtt_info[256];
		mqtt_get_config(my_mqtt_info,sizeof(my_mqtt_info)-1,c->m,0);
		dprintf(0,"my_mqtt_info: %s\n", my_mqtt_info);
		exit(0);
	}
#endif
#ifdef JS
	dprintf(dlevel,"nojs: %d\n", check_bit(c->flags, CLIENT_FLAG_NOJS));
	if (!check_bit(c->flags, CLIENT_FLAG_NOJS)) {
#if 0
		/* Add JS init funcs (define classes) */
		JS_EngineAddInitClass(c->js, "client_jsinit", js_InitClientClass);
		JS_EngineAddInitClass(c->js, "config_jsinit", js_InitConfigClass);
		JS_EngineAddInitClass(c->js, "mqtt_jsinit", js_InitMQTTClass);
		JS_EngineAddInitClass(c->js, "influx_jsinit", js_InitInfluxClass);
#endif

		/* Set script_dir if empty */
		dprintf(dlevel,"script_dir(%d): %s\n", strlen(c->script_dir), c->script_dir);
		if (!strlen(c->script_dir)) {
			dprintf(dlevel,"name: %s\n", c->name);
			sprintf(c->script_dir,"%s/clients/%s",SOLARD_LIBDIR,c->name);
			fixpath(c->script_dir,sizeof(c->script_dir));
			dprintf(dlevel,"NEW script_dir(%d): %s\n", strlen(c->script_dir), c->script_dir);
		}
	}
#endif

	config_add_props(c->cp, "client", client_props, CONFIG_FLAG_NOSAVE | CONFIG_FLAG_NOID);
	return 0;
}

solard_client_t *client_init(int argc,char **argv,char *version,opt_proctab_t *client_opts,char *Cname,int flags,
			config_property_t *props,config_function_t *funcs,client_callback_t *initcb, void *ctx) {
	solard_client_t *c;
	char mqtt_info[200],influx_info[200];
	char configfile[256];
	char name[64],sname[CFG_SECTION_NAME_SIZE];
#ifdef MQTT
	int config_from_mqtt;
	char sub_topic[200];
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
#endif
		{ "-c:%|config file",&configfile,DATA_TYPE_STRING,sizeof(configfile)-1,0,"" },
		{ "-s::|config section name",&sname,DATA_TYPE_STRING,sizeof(sname)-1,0,"" },
		{ "-n::|client name",&name,DATA_TYPE_STRING,sizeof(name)-1,0,"" },
#ifdef INFLUX
		{ "-i::|influx host[,user[,pass]]",&influx_info,DATA_TYPE_STRING,sizeof(influx_info)-1,0,"" },
#endif
#ifdef JS
		{ "-e:%|exectute javascript",&jsexec,DATA_TYPE_STRING,sizeof(jsexec)-1,0,"" },
                { "-U:#|javascript runtime size",&rtsize,DATA_TYPE_INT,0,0,"1M" },
                { "-K:#|javascript stack size",&stksize,DATA_TYPE_INT,0,0,"64K" },
		{ "-X::|execute JS script and exit",&script,DATA_TYPE_STRING,sizeof(script)-1,0,"" },
#endif
		OPTS_END
	}, *opts;
	int logopts = LOG_INFO|LOG_WARNING|LOG_ERROR|LOG_SYSERR|_ST_DEBUG;

	dprintf(dlevel,"init'ing\n");
	opts = opt_combine_opts(std_opts,client_opts);
	dprintf(dlevel,"opts: %p\n", opts);
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
	dprintf(dlevel,"argv: %p\n", argv);
	if (!Cname && argv) {
		dprintf(dlevel,"NEW Cname: %s\n", Cname);
		Cname = argv[0];
	}
	if (argv) {
		if (solard_common_init(argc,argv,version,opts,logopts))
			return 0;
	} else {
		*mqtt_info = *configfile = *sname = *name = *influx_info = 0;
		config_from_mqtt = 0;
#ifdef JS
		*jsexec = *script = 0;
		rtsize = stksize = 0;
#endif
	}
	if (opts) free(opts);

	dprintf(dlevel,"creating session...\n");
	c = calloc(1,sizeof(*c));
	if (!c) {
		log_syserror("client_init: malloc(%d)",sizeof(*c));
		return 0;
	}
	dprintf(dlevel,"name: %s\n", name);
	if (strlen(name)) {
		strncpy(c->name,name,sizeof(c->name)-1);
	} else if (Cname) {
		strncpy(c->name,Cname,sizeof(c->name)-1);
	} else {
		strncpy(c->name,"client",sizeof(c->name)-1);
	}
	dprintf(dlevel,"c->name: %s\n", c->name);
	dprintf(dlevel,"sname: %s\n", sname);
	if (strlen(sname)) strncpy(c->section_name,sname,sizeof(c->section_name)-1);
	else strncpy(c->section_name,c->name,sizeof(c->section_name)-1);
	dprintf(dlevel,"c->section_name: %s\n", c->section_name);
#ifdef MQTT
	c->config_from_mqtt = config_from_mqtt;
	c->mq = list_create();
#endif
	c->flags = flags;
#ifdef JS
	c->rtsize = rtsize;
	c->stacksize = stksize;
	c->roots = list_create();
#endif
	c->agents = list_create();
	c->addmq = false;			/* XXX dont add messages by default */

	/* Auto generate configfile if not specified */
	if (!strlen(configfile)) {
		char *types[] = { "json", "conf", 0 };
		char path[300];
		int j,f;

		f = 0;
		for(j=0; types[j]; j++) {
			sprintf(path,"%s/%s.%s",SOLARD_ETCDIR,c->name,types[j]);
			dprintf(dlevel,"trying: %s\n", path);
			if (access(path,0) == 0) {
				f = 1;
				break;
			}
		}
		if (f) strcpy(configfile,path);
	} else if (strcmp(configfile,"none") == 0) {
		configfile[0] = 0;
	}
        dprintf(dlevel,"configfile: %s\n", configfile);

	if (client_startup(c,configfile,mqtt_info,influx_info,props,funcs)) goto client_init_error;

	/* Call initcb if specified */
	dprintf(dlevel,"initcb: %p\n",initcb);
	if (initcb && initcb(ctx,c)) goto client_init_error;

#ifdef JS
	/* Execute any command-line javascript code */
	dprintf(dlevel,"jsexec(%d): %s\n", strlen(jsexec), jsexec);
	if (strlen(jsexec)) {
		if (JS_EngineExecString(c->js, jsexec))
			log_warning("error executing js expression\n");
	}

	/* Start the init script */
	snprintf(jsexec,sizeof(jsexec)-1,"%s/%s",c->script_dir,c->init_script);
	if (access(jsexec,0)) JS_EngineExec(c->js,jsexec,0,0,0,0);

	/* if specified, Execute javascript file then exit */
	dprintf(dlevel,"script: %s\n", script);
	if (strlen(script)) {
		JS_EngineExec(c->js,script,0,0,0,0);
		exit(0);
	}
#endif

#ifdef MQTT
	/* Sub to the agents */
	sprintf(sub_topic,"%s/%s/+/#",SOLARD_TOPIC_ROOT,SOLARD_TOPIC_AGENTS);
	dprintf(dlevel,"subscribing to: %s\n", sub_topic);
	mqtt_sub(c->m,sub_topic);

	/* Sleep a moment to ingest any messages */
	usleep(50000);
#endif

	if (!clients) clients = list_create();
	list_add(clients,c,0);

	dprintf(dlevel,"returning: %p\n", c);
	return c;
client_init_error:
	free(c);
	return 0;
}

void client_destroy_client(solard_client_t *c) {

	dprintf(dlevel,"c: %p\n", c);
	if (!c) return;

#if 0
	if (c->agents) {
		solard_agent_t *ap;

		list_reset(c->agents);
		while((ap = list_get_next(c->agents)) != 0) {
			dprintf(0,"ap: %p\n", ap);
			agent_destroy_agent(ap);
		}
		list_destroy(c->agents);
	}
#endif

	dprintf(dlevel,"cp: %p\n", c->cp);
	if (c->cp) {
		config_function_t *f;

		dprintf(dlevel,"count: %d\n", list_count(c->cp->funcs));
		list_reset(c->cp->funcs);
		while((f = list_get_next(c->cp->funcs)) != 0) {
			dprintf(0,"name: %s, flags: %x\n", f->name, f->flags);
			if (f->flags & CONFIG_FUNCTION_FLAG_ALLOCNAME)
				free(f->name);
		}
		config_destroy_config(c->cp);
	}
#ifdef MQTT
	if (c->m) mqtt_destroy_session(c->m);
	list_destroy(c->mq);
#endif
#ifdef INFLUX
	if (c->i) influx_destroy_session(c->i);
#endif
#ifdef JS
	if (c->props) free(c->props);
//	if (c->funcs) free(c->funcs);
	if (c->roots) {
		struct js_agent_rootinfo *ri;

		list_reset(c->roots);
		while((ri = list_get_next(c->roots)) != 0) {
			dprintf(dlevel,"removing root: %s\n", ri->name);
			JS_RemoveRoot(ri->cx,ri->vp);
			JS_free(ri->cx,ri->name);
		}
		list_destroy(c->roots);
        }
	if (c->js) JS_EngineDestroy(c->js);
#endif
	if (c->e) event_destroy(c->e);
	if (clients) list_delete(clients,c);
	free(c);
}

void client_shutdown(void) {
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
	config_shutdown();
	mqtt_shutdown();
#ifdef INFLUX
	influx_shutdown();
#endif
	common_shutdown();
}

#ifdef JS
enum CLIENT_AGENTINFO_ID {
	CLIENT_AGENTINFO_ID_NAME=1,
};

enum CLIENT_PROPERTY_ID {
	CLIENT_PROPERTY_ID_CONFIG=1024,
	CLIENT_PROPERTY_ID_MQTT,
	CLIENT_PROPERTY_ID_INFLUX,
	CLIENT_PROPERTY_ID_AGENTS,
	CLIENT_PROPERTY_ID_MSG,
	CLIENT_PROPERTY_ID_ADDMQ,
};

static JSObject *js_create_agents_array(JSContext *cx, JSObject *parent, list l) {
	JSObject *rows,*obj;
	jsval val;
	solard_agent_t *ap;
	int i,c;

	dprintf(dlevel,"l: %p\n", l);

	c = (l ? list_count(l) : 0);
	dprintf(dlevel,"count: %d\n", c);
	rows = JS_NewArrayObject(cx, c, NULL);
	dprintf(dlevel,"rows: %p\n", rows);
	i = 0;
	list_reset(l);
	while((ap = list_get_next(l)) != 0) {
		obj = js_agent_new(cx,rows,ap);
		dprintf(dlevel,"obj: %p\n", obj);
		if (!obj) continue;
		val = OBJECT_TO_JSVAL(obj);
		JS_SetElement(cx, rows, i++, &val);
	}
	return rows;
}

static JSBool js_client_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	solard_client_t *c;
	config_property_t *p;

	c = JS_GetPrivate(cx,obj);
	dprintf(dlevel,"c: %p\n", c);
	if (!c) {
		JS_ReportError(cx, "js_client_getprop: private is null!");
		return JS_FALSE;
	}
	p = 0;
	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
//		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case CLIENT_PROPERTY_ID_CONFIG:
			*rval = c->config_val;
			break;
		case CLIENT_PROPERTY_ID_MQTT:
			*rval = c->mqtt_val;
			break;
		case CLIENT_PROPERTY_ID_INFLUX:
			*rval = c->influx_val;
			break;
		case CLIENT_PROPERTY_ID_AGENTS:
			*rval = OBJECT_TO_JSVAL(js_create_agents_array(cx,obj,c->agents));
			break;
		case CLIENT_PROPERTY_ID_MSG:
			*rval = OBJECT_TO_JSVAL(js_create_messages_array(cx,obj,c->mq));
			break;
		case CLIENT_PROPERTY_ID_ADDMQ:
			*rval = type_to_jsval(cx,DATA_TYPE_BOOL,&c->addmq,0);
			break;
		default:
			p = CONFIG_GETMAP(c->cp,prop_id);
			if (!p) p = config_get_propbyid(c->cp,prop_id);
			if (!p) {
				JS_ReportError(cx, "internal error: property %d not found", prop_id);
				return JS_FALSE;
			}
			break;
		}
	} else if (JSVAL_IS_STRING(id)) {
		char *sname, *name;
		JSClass *classp = OBJ_GET_CLASS(cx, obj);

		sname = (char *)classp->name;
		name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
//		dprintf(dlevel,"sname: %s, name: %s\n", sname, name);
		if (sname && name) p = config_get_property(c->cp, sname, name);
		if (name) JS_free(cx, name);
	}
//	dprintf(dlevel,"p: %p\n", p);
	if (p && p->dest) {
//		dprintf(dlevel,"p: type: %d(%s), name: %s\n", p->type, typestr(p->type), p->name);
		*rval = type_to_jsval(cx,p->type,p->dest,p->len);
	}
	return JS_TRUE;
}

static JSBool js_client_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	solard_client_t *c;
	int prop_id;

	c = JS_GetPrivate(cx,obj);
	dprintf(dlevel,"c: %p\n", c);
	if (!c) {
		JS_ReportError(cx, "js_client_getprop: private is null!");
		return JS_FALSE;
	}
	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case CLIENT_PROPERTY_ID_ADDMQ:
			jsval_to_type(DATA_TYPE_BOOL,&c->addmq,0,cx,*vp);
			break;
		default:
			return js_config_common_setprop(cx, obj, id, vp, c->cp, c->props);
		}
	} else {
		return js_config_common_setprop(cx, obj, id, vp, c->cp, c->props);
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

static JSBool jsclient_mktopic(JSContext *cx, uintN argc, jsval *vp) {
//	solard_client_t *c;
	char topic[SOLARD_TOPIC_SIZE], *name, *func;
	int topicsz = SOLARD_TOPIC_SIZE;
	JSObject *obj;
	register char *p;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
//	c = JS_GetPrivate(cx, obj);

	if (argc != 1) {
		JS_ReportError(cx,"mktopic requires 1 arguments (func:string)\n");
		return JS_FALSE;
	}

        func = 0;
        if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "s s", &name, &func)) return JS_FALSE;
	dprintf(dlevel,"func: %s\n", func);

//	client_mktopic(topic,sizeof(topic)-1,c,c->instance_name,func);

//	dprintf(dlevel,"name: %s, func: %s\n", name, func);

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
        struct js_client_rootinfo ri;

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

	/* root it */
        ri.name = fname;
        JS_AddNamedRoot(cx,&ctx->func,fname);
        ri.cx = cx;
        ri.vp = &ctx->func;
        list_add(c->roots,&ri,sizeof(ri));

	if (name) JS_free(cx,name);
	if (module) JS_free(cx,action);
	if (action) JS_free(cx,action);
	return JS_TRUE;
}

static int _js_client_ctor_init(void *ctx, solard_client_t *c) {
	dprintf(0,"c: %p\n", c);
	return 0;
}

static JSBool js_client_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	solard_client_t *c;
	char *name;
	JSObject *newobj;

#if 0
	dprintf(dlevel,"argc: %d\n", argc);
	if (argc != 1) {
		JS_ReportError(cx, "client requires 1 argument (name:string)");
		return JS_FALSE;
	}
#endif

	name = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "/ s", &name)) return JS_FALSE;
	dprintf(dlevel,"name: %s\n", name);

	c = client_init(0,0,0,0,name ? name : "client",CLIENT_FLAG_NOJS,0,0,_js_client_ctor_init,0);
	dprintf(dlevel,"c: %p\n", c);
	if (!c) {
		JS_ReportError(cx, "client_init returned null\n");
		return JS_FALSE;
	}

	newobj = js_client_new(cx,JS_GetGlobalObject(cx),c);
	*rval = OBJECT_TO_JSVAL(newobj);

	return JS_TRUE;
}

JSObject *js_InitClientClass(JSContext *cx, JSObject *parent) {
	JSPropertySpec client_props[] = {
		{ "config", CLIENT_PROPERTY_ID_CONFIG, JSPROP_ENUMERATE },
		{ "mqtt", CLIENT_PROPERTY_ID_MQTT, JSPROP_ENUMERATE },
		{ "influx", CLIENT_PROPERTY_ID_INFLUX, JSPROP_ENUMERATE },
		{ "agents", CLIENT_PROPERTY_ID_AGENTS, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "messages", CLIENT_PROPERTY_ID_MSG, JSPROP_ENUMERATE },
		{ "addmq", CLIENT_PROPERTY_ID_ADDMQ, JSPROP_ENUMERATE },
		{ 0 }
	};
	JSFunctionSpec client_funcs[] = {
		JS_FN("mktopic",jsclient_mktopic,1,1,0),
		JS_FS("event",js_client_event,2,2,0),
		JS_FS("event_handler",js_client_event_handler,1,4,0),
		{ 0 }
	};
	JSObject *obj;

	dprintf(2,"creating %s class\n",js_client_class.name);
	obj = JS_InitClass(cx, parent, 0, &js_client_class, js_client_ctor, 1, client_props, client_funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s class", js_client_class.name);
		return 0;
	}

	dprintf(dlevel,"done!\n");
	return obj;
}

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
	if (c->cp) {
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

		/* Create the config object */
		dprintf(dlevel,"Creating config object...\n");
		c->config_val = OBJECT_TO_JSVAL(js_config_new(cx,newobj,c->cp));
		dprintf(dlevel,"config_val: %x\n", c->config_val);
		dprintf(dlevel,"Defining config property...\n");
		JS_DefineProperty(cx, newobj, "config", c->config_val, 0, 0, JSPROP_ENUMERATE);
	}

	dprintf(dlevel,"Setting private to: %p\n", c);
	JS_SetPrivate(cx,newobj,c);

	/* Only used by driver atm */
	dprintf(dlevel,"Creating client_val...\n");
	c->client_val = OBJECT_TO_JSVAL(newobj);
	dprintf(dlevel,"client_val: %x\n", c->client_val);

#ifdef MQTT
	if (c->m) {
		/* Create MQTT child object */
		dprintf(dlevel,"Creating mqtt_val...\n");
		if (c->m) c->mqtt_val = OBJECT_TO_JSVAL(js_mqtt_new(cx,newobj,c->m));
		JS_DefineProperty(cx, newobj, "mqtt", c->mqtt_val, 0, 0, JSPROP_ENUMERATE);
	}
#endif
#ifdef INFLUX
	if (c->i) {
		/* Create Influx child object */
		dprintf(dlevel,"Creating influx_val...\n");
		if (c->i) c->influx_val = OBJECT_TO_JSVAL(js_influx_new(cx,newobj,c->i));
		JS_DefineProperty(cx, newobj, "influx", c->influx_val, 0, 0, JSPROP_ENUMERATE);
	}
#endif

	return newobj;
}
#endif
#endif
