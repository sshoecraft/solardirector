
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "btc.h"
#include "__sd_build.h"

#define TESTING 0
#define TESTLVL 2

char *btc_version_string = STRINGIFY(__SD_BUILD);

static btc_agentinfo_t *get_agent(btc_session_t *s, char *name) {
	btc_agentinfo_t *info;

	list_reset(s->agents);
	while((info = list_get_next(s->agents)) != 0) {
		if (strcmp(info->name, name) == 0)
			return info;
	}
	return 0;
}

static void process_message(btc_session_t *s, solard_message_t *msg) {
	btc_agentinfo_t *info, newinfo;
	bool have_info;

	int ldlevel = dlevel;

	dprintf(ldlevel,"msg: name: %s, func: %s\n", msg->name, msg->func);
	info = get_agent(s, msg->name);
	have_info = info ? true : false;
	dprintf(dlevel,"have_info: %d\n", have_info);
	if (have_info) dprintf(ldlevel,"info->role: %s\n", info->role);
	if (strcmp(msg->func,"Info") == 0 && !have_info) {
		json_value_t *v;
		char *p;

		v = json_parse(msg->data);
		p = json_object_get_string(json_value_object(v), "agent_role");
		dprintf(ldlevel,"p: %p\n", p);
		if (p) {
			dprintf(ldlevel,"%s: agent_role: %s\n", msg->name, p);
			dprintf(ldlevel,"adding: %s\n", msg->name);
			memset(&newinfo,0,sizeof(newinfo));
			strcpy(newinfo.name,msg->name);
			strcpy(newinfo.role,p);
			list_add(s->agents,&newinfo,sizeof(newinfo));
		}
		json_destroy_value(v);
	} else if (strcmp(msg->func,"Data") == 0 && have_info && strcmp(info->role,SOLARD_ROLE_BATTERY) == 0) {
		json_value_t *v;

		dprintf(ldlevel,"getting data for %s\n", msg->name);
		v = json_parse(msg->data);
		dprintf(ldlevel,"v: %p\n", v);
		if (v) {
			char *j = json_dumps(v,4);
			json_destroy_value(v);
			dprintf(ldlevel,"j: %p\n", j);
			if (j) {
				battery_from_json(&info->data,j);
				free(j);
				time(&info->data.last_update);
				info->have_data = true;
			}
		}
	}
}

static void btc_getmsg(void *ctx, char *topic, char *message, int msglen, char *replyto) {
	solard_message_t newmsg;

	if (solard_getmsg(&newmsg,topic,message,msglen,replyto)) return;
	process_message((btc_session_t *)ctx,&newmsg);
}

static int btc_data_source_trigger(void *ctx, config_property_t *p, void *old_value) {
	btc_session_t *s = ctx;
	char value[1024];

	int ldlevel = dlevel;

	conv_type(DATA_TYPE_STRING,value,sizeof(value),p->type,p->dest,p->dsize);
	dprintf(ldlevel,"new value: %s, old_value: %s\n", value, old_value);

	dprintf(ldlevel,"m: %p\n", s->m);
	if (s->m) {
		char *topic;
		list_reset(s->m->subs);
		while((topic = list_get_next(s->m->subs)) != 0) {
			dprintf(ldlevel,"unsub topic: %s\n", topic);
			mqtt_unsub(s->m,topic);
		}
		mqtt_disconnect(s->m,0);
	} else {
		s->m = mqtt_new(false, btc_getmsg, s);
		dprintf(ldlevel,"s->m: %p\n", s->m);
		if (!s->m) {
			sprintf(s->ap->cp->errmsg,"btc_data_source_trigger: internal error: unable to create new mqtt session!");
			log_error(s->ap->cp->errmsg);
			return 1;
		}
	}
	if (strlen(value)) {
		s->m->enabled = true;
		mqtt_parse_config(s->m,value);
		mqtt_newclient(s->m);
		mqtt_connect(s->m,20);
#define TOPIC SOLARD_TOPIC_ROOT"/"SOLARD_TOPIC_AGENTS"/#"
		dprintf(ldlevel,"sub topic: %s\n", TOPIC);
		mqtt_sub(s->m,TOPIC);
	}
	return 0;
}

static int btc_agent_init(int argc, char **argv, opt_proctab_t *opts, btc_session_t *s) {
	config_property_t btc_props[] = {
		/* name, type, dest, dsize, def, flags, scope, values, labels, units, scale, precision, trigger, ctx */
		{ "log_power", DATA_TYPE_BOOL, &s->log_power, 0, "0", 0, "select", "0, 1", "log output_power from each read", 0, 1, 0 },
		{ "data_source", DATA_TYPE_STRING, &s->data_source, sizeof(s->data_source), "localhost", 0, 0, 0, 0, 0, 0, 0, btc_data_source_trigger, s },
		{ "interval", DATA_TYPE_INT, 0, 0, "10" },
		{ 0 }
	};

	s->ap = agent_init(argc,argv,btc_version_string,opts,&btc_driver,s,0,btc_props,0);
	if (!s->ap) return 1;

//	agent_set_callback(s->ap, btc_cb, s);
	return 0;
}

int main(int argc, char **argv) {
	btc_session_t *s;
#if TESTING
        char *args[] = { "btc", "-d", STRINGIFY(TESTLVL), "-c", "test.json" };
        argc = (sizeof(args)/sizeof(char *));
        argv = args;
#endif
	bool norun;
	opt_proctab_t opts[] = {
		{ "-1|dont enter run loop",&norun,DATA_TYPE_BOOL,0,0,"no" },
		OPTS_END
	};

	s = btc_driver.new(0,0);
	if (!s) {
		perror("driver.new");
		return 1;
	}
	s->agents = list_create();

	if (btc_agent_init(argc,argv,opts,s)) return 1;

	if (norun) btc_driver.read(s,0,0,0);
	else agent_run(s->ap);

	btc_driver.destroy(s);
	agent_shutdown();
#ifdef DEBUG_MEM
	log_info("final mem used: %ld\n", mem_used());
#endif

	return 0;
}
