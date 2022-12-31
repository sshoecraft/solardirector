/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "jk.h"
#include "transports.h"

#if defined(__WIN32) || defined(__WIN64)
static solard_driver_t *jk_transports[] = { &ip_driver, &serial_driver, &rdev_driver, 0 };
#else
#ifdef BLUETOOTH
static solard_driver_t *jk_transports[] = { &ip_driver, &bt_driver, &serial_driver, &can_driver, &rdev_driver, 0 };
#else
static solard_driver_t *jk_transports[] = { &ip_driver, &serial_driver, &can_driver, &rdev_driver, 0 };
#endif
#endif

/* Function cannot return error */
int jk_tp_init(jk_session_t *s) {
	solard_driver_t *old_driver;
	void *old_handle;

	/* If it's open, close it */
	if (check_state(s,JK_STATE_OPEN)) jk_close(s);

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
	if (strlen(s->transport)) s->tp = find_driver(jk_transports,s->transport);
	dprintf(1,"s->tp: %p\n", s->tp);
	if (!s->tp) {
		/* Restore old driver & handle, open it and return */
		dprintf(1,"old_driver: %p, old_handle: %p\n", old_driver, old_handle);
		if (old_driver && old_handle) {
			dprintf(1,"restoring...\n");
			s->tp = old_driver;
			s->tp_handle = old_handle;
			return 1;
#if 0
			return jk_open(s);
		/* Otherwise fallback to null driver if enabled */
		} else if (s->tp_fallback) {
			dprintf(1,"setting null driver\n");
			s->tp = &null_driver;
#endif
		} else if (strlen(s->transport)) {
			sprintf(s->errmsg,"unable to find driver for transport: %s",s->transport);
			return 1;
		} else {
			sprintf(s->errmsg,"transport is empty!\n");
			return 1;
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
#if 0
		/* Otherwise fallback to null driver if enabled */
		} else if (s->tp_fallback) {
			s->tp = &null_driver;
			s->tp_handle = s->tp->new(0, 0, 0);
#endif
		} else {
			sprintf(s->errmsg,"unable to create new instance of %s driver",s->tp->name);
			return 1;
		}
	}

	/* Warn if using the null driver */
	if (strcmp(s->tp->name,"null") == 0) log_warning("using null driver for I/O");

#if 0
	/* Open the new driver */
	if (jk_open(s)) {
		sprintf(s->errmsg,"open failed!");
		return 1;
	}
#endif

	/* Close and free the old handle */
	dprintf(1,"old_driver: %p, old_handle: %p\n", old_driver, old_handle);
	if (old_driver && old_handle) {
		old_driver->close(old_handle);
		old_driver->destroy(old_handle);
	}
	return 0;
}

void *jk_new(void *driver, void *driver_handle) {
	jk_session_t *s;

	s = calloc(1,sizeof(*s));
	if (!s) {
		perror("jk_new: calloc");
		return 0;
	}
	if (driver && driver_handle) {
		s->tp = driver;
		s->tp_handle = driver_handle;
	}

	return s;
}

int jk_open(void *handle) {
	jk_session_t *s = handle;
	int r;

	dprintf(3,"s: %p\n", s);
	if (!s) return 1;
	dprintf(3,"open: %d\n", check_state(s,JK_STATE_OPEN));

	r = 0;
	if (!check_state(s,JK_STATE_OPEN)) {
		dprintf(1,"tp: %p\n", s->tp);
		dprintf(1,"tp->open: %p\n", s->tp->open);
		if (s->tp->open(s->tp_handle) == 0)
			set_state(s,JK_STATE_OPEN);
		else
			r = 1;
	}
	dprintf(3,"returning: %d\n", r);
	return r;
}

