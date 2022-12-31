
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include <stdlib.h>
#include "buffer.h"
#include "debug.h"

buffer_t *buffer_init(int size, buffer_read_t *func, void *ctx) {
	buffer_t *b;

	b = malloc(sizeof(*b));
	if (!b) return 0;
	b->buffer = malloc(size);
	if (!b->buffer) {
		free(b);
		return 0;
	}
	b->ctx = ctx;
	b->size = size;
	b->index = b->bytes = 0;
	b->read = func;

	return b;
}

void buffer_free(buffer_t *b) {
	if (!b) return;
	free(b->buffer);
	free(b);
}

int buffer_get(buffer_t *b, uint8_t *data, int datasz) {
	register int i;

	dprintf(8,"datasz: %d\n", datasz);

	for(i=0; i < datasz; i++) {
		dprintf(8,"i: %d, index: %d, bytes: %d\n", i, b->index, b->bytes);
		if (b->index >= b->bytes) {
			b->bytes = b->read(b->ctx, b->buffer, b->size);
			dprintf(8,"bytes: %d\n", b->bytes);
			if (b->bytes < 0) return -1;
			else if (b->bytes == 0) break;
			b->index = 0;
		}
		data[i] = b->buffer[b->index++];
	}
//	if (debug >= 8) bindump("data",data,i);
	return i;
}
