
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "osendian.h"
#define TARGET_ENDIAN LITTLE_ENDIAN
#include "types.h"

#define dlevel 2
#include "debug.h"

#define DISABLE_WRITE 0
#define DEBUG_BITS 1

#include "si.h"
#include <pthread.h>
#ifdef __APPLE__
#include <signal.h>
#elif !defined(__WIN32)
#include <sys/signal.h>
#endif
#include "transports.h"
#include <math.h>

solard_driver_t *si_transports[] = { &can_driver, &rdev_driver, 0 };

#define SDATA(f,b) ((int16_t *) &s->frames[f].data[b]);
#define BDATA(f,b) ((int8_t *) &s->frames[f].data[b]);
#define UBDATA(f,b) ((uint8_t *) &s->frames[f].data[b]);
#define USDATA(f,b) ((uint16_t *) &s->frames[f].data[b]);
#define LDATA(f,b) ((int32_t *) &s->frames[f].data[b]);

static void _setraw(si_session_t *s) {
	struct si_raw_data *rdp = &s->raw_data;

	/* x300 Active power grid/gen */
	rdp->active_grid_l1 = SDATA(0,0);
	rdp->active_grid_l2 = SDATA(0,2);
	rdp->active_grid_l3 = SDATA(0,4);

	/* x301 Active power Sunny Island */
	rdp->active_si_l1 = SDATA(1,0);
	rdp->active_si_l2 = SDATA(1,2);
	rdp->active_si_l3 = SDATA(1,4);

	/* x302 Reactive power grid/gen */
	rdp->reactive_grid_l1 = SDATA(2,0);
	rdp->reactive_grid_l2 = SDATA(2,2);
	rdp->reactive_grid_l3 = SDATA(2,4);

	/* x303 Reactive power Sunny Island */
	rdp->reactive_si_l1 = SDATA(3,0);
	rdp->reactive_si_l2 = SDATA(3,2);
	rdp->reactive_si_l3 = SDATA(3,4);

	/* 0x304 AC1 Voltage L1 / AC1 Voltage L2 / AC1 Voltage L3 / AC1 Frequency */
	rdp->ac1_voltage_l1 = SDATA(4,0);
	rdp->ac1_voltage_l2 = SDATA(4,2);
	rdp->ac1_voltage_l3 = SDATA(4,4);
	rdp->ac1_frequency = SDATA(4,6);

	/* 0x305 Battery voltage Battery current Battery temperature SOC battery */
	rdp->battery_voltage = SDATA(5,0);
	rdp->battery_current = SDATA(5,2);
	rdp->battery_temp = SDATA(5,4);
	rdp->battery_soc = SDATA(5,6);

	/* 0x306 SOH battery / Charging procedure / Operating state / active Error Message / Battery Charge Voltage Set-point */
	rdp->battery_soh = SDATA(6,0);
	rdp->charging_proc = BDATA(6,2);
	rdp->state = BDATA(6,3);
	rdp->errmsg = SDATA(6,4);
	rdp->battery_cvsp = SDATA(6,6);

	/* 0x307 Relay state / Relay function bit 1 / Relay function bit 2 / Synch-Bits */
	rdp->relay_state = USDATA(7,0);
	rdp->relay_bits1 = USDATA(7,2);
	rdp->relay_bits2 = USDATA(7,4);
	rdp->synch_bits = UBDATA(7,6);

	/* 0x308 TotLodPwr */
	rdp->TotLodPwr = SDATA(8,0);

	/* 0x309 AC2 Voltage L1 / AC2 Voltage L2 / AC2 Voltage L3 / AC2 Frequency */
	rdp->ac2_voltage_l1 = SDATA(9,0);
	rdp->ac2_voltage_l2 = SDATA(9,2);
	rdp->ac2_voltage_l3 = SDATA(9,4);
	rdp->ac2_frequency = SDATA(9,6);

	/* 0x30A PVPwrAt / GdCsmpPwrAt / GdFeedPwr */
	rdp->PVPwrAt = SDATA(10,0);
	rdp->GdCsmpPwrAt = SDATA(10,2);
	rdp->GdFeedPwr = SDATA(10,4);
}

static int _getcur(si_session_t *s, char *what, si_current_source_t *spec, double *dest) {
	uint8_t data[8];
	double val;

	dprintf(dlevel,"spec: id: %03x, offset: %d, size: %d, type: %d, mult: %f\n",
		spec->can.id, spec->can.offset, spec->can.size, spec->type, spec->mult);
	if (spec->can.offset > 7) {
		log_warning("%s_current_source offset > 7, using 0\n",what);
		spec->can.offset = 0;
	}
	if (s->can_read(s,spec->can.id,data,8)) return 1;
	switch(spec->can.size) {
	case 1:
		val = data[spec->can.offset];
		break;
	case 2:
		val = _getshort(&data[spec->can.offset]);
		break;
	case 4:
		val = _getlong(&data[spec->can.offset]);
		break;
	default:
		val = 0;
		log_warning("%s_current_source size must be 1,2, or 4",what);
		break;
	}
	if (!double_equals(spec->mult,0.0)) val *= spec->mult;
	dprintf(dlevel,"val: %.1f\n", val);
	*dest = val;
	return 0;
}

