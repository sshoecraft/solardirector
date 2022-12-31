
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "utils.h"
#include "debug.h"

#ifdef __WIN32
int become_daemon(void) {
	return 1;
}
#else
#include <sys/wait.h>
int become_daemon(void) {
	pid_t pid;

	/* Fork the process */
	dprintf(4,"become_daemon: forking process...");
	pid = fork();

	/* If pid < 0, error */
	if (pid < 0) {
		log_syserror("become_daemon: fork");
		return 1;
	}

	/* If pid > 0, parent */
	else if (pid > 0) {
		dprintf(4,"become_daemon: parent exiting...");
		_exit(0);
	}

	/* Set the session ID */
	setsid();

	/* Fork again */
	pid = fork();
	if (pid < 0)
		return 1;
	else if (pid != 0) {
		dprintf(4,"become_daemon(2): parent exiting...");
		_exit(0);
	}

	/* Set the umask */
	umask(022);

	/* Close stdin,stdout,stderr */
	close(0);
	close(1);
	close(2);
	return 0;
}
#endif
