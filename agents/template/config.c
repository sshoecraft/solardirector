
/*
Copyright (c) 2024, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "template.h"

extern char *template_version_string;

#if 0
static int set_location(void *ctx, config_property_t *p, void *old_value) {
	template_session_t *s = ctx;

	/* punt to javascript */
	if (s->ap) agent_jsexec(s->ap, "set_location();");
	return 0;
}
#endif

#if 0
static int template_cb(void *ctx) {
//	template_session_t *s = ctx;

	printf("template_cb!\n");
	return 0;
}
#endif

int template_agent_init(int argc, char **argv, opt_proctab_t *opts, template_session_t *s) {
	config_property_t template_props[] = {
		/* name, type, dest, dsize, def, flags, scope, values, labels, units, scale, precision, trigger, ctx */
		{ 0 }
	};

	s->ap = agent_init(argc,argv,template_version_string,opts,&template_driver,s,0,template_props,0);
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

//	agent_set_callback(s->ap, template_cb, s);
	return 0;
}

int template_config(void *h, int req, ...) {
	template_session_t *s = h;
	va_list va;
	int r;

	va_start(va,req);
	dprintf(dlevel,"req: %d\n", req);
	r = 1;
	switch(req) {
	case SOLARD_CONFIG_INIT:
		s->ap = va_arg(va,solard_agent_t *);
		r = jsinit(s);
		break;
	case SOLARD_CONFIG_GET_INFO:
		{
			json_value_t **vp = va_arg(va,json_value_t **);
			dprintf(dlevel,"vp: %p\n", vp);
			if (vp) {
				*vp = template_get_info(s);
				r = 0;
			}
		}
		break;
	}
	return r;
}
