
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "ah.h"

#define GAIN 1
static float gainvolts[] = { 6.144, 4.096, 2.048, 1.024, 0.512, 0.256 };

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
	int t,i;

	for(i=t=0; i < s->count; i++) {
		t += analogRead(AD_BASE+ch);
		usleep(10);
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

int ah_read(void *handle, uint32_t *what, void *buf, int buflen) {
	ah_session_t *s = handle;
	json_object_t *o;
	json_value_t *v;

	s->air_in = get_temp(s,s->air_in_ch);
	s->air_out = get_temp(s,s->air_out_ch);
	s->water_in = get_temp(s,s->water_in_ch);
	s->water_out = get_temp(s,s->water_out_ch);

	o = json_create_object();
	json_object_set_string(o,"name",s->ap->instance_name);
        json_object_set_number(o,"air_in",s->air_in);
        json_object_set_number(o,"air_out",s->air_out);
        json_object_set_number(o,"water_in",s->water_in);
        json_object_set_number(o,"water_out",s->water_out);
	v = json_object_value(o);
#ifdef MQTT
	if (mqtt_connected(s->ap->m)) agent_pubdata(s->ap, v);
#endif
#ifdef INFLUX
	dprintf(dlevel,"influx_connected: %d\n", influx_connected(s->ap->i));
        if (!influx_connected(s->ap->i)) {
		influx_connect(s->ap->i);
		dprintf(dlevel,"influx_connected: %d\n", influx_connected(s->ap->i));
	}
	if (influx_connected(s->ap->i)) influx_write_json(s->ap->i, "air_handler", v);
#endif
	json_destroy_value(v);
	return 0;
}

solard_driver_t ah_driver = {
	AGENT_NAME,
	ah_new,			/* New */
	ah_free,			/* Free */
	0,			/* Open */
	0,			/* Close */
	ah_read,			/* Read */
	0,			/* Write */
	ah_config			/* Config */
};
