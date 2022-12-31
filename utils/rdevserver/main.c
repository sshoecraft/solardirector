
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "rdevserver.h"
#include <pthread.h>
#include "transports.h"
#include "__sd_build.h"

#define TESTING 0
#define DEBUG_STARTUP 0

#if DEBUG_STARTUP
#define _ST_DEBUG LOG_DEBUG
#else
#define _ST_DEBUG 0
#endif

#define dlevel 1

char *rdevserver_version = "1.0-" STRINGIFY(__SD_BUILD);

int main(int argc,char **argv) {
	rdev_config_t *conf;
	char configfile[256];
	opt_proctab_t opts[] = {
                /* Spec, dest, type len, reqd, default val, have */
		{ "-c:%|config file",&configfile,DATA_TYPE_STRING,sizeof(configfile)-1,1,"" },
		OPTS_END
	};
	int logopts = LOG_INFO|LOG_WARNING|LOG_ERROR|LOG_SYSERR|_ST_DEBUG;
#if TESTING
	char *args[] = { "rdevd", "-d", "4", "-c", "rdtest.conf" };
	argc = (sizeof(args)/sizeof(char *));
	argv = args;
#endif

	if (solard_common_init(argc,argv,"1.0",opts,logopts)) return 1;

	if (!strlen(configfile)) {
		log_write(LOG_ERROR,"configfile required.\n");
		return 1;
	}

	conf = calloc(1,sizeof(*conf));
	if (!conf) {
		log_write(LOG_SYSERR,"calloc config");
		goto init_error;
	}
	rdev_get_config(conf,configfile);

	set_state(conf,RDEVSERVER_RUNNING);
	server(conf);
	return 0;
init_error:
	free(conf);
	return 1;
}
