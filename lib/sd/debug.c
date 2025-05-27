
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "debug.h"
#include "config.h"

/* Debug global var */
int debug;

#if 0
void debug_add_props(config_t *cp, char *name) {
	config_property_t props[] = {
		{ "debug", DATA_TYPE_INT, &debug, 0, 0, CONFIG_FLAG_PRIVATE, "range", "0,99,1", "debug level", 0, 1, 0 },
		{ 0 }
	};
	config_add_props(cp, name, props, 0);
}
#endif
