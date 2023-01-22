
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "pvc.h"

#define dlevel 4

extern char *pvc_agent_version_string;
#define PVINVERTER_STATE_UPDATED 0x2000

solard_pvinverter_t *find_pv_by_name(pvc_session_t *s, char *name) {
	solard_pvinverter_t *pvp;

	dprintf(dlevel,"name: %s\n", name);
	list_reset(s->pvs);
	while((pvp = list_get_next(s->pvs)) != 0) {
		if (strcmp(pvp->name,name) == 0) {
			dprintf(dlevel,"found!\n");
			return pvp;
		}
	}
	dprintf(dlevel,"NOT found!\n");
	return 0;
}

int sort_pvs(void *i1, void *i2) {
	solard_pvinverter_t *p1 = (solard_pvinverter_t *)i1;
	solard_pvinverter_t *p2 = (solard_pvinverter_t *)i2;
	int val;

	dprintf(dlevel,"p1: %s, p2: %s\n", p1->name, p2->name);
	val = strcmp(p1->name,p2->name);
	if (val < 0)
		val =  -1;
	else if (val > 0)
		val = 1;
	dprintf(dlevel,"returning: %d\n", val);
	return val;

}

void getpv(pvc_session_t *s, char *name, char *data) {
	solard_pvinverter_t pv,*pvp = &pv;

	dprintf(dlevel,"getting pv: name: %s, data: %s\n", name, data);
	if (pvinverter_from_json(&pv,data)) {
		dprintf(dlevel,"pvinverter_from_json failed with %s\n", data);
		return;
	}
	dprintf(dlevel,"dumping...\n");
	pvinverter_dump(&pv,dlevel+1);
	set_state((&pv),PVINVERTER_STATE_UPDATED);
	time(&pv.last_update);
	if (!strlen(pv.name)) return;

	pvp = find_pv_by_name(s,pv.name);
	dprintf(dlevel,"pvp: %p\n", pvp);
	if (!pvp) {
		dprintf(dlevel,"adding pv...\n");
		list_add(s->pvs,&pv,sizeof(pv));
		dprintf(dlevel,"sorting pvs...\n");
		list_sort(s->pvs,sort_pvs,0);
	} else {
		dprintf(dlevel,"updating pv...\n");
		memcpy(pvp,&pv,sizeof(pv));
	}
	return;
};

#ifdef MQTT
static int pvc_cb(void *h) {
	pvc_session_t *s = h;
}
#endif

static void *pvc_new(void *driver, void *driver_handle) {
	pvc_session_t *s;

	s = calloc(1,sizeof(*s));
	if (!s) {
		log_syserror("pvc_new: calloc");
		return 0;
	}
	s->pvs = list_create();

	return s;
}

static int pvc_destroy(void *handle) {
	pvc_session_t *s = handle;

	free(s);
	return 0;
}

