
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "jbd.h"
#include "transports.h"
#include <pthread.h>

#define MIN_CMD_LEN 7

#if defined(__WIN32) || defined(__WIN64)
solard_driver_t *jbd_transports[] = { &ip_driver, &serial_driver, 0 };
#else
#ifdef BLUETOOTH
solard_driver_t *jbd_transports[] = { &ip_driver, &bt_driver, &serial_driver, &can_driver, 0 };
#else
solard_driver_t *jbd_transports[] = { &ip_driver, &serial_driver, &can_driver, 0 };
#endif
#endif

/* init the transport */
int jbd_tp_init(jbd_session_t *s) {
	solard_driver_t *old_driver;
	void *old_handle;

	/* If it's open, close it */
	if (check_state(s,JBD_STATE_OPEN)) jbd_close(s);

	/* Save current driver & handle */
	dprintf(1,"s->tp: %p, s->tp_handle: %p\n", s->tp, s->tp_handle);
	if (s->tp && s->tp_handle) {
		old_driver = s->tp;
		old_handle = s->tp_handle;
	} else {
		old_driver = 0;
		old_handle = 0;
	}

	/* Find the driver */
	dprintf(1,"transport: %s\n", s->transport);
	if (strlen(s->transport)) s->tp = find_driver(jbd_transports,s->transport);
	dprintf(1,"s->tp: %p\n", s->tp);
	if (!s->tp) {
		/* Restore old driver & handle, open it and return */
		dprintf(1,"old_driver: %p, old_handle: %p\n", old_driver, old_handle);
		if (old_driver && old_handle) {
			dprintf(1,"restoring...\n");
			s->tp = old_driver;
			s->tp_handle = old_handle;
			return 1;
		} else if (strlen(s->transport)) {
			sprintf(s->errmsg,"unable to find driver for transport: %s",s->transport);
			return 1;
		} else {
			sprintf(s->errmsg,"transport is empty!\n");
			return 2;
		}
	}

	/* Open new one */
	dprintf(1,"using driver: %s\n", s->tp->name);
	s->tp_handle = s->tp->new(s->target, s->topts);
	dprintf(1,"tp_handle: %p\n", s->tp_handle);
	if (!s->tp_handle) {
		dprintf(1,"old_driver: %p, old_handle: %p\n", old_driver, old_handle);
		/* Restore old driver */
		if (old_driver && old_handle) {
			dprintf(1,"restoring...\n");
			s->tp = old_driver;
			s->tp_handle = old_handle;
			/* Make sure we dont close it below */
			old_driver = 0;
		} else {
			sprintf(s->errmsg,"unable to create new instance %s driver",s->tp->name);
			return 1;
		}
	}

	/* Warn if using the null driver */
	if (strcmp(s->tp->name,"null") == 0) log_warning("using null driver for I/O\n");

	/* Open the new driver */
//	jbd_open(s);

	/* Close and free the old handle */
	dprintf(1,"old_driver: %p, old_handle: %p\n", old_driver, old_handle);
	if (old_driver && old_handle) {
		old_driver->close(old_handle);
		old_driver->destroy(old_handle);
	}
	return 0;
}

uint16_t jbd_crc(unsigned char *data, int len) {
	uint16_t crc = 0;
	register int i;

//	bindump("jbd_crc",data,len);
	dprintf(5,"len: %d\n", len);
	dprintf(5,"crc: %x\n", crc);
	for(i=0; i < len; i++) crc -= data[i];
	dprintf(5,"crc: %x\n", crc);
	return crc;
}

int jbd_verify(uint8_t *buf, int len) {
	uint16_t my_crc,pkt_crc;
	int i,data_length;

	/* Anything less than 7 bytes is an error */
	dprintf(3,"len: %d\n", len);
	if (len < 7) return 1;
#ifdef DEBUG
	if (debug >= 5) bindump("verify",buf,len);
#endif

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
	my_crc = jbd_crc(&buf[2],data_length+2);
	i += data_length;
	/* CRC */
	pkt_crc = jbd_getshort(&buf[i]);
	dprintf(5,"my_crc: %x, pkt_crc: %x\n", my_crc, pkt_crc);
	if (my_crc != pkt_crc) {
		log_error("CRC ERROR: my_crc: %x, pkt_crc: %x\n", my_crc, pkt_crc);
//		bindump("data",buf,len);
		return 1;
	}
	i += 2;
	/* Stop bit */
	dprintf(5,"stop bit: %x\n", buf[i]);
	if (buf[i++] != 0x77) return 1;

	dprintf(3,"good data!\n");
	return 0;
}