void si_can_get_relays(si_session_t *s) {
	si_raw_data_t *rdp = &s->raw_data;

#define RS(m,b) { s->data.m = ((_gets16(rdp->relay_state) & b) != 0); }
	RS(relay1,	0x0001);
	RS(relay2,	0x0002);
	RS(s1_relay1,	0x0004);
	RS(s1_relay2,	0x0008);
	RS(s2_relay1,	0x0010);
	RS(s2_relay2,	0x0020);
	RS(s3_relay1,	0x0040);
	RS(s3_relay2,	0x0080);
	RS(GnRn,	0x0100);
	RS(s1_GnRn,	0x0200);
	RS(s2_GnRn,	0x0400);
	RS(s3_GnRn,	0x0800);

	s->data.GnOn = (s->data.GnRn && s->data.AutoGn && s->data.ac2_frequency > 45.0) ? 1 : 0;

	/* 2-3 Relay function bit 1 */
#define RB1(m,b) { s->data.m = ((_gets16(rdp->relay_bits1) & b) != 0); }
	RB1(AutoGn,	0x0001);
	RB1(AutoLodExt,	0x0002);
	RB1(AutoLodSoc,	0x0004);
	RB1(Tm1,	0x0008);
	RB1(Tm2,	0x0010);
	RB1(ExtPwrDer,	0x0020);
	RB1(ExtVfOk,	0x0040);
	RB1(GdOn,	0x0080);
	RB1(Error,	0x0100);
	RB1(Run,	0x0200);
	RB1(BatFan,	0x0400);
	RB1(AcdCir,	0x0800);
	RB1(MccBatFan,	0x1000);
	RB1(MccAutoLod,	0x2000);
	RB1(Chp,	0x4000);
	RB1(ChpAdd,	0x8000);

	/* 4 & 5 Relay function bit 2 */
#define RB2(m,b) { s->data.m = ((_gets16(rdp->relay_bits2) & b) != 0); }
	RB2(SiComRemote,0x0001);
	RB2(OverLoad,	0x0002);
	RB2(ExtSrcConn,	0x0100);
	RB2(Silent,	0x0200);
	RB2(Current,	0x0400);
	RB2(FeedSelfC,	0x0800);
	RB2(Esave,	0x1000);
}

#define GETFVAL(val) (double)_gets16(val)
#define GETFS100(val) (((double)_gets16(val))*100)
#define GETF100(val) (((double)_gets16(val))*100)
#define GETD10(val) (((double)_gets16(val))/10)
#define GETD100(val) (((double)_gets16(val))/100)

