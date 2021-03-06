
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __MYBMM_SI_H
#define __MYBMM_SI_H

#include "module.h"

struct si_session {
	mybmm_config_t *conf;
	mybmm_inverter_t *inv;
	pthread_t th;
	uint32_t bitmap;
	uint8_t messages[16][8];
	mybmm_module_t *tp;
	void *tp_handle;
	int stop;
	int open;
	int (*get_data)(struct si_session *, int id, uint8_t *data, int len);
};
typedef struct si_session si_session_t;

#endif /* __MYBMM_SI_H */