int jbd_cmd(uint8_t *pkt, int pkt_size, int action, uint16_t reg, uint8_t *data, int data_len) {
	unsigned short crc;
	int idx;

	/* Make sure no data in command */
	if (action == JBD_CMD_READ) data_len = 0;

	dprintf(3,"action: %x, reg: %x, data: %p, len: %d\n", action, reg, data, data_len);

	memset(pkt,0,pkt_size);
	idx = 0;
	pkt[idx++] = JBD_PKT_START;
	pkt[idx++] = action;
	pkt[idx++] = reg;
	pkt[idx++] = data_len;
	dprintf(5,"idx: %d, data_len: %d, pkt_size: %d\n", idx, data_len, pkt_size);
	if (idx + data_len > pkt_size) return -1;
	memcpy(&pkt[idx],data,data_len);
	crc = jbd_crc(&pkt[2],data_len+2);
	idx += data_len;
	jbd_putshort(&pkt[idx],crc);
	idx += 2;
	pkt[idx++] = JBD_PKT_END;
//	bindump("pkt",pkt,idx);
	dprintf(3,"returning: %d\n", idx);
	return idx;
}

#define         CRC_16_POLYNOMIALS  0xa001    
unsigned short jbd_can_crc(unsigned char *pchMsg) {
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

int jbd_can_get(jbd_session_t *s, uint32_t id, unsigned char *data, int datalen, int chk) {
        unsigned short crc, mycrc;
	int retries,bytes,len;
	struct can_frame frame;

	dprintf(5,"id: %03x, data: %p, len: %d, chk: %d\n", id, data, datalen, chk);
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
		crc = jbd_getshort(&data[6]);
		dprintf(3,"crc: %x\n", crc);
		mycrc = jbd_can_crc(data);
		dprintf(3,"mycrc: %x\n", mycrc);
		if (crc == 0 || crc == mycrc) break;
	} while(retries--);
	dprintf(5,"retries: %d\n", retries);
	if (!retries) printf("ERROR: CRC failed retries for ID %03x!\n", id);
	return (retries < 0);
}

int jbd_can_get_crc(jbd_session_t *s, int id, unsigned char *data, int len) {
	return jbd_can_get(s,id,data,len,1);
}

int jbd_rw(jbd_session_t *s, uint8_t action, uint8_t reg, uint8_t *data, int datasz) {
	uint8_t cmd[256],buf[256];
	int cmdlen,bytes,retries;

	dprintf(5,"action: %x, reg: %x, data: %p, datasz: %d\n", action, reg, data, datasz);
	cmdlen = jbd_cmd(cmd, sizeof(cmd), action, reg, data, datasz);
	/* Must be at least 5 long */
	dprintf(5,"cmdlen: %d, min: %d\n", cmdlen, MIN_CMD_LEN);
	if (cmdlen < MIN_CMD_LEN) return -1;
#ifdef DEBUG
	if (debug >= 5) bindump("cmd",cmd,cmdlen);
#endif

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
		if (!jbd_verify(buf,bytes)) break;
		sleep(1);
	}
	memcpy(data,&buf[4],buf[3]);
	dprintf(5,"returning: %d\n",buf[3]);
	return buf[3];
}

int jbd_eeprom_open(jbd_session_t *s) {
	uint8_t payload[2] = { 0x56, 0x78 };
	int r;

	dprintf(4,"opening...\n");
	r = jbd_rw(s, JBD_CMD_WRITE, JBD_REG_EEPROM, payload, sizeof(payload) );
	if (r >= 0) set_state(s,JBD_STATE_EEPROM_OPEN);
	return r;
}

