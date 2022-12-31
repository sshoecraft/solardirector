/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "sb.h"
#include "__sd_build.h"
#include <sys/stat.h>

#define TESTING 0
#define TESTLVL 4

char *sb_agent_version_string = "1.0-" STRINGIFY(__SD_BUILD);
char strfile[256],objfile[256];

int main(int argc, char **argv) {
	sb_session_t *s;
#if TESTING
	char *args[] = { "sb", "-d", STRINGIFY(TESTLVL), "-c", "sbtest.conf", "-U", "1048576", "-K", "65536" };
	argc = sizeof(args)/sizeof(char *);
	argv = args;
#endif

	/* Init the driver */
	s = sb_driver.new(0,0);
	if (!s) return 1;

	/* Agent init */
	if (sb_agent_init(s,argc,argv)) {
		sb_driver.close(s);
		return 1;
	}

	/* Main loop */
	dprintf(1,"norun_flag: %d", s->norun_flag);
	if (!s->norun_flag) agent_run(s->ap);
	else sb_driver.read(s,0,0,0);
	sb_driver.close(s);
	return 0;
}
