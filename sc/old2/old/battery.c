
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "solard.h"

solard_battery_t *find_pack_by_name(solard_config_t *conf, char *name) {
	solard_battery_t *pp;

	dprintf(7,"name: %s\n", name);
	list_reset(conf->batteries);
	while((pp = list_get_next(conf->batteries)) != 0) {
		if (strcmp(pp->name,name) == 0) {
			dprintf(7,"found!\n");
			return pp;
		}
	}
	dprintf(7,"NOT found!\n");
	return 0;
}

int sort_batteries(void *i1, void *i2) {
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

#if 0
static void _ic_arr(char *name,void *dest, int len,json_value_t *v) {
	solard_battery_t *bp = dest;
	char label[32];
	int i;

	dprintf(1,"len: %d\n", len);
	if (strcmp(name,"temps")==0) {
		for(i=0; i < len; i++) {
			sprintf(label,"temp_%02d",i);
			ic_double(label,bp->temps[i]);
		}
	} else if (strcmp(name,"cellvolt")==0) {
		for(i=0; i < len; i++) {
			sprintf(label,"cell_%02d",i);
			ic_double(label,bp->cellvolt[i]);
		}
#if BATTERY_CELLRES
	} else if (strcmp(name,"cellres")==0) {
		for(i=0; i < len; i++) {
			sprintf(label,"res_%02d",i);
			ic_double(label,bp->cellres[i]);
		}
#endif
	}

	return;
}

int battery_to_influxdb(battery_session_t *s) {
	solard_battery_t *bp = &s->info;
	json_proctab_t battery_tab[] = { BATTERY_TAB(bp->ntemps,bp->ncells,_ic_arr,_set_state) }, *p;
        int *ip;
        float *fp;
        double *dp;

	influx_set_measurement("Battery");
        for(p=battery_tab; p->field; p++) {
                if (p->cb) p->cb(p->field,p->ptr,p->len,0);
                else {
                        switch(p->type) {
                        case DATA_TYPE_STRING:
				ic_string(p->field,p->ptr);
                                break;
                        case DATA_TYPE_INT:
                                ip = p->ptr;
				ic_long(p->field,*ip);
                                break;
                        case DATA_TYPE_FLOAT:
                                fp = p->ptr;
				ic_double(p->field,*fp);
                                break;
                        case DATA_TYPE_DOUBLE:
                                dp = p->ptr;
				ic_double(p->field,*dp);
                                break;
                        default:
                                dprintf(1,"battery_to_ic: unhandled type: %d\n", p->type);
                                break;
                        }
                }
	}
	ic_measureend();
	ic_push();
	return 0;
}
#endif

//void getpack(solard_config_t *conf, char *name, char *data) {
void getpack(solard_config_t *conf, client_agentinfo_t *ap) {
	solard_battery_t bat,*pp;
	solard_message_t *msg;

	dprintf(4,"agent %s count %d\n", ap->name, list_count(ap->mq));
	list_reset(ap->mq);
	while((msg = list_get_next(ap->mq)) != 0) {
		if (strcmp(msg->func,SOLARD_FUNC_DATA) != 0) {
			dprintf(4,"error: NOT data\n");
			list_delete(ap->mq,msg);
			continue;
		}
		dprintf(4,"updating pack: %s\n", ap->name);
		if (battery_from_json(&bat,msg->data)) {
			dprintf(0,"battery_from_json failed on %s\n", msg->data);
			return;
		}
//		battery_dump(&bat,4);
		/* See if this pack is ours */
		dprintf(4,"name: %s\n", bat.name);
		pp = find_pack_by_name(conf,bat.name);
		if (!pp) {
			dprintf(4,"not our pack: %s\n", bat.name);
			return;
		}
		*pp = bat;
//		memcpy(pp,&bat,sizeof(bat));
		set_state(pp,BATTERY_STATE_UPDATED);
		time(&pp->last_update);
		list_delete(ap->mq,msg);
	}
	return;
};
