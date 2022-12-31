/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "jbd.h"
#include "__sd_build.h"

#define TESTING 0

char *jbd_version_string = "1.0-" STRINGIFY(__SD_BUILD);

int main(int argc, char **argv) {
	jbd_session_t *s;
#if TESTING
	char *args[] = { "jbd", "-d", "4", "-e", "agent.script_dir=\".\"; agent.libdir=\"../../\";", "-c", "jbdtest.conf" };
	argc = sizeof(args)/sizeof(char *);
	argv = args;
#endif

	/* Init the driver */
	s = jbd_driver.new(0,0);
	if (!s) return 1;

	/* Agent init */
	if (jbd_agent_init(s,argc,argv)) return 1;

	/* Main loop */
	if (s->ap) agent_run(s->ap);
	return 0;
}