static int pvc_read(void *handle, uint32_t *control, void *buf, int buflen) {
	pvc_session_t *s = handle;
	solard_pvinverter_t pv,*pvp;
	json_value_t *v;
	int count;

	solard_message_t *msg;
	client_agentinfo_t *info;
	char *p;

	dprintf(dlevel,"agent count: %d\n", list_count(s->c->agents));
	list_reset(s->c->agents);
	while((info = list_get_next(s->c->agents)) != 0) {
		p = client_getagentrole(info);
		dprintf(dlevel,"agent: %s, role: %s\n", info->name, p);
		if (p && strcmp(p,SOLARD_ROLE_PVINVERTER) == 0) {
			dprintf(dlevel,"count: %d\n", list_count(info->mq));
			while((msg = list_get_next(info->mq)) != 0) {
				getpv(s,msg->name,msg->data);
			}
		}
		list_purge(info->mq);
	}

	count = list_count(s->pvs);
	dprintf(dlevel,"count: %d\n", count);
	if (!count) return 0;

	/* We only report 1 metric:  power (watts) */
	memset(&pv,0,sizeof(pv));
	strcpy(pv.name,"pvcombiner");
	list_reset(s->pvs);
	while((pvp = list_get_next(s->pvs)) != 0) {
		pv.input_voltage += pvp->input_voltage;
		pv.input_current += pvp->input_current;
		pv.input_power += pvp->input_power;
		pv.output_voltage += pvp->output_voltage;
		pv.output_frequency += pvp->output_frequency;
		pv.output_current += pvp->output_current;
		pv.output_power += pvp->output_power;
		pv.total_yield += pvp->total_yield;
		pv.daily_yield += pvp->daily_yield;
	}
	// only avg the freq
	pv.output_frequency /= count;

#ifdef JS
	/* If read script exists, we'll use that */
	if (agent_script_exists(s->ap, s->ap->js.read_script)) return 0;
#endif

	v = pvinverter_to_json(&pv);
	if (!v) return 0;
	pvinverter_dump(&pv,dlevel+1);

#ifdef MQTT
	dprintf(dlevel,"mqtt_connected: %d\n", mqtt_connected(s->ap->m));
	if (mqtt_connected(s->c->m)) agent_pubdata(s->ap, v);
#endif
#ifdef INFLUX
	dprintf(dlevel,"influx_connected: %d\n", influx_connected(s->ap->i));
	if (influx_connected(s->ap->i)) influx_write_json(s->ap->i, "pvinverter", v);
#endif
	json_destroy_value(v);

	dprintf(2,"log_power: %d\n", s->log_power);
	if (s->log_power && (pv.output_power != s->last_power)) {
		log_info("%.1f\n",pv.output_power);
		s->last_power = pv.output_power;
	}

#ifdef MQTT
	/* Purge any messages in client que */
	list_purge(s->c->mq);
#endif

	return 0;
}

static json_value_t *pvc_get_info(pvc_session_t *s) {
	json_object_t *o;

	dprintf(dlevel,"creating info...\n");

	o = json_create_object();
	if (!o) return 0;
	json_object_set_string(o,"agent_name",pvc_driver.name);
	json_object_set_string(o,"agent_role",SOLARD_ROLE_UTILITY);
	json_object_set_string(o,"agent_description","PV Combiner Agent");
	json_object_set_string(o,"agent_version",pvc_agent_version_string);
	json_object_set_string(o,"agent_author","Stephen P. Shoecraft");
	json_object_set_string(o,"device_manufacturer","SPS");
	config_add_info(s->ap->cp, o);

	return json_object_value(o);
}

static int pvc_config(void *h, int req, ...) {
	pvc_session_t *s = h;
	va_list va;
	int r;

	va_start(va,req);
	dprintf(dlevel,"req: %d\n", req);
	r = 1;
	switch(req) {
	case SOLARD_CONFIG_INIT:
		s->ap = va_arg(va,solard_agent_t *);
//		if (s->ap->js.e) JS_EngineAddInitFunc(s->ap->js.e,"pvc_jsinit",pvc_jsinit,s);
#ifdef MQTT
		{
			char mqtt_info[256];

			agent_set_callback(s->ap,pvc_cb,s);
			s->c = client_init(0,0,pvc_agent_version_string,0,"pvc",CLIENT_FLAG_NOJS,0,0,0,0);
			if (!s->c) return 1;
			s->c->addmq = true;
			mqtt_disconnect(s->c->m,1);
			mqtt_get_config(mqtt_info,sizeof(mqtt_info)-1,s->ap->m,0);
			mqtt_parse_config(s->c->m,mqtt_info);
			mqtt_newclient(s->c->m);
			mqtt_connect(s->c->m,10);
			mqtt_resub(s->c->m);
		}
#endif
		r = 0;
		break;
	case SOLARD_CONFIG_GET_INFO:
		{
			json_value_t **vp = va_arg(va,json_value_t **);
			dprintf(dlevel,"vp: %p\n", vp);
			if (vp) {
				*vp = pvc_get_info(s);
				r = 0;
			}
		}
		break;
	}
	return r;
}

solard_driver_t pvc_driver = {
	"pvc",
	pvc_new,			/* New */
	pvc_destroy,			/* Destroy */
	0,				/* Open */
	0,				/* Close */
	pvc_read,			/* Read */
	0,				/* Write */
	pvc_config			/* Config */
};
