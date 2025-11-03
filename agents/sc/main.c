
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "sc.h"
#include "__sd_build.h"

#define TESTING 0
#define TESTLVL 2

char *sc_version_string = STRINGIFY(__SD_BUILD);

#if 0
static int set_location(void *ctx, config_property_t *p, void *old_value) {
	sc_session_t *s = ctx;

	/* punt to javascript */
	if (s->ap) agent_jsexec(s->ap, "set_location();");
	return 0;
}
#endif

#if 0
static int sc_cb(void *ctx) {
//	sc_session_t *s = ctx;

	printf("sc_cb!\n");
	return 0;
}
#endif

json_value_t *sc_get_info(sc_session_t *s) {
	json_object_t *o;

	dprintf(dlevel,"creating info...\n");

	o = json_create_object();
	if (!o) return 0;
	json_object_set_string(o,"agent_name",sc_driver.name);
	json_object_set_string(o,"agent_role",SOLARD_ROLE_UTILITY);
        json_object_set_string(o,"agent_description","+DESC+");
	json_object_set_string(o,"agent_version",sc_version_string);
	json_object_set_string(o,"agent_author","Stephen P. Shoecraft");

	config_add_info(s->ap->cp, o);

	dprintf(dlevel,"returning: %p\n", json_object_value(o));
	return json_object_value(o);
}

int sc_agent_init(int argc, char **argv, opt_proctab_t *opts, sc_session_t *s) {
	config_property_t sc_props[] = {
		/* name, type, dest, dsize, def, flags, scope, values, labels, units, scale, precision, trigger, ctx */
		{ 0 }
	};

	s->ap = agent_init(argc,argv,sc_version_string,opts,&sc_driver,s,0,sc_props,0);
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

//	agent_set_callback(s->ap, sc_cb, s);
	return 0;
}

int main(int argc, char **argv) {
	sc_session_t *s;
#if TESTING
        char *args[] = { "sc", "-d", STRINGIFY(TESTLVL), "-c", "test.json" };
        argc = (sizeof(args)/sizeof(char *));
        argv = args;
#endif
	bool norun;
	opt_proctab_t opts[] = {
		{ "-1|dont enter run loop",&norun,DATA_TYPE_BOOL,0,0,"no" },
		OPTS_END
	};

	s = sc_driver.new(0,0);
	if (!s) {
		perror("driver.new");
		return 1;
	}
	if (sc_agent_init(argc,argv,opts,s)) return 1;

	if (norun) sc_driver.read(s,0,0,0);
	else agent_run(s->ap);

	sc_driver.destroy(s);
	agent_shutdown();
#ifdef DEBUG_MEM
	log_info("final mem used: %ld\n", mem_used());
#endif

	return 0;
}
