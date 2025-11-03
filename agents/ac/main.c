
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "ac.h"
#include "__sd_build.h"

#define TESTING 0
#define TESTLVL 2

// must be a global for INIT
bool ignore_wpi_flag;

static int ac_agent_init(int argc, char **argv, opt_proctab_t *opts, ac_session_t *s) {
	config_property_t ac_props[] = {
		/* name, type, dest, dsize, def, flags, scope, values, labels, units, scale, precision, trigger, ctx */
		{ "can_target", DATA_TYPE_STRING, s->can_target, sizeof(s->can_target)-1, "can0", 0, },
		{ "can_topts", DATA_TYPE_STRING, s->can_topts, sizeof(s->can_topts)-1, "500000", 0, },
		{ "interval", DATA_TYPE_INT, 0, 0, "10", CONFIG_FLAG_NOWARN },
		{ 0 }
	};
	int r;

	s->ap = agent_init(argc,argv,s->version,opts,&ac_driver,s,0,ac_props,0);
	if (!s->ap) return 1;

	r = ac_can_init(s);
	dprintf(dlevel,"r: %d\n", r);
	if (r && ignore_wpi_flag) r = 0;
	if (r) {
		log_error("ac_can_init failed!\n");
		return 1;
	}
	log_info("Calling wpi_init with js engine: %p\n", s->ap->js.e);
	r = wpi_init(s->ap->js.e);
	dprintf(dlevel,"r: %d, ignore_wpi: %d\n", r, ignore_wpi_flag);
	if (r && ignore_wpi_flag) r = 0;
	if (r) {
		log_error("wpi_init failed!\n");
		return 1;
	}
	return 0;
}

int main(int argc, char **argv) {
	ac_session_t *s;
#if TESTING
        char *args[] = { "ac", "-d", STRINGIFY(TESTLVL), "-c", "test.json" };
        argc = (sizeof(args)/sizeof(char *));
        argv = args;
#endif
	opt_proctab_t opts[] = {
		{ "-i|ignore wpi_init errors",&ignore_wpi_flag,DATA_TYPE_BOOL,0,0,"no" },
		OPTS_END
	};

	s = ac_driver.new(0,0);
	if (!s) {
		perror("driver.new");
		return 1;
	}
	s->version = STRINGIFY(__SD_BUILD);

	if (ac_agent_init(argc,argv,opts,s)) return 1;
	s->ap->purge = true;

	agent_run(s->ap);
	ac_driver.destroy(s);
	agent_shutdown();
#ifdef DEBUG_MEM
	log_info("final mem used: %ld\n", mem_used());
#endif

	return 0;
}
