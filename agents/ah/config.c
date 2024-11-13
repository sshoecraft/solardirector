
/*
Copyright (c) 2024, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "ah.h"

extern char *ah_version_string;

#if 0
static int set_location(void *ctx, config_property_t *p, void *old_value) {
	ah_session_t *s = ctx;

	/* punt to javascript */
	if (s->ap) agent_jsexec(s->ap, "set_location();");
	return 0;
}
#endif

#if 0
static int ah_cb(void *ctx) {
//	ah_session_t *s = ctx;

	printf("ah_cb!\n");
	return 0;
}
#endif

int ah_agent_init(int argc, char **argv, opt_proctab_t *opts, ah_session_t *s) {
	config_property_t ah_props[] = {
		/* name, type, dest, dsize, def, flags, scope, values, labels, units, scale, precision, trigger, ctx */
		{ "count", DATA_TYPE_INT, &s->count, 0, "5", 0, "range", "1,100,1", "number of readings to average", 0, 0, 0 },
		{ "refres", DATA_TYPE_INT, &s->refres, 0, "10", 0, "range", "1,999999,1", "reference resistance", 0, 0, 0 },
		{ "reftemp", DATA_TYPE_INT, &s->reftemp, 0, "25", 0, "range", "1,999999,1", "reference temperature in C", 0, 0, 0 },
		{ "beta", DATA_TYPE_INT, &s->beta, 0, "3950", 0, "range", "1,999999,1", "beta value", 0, 0, 0 },
		{ "divres", DATA_TYPE_INT, &s->divres, 0, "10", 0, "range", "1,999999,1", "divider resistance", 0, 0, 0 },
		{ "air_in_ch", DATA_TYPE_INT, &s->air_in_ch, 0, "-1", 0, "select", "0,1,2,3", "air_in channel", 0, 0, 0 },
		{ "air_in", DATA_TYPE_FLOAT, &s->air_in, 0, 0, CONFIG_FLAG_READONLY },
		{ "air_out_ch", DATA_TYPE_INT, &s->air_out_ch, 0, "-1", 0, "select", "0,1,2,3", "air_out channel", 0, 0, 0 },
		{ "air_out", DATA_TYPE_FLOAT, &s->air_out, 0, 0, CONFIG_FLAG_READONLY },
		{ "water_in_ch", DATA_TYPE_INT, &s->water_in_ch, 0, "-1", 0, "select", "0,1,2,3", "water_in channel", 0, 0, 0 },
		{ "water_in", DATA_TYPE_FLOAT, &s->water_in, 0, 0, CONFIG_FLAG_READONLY },
		{ "water_out_ch", DATA_TYPE_INT, &s->water_out_ch, 0, "-1", 0, "select", "0,1,2,3", "water_out channel", 0, 0, 0 },
		{ "water_out", DATA_TYPE_FLOAT, &s->water_out, 0, 0, CONFIG_FLAG_READONLY },
		{ 0 }
	};

	s->ap = agent_init(argc,argv,ah_version_string,opts,&ah_driver,s,0,ah_props,0);
	if (!s->ap) return 1;

#if 0
	if (!strlen(s->location)) {
		json_value_t *v;
		http_session_t *h;

		h = http_new("https://geolocation-db.com");
		v = http_request(h,"json/",0);
		dprintf(dlevel,"v: %p\n", v);
		if (v) {
			json_object_t *o = json_value_object(v);
			json_value_t *lat,*lon;

			lat = json_object_dotget_value(o,"latitude");
			lon = json_object_dotget_value(o,"longitude");
			dprintf(dlevel,"lat: %p, lon: %p\n", lat, lon);
			if (lat && lon) {
				float flat = json_value_get_number(lat);
				float flon = json_value_get_number(lon);
				char location[32];
				config_property_t *p;

				sprintf(location,"%f,%f", flat, flon);
				p = config_get_property(s->ap->cp,s->ap->instance_name,"location");
				dprintf(dlevel,"p: %p\n", p);
	}
#endif

//	agent_set_callback(s->ap, ah_cb, s);
	return 0;
}

int ah_config(void *h, int req, ...) {
	ah_session_t *s = h;
	va_list va;
	int r;

	va_start(va,req);
	dprintf(dlevel,"req: %d\n", req);
	r = 1;
	switch(req) {
	case SOLARD_CONFIG_INIT:
		s->ap = va_arg(va,solard_agent_t *);
		r = 0;
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