int jk_close(void *handle) {
	jk_session_t *s = handle;
	int r;

	dprintf(3,"s: %p\n", s);
	if (!s) return 1;
	dprintf(3,"open: %d\n", check_state(s,JK_STATE_OPEN));


	r = 0;
	/* If it's bluetooth, dont bother closing - it's a waste of time and stability */
	if (check_state(s,JK_STATE_OPEN) && strcmp(s->tp->name,"bt") != 0) {
		if (s->tp->close(s->tp_handle) == 0)
			clear_state(s,JK_STATE_OPEN);
		else
			r = 1;
	}
	dprintf(3,"returning: %d\n", r);
	return r;
}

static int jk_can_read(jk_session_t *s) {
	return 1;
}

void dshort(int i, uint8_t *p) {
	return;
	uint16_t dw = _getshort(p);
	int ival = _getlong(p);
	double wf10 = dw / 10.0;
	double wf1000 = dw / 1000.0;
	double if10 = ival / 10.0;
	double if1000 = ival / 1000.0;
	dprintf(1,"%02x: %d %04x d10: %f, d1000: %f\n", i, dw, dw, wf10, wf1000);
	dprintf(1,"%02x: %d %08x d10: %f, d1000: %f\n", i, ival, ival, if10, if1000);
}

static void _getvolts(jk_data_t *dp, uint8_t *data) {
	int i,j;
	double f;

	if (debug) bindump("getvolts",data,300);

	i = 6;
	/* 6: Cell voltages */
	for(j=0; j < 24; j++) {
		dp->cellvolt[j] = _getshort(&data[i]) / 1000.0;
		if (!dp->cellvolt[j]) break;
		dprintf(1,"cellvolt[%02d] = data[%02d] = %.3f\n", j, i, (_getshort(&data[i]) / 1000.0));
		i += 2;
	}
	dp->ncells = j;
	dprintf(4,"cells: %d\n", dp->ncells);
	while(i < 58) {
		dshort(i,&data[i]);
		i += 2;
	}
	dprintf(1,"i: %02x\n", i);
	/* 0x3a: Average cell voltage */
//	dp->avg_cell = data[34];
	dprintf(1,"cell_avg: %f\n", _getshort(&data[58]) / 1000.0);
	i += 2;
	/* 0x3c: Delta cell voltage */
	dprintf(1,"cell_diff: %f\n", _getshort(&data[60]) / 1000.0);
	i += 2;
	/* 0x3e: Current balancer?? */
	dprintf(1,"current: %d\n", _getshort(&data[62]));
	i += 2;
	dprintf(1,"i: %02x\n", i);
	/* 0x40: Cell resistances */
	for(j=0; j < 24; j++) {
		dp->cellres[j] = _getshort(&data[i]) / 1000.0;
		if (!dp->cellres[j]) break;
		dprintf(1,"cellres[%02d] = data[%02d] = %.3f\n", j, i, (_getshort(&data[i]) / 1000.0));
		i += 2;
	}
	dprintf(1,"i: %02x\n", i);
	while(i < 0x76) {
		dshort(i,&data[i]);
		i += 2;
	}
	/* 0x76: Battery voltage */
	dprintf(1,"i: %02x\n", i);
	dp->voltage = ((unsigned short)_getshort(&data[i])) / 1000.0;
	dprintf(1,"voltage: %.2f\n", dp->voltage);
	i += 2;
	while(i < 0x7e) {
		dshort(i,&data[i]);
		i += 2;
	}
	/* 0x7e: Battery current */
	dprintf(1,"i: %02x\n", i);
	dp->current = _getshort(&data[i]) / 1000.0;
	dprintf(1,"current: %.2f\n", dp->current);
	i += 2;
	while(i < 0x82) {
		dshort(i,&data[i]);
		i += 2;
	}
	/* 0x82: Temps */
	dprintf(1,"i: %02x\n", i);
	dp->ntemps = 2;
	dp->temps[0] = ((unsigned short)_getshort(&data[i])) / 10.0;
	dprintf(1,"temp 1: %f\n", dp->temps[0]);
	i += 2;
	dp->temps[1] = ((unsigned short)_getshort(&data[i])) / 10.0;
	dprintf(1,"temp 2: %f\n", dp->temps[1]);
	i += 2;
	dp->temps[2] = ((unsigned short)_getshort(&data[i])) / 10.0;
	dprintf(1,"MOS Temp: %f\n", dp->temps[2]);
	i += 2;
	dprintf(1,"i: %02x\n", i);
	while(i < 0x8e) {
		dshort(i,&data[i]);
		i += 2;
	}
#if 0
read.c(26) dshort: 8e: 59867 e9db d10: 5986.700195, d1000: 59.867001
read.c(27) dshort: 8e: 59867 0000e9db d10: 5986.700195, d1000: 59.867001
read.c(26) dshort: 90: 0 0000 d10: 0.000000, d1000: 0.000000
read.c(27) dshort: 90: 947912704 38800000 d10: 94791272.000000, d1000: 947912.687500
read.c(26) dshort: 92: 14464 3880 d10: 1446.400024, d1000: 14.464000
read.c(27) dshort: 92: 80000 00013880 d10: 8000.000000, d1000: 80.000000
	/* 0x8d: Percent remain */
	dprintf(1,"i: %02x\n", i);
	f = _getlong(&data[i]);
	dprintf(1,"% remain: %f\n", f);
	i += 4;
#endif
	/* 0x8e: Capacity remain */
	dprintf(1,"i: %02x\n", i);
	f = _getlong(&data[i]);
	dprintf(1,"capacity remain: %f\n", f);
	i += 4;
	/* 0x92: Nominal capacity */
	dprintf(1,"i: %02x\n", i);
	dp->capacity = _getlong(&data[i]) / 1000.0;
	dprintf(1,"capacity: %.2f\n", dp->capacity);
	i += 4;
	dprintf(1,"i: %02x\n", i);
	while(i < 0xd4) {
		dshort(i,&data[i]);
		i += 2;
	}
}

