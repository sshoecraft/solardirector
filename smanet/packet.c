
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "smanet_internal.h"
#include <time.h>

#define dlevel 4

#define CONTROL_GROUP 0x80
#define CONTROL_RESPONSE 0x40
#define CONTROL_BLOCK  0x20

smanet_packet_t *smanet_alloc_packet(int data_len) {
	smanet_packet_t *p;

	dprintf(dlevel,"data_len: %d\n", data_len);

	p = malloc(sizeof(*p));
	dprintf(dlevel,"p: %p\n", p);
	if (!p) return 0;
	memset(p,0,sizeof(*p));
	p->data = malloc(data_len);
	dprintf(dlevel,"p->data: %p\n", p->data);
	if (!p->data) {
		free(p);
		return 0;
	}
	p->datasz = data_len;
	return p;
}

void smanet_free_packet(smanet_packet_t *p) {
	dprintf(dlevel,"p: %p\n", p);
	if (!p) return;
	free(p->data);
	free(p);
	return;
}

int smanet_recv_packet(smanet_session_t *s, uint8_t rcount, int rcmd, smanet_packet_t *packet, int timeout) {
	uint8_t data[284];
	int len,data_len;
	uint16_t src, dest;
	uint8_t control,hcount,hcmd,cmo,*newd;
#if SMANET_RECV_THREAD
	time_t start,now,diff;
	struct rdata *rp;
#endif

	dprintf(dlevel,"packet: %p\n", packet);
	if (!s || !packet) return 1;

	dprintf(dlevel,"command: %02x, count: %02x, timeout: %d\n", rcmd, rcount, timeout);

#if SMANET_RECV_THREAD
	time(&start);
	do {
		rp = list_get_first(s->frames);
		dprintf(dlevel+1,"rp: %p\n", rp);
		if (!rp) {
			time(&now);
			diff = now - start;
			dprintf(dlevel+1,"diff: %d\n", diff);
			if (diff >= 5) return 2;
			sleep(1);
		}
	} while(!rp);
	len = rp->len;
	memcpy(data,rp->data,len);
	list_delete(s->frames,rp);
#else
	len = smanet_read_frame(s,data,PACKET_DATA_SIZE,5);
	if (len < 0) return -1;
	if (len == 0) {
		dprintf(dlevel,"timeout!\n");
		return 2;
	}
#endif
//	if (debug >= dlevel+1) bindump("recv packet",data,len);

	/* Get header */
	src = _getu16(&data[0]);
	dest = _getu16(&data[2]);
	control = data[4];
	hcount = data[5];
	hcmd = data[6];
	dprintf(dlevel,"src: %04x, dest: %04x, control: %02x, hcount: %d, hcmd: %02x\n",
		src, dest, control, hcount, hcmd);

	/* Not the packet we're looking for, re-q the request */
	dprintf(dlevel,"hcmd: %02x, rcmd: %02x, test: %d\n", hcmd, rcmd, hcmd != rcmd);
	cmo = (uint8_t) ((char) rcount) - 1;
	dprintf(dlevel,"hcount: %02x, cmo: %02x, test: %d\n", hcount, cmo, hcount && hcount != cmo);
	if ((hcount && hcount != cmo) || hcmd != rcmd) return 3;
	packet->src = src;
	packet->dest = dest;
	packet->control = control;
	packet->count = hcount;
	packet->command = hcmd;
	if (packet->control & CONTROL_GROUP) packet->group = 1;
	if (packet->control & CONTROL_RESPONSE) packet->response = 1;
	if (packet->control & CONTROL_BLOCK) packet->block = 1;

//	dprintf(dlevel,"sizeof(h): %d\n", sizeof(*h));
	data_len = len - 7;
	dprintf(dlevel,"packet->dataidx: %d, data_len: %d, packet->datasz: %d\n", packet->dataidx, data_len, packet->datasz);
	if (packet->dataidx + data_len > packet->datasz) {
		packet->datasz *= 2;
		dprintf(dlevel,"NEW datasz: %d\n", packet->datasz);
		newd = realloc(packet->data,packet->datasz);
		if (newd) packet->data = newd;
		else return 1;
	}
	if (data_len) {
		dprintf(dlevel,"packet->data: %p, datasz: %d\n", packet->data, packet->datasz);
		memcpy(&packet->data[packet->dataidx],&data[7],data_len);
		packet->dataidx += data_len;
	}
	return 0;
}

int smanet_send_packet(smanet_session_t *s, uint16_t src, uint16_t dest, uint8_t ctrl, uint8_t cnt, uint8_t cmd, uint8_t *buffer, int buflen) {
	uint8_t data[264];
	int i;

	dprintf(dlevel,"src: %04x, dest: %04x, ctrl: %02x, cnt: %d, cmd: %02x, buffer: %p, buflen: %d\n",
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
//	if (debug >= dlevel) bindump("send packet",data,i);
//	bindump("send packet",data,i);
	smanet_write_frame(s,data,i);
	return 0;
}