int si_can_get_data(si_session_t *s) {
	si_raw_data_t *rdp = &s->raw_data;

//	printf("\n\n***** GETTING DATA *****\n\n\n");

	s->data.active_grid_l1 = GETF100(rdp->active_grid_l1);
	s->data.active_grid_l2 = GETF100(rdp->active_grid_l2);
	s->data.active_grid_l3 = GETF100(rdp->active_grid_l3);

	s->data.active_si_l1 = GETF100(rdp->active_si_l1);
	s->data.active_si_l2 = GETF100(rdp->active_si_l2);
	s->data.active_si_l3 = GETF100(rdp->active_si_l3);

	s->data.reactive_grid_l1 = GETF100(rdp->reactive_grid_l1);
	s->data.reactive_grid_l2 = GETF100(rdp->reactive_grid_l2);
	s->data.reactive_grid_l3 = GETF100(rdp->reactive_grid_l3);

	s->data.reactive_si_l1 = GETF100(rdp->reactive_si_l1);
	s->data.reactive_si_l2 = GETF100(rdp->reactive_si_l2);
	s->data.reactive_si_l3 = GETF100(rdp->reactive_si_l3);

	s->data.ac1_voltage_l1 =  GETD10(rdp->ac1_voltage_l1);
	s->data.ac1_voltage_l2 =  GETD10(rdp->ac1_voltage_l2);
	s->data.ac1_voltage_l3 =  GETD10(rdp->ac1_voltage_l3);
	s->data.ac1_voltage = s->data.ac1_voltage_l1 + (double_equals(s->data.ac1_voltage_l2,0.0) ? s->data.ac1_voltage_l3 : s->data.ac1_voltage_l2);
	s->data.ac1_frequency = GETD100(rdp->ac1_frequency);

	s->data.battery_voltage =  GETD10(rdp->battery_voltage);
	s->data.battery_current = GETD10(rdp->battery_current);
	s->data.battery_power = s->data.battery_voltage * s->data.battery_current;
	s->data.battery_temp = GETD10(rdp->battery_temp);
	s->data.battery_soc = GETD10(rdp->battery_soc);

	s->data.battery_soh = GETFVAL(rdp->battery_soh);
	s->data.charging_proc = _gets8(rdp->charging_proc);
	s->data.state = _gets8(rdp->state);
	s->data.errmsg = GETFVAL(rdp->errmsg);
	s->data.battery_cvsp = GETD10(rdp->battery_cvsp);

	si_can_get_relays(s);
	dprintf(dlevel,"running: %d\n", s->running);
	if (s->running == -1) {
		s->running = s->data.Run;
		dprintf(dlevel,"Initial run state: %d\n", s->running);
	}

	if (s->data.Run != s->running) {
		log_info("NEW run state: %d\n", s->data.Run);
		s->running = s->data.Run;
		if (s->running) s->startup = 1;
		if (s->ap) agent_event(s->ap, "State", s->running ? "Run" : "Stop"); 
	}

	s->data.TotLodPwr = GETFS100(rdp->TotLodPwr);

	s->data.ac2_voltage_l1 = GETD10(rdp->ac2_voltage_l1);
	s->data.ac2_voltage_l2 = GETD10(rdp->ac2_voltage_l2);
	s->data.ac2_voltage_l3 = GETD10(rdp->ac2_voltage_l3);
	s->data.ac2_voltage = s->data.ac2_voltage_l1 + (double_equals(s->data.ac2_voltage_l2,0.0) ? s->data.ac2_voltage_l3 : s->data.ac2_voltage_l2);
	s->data.ac2_frequency = GETD100(rdp->ac2_frequency);

	s->data.PVPwrAt =  GETD10(rdp->PVPwrAt);
	s->data.GdCsmpPwrAt = GETD10(rdp->GdCsmpPwrAt);
	s->data.GdFeedPwr = GETD10(rdp->GdFeedPwr);

	dprintf(dlevel,"input.source: %d\n", s->input.source);
	if (s->input.source == CURRENT_SOURCE_CAN) {
		if (s->input.type == CURRENT_TYPE_WATTS) {
			if (_getcur(s, "input", &s->input, &s->data.ac2_power)) return 1;
			s->data.ac2_current = s->data.ac2_power / s->data.ac2_voltage_l1;
		} else {
			if (_getcur(s, "input", &s->input, &s->data.ac2_current)) return 1;
			s->data.ac2_power = s->data.ac2_current * s->data.ac2_voltage_l1;
		}
//		dprintf(dlevel,"ac2_current: %.1f, ac2_power: %.1f\n", s->data.ac2_current, s->data.ac2_power);
	} else if (s->input.source == CURRENT_SOURCE_CALCULATED) {
		double n,t,a;

		n = t = a = 0.0;
//		dprintf(dlevel,"active_grid: l1: %lf, l2: %lf, l3: %lf\n", s->data.active_grid_l1, s->data.active_grid_l2, s->data.active_grid_l3);
		if (!double_equals(s->data.active_grid_l1,0.0)) { t += s->data.active_grid_l1; n++; }
		if (!double_equals(s->data.active_grid_l2,0.0)) { t += s->data.active_grid_l2; n++; }
		if (!double_equals(s->data.active_grid_l3,0.0)) { t += s->data.active_grid_l3; n++; }
		if (n) a = t / n;
		else a = 0;
		dprintf(dlevel,"a: %f, t: %f, n: %f\n", a, t, n);

		s->data.ac2_power = (t + a) * (-1);
		s->data.ac2_current = s->data.ac2_power / s->data.ac2_voltage;
		dprintf(dlevel,"ac2_current: %.1f, ac2_power: %.1f\n", s->data.ac2_current, s->data.ac2_power);
	}

//	dprintf(dlevel,"output.source: %d\n", s->output.source);
	if (s->output.source == CURRENT_SOURCE_CAN) {
		if (s->output.type == CURRENT_TYPE_WATTS) {
			if (_getcur(s, "output", &s->output, &s->data.ac1_power)) return 1;
			s->data.ac1_current = s->data.ac1_power / s->data.ac1_voltage_l1;
		} else {
			if (_getcur(s, "output", &s->output, &s->data.ac1_current)) return 1;
			s->data.ac1_power = s->data.ac1_current * s->data.ac1_voltage_l1;
		}
//		dprintf(dlevel,"ac1_current: %.1f, ac1_power: %.1f\n", s->data.ac1_current, s->data.ac1_power);
	} else if (s->output.source == CURRENT_SOURCE_CALCULATED) {
		double n,t,a;

		n = t = a = 0.0;
		if (!double_equals(s->data.active_si_l1,0.0)) { t += s->data.active_si_l1; n++; }
		if (!double_equals(s->data.active_si_l2,0.0)) { t += s->data.active_si_l2; n++; }
		if (!double_equals(s->data.active_si_l3,0.0)) { t += s->data.active_si_l3; n++; }
		if (n) a = t / n;
		else a = 0;
//		dprintf(dlevel,"a: %f, t: %f, n: %f\n", a, t, n);

		s->data.ac1_power = (t + a) * (-1);
		s->data.ac1_current = s->data.ac1_power / s->data.ac1_voltage;
		dprintf(dlevel,"ac1_current: %.1f, ac1_power: %.1f\n", s->data.ac1_current, s->data.ac1_power);
	}

	return 0;
}