static void _getinfo(jk_hwinfo_t *info, uint8_t *data) {
	int i,j,uptime;

//	bindump("getInfo",data,300);

	j=0;
	/* Model */
	for(i=6; i < 300 && data[i]; i++) {
		info->model[j++] = data[i];
		if (j >= sizeof(info->model)-1) break;
	}
	info->model[j] = 0;
	dprintf(1,"Model: %s\n", info->model);
	/* Skip 0s */
	while(i < 300 && !data[i]) i++;
	/* HWVers */
	dprintf(1,"i: %x\n", i);
	j=0;
	while(i < 300 && data[i]) {
		info->hwvers[j++] = data[i++];
		if (j >= sizeof(info->hwvers)-1) break;
	}
	info->hwvers[j] = 0;
	dprintf(1,"HWVers: %s\n", info->hwvers);
	/* Skip 0s */
	while(i < 300 && !data[i]) i++;
	/* SWVers */
	j=0;
	dprintf(1,"i: %x\n", i);
	while(i < 300 && data[i]) {
		info->swvers[j++] = data[i++];
		if (j >= sizeof(info->swvers)-1) break;
	}
	info->swvers[j] = 0;
	dprintf(1,"SWVers: %s\n", info->swvers);
	/* Skip 0s */
	while(i < 300 && !data[i]) i++;
	dprintf(1,"i: %x\n", i);
	uptime = _getlong(&data[i]);
	dprintf(1,"uptime: %04x\n", uptime);
	i += 4;
//	unk = _getshort(&data[i]);
//	i += 2;
	/* Skip 0s */
	while(i < 300 && !data[i]) i++;
	/* Device */
	j=0;
	dprintf(1,"i: %x\n", i);
	while(i < 300 && data[i]) {
		info->device[j++] = data[i++];
		if (j >= sizeof(info->device)-1) break;
	}
	info->device[j] = 0;
	dprintf(1,"Device: %s\n", info->device);
}

