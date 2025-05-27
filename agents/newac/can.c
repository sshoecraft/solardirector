
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

#define CAN_START 0x450
#define CAN_END 0x45F

#define DISABLE_WRITE 0
#define DEBUG_BITS 1

#include "ac.h"
#include <pthread.h>
#ifndef __WIN32
#include <sys/signal.h>
#endif
#include "transports.h"
#include <math.h>

#define SDATA(f,b) ((int16_t *) &s->frames[f].data[b]);
#define BDATA(f,b) ((int8_t *) &s->frames[f].data[b]);
#define UBDATA(f,b) ((uint8_t *) &s->frames[f].data[b]);
#define USDATA(f,b) ((uint16_t *) &s->frames[f].data[b]);
#define LDATA(f,b) ((int32_t *) &s->frames[f].data[b]);

#define GETFVAL(val) (double)_gets16(val)
#define GETFS100(val) (((double)_gets16(val))*100)
#define GETF100(val) (((double)_gets16(val))*100)
#define GETD10(val) (((double)_gets16(val))/10)
#define GETD100(val) (((double)_gets16(val))/100)

static void *ac_can_recv_thread(void *handle) {
	ac_session_t *s = handle;
	struct can_frame frame;
	int bytes,fidx;
	uint32_t can_id;
//	uint32_t mask;
#ifndef __WIN32
	sigset_t set;

	/* Ignore SIGPIPE */
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	sigprocmask(SIG_BLOCK, &set, NULL);
#endif
	int ldlevel = 4;

	can_id = 0xffff;
	dprintf(ldlevel,"thread starting\n");
	set_state(s,AC_CAN_RUN);
	while(check_state(s,AC_CAN_RUN)) {
		dprintf(ldlevel,"%d: open: %d\n", getpid(), check_state(s,AC_CAN_OPEN));
		if (!check_state(s,AC_CAN_OPEN)) {
//			memset(&s->bitmap,0,sizeof(s->bitmap));
			sleep(1);
			continue;
		}
		dprintf(ldlevel,"reading...\n");
		bytes = s->can->read(s->can_handle,&can_id,&frame,sizeof(frame));
		dprintf(ldlevel,"%d: bytes: %d\n", getpid(),bytes);
		if (bytes < 1) {
//			memset(&s->bitmap,0,sizeof(s->bitmap));
			sleep(1);
			continue;
		}
		frame.can_id &= 0x1FFFFFFF;
		dprintf(ldlevel,"frame.can_id: %03x\n",frame.can_id);
		if (frame.can_id < CAN_START || frame.can_id > CAN_END) continue;
//		bindump("frame",&frame,sizeof(frame));
		fidx = frame.can_id - CAN_START;
//		mask = 1 << frame.can_id;
//		dprintf(ldlevel,"id: %03x, fidx: %x, mask: %08x, bitmap: %08x\n", frame.can_id, fidx, mask, s->bitmap);
		dprintf(ldlevel,"id: %03x, fidx: %x\n", frame.can_id, fidx);
		memcpy(&s->frames[fidx],&frame,sizeof(frame));
		time(&s->times[fidx]);
//		s->bitmap |= mask;
	}
	dprintf(dlevel,"thread exiting\n");
	return 0;
}

int _can_read(ac_session_t *s, uint32_t id, struct can_frame **fp, bool wait) {
	int fidx,retries;
	int r;
	time_t t;

	int ldlevel = dlevel;
//	int ldlevel = -1;

	dprintf(ldlevel,"id: %03x\n", id);
	if (id < CAN_START || id > CAN_END) return 1;

	fidx = id - CAN_START;
	dprintf(ldlevel,"fidx: %x, wait: %d\n", fidx, wait);
	r = 1;
	time(&t);
	if (wait) {
		retries=5;
		while(retries--) {
			dprintf(ldlevel,"times[%d], time: %d, diff: %d\n", s->times[fidx], t, t - s->times[fidx]);
			if (t - s->times[fidx] > 5) {
				sleep(1);
				dprintf(ldlevel,"retries: %d\n", retries);
				continue;
			}
			*fp = &s->frames[fidx];
			r = 0;
			break;
		}
	} else {
		dprintf(ldlevel,"times[%d], time: %d, diff: %d\n", s->times[fidx], t, t - s->times[fidx]);
		if (t - s->times[fidx] < 5) {
			*fp = &s->frames[fidx];
			r = 0;
		}
	}
	dprintf(ldlevel,"returning: %d\n", r);
	return r;
}

