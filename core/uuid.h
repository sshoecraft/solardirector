
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __UUID_H
#define __UUID_H

#define UUID_LEN 37

#ifndef uuid_t
typedef unsigned char uuid_t[16];
#endif

#if 0
typedef struct {
	unsigned char b[16];
} uuid_t;
#endif

int uuid_is_valid(const char *uuid);
void uuid_generate_random(uint8_t *);
int uuid_parse(const char *uuid, uint8_t *u);
void my_uuid_unparse(uint8_t *uu, char *out);
//void uuid_unparse_upper(uuid_t uu, char *out);
//void uuid_unparse_lower(uuid_t uu, char *out);

#endif
