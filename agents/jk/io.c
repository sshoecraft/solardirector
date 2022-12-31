
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "jk.h"
#include "can.h"

#define MIN_CMD_LEN 7

uint16_t jk_crc(unsigned char *data, int len) {
	uint16_t crc = 0;
	register int i;

//	bindump("jk_crc",data,len);
	dprintf(5,"len: %d\n", len);
	dprintf(5,"crc: %x\n", crc);
	for(i=0; i < len; i++) crc -= data[i];
	dprintf(5,"crc: %x\n", crc);
	return crc;
}

int jk_verify(uint8_t *buf, int len) {
	uint16_t my_crc,pkt_crc;
	int i,data_length;

	/* Anything less than 7 bytes is an error */
	dprintf(3,"len: %d\n", len);
	if (len < 7) return 1;
	if (debug >= 5) bindump("verify",buf,len);

	i=0;
	/* 0: Start bit */
	dprintf(5,"start bit: %x\n", buf[i]);
	if (buf[i++] != 0xDD) return 1;
	/* 1: Register */
	dprintf(5,"register: %x\n", buf[i]);
	i++;
	/* 2: Status */
	dprintf(5,"status: %d\n", buf[i]);
//	if (buf[i++] != 0) return 1;
	i++;
	/* 3: Length - must be size of packet minus protocol bytes */
	data_length = buf[i++];
	dprintf(5,"data_length: %d, len - 7: %d\n", data_length, len - 7);
	if (data_length != (len - 7)) return 1;
	/* Data */
	my_crc = jk_crc(&buf[2],data_length+2);
	i += data_length;
	/* CRC */
	pkt_crc = jk_getshort(&buf[i]);
	dprintf(5,"my_crc: %x, pkt_crc: %x\n", my_crc, pkt_crc);
	if (my_crc != pkt_crc) {
		dprintf(1,"CRC ERROR: my_crc: %x, pkt_crc: %x\n", my_crc, pkt_crc);
		bindump("data",buf,len);
		return 1;
	}
	i += 2;
	/* Stop bit */
	dprintf(5,"stop bit: %x\n", buf[i]);
	if (buf[i++] != 0x77) return 1;

	dprintf(3,"good data!\n");
	return 0;
}

int jk_cmd(uint8_t *pkt, int pkt_size, int action, uint16_t reg, uint8_t *data, int data_len) {
	unsigned short crc;
	int idx;

	/* Make sure no data in command */
	if (action == JK_CMD_READ) data_len = 0;

	dprintf(5,"action: %x, reg: %x, data: %p, len: %d\n", action, reg, data, data_len);

	memset(pkt,0,pkt_size);
	idx = 0;
	pkt[idx++] = JK_PKT_START;
	pkt[idx++] = action;
	pkt[idx++] = reg;
	pkt[idx++] = data_len;
	dprintf(1,"idx: %d, data_len: %d, pkt_size: %d\n", idx, data_len, pkt_size);
	if (idx + data_len > pkt_size) return -1;
	memcpy(&pkt[idx],data,data_len);
	crc = jk_crc(&pkt[2],data_len+2);
	idx += data_len;
	jk_putshort(&pkt[idx],crc);
	idx += 2;
	pkt[idx++] = JK_PKT_END;
//	bindump("pkt",pkt,idx);
	dprintf(5,"returning: %d\n", idx);
	return idx;
}

#define         CRC_16_POLYNOMIALS  0xa001    
unsigned short jk_can_crc(unsigned char *pchMsg) {
	unsigned char i, chChar;
	unsigned short wCRC = 0xFFFF;
	int wDataLen = 6;

	while (wDataLen--) {
		chChar = *pchMsg++;
		wCRC ^= (unsigned short) chChar;
		for (i = 0; i < 8; i++) {
			if (wCRC & 0x0001)
				wCRC = (wCRC >> 1) ^ CRC_16_POLYNOMIALS;
			else
				wCRC >>= 1;
		}
	}
	return wCRC;
}