static void *si_can_recv_thread(void *handle) {
	si_session_t *s = handle;
	struct can_frame frame;
	int bytes,fidx;
	uint32_t can_id;
//	uint32_t mask;
#if !defined(__WIN32) && !defined(__APPLE__)
	sigset_t set;

	/* Ignore SIGPIPE */
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	sigprocmask(SIG_BLOCK, &set, NULL);
#elif defined(__APPLE__)
	/* On macOS, use signal() instead of sigprocmask */
	signal(SIGPIPE, SIG_IGN);
#endif

	can_id = 0xffff;
	dprintf(dlevel,"thread starting\n");
	set_state(s,SI_STATE_STARTED);
	while(check_state(s,SI_STATE_RUNNING)) {
		dprintf(8,"%d: open: %d\n", getpid(), check_state(s,SI_STATE_OPEN));
		if (!check_state(s,SI_STATE_OPEN)) {
//			memset(&s->bitmap,0,sizeof(s->bitmap));
			sleep(1);
			continue;
		}
		dprintf(8,"reading...\n");
		bytes = s->can->read(s->can_handle,&can_id,&frame,sizeof(frame));
		dprintf(8,"%d: bytes: %d\n", getpid(),bytes);
		if (bytes < 1) {
//			memset(&s->bitmap,0,sizeof(s->bitmap));
			sleep(1);
			continue;
		}
		dprintf(8,"frame.can_id: %03x\n",frame.can_id);
		if (frame.can_id < 0x300 || frame.can_id > 0x30f) continue;
//		bindump("frame",&frame,sizeof(frame));
		fidx = frame.can_id - 0x300;
//		mask = 1 << frame.can_id;
//		dprintf(8,"id: %03x, fidx: %x, mask: %08x, bitmap: %08x\n", frame.can_id, fidx, mask, s->bitmap);
		dprintf(8,"id: %03x, fidx: %x\n", frame.can_id, fidx);
		memcpy(&s->frames[fidx],&frame,sizeof(frame));
		time(&s->times[fidx]);
//		s->bitmap |= mask;
	}
	dprintf(dlevel,"thread exiting\n");
	clear_state(s,SI_STATE_STARTED);
	return 0;
}

static int si_can_get_local(si_session_t *s, uint32_t id, uint8_t *data, int datasz) {
//	uint32_t mask;
	int fidx,retries,len;
	int r;
	time_t t;

	dprintf(dlevel,"id: %03x, data: %p, len: %d\n", id, data, datasz);
	if (id < 0x300 || id > 0x30f) return 1;

	fidx = id - 0x300;
	dprintf(dlevel,"id: %03x, fidx: %x\n", id, fidx);
	retries=5;
	r = 1;
	time(&t);
	while(retries--) {
		dprintf(dlevel,"times[%d], time: %d, diff: %d\n", s->times[fidx], t, t - s->times[fidx]);
		if (t - s->times[fidx] > 5) {
			sleep(1);
			continue;
		}
		len = (datasz > 8 ? 8 : datasz);
		memcpy(data,s->frames[fidx].data,len);
		r = 0;
		break;
	}
	dprintf(dlevel,"returning: %d\n", r);
	return r;
}

/* Func for can data that is remote (dont use thread/messages) */
static int si_can_get_remote(si_session_t *s, uint32_t id, uint8_t *data, int datasz) {
	int retries,bytes,len;
	struct can_frame frame;

	dprintf(dlevel,"id: %03x, data: %p, data_len: %d\n", id, data, datasz);
	retries=5;
	while(retries--) {
		bytes = s->can->read(s->can_handle,&id,&frame,sizeof(frame));
		dprintf(dlevel,"bytes read: %d\n", bytes);
		if (bytes < 0) return 1;
		dprintf(dlevel,"sizeof(frame): %d\n", sizeof(frame));
		if (bytes == sizeof(frame)) {
			len = (frame.can_dlc > datasz ? datasz : frame.can_dlc);
			memcpy(data,&frame.data,len);
//			if (debug >= 7) bindump("FROM DRIVER",data,len);
			break;
		}
		sleep(1);
	}
	dprintf(dlevel,"returning: %d\n", (retries > 0 ? 0 : 1));
	return (retries > 0 ? 0 : 1);
}