int jbd_eeprom_close(jbd_session_t *s) {
	uint8_t payload[2] = { 0x00, 0x00 };
	int r;

	dprintf(4,"closing...\n");
	r = jbd_rw(s, JBD_CMD_WRITE, JBD_REG_CONFIG, payload, sizeof(payload) );
	if (r >= 0) clear_state(s,JBD_STATE_EEPROM_OPEN);
	return r;
}

int jbd_set_mosfet(jbd_session_t *s, int val) {
	uint8_t payload[2];
	int r;

	dprintf(5,"val: %x\n", val);
	jbd_putshort(payload,val);
	if (!check_state(s,JBD_STATE_OPEN) && jbd_open(s)) return 1;
	if (jbd_eeprom_open(s) < 0) return 1;
	r = jbd_rw(s, JBD_CMD_WRITE, JBD_REG_MOSFET, payload, sizeof(payload));
	if (jbd_eeprom_close(s) < 0) return 1;
	return (r < 0 ? 1 : 0);
}

int jbd_get_balance(jbd_session_t *s) {
	uint8_t payload[2];
	int r,bits;

	if (!check_state(s,JBD_STATE_OPEN) && jbd_open(s)) return -1;
	if (jbd_eeprom_open(s) < 0) return -1;
	r = jbd_rw(s, JBD_CMD_READ, JBD_REG_FUNCMASK, payload, sizeof(payload));
	dprintf(0,"r: %d\n", r);
	if (jbd_eeprom_close(s) < 0) return -1;
	else bits = (jbd_getshort(payload) & (JBD_FUNC_BALANCE_EN | JBD_FUNC_CHG_BALANCE));
	dprintf(0,"bits: %d\n", bits);
	return (r < 0 ? -1 : bits);
}

int jbd_set_balance(jbd_session_t *s, int newbits) {
	uint8_t payload[2];
	int r,bits;

	dprintf(5,"newbits: %x\n", newbits);

	if (!check_state(s,JBD_STATE_OPEN) && jbd_open(s)) return 1;
	if (jbd_eeprom_open(s) < 0) return 1;
	r = jbd_rw(s, JBD_CMD_READ, JBD_REG_FUNCMASK, payload, sizeof(payload));
	dprintf(0,"r: %d\n", r);
	if (r == 2) {
		bits = jbd_getshort(payload);
		dprintf(0,"bits: %x\n", bits);
		bits &= ~(JBD_FUNC_BALANCE_EN | JBD_FUNC_CHG_BALANCE);
		bits |= newbits;
		dprintf(0,"bits: %x\n", bits);
		jbd_putshort(payload,bits);
		r = jbd_rw(s, JBD_CMD_WRITE, JBD_REG_FUNCMASK, payload, sizeof(payload));
		dprintf(0,"r: %d\n", r);
	}
	if (jbd_eeprom_close(s) < 0) return 1;
	return (r < 0 ? 1 : 0);
}

int jbd_reset(jbd_session_t *s) {
	uint8_t payload[2];
	uint16_t val;
	int r;

	if (!check_state(s,JBD_STATE_OPEN) && jbd_open(s)) return 1;
	val = 0x4321;
	jbd_putshort(payload,val);
	r = jbd_rw(s, JBD_CMD_WRITE, JBD_REG_RESET, payload, sizeof(payload));
	if (!r) {
		val = 0x1881;
		jbd_putshort(payload,val);
		r = jbd_rw(s, JBD_CMD_WRITE, JBD_REG_FRESET, payload, sizeof(payload));
	}
        return r;
}

void jbd_get_protect(struct jbd_protect *p, unsigned short bits) {
#ifdef DEBUG
	{
		char bitstr[40];
		unsigned short mask = 1;
		int i;

		i = 0;
		while(mask) {
			bitstr[i++] = ((bits & mask) != 0 ? '1' : '0');
			mask <<= 1;
		}
		bitstr[i] = 0;
		dprintf(5,"protect: %s\n",bitstr);
	}
#endif
#if 0
Bit0 monomer overvoltage protection
Bit1 monomer under voltage protection
Bit2 whole group overvoltage protection
Bit3 whole group under voltage protection
Bit4 charging over temperature protection
Bit5 charging low temperature protection
Bit6 discharge over temperature protection
Bit7 discharge low temperature protection
Bit8 charging over-current protection
Bit9 discharge over current protection
Bit10 short circuit protection
Bit11 front end detection IC error
Bit12 software lock MOS
Bit13 ~ bit15 reserved
bit0	......
Single overvoltage protection
bit1	......
Single undervoltage protection
bit2	......
Whole group overvoltage protection
bit3	......
Whole group undervoltage protection
bit4	......
Charge over temperature protection
bit5	......
Charge low temperature protection
bit6	......
Discharge over temperature protection
bit7	......
Discharge low temperature protection
bit8	......
Charge overcurrent protection
bit9	......
Discharge overcurrent protection
bit10	....
Short circuit protection
bit11	....IC..
Front detection IC error
bit12	....MOS
Software lock MOS
      bit13~bit15	..
Reserved

#endif
}

