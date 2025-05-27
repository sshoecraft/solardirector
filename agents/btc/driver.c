
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "btc.h"

extern char *btc_version_string;

static void *btc_new(void *transport, void *transport_handle) {
	btc_session_t *s;

	s = calloc(1,sizeof(*s));
	if (!s) {
		log_syserr("btc_new: calloc");
		return 0;
	}
	return s;
}

static int btc_free(void *handle) {
	btc_session_t *s = handle;

        if (!s) return 1;

        /* must be last */
        dprintf(dlevel,"destroying agent...\n");
        if (s->ap) agent_destroy_agent(s->ap);

	free(s);
	return 0;
}

static int combine_data(btc_session_t *s, solard_battery_t *bat) {
	btc_agentinfo_t *info;
	solard_battery_t *bp;
	float min,max;
	time_t cur;
	int count,i;

	int ldlevel = dlevel;

	memset(bat,0,sizeof(*bat));
	strcpy(bat->name,"bcombiner");
	count = 0;
	min = 9999990.0;
	max = 0.0;
	time(&cur);
	count = 0;
	list_reset(s->agents);
	while((info = list_get_next(s->agents)) != 0) {
		if (strcmp(info->role,SOLARD_ROLE_BATTERY) != 0) continue;
		if (!info->have_data) continue;
		bp = &info->data;
//		battery_dump(bp,0);
		/* ignore any that havent reported in 120 seconds */
		dprintf(dlevel,"%s: last_update: %d, diff: %d\n", bp->name, bp->last_update, cur - bp->last_update);
		if (cur - bp->last_update > 120) continue;
		/* ncells MUST match */
		if (!bat->ncells) bat->ncells = bp->ncells;
		else if (bp->ncells != bat->ncells) {
			log_error("error: ncells (%d) for battery %s does not match the first (%d), ignoring!\n", bp->ncells, bp->name, bat->ncells);
			continue;
		}
#define ADD(n) bat->n += bp->n
		ADD(capacity);
		ADD(voltage);
		ADD(current);
		ADD(power);
		dprintf(dlevel,"%s: ntemps: %d\n", bp->name, bp->ntemps);
		for(i=0; i < bp->ntemps; i++) {
			dprintf(dlevel+1,"%s: temp[%d]: %f\n", bp->name, i, bp->temps[i]);
			if (bp->temps[i] < min) min = bp->temps[i];
			if (bp->temps[i] > max) max = bp->temps[i];
		}
		for(i=0; i < bp->ncells; i++) {
			dprintf(dlevel+1,"%s: cellvolt[%d]: %f\n", bp->name, i, bp->cellvolt[i]);
			ADD(cellvolt[i]);
			bat->cell_total += bp->cellvolt[i];
		}
		for(i=0; i < bp->ncells; i++) ADD(cellres[i]);
		bat->state |= bp->state;
		count++;
	}
	dprintf(dlevel,"count: %d\n", count);
	if (!count) return 0;

	dprintf(dlevel,"ncells: %d\n", bat->ncells);
	if (!bat->ncells) return 0;
	bat->voltage /= count;
	bat->cell_min = 9999999;
	bat->ntemps = 2;
	bat->temps[0] = pround(min,1);
	bat->temps[1] = pround(max,1);
	dprintf(dlevel,"min: %f, max: %f\n", min, max);
	for(i=0; i < bat->ncells; i++) {
		bat->cellvolt[i] = pround(bat->cellvolt[i] / count, 3);
		bat->cellres[i] = pround(bat->cellres[i] / count, 3);
		if (bat->cellvolt[i] < bat->cell_min) bat->cell_min = bat->cellvolt[i];
		if (bat->cellvolt[i] >= bat->cell_max) bat->cell_max = bat->cellvolt[i];
	}
	bat->cell_min = pround(bat->cell_min,3);
	bat->cell_max = pround(bat->cell_max,3);
	bat->cell_total = pround(bat->cell_total / count,1);
	bat->cell_avg = pround(bat->cell_total / bat->ncells,3);
	bat->cell_diff = pround(bat->cell_max - bat->cell_min,3);;

	dprintf(ldlevel,"returning: %d\n", count);
	return count;
}

int btc_read(void *handle, uint32_t *what, void *buf, int buflen) {
	btc_session_t *s = handle;
	solard_battery_t bat;
	json_value_t *v;

	if (combine_data(s,&bat)) {
//		battery_dump(&bat,dlevel+2);

#ifdef MQTT
		dprintf(dlevel,"mqtt_connected: %d\n", mqtt_connected(s->ap->m));
		if (mqtt_connected(s->ap->m)) {
			v = battery_to_json(&bat);
			if (v) {
				agent_pubdata(s->ap, v);
				json_destroy_value(v);
			}
		}
#endif
#ifdef INFLUX
		dprintf(dlevel,"influx_connected: %d\n", influx_connected(s->ap->i));
		if (influx_connected(s->ap->i)) {
			v = battery_to_flat_json(&bat);
			if (v) {
				influx_write_json(s->ap->i, "battery", v);
				json_destroy_value(v);
			}
		}
#endif

		dprintf(dlevel,"log_power: %d\n", s->log_power);
		if (s->log_power && (bat.power != s->last_power)) {
			log_info("%.1f\n",bat.power);
			s->last_power = bat.power;
		}
	}

	return 0;
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
					json_object_set_string(o,"agent_name",btc_driver.name);
					json_object_set_string(o,"agent_role",SOLARD_ROLE_UTILITY);
					json_object_set_string(o,"agent_version",btc_version_string);
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

solard_driver_t btc_driver = {
	"btc",
	btc_new,		/* New */
	btc_free,		/* Free */
	0,			/* Open */
	0,			/* Close */
	btc_read,		/* Read */
	0,			/* Write */
	btc_config		/* Config */
};