int si_can_set_reader(si_session_t *s) {
	/* Start background recv thread */
	dprintf(dlevel,"driver name: %s\n", s->can->name);
	if (strcmp(s->can->name,"can") == 0 && s->th == 0) {
		pthread_attr_t attr;

		/* Set the filter to our range */
		s->can->config(s->can_handle,CAN_CONFIG_SET_RANGE,0x300,0x30F);

		/* Create a detached thread */
		if (pthread_attr_init(&attr)) {
			sprintf(s->errmsg,"pthread_attr_init: %s",strerror(errno));
			goto si_can_set_reader_error;
		}
		if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) {
			sprintf(s->errmsg,"pthread_attr_setdetachstate: %s",strerror(errno));
			goto si_can_set_reader_error;
		}
		set_state(s,SI_STATE_RUNNING);
		if (pthread_create(&s->th,&attr,&si_can_recv_thread,s)) {
			sprintf(s->errmsg,"pthread_create: %s",strerror(errno));
			goto si_can_set_reader_error;
		}
		pthread_attr_destroy(&attr);

		dprintf(dlevel,"setting func to local data\n");
		s->can_read = si_can_get_local;
	} else {
		dprintf(dlevel,"setting func to remote data\n");
		s->can_read = si_can_get_remote;
	}
	return 0;
si_can_set_reader_error:
	s->can_read = si_can_get_remote;
	return 1;
}

static void si_can_stop_thread(si_session_t *s) {
	/* Stop the read thread and wait for it to exit */
	dprintf(dlevel,"SI_STATE_RUNNING: %d\n", check_state(s,SI_STATE_RUNNING));
	if (check_state(s,SI_STATE_RUNNING)) {
//		int i;

		dprintf(dlevel,"stopping thread...\n");
		clear_state(s,SI_STATE_RUNNING);
#if 0
		for(i=0; i < 10; i++) {
			if (!check_state(s,SI_STATE_STARTED)) break;
			sleep(1);
		}
		if (check_state(s,SI_STATE_STARTED)) {
			log_error("si_can_stop_thread: timeout waiting for thread exit\n");
		} else {
			dprintf(dlevel,"done!\n");
			s->th = 0;
		}
#endif
		pthread_cancel(s->th);
		s->th = 0;
	}
}

static int si_can_cb(void *ctx) {
        si_session_t *s = ctx;
	int write;

//	dprintf(dlevel,"*** IN CB ***\n");
	if (!s->can_connected) return 0;

	/* refresh data */
	si_can_get_data(s);

	dprintf(dlevel,"GdOn: %d, grid_connected: %d, GnOn: %d, gen_connected: %d\n",
		s->data.GdOn, s->grid_connected, s->data.GnOn, s->gen_connected);

	write = 0;
	if (s->data.GdOn != s->grid_connected) {
		agent_event(s->ap,"Grid",s->data.GdOn ? "On" : "Off");
		s->grid_connected = s->data.GdOn;
		write = 1;
	}
	if (s->data.GnOn != s->gen_connected) {
		agent_event(s->ap,"Gen",s->data.GnOn ? "On" : "Off");
		s->gen_connected = s->data.GnOn;
		write = 1;
	}
	if (write) si_can_write_va(s);

        return 0;
}

int si_can_connect(si_session_t *s) {

	dprintf(dlevel,"can: %p\n", s->can);
	if (!s->can) return 1;

	if (si_driver.open(s)) return 1;

	/* If local, "prime" the frames by reading a few IDs */
	dprintf(dlevel,"name: %s\n", s->can->name);
	if (strcmp(s->can->name,"can") == 0) {
		uint8_t data[8];

		if (s->can_read(s,0x304,data,8)) return 1;
		if (s->can_read(s,0x305,data,8)) return 1;
		if (s->can_read(s,0x307,data,8)) return 1;
		if (s->can_read(s,0x308,data,8)) return 1;
		if (s->can_read(s,0x309,data,8)) return 1;
		si_can_get_data(s);

	/* Otherwise, do an initial read */
	} else if (si_can_read_data(s,0)) {
		return 1;
	}

	/* OK, we're connected */
	s->can_connected = true;
	if (s->ap) agent_event(s->ap,"CAN","Connected");

	s->startup = 1;
	s->grid_connected = s->data.GdOn;
	s->gen_connected = s->data.GnOn;
	if (s->ap) agent_set_callback(s->ap,si_can_cb,s);

	return 0;
}

void si_can_disconnect(si_session_t *s) {
	dprintf(dlevel,"can: %p, can_handle: %p, close: %p\n", s->can, s->can_handle, s->can ? s->can->close : 0);
//	if (s->can && s->can_handle && s->can->close) s->can->close(s->can_handle);
	dprintf(dlevel,"closing...\n");
	si_driver.close(s);
	dprintf(dlevel,"setting flag...\n");
	s->can_connected = false;
	if (s->ap) {
		dprintf(dlevel,"sending event...\n");
		agent_event(s->ap,"CAN","Disconnected");
	}
	dprintf(dlevel,"done!\n");
}

