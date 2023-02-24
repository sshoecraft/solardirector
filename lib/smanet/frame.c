
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 7
#include "debug.h"

#include "smanet_internal.h"
#include "fcstab.h"

enum STATES {
	STATE_UNKNOWN,
	STATE_START,
	STATE_ADDR,
	STATE_CTRL,
	STATE_PROTOH,
	STATE_PROTOL,
	STATE_PACKET,
};

#define SMANET_PROT 0x4041
#define SMANET_ACCM 0x000E0000
// 11,12, and 13
//if (ch == 11 || ch == 12 || ch == 13) 

uint16_t calcfcs(uint8_t *buffer, int buflen) {
	register uint16_t fcs;
	register uint8_t *p;

//	bindump("fcs buffer", buffer, buflen);

	/* init */
	fcs = 0xffff;
	/* calc */
	for(p=buffer; p < buffer + buflen; p++) fcs = (uint16_t)((fcs >> 8) ^ fcstab[(uint8_t)((fcs ^ *p) & 0xff)]);
	/* complement */
	fcs ^= 0xffff;
//	dprintf(dlevel,"fcs: %04x\n", fcs);
	return fcs;
}

/* PPP encapsulation:  a variable length protocol that doesnt have the length in the header... */
int smanet_read_frame(smanet_session_t *s, uint8_t *buffer, int buflen, int timeout) {
//	uint8_t buf[8192];
	uint8_t data[284], ch;
//	register uint8_t *bptr, *eptr;
	int i,j,bytes,state,have_esc,count;
	uint16_t proto,fcs,frame_fcs;
	time_t start,cur,diff;
	int have_start;

	dprintf(dlevel,"buffer: %p, buflen: %d, timeout: %d\n", buffer, buflen, timeout);

	time(&start);
	have_start = have_esc = i = 0;
//	bptr = eptr = buf;
	while(1) {
		if (timeout > 0) {
			time(&cur);
			diff = cur - start;
			dprintf(dlevel,"diff: %d\n", diff);
			if (diff >= timeout) {
				s->timeouts++;
				dprintf(dlevel,"timeout!\n");
				return 0;
			}
		}
#if SMANET_USE_BUFFER
		bytes = buffer_get(s->b,&ch,1);
#else
		bytes = s->tp->read(s->tp_handle,0,&ch,1);
#endif
		dprintf(dlevel,"bytes: %d\n", bytes);
		if (bytes < 0) {
			dprintf(dlevel,"read error!\n");
			return -1;
		}
		if (bytes == 0) {
			if (timeout < 0) return 0;
			continue;
		}
		dprintf(dlevel,"ch[%d]: %02X\n", i, ch);
		if (have_start) {
			if (ch == 0x7e) break;
			if (have_esc) {
				ch ^= 0x20;
				dprintf(dlevel,"NEW ch: %02X\n", ch);
				have_esc = 0;
			} else if (ch == 0x7d) {
				have_esc = 1;
				continue;
			}
			data[i++] = ch;
		} else if (ch == 0x7e) {
			i = 0;
			have_start = 1;
		}
	}
	if (!i) {
		dprintf(dlevel,"no data?\n");
		return 0;
	}
//	if (debug >= dlevel+1) bindump("frame data",data,i);
//	bindump("frame data",data,i);
	frame_fcs = data[i-1] << 8 | data[i-2];
	fcs = calcfcs(data,i-2);
	dprintf(dlevel,"fcs: %04x, frame_fcs: %04x\n", fcs, frame_fcs);
	if (fcs != frame_fcs) {
		dprintf(dlevel,"fcs mismatch (fcs: %04x, frame_fcs: %04x)\n", fcs, frame_fcs);
		return 0;
	}
	count = i - 2;

	/* we already have the 0x7E start byte */
	state = STATE_ADDR;
	for(i=j=0; i < count; i++) {
		dprintf(dlevel,">>> state: %d\n", state);
		ch = data[i];
		dprintf(dlevel,"ch: %02X\n", ch);
		switch(state) {
		case STATE_START:
			dprintf(dlevel,"want: 0x7e, got: 0x%02x\n", ch);
			if (ch == 0x7e) state++;
			break;
		case STATE_ADDR:
			dprintf(dlevel,"want: 0xff, got: 0x%02x\n", ch);
			/* Could have gotten 7e 7e (end of last frame + start of new one) */
			if (ch == 0x7e) continue;
			else if (ch != 0xFF) break;
			else state++;
			break;
		case STATE_CTRL:
			dprintf(dlevel,"want: 0x03, got: 0x%02x\n", ch);
			if (ch != 0x03) break;
			else state++;
			break;
		case STATE_PROTOH:
			proto = ch << 8;
			state++;
			break;
		case STATE_PROTOL:
			proto |= ch;
			dprintf(dlevel,"proto: %04x\n", proto);
			if (proto != SMANET_PROT) break;
			else state++;
			break;
		case STATE_PACKET:
			buffer[j++] = ch;
			break;
		}
	}
	dprintf(dlevel,"returning: %d\n", j);
	return j;
}

