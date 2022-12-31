
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __CELLMON_H
#define __CELLMON_H

#include "client.h"
#include "battery.h"

struct cellmon_config {
	solard_client_t *c;
	int cells;
	list packs;
	int state;
};
typedef struct cellmon_config cellmon_config_t;

#define BATTERY_MAX_PACKS 32

void display(cellmon_config_t *);
void web_display(cellmon_config_t *, int);

#endif