extern int agent_pubconfig(solard_agent_t *ap);

int si_can_init(si_session_t *s) {

	/* Find the driver */
	dprintf(dlevel,"getting driver for transport: %s\n", s->can_transport);
	s->can = find_driver(si_transports,s->can_transport);
	dprintf(dlevel,"s->can: %p\n", s->can);
	if (!s->can) {
		 if (strlen(s->can_transport)) {
			sprintf(s->errmsg,"unable to find CAN driver for transport: %s",s->can_transport);
			return 1;
		} else {
			sprintf(s->errmsg,"can_transport is empty!\n");
			return 1;
		}
	}

	/* Create new instance */
	dprintf(dlevel,"using driver: %s\n", s->can->name);
	s->can_handle = s->can->new(s->can_target, s->can_topts);
	dprintf(dlevel,"can_handle: %p\n", s->can_handle);
	if (!s->can_handle) {
		sprintf(s->errmsg,"unable to create new instance of %s driver: %s",
			s->can->name,strerror(errno));
		return 1;
	}
	if (si_can_set_reader(s)) return 1;

	/* Set the pointers into the raw data */
	_setraw(s);

	if (!strlen(s->input.text)) s->input.source = CURRENT_SOURCE_NONE;
	if (!strlen(s->input.text)) s->output.source = CURRENT_SOURCE_NONE;
	/* Set inteval to 10s */
	if (s->ap && s->ap->interval != 10) {
		config_property_t *p = config_get_property(s->ap->cp,si_driver.name,"interval");
		dprintf(dlevel,"p: %p\n", p);
		if (p) {
			config_property_set_value(p,DATA_TYPE_STRING,"10",2,true,true);
//			config_dump_property(p,0);
			agent_pubconfig(s->ap);
		}
	}
	s->can_init = true;
	if (s->ap) agent_set_callback(s->ap,si_can_cb,s);
	return 0;
}

void si_can_destroy(si_session_t *s) {
	dprintf(dlevel,"s->can: %p\n", s->can);
	if (s->can) {
		dprintf(dlevel,"disconnecting...\n");
		si_can_disconnect(s);

		/* Clear any previous filters */
		dprintf(dlevel,"clearing filters...\n");
		s->can->config(s->can_handle,CAN_CONFIG_CLEAR_FILTER);

		/* Stop any buffering */
		dprintf(dlevel,"stopping buffering...\n");
		s->can->config(s->can_handle,CAN_CONFIG_STOP_BUFFER);

		// Close the handle 1st so the thread read returns -1 vs hanging
		dprintf(dlevel,"closing handle...\n");
		if (check_state(s,SI_STATE_OPEN)) s->can->close(s->can_handle);
		dprintf(dlevel,"stopping read thread...\n");
		if (strcmp(s->can->name,"can") == 0) si_can_stop_thread(s);
		s->can_init = false;
		dprintf(dlevel,"clearing agent callback...\n");
		if (s->ap) agent_clear_callback(s->ap);
		dprintf(dlevel,"destroying handle...\n");
		s->can->destroy(s->can_handle);
		s->can = 0;
		dprintf(dlevel,"done!\n");
	}
}

int si_can_read_data(si_session_t *s, int all) {

	dprintf(dlevel,"all: %d\n",all);

	/* If local can driver running, just refresh data */
	dprintf(dlevel,"s->can->name: %s\n", s->can->name);
	if (strcmp(s->can->name,"can") == 0) return si_can_get_data(s);

	if (all || s->input.source == CURRENT_SOURCE_CALCULATED) {
		if (s->can_read(s,0x300,s->frames[0].data,8)) return 1;
		if (s->can_read(s,0x302,s->frames[2].data,8)) return 1;
	}

	if (all || s->output.source == CURRENT_SOURCE_CALCULATED) {
		if (s->can_read(s,0x301,s->frames[1].data,8)) return 1;
		if (s->can_read(s,0x303,s->frames[3].data,8)) return 1;
	}

	if (s->can_read(s,0x304,s->frames[4].data,8)) return 1;
	if (s->can_read(s,0x305,s->frames[5].data,8)) return 1;
	if (s->can_read(s,0x306,s->frames[6].data,8)) return 1;
	if (s->can_read(s,0x307,s->frames[7].data,8)) return 1;
	if (s->can_read(s,0x308,s->frames[8].data,8)) return 1;
	if (s->can_read(s,0x309,s->frames[9].data,8)) return 1;
	if (s->can_read(s,0x30a,s->frames[10].data,8)) return 1;

//	dprintf(dlevel,"*** IN READ ***\n");
	si_can_get_data(s);
	return 0;
}

