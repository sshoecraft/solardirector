
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "solard.h"

static void *sd_new(void *driver, void *driver_handle) {
	solard_config_t *conf;

	conf = calloc(1,sizeof(*conf));
	if (!conf) {
		log_syserror("calloc(%d)",sizeof(*conf));
		return 0;
	}
	dprintf(1,"conf: %p\n", conf);
	conf->agents = list_create();
	conf->batteries = list_create();
	conf->inverters = list_create();

	return conf;
}

static int sd_destroy(void *h) {
	solard_config_t *conf = h;
	if (!conf) return 1;
	free(conf);
	return 0;
}

static int sd_read(void *handle, uint32_t *control, void *buf, int buflen) {
//	solard_config_t *conf = handle;

	return 0;
}

solard_driver_t sd_driver = {
	"sd",
	sd_new,		/* New */
	sd_destroy,	/* Free */
	0,		/* Open */
	0,		/* Close */
	sd_read,	/* Read */
	0,		/* Write */
	sd_config,	/* Config */
};
