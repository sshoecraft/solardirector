
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "cellmon.h"

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

	battery_from_json(&bat,data);
	battery_dump(&bat,3);
	solard_set_state((&bat),BATTERY_STATE_UPDATED);
	time(&bat.last_update);

	pp = find_pack_by_name(conf,bat.name);
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
//	char *args[] = { "t2", "-d", "2", "-c", "cellmon.conf" };
//	#define nargs (sizeof(args)/sizeof(char *))
	cellmon_config_t *conf;
	solard_message_t *msg;
	char configfile[256],topic[SOLARD_TOPIC_SIZE],*p;
	long start;
//	int web_flag;
//		{ "-w|web output",&web_flag,DATA_TYPE_BOOL,0,0,"false" },
	opt_proctab_t opts[] = {
		/* Spec, dest, type len, reqd, default val, have */
		{ "-t::|topic",&topic,DATA_TYPE_STRING,sizeof(topic)-1,0,"" },
		OPTS_END
	};
//	time_t last_read,cur,diff;

	find_config_file("cellmon.conf",configfile,sizeof(configfile)-1);
	dprintf(1,"configfile: %s\n", configfile);

	conf = calloc(1,sizeof(*conf));
	if (!conf) return 1;
//	conf->c = client_init(nargs,args,opts,"cellmon");
	conf->c = client_init(argc,argv,opts,"cellmon");
	if (!conf->c) return 1;
	conf->packs = list_create();

	if (!strlen(topic)) {
		p = cfg_get_item(conf->c->cfg,"cellmon","topic");
		if (p) strncat(topic,p,sizeof(topic)-1);
		else strcpy(topic,"SolarD/Battery/+/Data");
	}

	if (mqtt_sub(conf->c->m,topic)) return 1;

	/* main loop */
	start = mem_used();
	while(1) {
		dprintf(1,"count: %d\n", list_count(conf->c->messages));
		list_reset(conf->c->messages);
		while((msg = list_get_next(conf->c->messages)) != 0) {
			getpack(conf,msg->name,msg->data);
			list_delete(conf->c->messages,msg);
		}
		display(conf);
		dprintf(1,"used: %ld\n", mem_used() - start);
		sleep(1);
	}
	return 0;
}
