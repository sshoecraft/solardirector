
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __WIN32
#include "common.h"
#include "debug.h"

static int issetugid(void) {
	if (getuid() != geteuid()) return 1; 
	if (getgid() != getegid()) return 1; 
	return 0; 
}

void tmpdir(char *dest, int dest_len) {
	char *p,*vars[] = { "TMPDIR", "TMP", "TEMP", "TEMPDIR" };
	int i;

	if (!dest) return;

	*dest = 0;
	if (!issetugid()) {
		for(i=0; i < (sizeof(vars)/sizeof(char *)); i++) {
//			dprintf(1,"trying: %s\n", vars[i]);
			p = os_getenv(vars[i]);
			if (p) break;
		}
	}
	if (!p) p = "/tmp";
	strncat(dest,p,dest_len);
//	dprintf(1,"dest: %s\n", dest);
	return;
}
#else
#include <windows.h>
void tmpdir(char *dest, int dest_len) { GetTempPath(dest_len, dest); }
#endif
