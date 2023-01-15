
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "cellmon.h"

#define TESTING 0

solard_battery_t *find_pack_by_name(cellmon_config_t *conf, char *name) {
	solard_battery_t *pp;

	dprintf(1,"name: %s\n", name);
	list_reset(conf->packs);
	while((pp = list_get_next(conf->packs)) != 0) {
		if (strcmp(pp->name,name) == 0) {
			dprintf(1,"found!\n");
			return pp;
		}
	}
	dprintf(1,"NOT found!\n");
	return 0;
}

int sort_packs(void *i1, void *i2) {
	solard_battery_t *p1 = (solard_battery_t *)i1;
	solard_battery_t *p2 = (solard_battery_t *)i2;
	int val;

	dprintf(7,"p1: %s, p2: %s\n", p1->name, p2->name);
	val = strcmp(p1->name,p2->name);
	if (val < 0)
		val =  -1;
	else if (val > 0)
		val = 1;
	dprintf(7,"returning: %d\n", val);
	return val;

}

void getpack(cellmon_config_t *conf, char *name, char *data) {
	solard_battery_t bat,*pp = &bat;

	dprintf(1,"getting pack: name: %s, data: %p\n", name, data);
	if (battery_from_json(&bat,data)) {
		dprintf(1,"battery_from_json failed\n");
		return;
	}
	dprintf(1,"dumping...\n");
	battery_dump(&bat,1);
	set_state((&bat),BATTERY_STATE_UPDATED);
	time(&bat.last_update);
	if (!strlen(bat.name)) return;

	pp = find_pack_by_name(conf,bat.name);
	dprintf(1,"pp: %p\n", pp);
	if (!pp) {
		dprintf(1,"adding pack...\n");
		list_add(conf->packs,&bat,sizeof(bat));
		dprintf(1,"sorting packs...\n");
		list_sort(conf->packs,sort_packs,0);
	} else {
		dprintf(1,"updating pack...\n");
		memcpy(pp,&bat,sizeof(bat));
	}
	return;
};


int main(int argc,char **argv) {
	cellmon_config_t *conf;
	client_agentinfo_t *info;
	solard_message_t *msg;
	char configfile[256],topic[SOLARD_TOPIC_SIZE];
//	char target[SOLARD_ROLE_LEN+SOLARD_NAME_LEN+2];
//	char role[SOLARD_ROLE_LEN];
//	char name[SOLARD_NAME_LEN];
//	int web_flag;
	int refresh;
	opt_proctab_t opts[] = {
		/* Spec, dest, type len, reqd, default val, have */
		{ "-t::|topic",&topic,DATA_TYPE_STRING,sizeof(topic)-1,0,"" },
		{ "-r:#|refresh rate",&refresh,DATA_TYPE_INT,0,0,"1" },
//		{ "-w|web output",&web_flag,DATA_TYPE_BOOL,0,0,"false" },
		OPTS_END
	};
	register char *p;
	list agents;
#if TESTING
	char *args[] = { "cellmon", "-d", "2", "-c", "cellmon.conf" };
	argc = (sizeof(args)/sizeof(char *));
	argv = args;
#endif

	find_config_file("cellmon.conf",configfile,sizeof(configfile)-1);
	dprintf(1,"configfile: %s\n", configfile);

	conf = malloc(sizeof(*conf));
	if (!conf) return 1;
	memset(conf,0,sizeof(*conf));
	conf->c = client_init(argc,argv,"1.0",opts,"cellmon",CLIENT_FLAG_NOJS,0,0,0);
	if (!conf->c) return 1;
	conf->c->addmq = true;
	conf->packs = list_create();

	agents = conf->c->agents;
#if 0
	sleep(1);
        if (!list_count(agents)) {
                sleep(1);
                if (!list_count(agents)) {
                        log_error("no agents responded, aborting\n");
                        return 1;
                }
        }
#endif

	/* main loop */
	while(1) {
		list_reset(agents);
		while((info = list_get_next(agents)) != 0) {
			p = client_getagentrole(info);
			dprintf(1,"agent: %s, role: %s\n", info->name, p);
			if (p && strcmp(p,SOLARD_ROLE_BATTERY) == 0) {
				dprintf(1,"count: %d\n", list_count(info->mq));
				while((msg = list_get_next(info->mq)) != 0) getpack(conf,msg->name,msg->data);
			}
			list_purge(info->mq);
		}
#ifdef MQTT
		/* Purge any messages in main message que */
		list_purge(conf->c->mq);
#endif
		display(conf);
		sleep(refresh);
//		dprintf(0,"count: %d, used: %ld\n", list_count(conf->packs), mem_used());
	}
	return 0;
}
