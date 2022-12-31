
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "solard.h"

static solard_inverter_t *find_by_name(solard_config_t *conf, char *name) {
	solard_inverter_t *inv;

	dprintf(7,"name: %s\n", name);
	list_reset(conf->inverters);
	while((inv = list_get_next(conf->inverters)) != 0) {
		if (strcmp(inv->name,name) == 0) {
			dprintf(7,"found!\n");
			return inv;
		}
	}
	dprintf(7,"NOT found!\n");
	return 0;
}

static int sort_inv(void *i1, void *i2) {
	solard_inverter_t *p1 = (solard_inverter_t *)i1;
	solard_inverter_t *p2 = (solard_inverter_t *)i2;
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

void getinv(solard_config_t *conf, client_agentinfo_t *ap) {
	solard_inverter_t inverter,*inv = &inverter;
	solard_message_t *msg;

//	dprintf(0,"agent %s count %d\n", ap->name, list_count(ap->mq));
	list_reset(ap->mq);
	while((msg = list_get_next(ap->mq)) != 0) {
		if (strcmp(msg->func,SOLARD_FUNC_DATA) != 0) {
			list_delete(ap->mq,msg);
			continue;
		}
		inverter_from_json(&inverter,msg->data);
		inverter_dump(&inverter,3);
		set_state((&inverter),SOLARD_INVERTER_STATE_UPDATED);
		time(&inverter.last_update);

		inv = find_by_name(conf,inverter.name);
		if (!inv) {
			dprintf(7,"adding inv...\n");
			list_add(conf->inverters,&inverter,sizeof(inverter));
			dprintf(7,"sorting invs...\n");
			list_sort(conf->inverters,sort_inv,0);
		} else {
			dprintf(7,"updating inv...\n");
			memcpy(inv,&inverter,sizeof(inverter));
		}
	}
	return;
}
