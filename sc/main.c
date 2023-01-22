
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "sc.h"
#include "__sd_build.h"

#define TESTING 1
#define TESTLVL 0

#if DEBUG_STARTUP
#define _ST_DEBUG LOG_DEBUG
#else
#define _ST_DEBUG 0
#endif

#define dlevel 0

char *sc_version_string = "1.0-" STRINGIFY(__SD_BUILD);

int main(int argc,char **argv) {
	sc_session_t *s;
	bool norun_flag;
	opt_proctab_t opts[] = {
		/* Spec, dest, type len, reqd, default val, have */
		{ "-N|dont enter run loop",&norun_flag,DATA_TYPE_BOOL,0,0,"no" },
		OPTS_END
	};
#if TESTING
	char *args[] = { "sc", "-d", STRINGIFY(TESTLVL), "-c", "sctest.json" };
	argc = (sizeof(args)/sizeof(char *));
	argv = args;
#endif

//	log_open("solard",0,LOG_INFO|LOG_WARNING|LOG_ERROR|LOG_SYSERR|_ST_DEBUG);

	s = sc_driver.new(0,0);
	if (!s) return 1;
	if (sc_agent_init(argc,argv,opts,s)) return 1;

#if 0
	if (!strlen(s->location)) {
		json_value_t *v;

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

	dprintf(0,"ap: %p\n", s->ap);
	if (!norun_flag) agent_run(s->ap);
	else sc_driver.read(s,0,0,0);

	/* Teardown */
	list_destroy(s->names);
	list_destroy(s->agents);
	sc_driver.destroy(s);
	return 0;
}