/* Read fetstate into s->fetstate */
int jbd_can_get_fetstate(struct jbd_session *s) {
	uint8_t data[8];

	/* 0x103 FET control status, production date, software version */
	if (jbd_can_get_crc(s,0x103,data,8)) return 1;
	s->fetstate = jbd_getshort(&data[0]);
	dprintf(2,"fetstate: %02x\n", s->fetstate);
	return 0;
}

/* For CAN bus only */
int jbd_can_read(struct jbd_session *s) {
	jbd_data_t *dp = &s->data;
	uint8_t data[8];
	int id,i;
	uint16_t protectbits;
	struct jbd_protect prot;

	/* 0x102 Equalization state low byte, equalized state high byte, protection status */
	if (jbd_can_get_crc(s,0x102,data,8)) return 1;
	dp->balancebits = jbd_getshort(&data[0]);
	dp->balancebits |= jbd_getshort(&data[2]) << 16;
	protectbits = jbd_getshort(&data[4]);
	/* Do we have any protection actions? */
	if (protectbits) {
		jbd_get_protect(&prot,protectbits);
	}

	/* 0x103 FET control status, production date, software version */
	if (jbd_can_get_crc(s,0x103,data,8)) return 1;
	s->fetstate = jbd_getshort(&data[0]);
	dprintf(2,"s->fetstate: %02x\n", s->fetstate);
	if (s->fetstate & JBD_MOS_CHARGE) set_state(dp,JBD_STATE_CHARGING);
	else clear_state(dp,JBD_STATE_CHARGING);
	if (s->fetstate & JBD_MOS_DISCHARGE) set_state(dp,JBD_STATE_DISCHARGING);
	else clear_state(dp,JBD_STATE_DISCHARGING);

	if (jbd_can_get_crc(s,0x104,data,8)) return 1;
	dp->ncells = data[0];
	dprintf(1,"strings: %d\n", dp->ncells);
	dp->ntemps = data[1];
	dprintf(1,"probes: %d\n", dp->ntemps);

	/* Get Temps */
	i = 0;
#define CTEMP(v) ( (v - 2731) / 10 )
	for(id = 0x105; id < 0x107; id++) {
		if (jbd_can_get_crc(s,id,data,8)) return 1;
		dp->temps[i++] = CTEMP((double)jbd_getshort(&data[0]));
		if (i >= dp->ntemps) break;
		dp->temps[i++] = CTEMP((double)jbd_getshort(&data[2]));
		if (i >= dp->ntemps) break;
		dp->temps[i++] = CTEMP((double)jbd_getshort(&data[4]));
		if (i >= dp->ntemps) break;
	}

	/* Cell volts */
	i = 0;
	for(id = 0x107; id < 0x111; id++) {
		if (jbd_can_get_crc(s,id,data,8)) return 1;
		dp->cellvolt[i++] = (double)jbd_getshort(&data[0]) / 1000;
		if (i >= dp->ncells) break;
		dp->cellvolt[i++] = (double)jbd_getshort(&data[2]) / 1000;
		if (i >= dp->ncells) break;
		dp->cellvolt[i++] = (double)jbd_getshort(&data[4]) / 1000;
		if (i >= dp->ncells) break;
	}

	return 0;
}

