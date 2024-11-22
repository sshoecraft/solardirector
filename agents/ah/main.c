
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "ah.h"

#define TESTING 0
#define TESTLVL 7

char *ah_version_string = "1.0-" STRINGIFY(__SD_BUILD);

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

	dprintf(dlevel,"air_in_ch: %d\n", s->air_in_ch);
	dprintf(dlevel,"air_out_ch: %d\n", s->air_out_ch);
	dprintf(dlevel,"water_in_ch: %d\n", s->water_in_ch);
	dprintf(dlevel,"water_out_ch: %d\n", s->water_out_ch);
	return 0;
}

json_value_t *ah_get_info(ah_session_t *s) {
	json_object_t *o;

	dprintf(dlevel,"creating info...\n");

	o = json_create_object();
	if (!o) return 0;
	json_object_set_string(o,"agent_name",ah_driver.name);
	json_object_set_string(o,"agent_role",SOLARD_ROLE_UTILITY);
        json_object_set_string(o,"agent_description","+DESC+");
	json_object_set_string(o,"agent_version",ah_version_string);
	json_object_set_string(o,"agent_author","Stephen P. Shoecraft");

	config_add_info(s->ap->cp, o);

	dprintf(dlevel,"returning: %p\n", json_object_value(o));
	return json_object_value(o);
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

int main(int argc, char **argv) {
	ah_session_t *s;
#if TESTING
        char *args[] = { "template", "-d", STRINGIFY(TESTLVL), "-c", "test.json" };
        argc = (sizeof(args)/sizeof(char *));
        argv = args;
#endif
	bool norun;
	opt_proctab_t opts[] = {
		{ "-1|dont enter run loop",&norun,DATA_TYPE_BOOL,0,0,"no" },
		OPTS_END
	};

	s = ah_driver.new(0,0);
	if (!s) {
		perror("driver.new");
		return 1;
	}
	if (ah_agent_init(argc,argv,opts,s)) return 1;

	wiringPiSetup();
	ads1115Setup(AD_BASE,0x48);

	if (norun) ah_driver.read(s,0,0,0);
	else agent_run(s->ap);

	ah_driver.destroy(s);
	agent_shutdown();
#ifdef DEBUG_MEM
	log_info("final mem used: %ld\n", mem_used());
#endif

	return 0;
}
