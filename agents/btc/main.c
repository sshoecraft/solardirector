/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "btc.h"
#include "__sd_build.h"

#define TESTING 0
#define TESTLVL 4

char *btc_agent_version_string = "1.0-" STRINGIFY(__SD_BUILD);

int btc_agent_init(int argc, char **argv, opt_proctab_t *btc_opts, btc_session_t *s) {
	config_property_t btc_props[] = {
		{ "log_power", DATA_TYPE_BOOL, &s->log_power, 0, "0", 0, },
//			"select", 2, (bool []){ 0, 1 }, 1, (char *[]){ "log output_power from each read" }, 0, 1, 0 },
		{0}
	};

	agent_init(argc,argv,btc_agent_version_string,btc_opts,&btc_driver,s,0,btc_props,0);
	if (!s->ap) return 1;
	s->ap->interval = 20;

	return 0;
}

int main(int argc,char **argv) {
	btc_session_t *s;
	bool norun_flag;
	opt_proctab_t btc_opts[] = {
		/* Spec, dest, type len, reqd, default val, have */
		{ "-1|dont enter run loop",&norun_flag,DATA_TYPE_BOOL,0,0,"no" },
		OPTS_END
	};

	s = btc_driver.new(0,0);
	if (!s) {
		perror("new");
		return 1;
	}

	if (btc_agent_init(argc,argv,btc_opts,s)) return 1;
	if (!norun_flag) agent_run(s->ap);
	else btc_driver.read(s,0,0,0);
	return 0;
}
