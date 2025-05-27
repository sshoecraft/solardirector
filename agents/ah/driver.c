
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "ah.h"

#define AD_BASE 120
#define GAIN 1
static float gainvolts[] = { 6.144, 4.096, 2.048, 1.024, 0.512, 0.256 };

extern char *ah_version_string;
extern bool ignore_wpi;

static void *ah_new(void *transport, void *transport_handle) {
	ah_session_t *s;

	s = calloc(1,sizeof(*s));
	if (!s) {
		log_syserr("ah_new: calloc");
		return 0;
	}
	return s;
}

static int ah_free(void *handle) {
	ah_session_t *s = handle;

        if (!s) return 1;

        /* must be last */
        dprintf(dlevel,"destroying agent...\n");
        if (s->ap) agent_destroy_agent(s->ap);

	free(s);
	return 0;
}

static int getval(ah_session_t *s, int ch) {
	int v,t,i;

	for(i=t=0; i < s->count; i++) {
		v = analogRead(AD_BASE+ch);
		usleep(10);
		t += v;
	}
	return (int)(t/s->count);
	
}

static float doread(ah_session_t *s, int ch) {
	dprintf(2,"ch: %d\n", ch);
	if (ch < 0 || ch > 3) return 0;
	int adc_value = getval(s,ch);
	dprintf(2,"adc_value: %d\n", adc_value);
	float gv = gainvolts[GAIN];
	dprintf(2,"gv: %f\n", gv);
	float m = gv / 32768;
	dprintf(2,"m: %f\n", m);
	float vout = adc_value * m;
	dprintf(2,"vout: %f\n", vout);
	return vout;
}

static float get_temp(ah_session_t *s, int ch) {
	float v,r,t,f;

	v = doread(s,ch);
	if (!v) return 0.0 / 0.0;
	dprintf(2,"ch[%d] v: %f\n", ch, v);
	r = s->divres * v / (3.3 - v);
	dprintf(2,"ch[%d] r: %f\n", ch, r);
	t = (1 / (log(r / s->refres) / s->beta + 1 / (273.15 + s->reftemp))) - 273.15;
	dprintf(2,"ch[%d] t: %f\n", ch, t);
	f = (t * 9/5) + 32;
	dprintf(2,"ch[%d] f: %f\n", ch, f);
	return f;
}

static int ah_read(void *handle, uint32_t *what, void *buf, int buflen) {
	ah_session_t *s = handle;
	char out[128];

	s->air_in = get_temp(s,s->air_in_ch);
	s->air_out = get_temp(s,s->air_out_ch);
	sprintf(out,"fan: %d, cool: %d, heat: %d, air: in: %3.1f, out: %3.1f", s->fan_state, s->cool_state, s->heat_state, s->air_in, s->air_out);
	if (strcmp(out,s->last_out) != 0) {
		log_info("%s\n", out);
		strcpy(s->last_out,out);
	}
	return 0;
}

static int ah_write(void *handle, uint32_t *what, void *buf, int buflen) {
	ah_session_t *s = handle;
	json_object_t *o;
	json_value_t *v;

	o = json_create_object();
	json_object_set_string(o,"name",s->ap->instance_name);
	json_object_set_string(o,"fan_state",s->fan_state ? "on" : "off");
	json_object_set_string(o,"cool_state",s->cool_state ? "on" : "off");
	json_object_set_string(o,"heat_state",s->heat_state ? "on" : "off");
	json_object_set_number(o,"air_in",s->air_in);
	json_object_set_number(o,"air_out",s->air_out);
	v = json_object_value(o);
//	j = json_dumps(v, 1);
//	printf("j: %s\n", j);
#ifdef MQTT
	dprintf(dlevel,"mqtt_connected: %d\n", mqtt_connected(s->ap->m));
	if (!mqtt_connected(s->ap->m)) mqtt_reconnect(s->ap->m);
	if (mqtt_connected(s->ap->m)) agent_pubdata(s->ap, v);
#endif
#ifdef INFLUX
	dprintf(dlevel,"influx_connected: %d\n", influx_connected(s->ap->i));
        if (!influx_connected(s->ap->i)) {
		influx_connect(s->ap->i);
		dprintf(dlevel,"influx_connected: %d\n", influx_connected(s->ap->i));
	}
	dprintf(dlevel,"influx_connected: %d\n", influx_connected(s->ap->i));
	if (influx_connected(s->ap->i)) influx_write_json(s->ap->i, "air_handler", v);
#endif
	json_destroy_value(v);

	return 0;
}

static json_value_t *ah_get_info(ah_session_t *s) {
	json_object_t *o;

	dprintf(dlevel,"creating info...\n");

	o = json_create_object();
	if (!o) return 0;
	json_object_set_string(o,"agent_name",ah_driver.name);
	json_object_set_string(o,"agent_role",SOLARD_ROLE_UTILITY);
	json_object_set_string(o,"agent_version",ah_version_string);
	json_object_set_string(o,"agent_author","Stephen P. Shoecraft");

	config_add_info(s->ap->cp, o);

	dprintf(dlevel,"returning: %p\n", json_object_value(o));
	return json_object_value(o);
}

static int ah_config(void *h, int req, ...) {
	ah_session_t *s = h;
	va_list va;
	int r;

	va_start(va,req);
	dprintf(dlevel,"req: %d\n", req);
	r = 1;
	switch(req) {
	case SOLARD_CONFIG_INIT:
		s->ap = va_arg(va,solard_agent_t *);
		dprintf(dlevel,"s->ap->e: %p\n", s->ap->js.e);
#ifdef JS
		r = wpi_init(s->ap->js.e);
#else
		r = wpi_init();
#endif
		if (r && ignore_wpi) r = 0;
		if (r) log_error("wpi_init failed\n");
		else ads1115Setup(AD_BASE,0x48);
		break;
	case SOLARD_CONFIG_GET_INFO:
		{
			json_value_t **vp = va_arg(va,json_value_t **);
			dprintf(dlevel,"vp: %p\n", vp);
			if (vp) {
				*vp = ah_get_info(s);
				r = 0;
			}
		}
		break;
	}
	return r;
}

solard_driver_t ah_driver = {
	AGENT_NAME,
	ah_new,			/* New */
	ah_free,		/* Free */
	0,			/* Open */
	0,			/* Close */
	ah_read,		/* Read */
	ah_write,		/* Write */
	ah_config		/* Config */
};