#if DISABLE_WRITE
static int warned = 0;
#endif

int si_can_write(si_session_t *s, uint32_t id, uint8_t *data, int data_len) {
	struct can_frame frame;
	int len,bytes;

	dprintf(dlevel,"id: %03x, data: %p, data_len: %d\n",id,data,data_len);

	dprintf(dlevel,"readonly: %d\n", s->readonly);
	if (s->readonly) {
#if 0
		char temp[16];
		sprintf(temp,"%03x",id);
		bindump(temp,data,data_len);
#endif
		if (!s->readonly_warn) {
			printf("************************************************\n");
			printf("*** READONLY IS TRUE, NOT WRITING!\n");
			printf("************************************************\n");
//			log_warning("readonly is true, not writing!\n");
			s->readonly_warn = true;
		}
		return data_len;
	}

#if DISABLE_WRITE
	if (!warned) {
		log_warning("can_write disabled!\n");
		warned = 1;
	}
	return data_len;
#endif

	len = data_len > 8 ? 8 : data_len;

	frame.can_id = id;
	frame.can_dlc = len;
	memcpy(&frame.data,data,len);
//	bindump("write data",data,data_len);
	bytes = s->can->write(s->can_handle,&id,&frame,sizeof(frame));
	dprintf(dlevel,"bytes: %d\n", bytes);
	return bytes;
}

int si_can_write_va(si_session_t *s) {
	uint8_t data[8];
	double cv,ca,m,ocv;
//	double oca;
	bool ldlevel = dlevel;

	if (!si_isvrange(s->charge_voltage)) s->charge_voltage = s->data.battery_voltage;
	if (!si_isvrange(s->charge_voltage)) {
		log_error("charge_voltage is out of range\n");
		return -1;
	}

	// Voltage
	cv = s->charge_voltage;
	m = s->max_voltage;
	dprintf(ldlevel,"charge_mode: %d, GdOn: %d, GnOn: %d\n", s->charge_mode, s->data.GdOn, s->data.GnOn);
	if (s->charge_mode) {
		if (!s->data.GdOn && !s->data.GnOn) {
			cv += 1.0;
			m += 1.0;
		}
	} else {
		cv = s->data.battery_voltage + 0.1;
	}
	ocv = cv;
	dprintf(ldlevel,"cv: %.1f, min_voltage: %.1f, max_voltage: %.1f\n", cv, s->min_voltage, m);
        if (cv <= s->min_voltage) cv = s->min_voltage;
        if (cv >= m) cv = m;
	if (cv != ocv) dprintf(0,"FIXED cv: %.1f\n", cv);
#if 0
	static double last_cv = 0;
	if (cv != last_cv) {
		log_info("cv: %.1f\n", cv);
		last_cv = cv;
	}
#endif

	// Amps
	dprintf(ldlevel,"force_charge_amps: %d, charge_amps: %.1f, charge_mode: %d\n", s->force_charge_amps, s->charge_amps, s->charge_mode);
	if (s->force_charge_amps) {
		ca = s->charge_amps;
	} else if (s->charge_mode) {
		if (s->data.GdOn) {
			ca = s->grid_charge_amps;
		} else if (s->data.GnOn) {
			ca = s->gen_charge_amps;
		} else {
			ca = s->solar_charge_amps;
		}
	} else {
		/* make sure no charge goes into batts */
		ca = (s->zero_ca ? 0.0 : 0.1);
	}
	if (ca < 0.1 && !s->zero_ca) ca = 0.1;
#if 0
	oca = ca;
	dprintf(ldlevel,"ca: %.1f, min_charge_amps: %.1f, max_charge_amps: %.1f\n", ca, s->min_charge_amps, s->max_charge_amps);
	if (ca < s->min_charge_amps) ca = s->min_charge_amps;
	else if (ca > s->max_charge_amps) ca = s->max_charge_amps;
	if (ca != oca) dprintf(0,"FIXED ca: %.1f\n", ca);
#endif
	s->charge_amps = ca;

#if 0
	if (ca != s->last_ca) {
		log_info("cv: %.1f, ca: %.1f\n", cv, ca);
		s->last_ca = ca;
	}
#endif

	/* 0x351 Battery charge voltage / DC charge current limitation / DC discharge current limitation / discharge voltage */
	dprintf(dlevel,"0x351: charge_voltage: %.1f, charge amps: %.1f, discharge_amps: %.1f, min_voltage: %.1f\n",
		cv, ca, s->discharge_amps, s->min_voltage);
	_putu16(&data[0],(double)(cv * 10.0));
	_putu16(&data[2],(double)(ca * 10.0));
	_putu16(&data[4],(double)(s->discharge_amps * 10.0));
	_putu16(&data[6],(double)(s->min_voltage * 10.0));
//	bindump("351",data,8);
	return si_can_write(s,0x351,data,8);
}