#define GOT_RES 0x01
#define GOT_VOLT 0x02
#define GOT_INFO 0x04

static int getdata(jk_session_t *s, uint8_t *data, int bytes) {
	jk_hwinfo_t *info = &s->hwinfo;
	jk_data_t *dp = &s->data;
	uint8_t sig[] = { 0x55,0xAA,0xEB,0x90 };
	int i,j,start,r;

	r = 0;
//	bindump("data",data,bytes);
	for(i=j=0; i < bytes; i++) {
		dprintf(6,"data[%d]: %02x, sig[%d]: %02x\n", i, data[i], j, sig[j]);
		if (data[i] == sig[j]) {
			if (j == 0) start = i;
			j++;
			if (j >= sizeof(sig) && (start + 300) < bytes) {
				dprintf(1,"found sig, type: %d\n", data[i+1]);
				if (data[i+1] == 1)  {
#if BATTERY_CELLRES
					_getres(dp,&data[start]);
#endif
					r |= GOT_RES;
				} else if (data[i+1] == 2) {
					if (dp) _getvolts(dp,&data[start]);
					r |= GOT_VOLT;
				} else if (data[i+1] == 3) {
					if (!dp) _getinfo(info,&data[start]);
					r |= GOT_INFO;
				}
				i += 300;
				j = 0;
			}
		}
		if (r & GOT_VOLT) break;
	}
	dprintf(4,"returning: %d\n", r);
	return r;
}

int jk_bt_read(jk_session_t *s) {
	unsigned char getInfo[] =     { 0xaa,0x55,0x90,0xeb,0x97,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x11 };
	unsigned char getCellInfo[] = { 0xaa,0x55,0x90,0xeb,0x96,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10 };
	uint8_t data[2048];
	int bytes,r,retries;

	/* Have to getInfo before can getCellInfo ... */
	dprintf(1,"getInfo...\n");
	retries=5;
	while(retries--) {
		bytes = s->tp->write(s->tp_handle,0,getInfo,sizeof(getInfo));
		dprintf(4,"bytes: %d\n", bytes);
		if (bytes < 0) return -1;
		bytes = s->tp->read(s->tp_handle,0,data,sizeof(data));
		dprintf(4,"bytes: %d\n", bytes);
		if (bytes < 0) return -1;
		r = getdata(s,data,bytes);
		if (r & GOT_INFO) break;
		sleep(1);
	}
	dprintf(1,"retries: %d\n", retries);
	/* info-only flag?? */
//	if (!dp) return (retries < 1 ? -1 : 0);

	dprintf(1,"getCellInfo...\n");
	retries=5;
	bytes = s->tp->write(s->tp_handle,0,getCellInfo,sizeof(getCellInfo));
	dprintf(4,"bytes: %d\n", bytes);
	if (bytes < 0) return -1;
	while(retries--) {
		bytes = s->tp->read(s->tp_handle,0,data,sizeof(data));
		dprintf(4,"bytes: %d\n", bytes);
		r = getdata(s,data,bytes);
		if (r & GOT_VOLT) break;
	}
	return (retries < 1 ? -1 : 0);
}