static int jbd_std_get_fetstate(jbd_session_t *s) {
	uint8_t data[128];
	int bytes;

	dprintf(3,"getting HWINFO...\n");
	if ((bytes = jbd_rw(s, JBD_CMD_READ, JBD_CMD_HWINFO, data, sizeof(data))) < 0) {
		dprintf(1,"returning 1!\n");
		return 1;
	}
	s->fetstate = data[20];
	dprintf(2,"fetstate: %02x\n", s->fetstate);
	return 0;
}

int jbd_std_read(jbd_session_t *s) {
	jbd_data_t *dp = &s->data;
	uint8_t data[128];
	int i,bytes;;
	struct jbd_protect prot;

	dprintf(3,"getting HWINFO...\n");
	if ((bytes = jbd_rw(s, JBD_CMD_READ, JBD_CMD_HWINFO, data, sizeof(data))) < 0) {
		dprintf(1,"returning 1!\n");
		return 1;
	}

	dp->voltage = (double)jbd_getshort(&data[0]) / 100.0;
	dp->current = (double)jbd_getshort(&data[2]) / 100.0;
	dp->capacity = (double)jbd_getshort(&data[6]) / 100.0;
        dprintf(2,"voltage: %.2f\n", dp->voltage);
        dprintf(2,"current: %.2f\n", dp->current);
        dprintf(2,"capacity: %.2f\n", dp->capacity);

	/* Balance */
	dp->balancebits = jbd_getshort(&data[12]);
	dp->balancebits |= jbd_getshort(&data[14]) << 16;
#ifdef DEBUG
	{
		char bits[40];
		unsigned short mask = 1;
		i = 0;
		while(mask) {
			bits[i++] = ((dp->balancebits & mask) != 0 ? '1' : '0');
			mask <<= 1;
		}
		bits[i] = 0;
		dprintf(5,"balancebits: %s\n",bits);
	}
#endif

	/* Protectbits */
	jbd_get_protect(&prot,jbd_getshort(&data[16]));

	s->fetstate = data[20];
	dprintf(2,"s->fetstate: %02x\n", s->fetstate);
	if (s->fetstate & JBD_MOS_CHARGE) set_state(dp,JBD_STATE_CHARGING);
	else clear_state(dp,JBD_STATE_CHARGING);
	if (s->fetstate & JBD_MOS_DISCHARGE) set_state(dp,JBD_STATE_DISCHARGING);
	else clear_state(dp,JBD_STATE_DISCHARGING);

	dp->ncells = data[21];
	dprintf(2,"cells: %d\n", dp->ncells);
	dp->ntemps = data[22];

	/* Temps */
#define CTEMP(v) ( (v - 2731) / 10 )
	for(i=0; i < dp->ntemps; i++) {
		dp->temps[i] = CTEMP((double)jbd_getshort(&data[23+(i*2)]));
	}

	/* Cell volts */
	if ((bytes = jbd_rw(s, JBD_CMD_READ, JBD_CMD_CELLINFO, data, sizeof(data))) < 0) return 1;
	for(i=0; i < dp->ncells; i++) dp->cellvolt[i] = (double)jbd_getshort(&data[i*2]) / 1000;

#ifdef DEBUG
	for(i=0; i < dp->ncells; i++) dprintf(2,"cell[%d]: %.3f\n", i, dp->cellvolt[i]);
#endif

	return 0;
}

int jbd_get_fetstate(jbd_session_t *s) {
	int r;

	if (!check_state(s,JBD_STATE_OPEN) && jbd_open(s)) return 1;

	dprintf(2,"transport: %s\n", s->tp->name);
	if (strncmp(s->tp->name,"can",3)==0) 
		r = jbd_can_get_fetstate(s);
	else
		r = jbd_std_get_fetstate(s);
	return r;
}

void *jbd_new(void *transport, void *transport_handle) {
	jbd_session_t *s;

	s = calloc(1,sizeof(*s));
	if (!s) {
		log_syserr("jbd_new: calloc");
		return 0;
	}
//	pthread_mutex_init(&s->lock,0);
	if (transport && transport_handle) {
		s->tp = transport;
		s->tp_handle = transport_handle;
#if 0
		if (strcmp(s->tp->name,"can") == 0 &&_can_init(s)) {
			free(s);
			return 0;
		}
#endif
	}

	return s;
}

