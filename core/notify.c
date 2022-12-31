
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "common.h"
#include "utils.h"
#include "debug.h"

static int ncount = 0;

void solard_notify_list(char *prog, list lp) {
	char logfile[300],*args[32],*p;
	int i,status;
	pid_t pid;

	pid = getpid();
	sprintf(logfile,"%s/notify_%d.log",SOLARD_LOGDIR,ncount++);
	dprintf(1,"logfile: %s\n", logfile);

	/* Exec the agent */
	i = 0;
	args[i++] = prog;
	list_reset(lp);
	while((p = list_get_next(lp)) != 0) args[i++] = p;
	args[i++] = 0;
	pid = solard_exec(prog,args,logfile,0);
	dprintf(1,"pid: %d\n", pid);
	if (pid > 0) status = solard_wait(pid);
	else status = 1;
	if (status != 0) {
		log_write(LOG_SYSERR,"unable to exec program(%s): see %s for details",prog,logfile);
		return;
	}
	unlink(logfile);
}

void solard_notify(char *prog, char *format, ...) {
	va_list ap;
	char message[1024],logfile[300],*args[32];
	int i,status;
	pid_t pid;

	va_start(ap,format);
	vsnprintf(message,sizeof(message)-1,format,ap);
	dprintf(1,"message: %s\n", message);
	va_end(ap);

	pid = getpid();
	sprintf(logfile,"%s/notify_%d.log",SOLARD_LOGDIR,ncount++);
	dprintf(1,"logfile: %s\n", logfile);

	/* Exec the agent */
	i = 0;
	args[i++] = prog;
	args[i++] = message;
	args[i++] = 0;
	pid = solard_exec(prog,args,logfile,0);
	dprintf(1,"pid: %d\n", pid);
	if (pid > 0) status = solard_wait(pid);
	else status = 1;
	if (status != 0) {
		log_write(LOG_SYSERR,"unable to exec program(%s): see %s for details",prog,logfile);
		return;
	}
	unlink(logfile);
}
