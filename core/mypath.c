
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include<stdio.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>

/* TODO: __WIN32 */
int getmypath(char *dest, int len) {
	return readlink ("/proc/self/exe", dest, len);
}
