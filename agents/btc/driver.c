
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "btc.h"

#define dlevel 0

extern char *btc_agent_version_string;

static solard_battery_t *find_by_name(btc_session_t *s, char *name) {
	solard_battery_t *bp;

	dprintf(dlevel,"name: %s\n", name);
	list_reset(s->bats);
	while((bp = list_get_next(s->bats)) != 0) {
		if (strcmp(bp->name,name) == 0) {
			dprintf(dlevel,"found!\n");
			return bp;
		}
	}
	dprintf(dlevel,"NOT found!\n");
	return 0;
}

static int sort_bats(void *i1, void *i2) {
	solard_battery_t *p1 = (solard_battery_t *)i1;
	solard_battery_t *p2 = (solard_battery_t *)i2;
	int val;

	dprintf(6,"p1: %s, p2: %s\n", p1->name, p2->name);
	val = strcmp(p1->name,p2->name);
	if (val < 0)
		val =  -1;
	else if (val > 0)
		val = 1;
	dprintf(6,"returning: %d\n", val);
	return val;

}

#if 1
static void getbat(btc_session_t *s, char *name, char *data) {
#else
static void getbat(btc_session_t *s, char *name, json_value_t *json) {
#endif
	solard_battery_t bat,*bp = &bat;

#if 1
	dprintf(dlevel,"getting bat: name: %s, data: %p\n", name, data);
	if (battery_from_json(&bat,data)) {
#else
	dprintf(dlevel,"getting bat: name: %s, json: %p\n", name, json);
	if (battery_from_json(&bat,json)) {
#endif
		dprintf(dlevel,"battery_from_json failed\n");
		return;
	}
	dprintf(dlevel,"dumping...\n");
//	battery_dump(&bat,dlevel+1);
	set_state((&bat),BATTERY_STATE_UPDATED);
	time(&bat.last_update);
	if (!strlen(bat.name)) return;

	bp = find_by_name(s,bat.name);
	dprintf(dlevel,"bp: %p\n", bp);
	if (!bp) {
		dprintf(dlevel,"adding bat...\n");
		list_add(s->bats,&bat,sizeof(bat));
		dprintf(dlevel,"sorting bats...\n");
		list_sort(s->bats,sort_bats,0);
	} else {
		dprintf(dlevel,"updating bat...\n");
		memcpy(bp,&bat,sizeof(bat));
	}
	return;
};

#if 0
static int btc_cb(void *h) {
	btc_session_t *s = h;
	solard_message_t *msg;
	client_agentinfo_t *info;
	char *p;

	dprintf(dlevel,"agent count: %d\n", list_count(s->c->agents));
	list_reset(s->c->agents);
	while((info = list_get_next(s->c->agents)) != 0) {
		p = client_getagentrole(info);
		dprintf(dlevel,"agent: %s, role: %s\n", info->name, p);
		if (p && strcmp(p,SOLARD_ROLE_BATTERY) == 0) {
#if 1
			dprintf(dlevel,"count: %d\n", list_count(info->mq));
			while((msg = list_get_next(info->mq)) != 0) getbat(s,msg->name,msg->data);
#else
			dprintf(dlevel,"info->private: %p\n", info->private);
			if (info->private) getbat(s,info->name,(json_value_t *)info->private);
#endif
		}
		list_purge(info->mq);
	}
	return 0;
}
#endif

static void *btc_new(void *driver, void *driver_handle) {
	btc_session_t *s;

	s = calloc(1,sizeof(*s));
	if (!s) {
		log_syserror("btc_new: calloc");
		return 0;
	}
	s->bats = list_create();

	return s;
}

static int btc_destroy(void *handle) {
	btc_session_t *s = handle;

	free(s);
	return 0;
}

static int btc_read(void *handle, uint32_t *control, void *buf, int buflen) {
	btc_session_t *s = handle;
	solard_battery_t bat,*bp;
	solard_agent_t *ap;
	json_value_t *v;
	float min,max;
	int count,i;
	time_t cur;
	char *p;
	solard_message_t *msg;

	dprintf(dlevel,"agent count: %d\n", list_count(s->c->agents));
	list_reset(s->c->agents);
	while((ap = list_get_next(s->c->agents)) != 0) {
		p = client_getagentrole(ap);
		dprintf(dlevel+1,"agent: %s, role: %s\n", ap->name, p);
		if (p && strcmp(p,SOLARD_ROLE_BATTERY) == 0) {
			dprintf(dlevel+1,"count: %d\n", list_count(ap->mq));
			while((msg = list_get_next(ap->mq)) != 0) getbat(s,msg->name,msg->data);
		}
		list_purge(ap->mq);
	}

	count = list_count(s->bats);
	dprintf(dlevel,"count: %d\n", count);
	if (!count) return 0;

	memset(&bat,0,sizeof(bat));
	strcpy(bat.name,"bcombiner");
	list_reset(s->bats);
	/* unlike pvc we average everything */
	count = 0;
	min = 9999990.0;
	max = 0.0;
	time(&cur);
	while((bp = list_get_next(s->bats)) != 0) {
//		battery_dump(bp,0);
		/* ignore any that havent reported in 120 seconds */
		dprintf(dlevel,"%s: last_update: %d, diff: %d\n", bp->name, bp->last_update, cur - bp->last_update);
		if (cur - bp->last_update > 120) continue;
		/* ncells MUST match */
		if (!bat.ncells) bat.ncells = bp->ncells;
		else if (bp->ncells != bat.ncells) {
			log_error("error: ncells (%d) for battery %s does not match the first (%d), ignoring!\n", bp->ncells, bp->name, bat.ncells);
			continue;
		}
#define ADD(n) bat.n += bp->n
		ADD(capacity);
		ADD(voltage);
		ADD(current);
		ADD(power);
		dprintf(0,"%s: ntemps: %d\n", bp->name, bp->ntemps);
		for(i=0; i < bp->ntemps; i++) {
			dprintf(dlevel+1,"%s: temp[%d]: %f\n", bp->name, i, bp->temps[i]);
			if (bp->temps[i] < min) min = bp->temps[i];
			if (bp->temps[i] > max) max = bp->temps[i];
		}
		for(i=0; i < bp->ncells; i++) {
			dprintf(dlevel+1,"%s: cellvolt[%d]: %f\n", bp->name, i, bp->cellvolt[i]);
			ADD(cellvolt[i]);
			bat.cell_total += bp->cellvolt[i];
		}
		for(i=0; i < bp->ncells; i++) ADD(cellres[i]);
		bat.state |= bp->state;
		count++;
	}
	dprintf(dlevel,"count: %d\n", count);
	if (!count) return 0;
	dprintf(dlevel,"ncells: %d\n", bat.ncells);
	if (!bat.ncells) return 0;
	bat.voltage /= count;
	bat.cell_min = 9999999;
	bat.ntemps = 2;
	bat.temps[0] = pround(min,1);
	bat.temps[1] = pround(max,1);
	dprintf(0,"min: %f, max: %f\n", min, max);
	for(i=0; i < bat.ncells; i++) {
		bat.cellvolt[i] = pround(bat.cellvolt[i] / count, 3);
		bat.cellres[i] = pround(bat.cellres[i] / count, 3);
		if (bat.cellvolt[i] < bat.cell_min) bat.cell_min = bat.cellvolt[i];
		if (bat.cellvolt[i] >= bat.cell_max) bat.cell_max = bat.cellvolt[i];
	}
	bat.cell_min = pround(bat.cell_min,3);
	bat.cell_max = pround(bat.cell_max,3);
	bat.cell_total = pround(bat.cell_total / count,1);
	bat.cell_avg = pround(bat.cell_total / bat.ncells,3);
	bat.cell_diff = pround(bat.cell_max - bat.cell_min,3);;
	battery_dump(&bat,dlevel+1);

#ifdef JS
	/* If read script exists, we'll use that */
	if (agent_script_exists(s->ap, s->ap->js.read_script)) return 0;
#endif

#ifdef MQTT
	if (mqtt_connected(s->ap->m)) {
		v = battery_to_json(&bat);
		if (v) {
			agent_pubdata(s->ap, v);
			json_destroy_value(v);
		}
	}
#endif
#ifdef INFLUX
	if (influx_connected(s->ap->i)) {
		v = battery_to_flat_json(&bat);
		if (v) {
			influx_write_json(s->ap->i, "battery", v);
			json_destroy_value(v);
		}
	}
#endif

	if ((bat.power != s->last_power) && (s->log_power == true)) {
		log_info("%.1f\n",bat.power);
		s->last_power = bat.power;
	}

#if 0
#ifdef MQTT
	/* Purge any messages in client que */
	list_purge(s->c->mq);
#endif
#endif

	return 0;
}

static json_value_t *btc_get_info(btc_session_t *s) {
	json_object_t *o;

	dprintf(dlevel,"creating info...\n");

	o = json_create_object();
	if (!o) return 0;
	json_object_set_string(o,"agent_name",btc_driver.name);
	json_object_set_string(o,"agent_role",SOLARD_ROLE_UTILITY);
	json_object_set_string(o,"agent_description","PV Combiner Agent");
	json_object_set_string(o,"agent_version",btc_agent_version_string);
	json_object_set_string(o,"agent_author","Stephen P. Shoecraft");
	json_object_set_string(o,"device_manufacturer","SPS");
	config_add_info(s->ap->cp, o);

	return json_object_value(o);
}

static int btc_config(void *h, int req, ...) {
	btc_session_t *s = h;
	va_list va;
	int r;

	va_start(va,req);
	dprintf(dlevel,"req: %d\n", req);
	r = 1;
	switch(req) {
	case SOLARD_CONFIG_INIT:
		{
		char mqtt_info[256];

		s->ap = va_arg(va,solard_agent_t *);
//		agent_set_callback(s->ap,btc_cb,s);
		s->c = client_init(0,0,btc_agent_version_string,0,"btc",CLIENT_FLAG_NOJS,0,0,0);
		if (!s->c) return 1;
		s->c->addmq = true;
		mqtt_disconnect(s->c->m,1);
		mqtt_get_config(mqtt_info,sizeof(mqtt_info),s->ap->m,0);
		mqtt_parse_config(s->c->m,mqtt_info);
		mqtt_newclient(s->c->m);
		mqtt_connect(s->c->m,10);
		mqtt_resub(s->c->m);
		r = 0;
		}
		break;
	case SOLARD_CONFIG_GET_INFO:
		{
			json_value_t **vp = va_arg(va,json_value_t **);
			dprintf(dlevel,"vp: %p\n", vp);
			if (vp) {
				*vp = btc_get_info(s);
				r = 0;
			}
		}
		break;
	}
	return r;
}

solard_driver_t btc_driver = {
	"btc",
	btc_new,			/* New */
	btc_destroy,			/* Destroy */
	0,				/* Open */
	0,				/* Close */
	btc_read,			/* Read */
	0,				/* Write */
	btc_config			/* Config */
};