static int jk_std_read(jk_session_t *s) {
	jk_data_t *dp = &s->data;
	uint8_t data[128];
	int i,j,bytes;
//	struct jk_protect prot;

	dprintf(3,"getting HWINFO...\n");
	if ((bytes = jk_rw(s, JK_CMD_READ, JK_CMD_HWINFO, data, sizeof(data))) < 0) {
		dprintf(1,"returning 1!\n");
		return 1;
	}

	dp->voltage = (double)jk_getshort(&data[0]) / 100.0;
	dp->current = (double)jk_getshort(&data[2]) / 100.0;
	dp->capacity = (double)jk_getshort(&data[6]) / 100.0;
        dprintf(2,"voltage: %.2f\n", dp->voltage);
        dprintf(2,"current: %.2f\n", dp->current);
        dprintf(2,"capacity: %.2f\n", dp->capacity);

	/* Balance */
	dp->balancebits = jk_getshort(&data[12]);
	dp->balancebits |= jk_getshort(&data[14]) << 16;
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
//	jk_get_protect(&prot,jk_getshort(&data[16]));

	s->fetstate = data[20];
	dprintf(2,"s->fetstate: %02x\n", s->fetstate);
	if (s->fetstate & JK_MOS_CHARGE) set_state(dp,JK_STATE_CHARGING);
	else clear_state(dp,JK_STATE_CHARGING);
	if (s->fetstate & JK_MOS_DISCHARGE) set_state(dp,JK_STATE_DISCHARGING);
	else clear_state(dp,JK_STATE_DISCHARGING);

	dp->ncells = data[21];
	dprintf(2,"cells: %d\n", dp->ncells);
	dp->ntemps = data[22];

	/* Temps */
#define CTEMP(v) ( (v - 2731) / 10 )
	/* 1st temp is MOS - we dont care about it */
	j=0;
	for(i=1; i < dp->ntemps; i++) {
		dp->temps[j++] = CTEMP((double)jk_getshort(&data[23+(i*2)]));
	}
	dp->ntemps--;

	/* Cell volts */
	if ((bytes = jk_rw(s, JK_CMD_READ, JK_CMD_CELLINFO, data, sizeof(data))) < 0) return 1;
	for(i=0; i < dp->ncells; i++) dp->cellvolt[i] = (double)jk_getshort(&data[i*2]) / 1000;

#ifdef DEBUG
	for(i=0; i < dp->ncells; i++) dprintf(2,"cell[%d]: %.3f\n", i, dp->cellvolt[i]);
#endif

	return 0;
}

static int jk_can_get_hwinfo(jk_session_t *s) {
	return 1;
}

static int jk_std_get_hwinfo(jk_session_t *s) {
	return 0;
}

int jk_get_hwinfo(jk_session_t *s) {
	int r;

	dprintf(5,"transport: %s\n", s->tp->name);
	if (strncmp(s->tp->name,"can",3)==0) 
		r = jk_can_get_hwinfo(s);
	else if (strcmp(s->tp->name,"bt")==0 || (strcmp(s->transport,"rdev")==0 && strncmp(s->topts,"bt",2) == 0))
		r = jk_bt_read(s);
	else
		r = jk_std_get_hwinfo(s);
	dprintf(1,"r: %d\n", r);
	return r;
}

static int jk_read(void *handle, uint32_t *control, void *buf, int buflen) {
	jk_session_t *s = handle;
	solard_battery_t *bp = &s->data;
	double v,min,max,avg,tot;
	int i,r;

	dprintf(2,"transport: %s\n", s->tp->name);
	r = 1;
	memset(bp,0,sizeof(*bp));
	if (strcmp(s->tp->name,"can")==0) 
		r = jk_can_read(s);
	else if (strcmp(s->tp->name,"bt")==0 || (strcmp(s->transport,"rdev") == 0 && strncmp(s->topts,"bt",2) == 0)) 
		r = jk_bt_read(s);
	else
		r = jk_std_read(s);
	dprintf(1,"r: %d\n", r);
	if (r) return 1;
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
        bp->cell_min = min;
        bp->cell_max = max;
        bp->cell_diff = max - min;
        bp->cell_avg = avg;
        bp->cell_total = tot;
        if (s->balancing) set_state(bp,JK_STATE_BALANCING);
        else clear_state(bp,JK_STATE_BALANCING);

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

solard_driver_t jk_driver = {
	"jk",
	jk_new,				/* New */
	0,				/* Destroy */
	jk_open,			/* Open */
	jk_close,			/* Close */
	jk_read,			/* Read */
	0,				/* Write */
	jk_config			/* Config */
};