#if 0
int can_read(ac_session_t *s, uint32_t id, uint8_t *data, int datasz) {
//	uint32_t mask;
	int fidx,retries,len;
	int r;
	time_t t;

	dprintf(dlevel,"id: %03x, data: %p, len: %d\n", id, data, datasz);
	if (id < CAN_START || id > CAN_END) return 1;

	fidx = id - CAN_START;
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
static int ac_can_get_remote(ac_session_t *s, uint32_t id, uint8_t *data, int datasz) {
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
#endif

static void ac_can_stop_thread(ac_session_t *s) {
	/* Stop the read thread and wait for it to exit */
	dprintf(dlevel,"AC_CAN_RUN: %d\n", check_state(s,AC_CAN_RUN));
	if (check_state(s,AC_CAN_RUN)) {
//		int i;

		dprintf(dlevel,"stopping thread...\n");
		clear_state(s,AC_CAN_RUN);
#if 0
		for(i=0; i < 10; i++) {
			if (!check_state(s,AC_CAN_RUN)) break;
			sleep(1);
		}
		if (check_state(s,AC_CAN_RUN)) {
			log_error("ac_can_stop_thread: timeout waiting for thread exit\n");
		} else {
			dprintf(dlevel,"done!\n");
			s->th = 0;
		}
#endif
		pthread_cancel(s->th);
		s->th = 0;
	}
}

#if 0
void ac_can_disconnect(ac_session_t *s) {
	dprintf(dlevel,"can: %p, can_handle: %p, close: %p\n", s->can, s->can_handle, s->can ? s->can->close : 0);
//	if (s->can && s->can_handle && s->can->close) s->can->close(s->can_handle);
	dprintf(dlevel,"closing...\n");
	ac_driver.close(s);
//	dprintf(dlevel,"setting flag...\n");
//	s->can_connected = false;
	if (s->ap) {
		dprintf(dlevel,"sending event...\n");
		agent_event(s->ap,"CAN","Disconnected");
	}
	dprintf(dlevel,"done!\n");
}

extern int agent_pubconfig(solard_agent_t *ap);
#endif

int ac_can_init(ac_session_t *s) {
	pthread_attr_t attr;
	int retries;
//	can_frame_t *fp;

#if 0
	/* Find the driver */
	dprintf(dlevel,"getting driver for transport: %s\n", s->can_transport);
	s->can = find_driver(ac_transports,s->can_transport);
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
#endif

	s->can = &can_driver;

	/* Create new instance */
	s->can_handle = s->can->new(s->can_target, s->can_topts);
	dprintf(dlevel,"can_handle: %p\n", s->can_handle);
	if (!s->can_handle) {
		log_error("unable to create new instance of %s driver: %s", s->can->name,strerror(errno));
		return 1;
	}
	if (s->can->open(s->can_handle)) {
		log_error("error opening CAN bus\n");
		return 1;
	}
	set_state(s,AC_CAN_OPEN);

	/* Start background recv thread */
	dprintf(dlevel,"driver name: %s\n", s->can->name);

	/* Set the filter to our range */
	s->can->config(s->can_handle,CAN_CONFIG_SET_RANGE,CAN_START,CAN_END);

	/* Create a detached thread */
	if (pthread_attr_init(&attr)) {
		log_error("pthread_attr_init: %s",strerror(errno));
		return 1;
	}
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) {
		log_error("pthread_attr_setdetachstate: %s",strerror(errno));
		return 1;
	}
	clear_state(s,AC_CAN_RUN);
	if (pthread_create(&s->th,&attr,&ac_can_recv_thread,s)) {
		log_error("pthread_create: %s",strerror(errno));
		return 1;
	}
	pthread_attr_destroy(&attr);

	/* Wait until the thread starts */
	dprintf(dlevel,"waiting for CAN thread...\n");
	retries = 5;
	while(!check_state(s,AC_CAN_RUN)) {
		if (!retries--) {
			log_error("error waiting for CAN thread to start!\n");
			return 1;
		}
		sleep(1);
	}

//	if (s->ap) agent_set_callback(s->ap,ac_can_cb,s);

	/* do a wait read on CAN_END to try and prime the frames array */
//	_can_read(s,CAN_END,&fp,true);
	sleep(1);

	dprintf(dlevel,"done!\n");
	return 0;
}

void ac_can_destroy(ac_session_t *s) {
	dprintf(dlevel,"s->can: %p\n", s->can);
	if (s->can) {
//		dprintf(dlevel,"disconnecting...\n");
//		ac_can_disconnect(s);

		/* Clear any previous filters */
		dprintf(dlevel,"clearing filters...\n");
		s->can->config(s->can_handle,CAN_CONFIG_CLEAR_FILTER);

		/* Stop any buffering */
		dprintf(dlevel,"stopping buffering...\n");
		s->can->config(s->can_handle,CAN_CONFIG_STOP_BUFFER);

		// Close the handle 1st so the thread read returns -1 vs hanging
		dprintf(dlevel,"closing handle...\n");
		if (check_state(s,AC_CAN_OPEN)) s->can->close(s->can_handle);
		dprintf(dlevel,"stopping read thread...\n");
		if (strcmp(s->can->name,"can") == 0) ac_can_stop_thread(s);
		dprintf(dlevel,"clearing agent callback...\n");
		if (s->ap) agent_clear_callback(s->ap);
		dprintf(dlevel,"destroying handle...\n");
		s->can->destroy(s->can_handle);
		s->can = 0;
		dprintf(dlevel,"done!\n");
	}
}

#if 0
int ac_can_read_data(ac_session_t *s, int all) {

	dprintf(dlevel,"*** IN READ ***\n");

	dprintf(dlevel,"all: %d\n",all);

#if 0
	/* If local can driver running, just refresh data */
	dprintf(dlevel,"s->can->name: %s\n", s->can->name);
	if (strcmp(s->can->name,"can") == 0) return ac_can_get_data(s);

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

	ac_can_get_data(s);
#endif
	return 0;
}
#endif

#if DISABLE_WRITE
static int warned = 0;
#endif

int can_write(ac_session_t *s, uint32_t id, uint8_t *data, int data_len) {
	struct can_frame frame;
	int len,bytes;

	dprintf(dlevel,"id: %03x, data: %p, data_len: %d\n",id,data,data_len);

#if 0
	dprintf(dlevel,"readonly: %d\n", s->readonly);
	if (s->readonly) {
		if (!s->readonly_warn) {
			printf("************************************************\n");
			printf("*** READONLY IS TRUE, NOT WRITING!\n");
			printf("************************************************\n");
//			log_warning("readonly is true, not writing!\n");
			s->readonly_warn = true;
		}
		return data_len;
	}
#endif

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

#ifdef JS
#include "jsobj.h"
JSBool js_can_read(JSContext *cx, uintN argc, jsval *vp) {
	ac_session_t *s;
	int id,r;
	jsval *argv = vp + 2;
	JSObject *obj;
	struct can_frame *fp;
	bool wait;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);

	if (argc < 1) {
		JS_ReportError(cx,"can_read requires 1 argument (id: number) and 1 optional argumnent (wait: boolean)\n");
		return JS_FALSE;
	}

	*vp = JSVAL_VOID;
	if (!jsval_to_type(DATA_TYPE_INT,&id,0,cx,argv[0])) {
		JS_ReportError(cx,"can_read: unable to get ID\n");
		return JS_TRUE;
	}
	wait = 0;
	if (argc > 1) {
		if (!jsval_to_type(DATA_TYPE_BOOL,&wait,0,cx,argv[1])) {
			log_warning("can_read: unable to convert jsargv to bool (wait)\n");
			wait = 0;
		}
	}
	dprintf(dlevel,"id: 0x%03x\n", id);
	if (id < CAN_START || id > CAN_END) {
		JS_ReportError(cx,"can_read: can ID %x is out of range (%x to %x)\n", id, CAN_START, CAN_END);
		return JS_TRUE;
	}

	r = _can_read(s,id,&fp,wait);
	dprintf(dlevel,"r: %d\n", r);
	if (!r) *vp = type_to_jsval(cx,DATA_TYPE_U8_ARRAY,fp->data,fp->can_dlc);
	return JS_TRUE;
}

JSBool js_can_write(JSContext *cx, uintN argc, jsval *vp) {
	ac_session_t *s;
	uint8_t data[8];
	struct can_frame frame;
	int len,r;
	uint32_t can_id;
	JSObject *obj;
	JSClass *classp;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx, "can_write: internal error: private is null!\n");
		return JS_FALSE;
	}

	if (argc != 2) {
		JS_ReportError(cx,"can_write requires 2 arguments (id: number, data: array)\n");
		return JS_FALSE;
	}

	/* Get the args*/
	if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "u o", &can_id, &obj)) return JS_FALSE;
	dprintf(dlevel,"can_id: %d, obj: %p\n", can_id, obj);

	/* Make sure the object is an array */
	classp = OBJ_GET_CLASS(cx,obj);
	if (!classp) {
		JS_ReportError(cx, "can_write: 2nd argument must be array\n");
		return JS_FALSE;
	}
	dprintf(dlevel,"class: %s\n", classp->name);
	if (classp && strcmp(classp->name,"Array")) {
		JS_ReportError(cx, "can_write: 2nd argument must be array\n");
		return JS_FALSE;
	}

	/* Convert the JS array to C array */
	len = jsval_to_type(DATA_TYPE_U8_ARRAY,data,8,cx,OBJECT_TO_JSVAL(obj));

	memset(&frame,0,sizeof(frame));
	frame.can_id = can_id;
	frame.can_dlc = len;
	memcpy(&frame.data,data,len);
	r = can_write(s,can_id,data,len);
	dprintf(dlevel,"r: %d\n", r);
	*vp = INT_TO_JSVAL(r);
	return JS_TRUE;
}
#endif
