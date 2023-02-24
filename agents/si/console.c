
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "common.h"
#include "socket.h"

static void *si_console(void *ctx) {
	si_session_t *s = ctx;

	return (void *)0;
}

int si_console_init(si_session_t *s) {
	return 1;
}