int si_can_write_data(si_session_t *s) {
	uint8_t data[8];
//	char temp[32];
	double soc;

	if (!s->can_connected) return 0;

	soc = s->soc;
	if (s->zero_ca) {
		dprintf(dlevel,"startup: %d, startup_soc: %d\n", s->startup, s->startup_soc);
		if (s->startup) {
			s->charge_amps = 0;
			s->tozero = 0;
			s->startup = 0;
			soc = (s->startup_soc ? 99.9 : s->soc);
		}

		/* Take charge amps to 0.0? need to go to 1 first then 0.0 next pass */
		dprintf(dlevel,"tozero: %d, charge_amps: %.1f\n", s->tozero, s->charge_amps);
		if (s->tozero == 1) {
			if (double_equals(s->charge_amps,1.0)) {
				dprintf(dlevel,"setting charge amps to 0...\n");
				s->charge_amps = 0.0;
				s->force_charge_amps = true;
				s->tozero = 2;
			} else {
				s->tozero = 0;
			}
		} else {
			if (double_equals(s->charge_amps,0.0)) {
				if (s->tozero != 2) {
					s->charge_amps = 1;
					s->force_charge_amps = true;
					s->tozero = 1;
				}
			} else if (s->tozero == 2) {
				s->tozero = 0;
			}
		}
	}

	dprintf(dlevel,"discharge_amps: %f\n", s->discharge_amps);
	if ((int)(s->discharge_amps) == 0) {
//		log_warning("discharge_amps is 0.0, setting to 1120.0\n");
		s->discharge_amps = 1120;
		dprintf(dlevel,"NEW discharge_amps: %f\n", s->discharge_amps);
	}

	/* 0x350 Active Current Set-point / Reactive current Set-Point */

	/* 0x351 Battery charge voltage / DC charge current limitation / DC discharge current limitation / discharge voltage */
	if (si_can_write_va(s) < 0) return 1;

	/* 0x352 Nominal frequency (F0) */
	/* 0x353 Nominal voltage L1 (U0) */
	/* 0x354 Function Bits SiCom V1 */

	/* 0x355 SOC value / SOH value / HiResSOC */
//	dprintf(dlevel,"0x355: SoC: %.1f, SoH: %.1f\n", soc, s->soh);
#if 0
	snprintf(temp,sizeof(temp)-1,"%.1f", soc);
	dprintf(dlevel,"temp: %s, last_soc: %s\n", temp, s->last_soc);
	if (strcmp(temp,s->last_soc) != 0) {
		log_info("SoC: %s\n", temp);
		strcpy(s->last_soc,temp);
	}
#endif
	_putu16(&data[0],(double)soc);
	_putu32(&data[4],(double)(soc * 100.0));
	_putu16(&data[2],(double)s->soh);
	if (si_can_write(s,0x355,data,8) < 0) return 1;

#if 0
	/* 0x356 Battery Voltage / Battery Current / Battery Temperature */
	dprintf(0,"0x356: battery_voltage: %3.2f, battery_amps: %3.2f, battery_temp: %3.2f\n",
		s->data.battery_voltage, s->data.battery_amps, s->data.battery_temp);
	_putu16(&data[0],(double)(s->data.battery_voltage * 100.0));
	_putu16(&data[2],(double)(s->data.battery_current * 10.0));
	if (s->have_battery_temp) {
		_putshort(&data[4],(double)(s->data.battery_temp * 10.0));
	} else {
		_putshort(&data[4],0);
	}
	if (si_can_write(s,0x356,data,8) < 0) return 1;
#endif

	/* 0x357 - 0x359 ?? */

	/* 0x35A Alarms / Warnings */
	memset(data,0,sizeof(data));
	if (si_can_write(s,0x35A,data,8) < 0) return 1;

	/* 0x35B Events */
	memset(data,0,sizeof(data));
	if (si_can_write(s,0x35B,data,8) < 0) return 1;

	/* 0x35C Function Bits / Function Bits Relays */

#if 0
	/* 0x35E Manufacturer-Name-ASCII (8 chars) */
	memset(data,0x20,sizeof(data));
	data[0] = 0x53; data[1] = 0x50; data[2] = 0x53;
	if (si_can_write(s,0x35E,data,8) < 0) return 1;

	/* 0x35F - Bat-Type / BMS Version / Bat-Capacity / reserved Manufacturer ID */
	_putshort(&data[0],1);
	/* major.minor.build.revision - encoded as MmBBr 10142 = 1.0.14.2 */
	_putshort(&data[2],10000);
	_putshort(&data[4],2500);
	_putshort(&data[6],1);
	if (si_can_write(s,0x35F,data,8) < 0) return 1;
#endif

	/* 0x360 Limitation of external current (Gen/Grid) */

	return 0;
}
