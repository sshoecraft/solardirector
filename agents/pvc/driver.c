
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 1
#include "debug.h"

#include "pvc.h"

extern char *pvc_version_string;

static void *pvc_new(void *transport, void *transport_handle) {
	pvc_session_t *s;

	s = calloc(1,sizeof(*s));
	if (!s) {
		log_syserr("pvc_new: calloc");
		return 0;
	}
	return s;
}

static int pvc_free(void *handle) {
	pvc_session_t *s = handle;

        if (!s) return 1;

        /* must be last */
        dprintf(dlevel,"destroying agent...\n");
        if (s->ap) agent_destroy_agent(s->ap);

	free(s);
	return 0;
}

static int combine_data(pvc_session_t *s, solard_pvinverter_t *pv) {
	solard_pvinverter_t *pvp;
	pvc_agentinfo_t *info;
	int count;
	time_t cur;

	int ldlevel = dlevel;

	/* We only report 1 metric:  power (watts) */
	memset(pv,0,sizeof(*pv));
	strcpy(pv->name,"pvcombiner");
	time(&cur);
	count = 0;
    dprintf(ldlevel,"agent_count: %d\n", list_count(s->agents));
	list_reset(s->agents);
	while((info = list_get_next(s->agents)) != 0) {
		if (strcmp(info->role,SOLARD_ROLE_PVINVERTER) != 0) continue;
		if (!info->have_data) continue;
		pvp = &info->data;
		/* ignore any that havent reported in 120 seconds */
		dprintf(dlevel,"%s: last_update: %d, diff: %d\n", pvp->name, pvp->last_update, cur - pvp->last_update);
		if (cur - pvp->last_update > 120) continue;
		pv->input_voltage += pvp->input_voltage;
		pv->input_current += pvp->input_current;
		pv->input_power += pvp->input_power;
		if (!pv->output_voltage && pvp->output_voltage) pv->output_voltage = pvp->output_voltage;
		if (!pv->output_frequency && pvp->output_frequency) pv->output_frequency = pvp->output_frequency;
		pv->output_frequency += pvp->output_frequency;
		pv->output_current += pvp->output_current;
		pv->output_power += pvp->output_power;
		pv->total_yield += pvp->total_yield;
		pv->daily_yield += pvp->daily_yield;
		count++;
	}

	dprintf(ldlevel,"returning: %d\n", count);
	return count;
}

int pvc_read(void *handle, uint32_t *what, void *buf, int buflen) {
	pvc_session_t *s = handle;
	solard_pvinverter_t pv;

	if (s->ap->m) dprintf(dlevel,"connected: %d\n", mqtt_connected(s->ap->m));
	if (s->ap->m && !mqtt_connected(s->ap->m)) {
		mqtt_reconnect(s->ap->m);
		sleep(1);
	}
	if (combine_data(s,&pv)) {
		json_value_t *v = pvinverter_to_json(&pv);
		if (!v) return 1;
//		pvinverter_dump(&pv,dlevel+2);

#ifdef MQTT
		dprintf(dlevel,"mqtt_connected: %d\n", mqtt_connected(s->ap->m));
		if (mqtt_connected(s->ap->m)) agent_pubdata(s->ap, v);
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
	}

	return 0;
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
//		r = jsinit(s);
		r = 0;
		break;
	case SOLARD_CONFIG_GET_INFO:
		{
			json_value_t **vp = va_arg(va,json_value_t **);
			dprintf(dlevel,"vp: %p\n", vp);
			if (vp) {
				json_object_t *o;

				dprintf(dlevel,"creating info...\n");

				o = json_create_object();
				if (o) {
					json_object_set_string(o,"agent_name",pvc_driver.name);
					json_object_set_string(o,"agent_role",SOLARD_ROLE_UTILITY);
					json_object_set_string(o,"agent_version",pvc_version_string);
					json_object_set_string(o,"agent_author","Stephen P. Shoecraft");

					config_add_info(s->ap->cp, o);

					dprintf(dlevel,"returning: %p\n", json_object_value(o));
					*vp = json_object_value(o);
					r = 0;
				} else {
					r = 1;
				}
			}
		}
		break;
	}
	return r;
}

solard_driver_t pvc_driver = {
	"pvc",
	pvc_new,			/* New */
	pvc_free,			/* Free */
	0,			/* Open */
	0,			/* Close */
	pvc_read,			/* Read */
	0,			/* Write */
	pvc_config			/* Config */
};
