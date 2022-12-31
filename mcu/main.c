
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "mcu.h"
#include "__sd_build.h"

#define TESTING 1
#define TESTLVL 2

#if DEBUG_STARTUP
#define _ST_DEBUG LOG_DEBUG
#else
#define _ST_DEBUG 0
#endif

#define dlevel 0

char *sd_version_string = "1.0-" STRINGIFY(__SD_BUILD);

static int mcu_add_agent_service(void *ctx, list args, char *errmsg, json_object_t *results) {
//	mcu_session_t *s = ctx;
	char **argv, *name, *value;
//	config_property_t *p;

	dprintf(dlevel,"args count: %d\n", list_count(args));
	list_reset(args);
	while((argv = list_get_next(args)) != 0) {
		name = argv[0];
		value = argv[1];
		dprintf(dlevel,"name: %s, value: %s\n", name, value);
	}

	return 0;
}

static int mcu_agent_init(int argc, char **argv, opt_proctab_t *sd_opts, mcu_session_t *sd) {
	config_property_t mcu_props[] = {
		{ "agent_notify_time", DATA_TYPE_INT, &sd->agent_notify, 0, "600", 0, },
//                        "range", 3, (int []){ 0, 1440, 1 }, 1, (char *[]) { "Dead agent notification timer" }, "S", 1, 0 },
		{ "agent_error_time", DATA_TYPE_INT, &sd->agent_error, 0, "300", 0, },
//                        "range", 3, (int []){ 0, 1440, 1 }, 1, (char *[]) { "Agent error timer" }, "S", 1, 0 },
		{ "agent_warning_time", DATA_TYPE_INT, &sd->agent_warning, 0, "300", 0, },
//                        "range", 3, (int []){ 0, 1440, 1 }, 1, (char *[]) { "Agent warning timer" }, "S", 1, 0 },
		{ "interval", DATA_TYPE_INT, &sd->interval, 0, "15", 0, },
//                        "range", 3, (int []){ 0, 1440, 1 }, 1, (char *[]) { "Agent check interval" }, "S", 1, 0 },
		{ "notify", DATA_TYPE_STRING, &sd->notify_path, sizeof(sd->notify_path)-1, "", 0, },
//			0, 0, 0, 0, 0, 0, 1, 0 },
		{ "agents", DATA_TYPE_STRING_LIST, sd->names, 0, 0, 0 },
		{0}
	};
	config_function_t mcu_funcs[] = {
		{ "add", (config_funccall_t *)mcu_add_agent_service, sd, 2 },
		{0}
	};

	return (agent_init(argc,argv,sd_version_string,sd_opts,&mcu_driver,sd,0,mcu_props,mcu_funcs) == 0);
}

int main(int argc,char **argv) {
	mcu_session_t *s;
	bool norun_flag;
	opt_proctab_t opts[] = {
		/* Spec, dest, type len, reqd, default val, have */
		{ "-N|dont enter run loop",&norun_flag,DATA_TYPE_BOOL,0,0,"no" },
		OPTS_END
	};
#if TESTING
	char *args[] = { "mcu", "-d", STRINGIFY(TESTLVL), "-c", "mcutest.json" };
	argc = (sizeof(args)/sizeof(char *));
	argv = args;
#endif

//	log_open("solard",0,LOG_INFO|LOG_WARNING|LOG_ERROR|LOG_SYSERR|_ST_DEBUG);

	s = mcu_driver.new(0,0);
	if (!s) return 1;
	if (mcu_agent_init(argc,argv,opts,s)) return 1;

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

	if (!norun_flag) agent_run(s->ap);
	else mcu_driver.read(s,0,0,0);

	/* Teardown */
	list_destroy(s->names);
	list_destroy(s->agents);
	mcu_driver.destroy(s);
	return 0;
}
