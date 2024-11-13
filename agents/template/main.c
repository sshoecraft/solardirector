
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "template.h"

#define TESTING 0
#define TESTLVL 7

char *template_version_string = "1.0-" STRINGIFY(__SD_BUILD);

int main(int argc, char **argv) {
	template_session_t *s;
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

	s = template_driver.new(0,0);
	if (!s) {
		perror("driver.new");
		return 1;
	}
	if (template_agent_init(argc,argv,opts,s)) return 1;

	if (norun) template_driver.read(s,0,0,0);
	else agent_run(s->ap);

	template_driver.destroy(s);
	agent_shutdown();
#ifdef DEBUG_MEM
	log_info("final mem used: %ld\n", mem_used());
#endif

	return 0;
}