int jk_can_get(jk_session_t *s, uint32_t id, unsigned char *data, int datalen, int chk) {
        unsigned short crc, mycrc;
	int retries,bytes,len;
	struct can_frame frame;

	dprintf(3,"id: %03x, data: %p, len: %d, chk: %d\n", id, data, datalen, chk);
	retries = 3;
	do {
		/* First, write a 0 len frame to the ID */
		frame.can_id = id;
		frame.can_dlc = 0;
		if (s->tp->write(s->tp_handle,&id,&frame,sizeof(frame)) < 0) return 1;
		/* Then read the response */
		bytes = s->tp->read(s->tp_handle,&id,&frame,sizeof(frame));
		dprintf(3,"bytes: %d\n", bytes);
		if (bytes < 0) return 1;
		memset(data,0,datalen);
		dprintf(3,"can_dlc: %d, datalen: %d\n", frame.can_dlc, datalen);
		len = (frame.can_dlc > datalen ? datalen : frame.can_dlc);
		memcpy(data,frame.data,len);
		if (!chk) break;
		/* Verify CRC */
		crc = jk_getshort(&data[6]);
		dprintf(3,"crc: %x\n", crc);
		mycrc = jk_can_crc(data);
		dprintf(3,"mycrc: %x\n", mycrc);
		if (crc == 0 || crc == mycrc) break;
	} while(retries--);
	dprintf(3,"retries: %d\n", retries);
	if (!retries) printf("ERROR: CRC failed retries for ID %03x!\n", id);
	return (retries < 0);
}

int jk_can_get_crc(jk_session_t *s, uint32_t id, unsigned char *data, int len) {
	return jk_can_get(s,id,data,len,1);
}

int jk_rw(jk_session_t *s, uint8_t action, uint8_t reg, uint8_t *data, int datasz) {
	uint8_t cmd[256],buf[256];
	int cmdlen,bytes,retries;

	dprintf(5,"action: %x, reg: %x, data: %p, datasz: %d\n", action, reg, data, datasz);
	cmdlen = jk_cmd(cmd, sizeof(cmd), action, reg, data, datasz);
	/* Must be at least 5 long */
	dprintf(1,"cmdlen: %d, min: %d\n", cmdlen, MIN_CMD_LEN);
	if (cmdlen < MIN_CMD_LEN) return -1;
	if (debug >= 5) bindump("cmd",cmd,cmdlen);

	/* Read the data */
	retries=3;
	while(1) {
		dprintf(5,"retries: %d\n", retries);
		if (!retries--) {
			dprintf(5,"returning: -1\n");
			return -1;
		}
		dprintf(5,"writing...\n");
		bytes = s->tp->write(s->tp_handle,0,cmd,cmdlen);
		dprintf(5,"bytes: %d\n", bytes);
		memset(data,0,datasz);
		bytes = s->tp->read(s->tp_handle,0,buf,sizeof(buf));
		dprintf(5,"bytes: %d\n", bytes);
		if (bytes < 0) return -1;
		if (!jk_verify(buf,bytes)) break;
		sleep(1);
	}
	memcpy(data,&buf[4],buf[3]);
	dprintf(5,"returning: %d\n",buf[3]);
	return buf[3];
}

#if 0
int jk_eeprom_start(jk_session_t *s) {
	uint8_t payload[2] = { 0x56, 0x78 };

	dprintf(1,"starting...\n");
	return jk_rw(s, JK_CMD_WRITE, JK_REG_EEPROM, payload, sizeof(payload) );
}

int jk_eeprom_end(jk_session_t *s) {
	uint8_t payload[2] = { 0x00, 0x00 };

	dprintf(1,"ending...\n");
	return jk_rw(s, JK_CMD_WRITE, JK_REG_CONFIG, payload, sizeof(payload) );
}

int jk_set_mosfet(jk_session_t *s, int val) {
	uint8_t payload[2];
	int r;

	dprintf(2,"val: %x\n", val);
	jk_putshort(payload,val);
	if (jk_eeprom_start(s) < 0) return 1;
	r = jk_rw(s, JK_CMD_WRITE, JK_REG_MOSFET, payload, sizeof(payload));
	if (jk_eeprom_end(s) < 0) return 1;
	return (r < 0 ? 1 : 0);
}
#endif
