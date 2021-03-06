
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "smanet_internal.h"
#include <time.h>

#define dlevel 6

#define CONTROL_GROUP 0x80
#define CONTROL_RESPONSE 0x40
#define CONTROL_BLOCK  0x20

smanet_packet_t *smanet_alloc_packet(int data_len) {
	smanet_packet_t *p;

	dprintf(1,"data_len: %d\n", data_len);

	p = calloc(1,sizeof(*p));
	if (!p) return 0;
	p->data = malloc(data_len);
	if (!p->data) {
		free(p);
		return 0;
	}
	p->datasz = data_len;
	return p;
}

void smanet_free_packet(smanet_packet_t *p) {
	if (!p) return;
	free(p->data);
	free(p);
	return;
}

struct __attribute__((packed, aligned(1))) packet_header {
	uint16_t src;
	uint16_t dest;
	uint8_t control;
	uint8_t count;
	uint8_t command;
};

int smanet_recv_packet(smanet_session_t *s, uint8_t count, int command, smanet_packet_t *packet, int timeout) {
	uint8_t data[284];
	int len,data_len;
	struct packet_header *h;
	uint8_t cmo;

	if (!s || !packet) return 1;

	dprintf(1,"command: %02x, count: %02x, timeout: %d\n", command, count, timeout);
	cmo = (uint8_t) ((char) count) - 1;
	dprintf(1,"cmo: %02x\n", cmo);

	len = smanet_read_frame(s,data,sizeof(data),5);
	if (len < 0) return -1;
	if (len == 0) {
		dprintf(1,"timeout!\n");
		return 2;
	}
	if (debug >= dlevel) bindump("recv packet",data,len);

	/* This method works on little endian */
	h = (struct packet_header *) data; 
	dprintf(1,"src: %04x, dest: %04x, control: %02x, count: %d, command: %02x\n",
		h->src, h->dest, h->control, h->count, h->command);
	/* Not the packet we're looking for, re-q the request */
	dprintf(1,"h->command: %02x, command: %02x, test: %d\n", h->command, command, h->command != command);
	dprintf(1,"h->count: %02x, cmo: %02x, test: %d\n", h->count, cmo, h->count && h->count != cmo);
	if ((h->count && h->count != cmo) || h->command != command) return 3;
	packet->src = h->src;
	packet->dest = h->dest;
	packet->control = h->control;
	packet->count = h->count;
	packet->command = h->command;
	if (packet->control & CONTROL_GROUP) packet->group = 1;
	if (packet->control & CONTROL_RESPONSE) packet->response = 1;
	if (packet->control & CONTROL_BLOCK) packet->block = 1;

	dprintf(1,"sizeof(h): %d\n", sizeof(*h));
	data_len = len - 7;
 	dprintf(1,"data_len: %d\n", data_len);
	if (packet->dataidx + data_len > packet->datasz) {
		packet->datasz += 1024;
		dprintf(1,"NEW datasz: %d\n", packet->datasz);
		packet->data = realloc(packet->data,packet->datasz);
		if (!packet->data) return 1;
	}
	if (data_len) {
		memcpy(&packet->data[packet->dataidx],data + sizeof(*h),data_len);
		packet->dataidx += data_len;
	}
	return 0;
}

int smanet_send_packet(smanet_session_t *s, uint16_t src, uint16_t dest, uint8_t ctrl, uint8_t cnt, uint8_t cmd, uint8_t *buffer, int buflen) {
	uint8_t data[264];
	int i;

	dprintf(1,"src: %04x, dest: %04x, ctrl: %02x, cnt: %d, cmd: %02x, buffer: %p, buflen: %d\n",
		src,dest,ctrl,cnt,cmd,buffer,buflen);
	if (buflen > 255) return 1;

	*((uint16_t *)&data[0]) = src;
	*((uint16_t *)&data[2]) = dest;
	i = 4;
	data[i++] = ctrl;
	data[i++] = cnt;
	data[i++] = cmd;
	memcpy(&data[i],buffer,buflen);
	i += buflen;
	if (debug >= dlevel) bindump("send packet",data,i);
	smanet_write_frame(s,data,i);
	return 0;
}
