
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include <stdio.h>

#define P       0x8408

int main(void) {
	register unsigned int b, v;
	register int i;
	FILE *fp;

	fp = fopen("fcstab.h","w+");
	if (!fp) {
		perror("fopen fcstab.h");
		return 1;
	}
	fprintf(fp,"\n#ifndef __FCSTAB_H\n");
	fprintf(fp,"#define __FCSTAB_H\n\n");
	fprintf(fp,"static unsigned short fcstab[256] = {");
	for (b = 0; ; ) {
		if (b % 8 == 0) fprintf(fp,"\n");

		v = b;
		for (i = 8; i--; ) v = v & 1 ? (v >> 1) ^ P : v >> 1;

		fprintf(fp,"\t0x%04x", v & 0xFFFF);
		if (++b == 256) break;
		fprintf(fp,",");
	}
	fprintf(fp,"\n};\n\n");
	fprintf(fp,"#endif /* __FCSTAB_H */\n");
	fclose(fp);
}