static int copy2buf(register uint8_t *dest, register uint8_t *buffer, int buflen) {
	register uint8_t *bptr;
	register int i;
	uint8_t *eptr;
//	uint32_t mask;

//	bindump("copy2buf",buffer,buflen);
	i = 0;
	eptr = buffer + buflen;
	for(bptr = buffer; bptr < eptr; bptr++) {
		dprintf(dlevel+1,"ch: %02X\n",*bptr);
#if 0
		if (*bptr < 0x20) {
			mask = 1 << *bptr;
			dprintf(dlevel,"mask: %08x\n",mask);
			if (*bptr == 0x7e || *bptr == 0x7e || (SMANET_ACCM & mask)) {
				dprintf(dlevel+1,"escaping...\n");
				dest[i++] = 0x7d;
				dest[i++] = *bptr ^ 0x20;
			} else {
				dest[i++] = *bptr;
			}
		} else {
			dest[i++] = *bptr;
		}
#else
		if (*bptr == 0x11 || *bptr == 0x12 || *bptr == 0x13 || *bptr == 0x7e || *bptr == 0x7d) {
			dprintf(dlevel+1,"escaping...\n");
			dest[i++] = 0x7d;
			dest[i++] = *bptr ^ 0x20;
		} else {
			dest[i++] = *bptr;
		}
#endif
	}
	dprintf(dlevel,"returning: %d\n", i);
	return i;
}

int smanet_write_frame(smanet_session_t *s, uint8_t *buffer, int buflen) {
	uint8_t data[284],fcsd[2];
	register uint8_t *p,*p2;
	register uint16_t fcs;
	int bytes;

	/* Init fcs - if you change any header values, recalc */
//	fcs = 0xffff;
	fcs = 0x3c7c;

	p = data;
	*p++ = 0x7e;
	*p++ = 0xff;
	*p++ = 0x03;
	*p++ = 0x40;
	*p++ = 0x41;
//	for(p2=&data[1]; p2 < p; p2++) fcs = (uint16_t)((fcs >> 8) ^ fcstab[(uint8_t)((fcs ^ *p2) & 0xff)]);
//	dprintf(dlevel,"HEADER fcs: %04x\n", fcs);
	for(p2=buffer; p2 < buffer + buflen; p2++) fcs = (uint16_t)((fcs >> 8) ^ fcstab[(uint8_t)((fcs ^ *p2) & 0xff)]);
	fcs ^= 0xffff;
	p += copy2buf(p,buffer,buflen);
	*(uint16_t *)fcsd = fcs;
	p += copy2buf(p,fcsd,2);
	*p++ = 0x7e;
//	if (debug >= dlevel+1) bindump("put frame",data,p - data);
	bytes = s->tp->write(s->tp_handle,0,data,p - data);
	dprintf(dlevel,"bytes: %d\n", bytes);
	if (bytes < 0) return -1;
	return 0;
}