int jbd_free(void *handle) {
	jbd_session_t *s = handle;

	if (check_state(s,JBD_STATE_OPEN)) jbd_close(s);
	if (s->tp->destroy) s->tp->destroy(s->tp_handle);
	free(s);
	return 0;
}

int jbd_open(void *handle) {
	jbd_session_t *s = handle;
	int r;

	dprintf(3,"s: %p\n", s);

	r = 0;
	if (!check_state(s,JBD_STATE_OPEN)) {
		if (s->tp && s->tp->open(s->tp_handle) == 0)
			set_state(s,JBD_STATE_OPEN);
		else
			r = 1;
	}
	dprintf(3,"returning: %d\n", r);
	return r;
}

int jbd_close(void *handle) {
	jbd_session_t *s = handle;
	int r;

	dprintf(3,"s: %p\n", s);
	r = 0;
	if (check_state(s,JBD_STATE_OPEN)) {
		if (s->tp && s->tp->close(s->tp_handle) == 0)
			clear_state(s,JBD_STATE_OPEN);
		else
			r = 1;
	}
	dprintf(3,"returning: %d\n", r);
	return r;
}

int jbd_read(void *handle, uint32_t *what, void *buf, int buflen) {
	jbd_session_t *s = handle;
	solard_battery_t *bp = &s->data;
	double v,min,max,avg,tot;
	int i;

	dprintf(1,"reader: %p\n", s->reader);
	if (!s->reader) {
		if (!s->tp) {
			log_error("jbd_read: tp is null!\n");
			return 1;
		}
		dprintf(2,"transport: %s\n", s->tp->name);
		if (strncmp(s->tp->name,"can",3)==0) 
			s->reader = jbd_can_read;
		else
			s->reader = jbd_std_read;
	}

	if (!check_state(s,JBD_STATE_OPEN) && jbd_open(s)) return 1;
	memset(bp,0,sizeof(*bp));
	if (s->reader(s)) {
		jbd_close(s);
		return 1;
	}
	strncat(bp->name,s->ap->instance_name,sizeof(bp->name)-1);
	bp->power = bp->voltage * bp->current;
	bp->one = s->start_at_one;

	/* min/max/avg/total */
	min = 9999999;
	tot = max = 0;
	for(i=0; i < bp->ncells; i++) {
		v = bp->cellvolt[i];
		if (v < min) min = v;
		else if (v > max) max = v;
		tot += v;
	}
	avg = tot / bp->ncells;
	bp->cell_min = pround(min,1);
	bp->cell_max = pround(max,1);
	bp->cell_diff = pround(max - min,3);
	bp->cell_avg = pround(avg,3);
	bp->cell_total = pround(tot,3);
	if (s->balancing) set_state((bp),JBD_STATE_BALANCING);
	else clear_state((bp),JBD_STATE_BALANCING);

	dprintf(2,"bp->state: %x\n", bp->state);

#ifdef JS
	/* If JS and read script exists, we'll use that */
	if (agent_script_exists(s->ap, s->ap->js.read_script)) return 0;
#endif
#ifdef MQTT
	if (mqtt_connected(s->ap->m)) {
		json_value_t *v = (s->flatten ? battery_to_flat_json(bp) : battery_to_json(bp));
		dprintf(2,"v: %p\n", v);
		if (v) {
			agent_pubdata(s->ap, v);
			json_destroy_value(v);
		}	
	}
#endif
#ifdef INFLUX
	if (influx_connected(s->ap->i)) {
		json_value_t *v = battery_to_flat_json(bp);
		dprintf(2,"v: %p\n", v);
		if (v) {
			influx_write_json(s->ap->i, "battery", v);
			json_destroy_value(v);
		}	
	}
#endif
	dprintf(2,"log_power: %d\n", s->log_power);
	if (s->log_power && (bp->power != s->last_power)) {
		log_info("%.1f\n",bp->power);
		s->last_power = bp->power;
	}
	return 0;
}

solard_driver_t jbd_driver = {
	"jbd",
	jbd_new,			/* New */
	jbd_free,			/* Free */
	jbd_open,			/* Open */
	jbd_close,			/* Close */
	jbd_read,			/* Read */
	0,				/* Write */
	jbd_config			/* Config */
};
