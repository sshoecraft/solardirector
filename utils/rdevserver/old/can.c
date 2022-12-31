
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "rdevserver.h"
#include "transports.h"

/* Set this to 1 to restrict frames to Sunny Island range */
#define DSCAN_SI_FRAMES 1

#ifdef DSCAN_SI_FRAMES
/* 0x300 to 0x30F */
#define DSCAN_NFRAMES 16
#define DSCAN_NBITMAPS 1
#define DSCAN_FRAME_START 0x300
#else
/* 0x000 to 0x7FF */
#define DSCAN_NFRAMES 2048
#define DSCAN_NBITMAPS (DSCAN_NFRAMES/32)
#define DSCAN_FRAME_START 0x000
#endif

struct dscan_session {
	void *can_handle;
	pthread_t th;
	uint16_t state;
	struct can_frame frames[DSCAN_NFRAMES];
	uint32_t bitmaps[DSCAN_NBITMAPS];
};
typedef struct dscan_session dscan_session_t;

#define DSCAN_OPENED	0x0001

#define dlevel 1

static void *dscan_new(void *target, void *topts) {
	dscan_session_t *s;

	s = calloc(1,sizeof(*s));
	if (!s) return 0;
	s->can_handle = can_driver.new(target,topts);
	if (!s->can_handle) {
		free(s);
		return 0;
	}
	return s;
}

static int dscan_destroy(void *handle) {
	dscan_session_t *s = handle;

	free(s);
	return 0;
}

static void *dscan_recv_thread(void *handle) {
	dscan_session_t *s = handle;
	struct can_frame frame;
	int bytes,id,idx,want_id;
	uint32_t mask;

	want_id = 0xffff;
	dprintf(3,"thread started!\n");
	while(check_state(s,DSCAN_OPENED)) {
		bytes = can_driver.read(s->can_handle,&want_id,&frame,sizeof(frame));
		dprintf(7,"bytes: %d\n", bytes);
		if (bytes < 0) {
			memset(&s->bitmaps,0,sizeof(s->bitmaps));
			sleep(1);
			continue;
		}
 		if (bytes != sizeof(frame)) continue;
		dprintf(7,"frame.can_id: %03x\n",frame.can_id);

		if (frame.can_id < DSCAN_FRAME_START || frame.can_id >= DSCAN_FRAME_START+DSCAN_NFRAMES) continue;
		bindump("frame",&frame,sizeof(frame));
		id = frame.can_id - DSCAN_FRAME_START;
		idx = id / 32;
		mask = 1 << (id % 32);
		memcpy(&s->frames[frame.can_id],&frame,sizeof(frame));
		s->bitmaps[idx] |= mask;
	}
	dprintf(3,"thread returning!\n");
	return 0;
}

static int dscan_open(void *handle) {
	dscan_session_t *s = handle;
	pthread_attr_t attr;
	int r;

	r = 1;

	/* Open the device */
	r = can_driver.open(s->can_handle);
	if (r) return r;
	set_state(s,DSCAN_OPENED);

	/* Start the CAN read thread */
	if (pthread_attr_init(&attr)) {
		log_write(LOG_SYSERR,"pthread_attr_init");
		goto rdev_can_open_error;
	}
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) {
		log_write(LOG_SYSERR,"pthread_attr_setdetachstate");
		goto rdev_can_open_error;
	}
	if (pthread_create(&s->th,&attr,&dscan_recv_thread,s)) {
		log_write(LOG_SYSERR,"pthread_create");
		goto rdev_can_open_error;
	}
	r = 0;
rdev_can_open_error:
	return r;
}

static int dscan_read(void *handle, void *what, void *data, int datasz) {
	dscan_session_t *s = handle;
	int *can_id = what;
	uint32_t mask;
	int id,idx,retries;

	dprintf(dlevel,"id: %03x, data: %p, len: %d\n", *can_id, data, datasz);
	if (*can_id < DSCAN_FRAME_START || *can_id >= DSCAN_FRAME_START+DSCAN_NFRAMES) return 1;

	id = *can_id - DSCAN_FRAME_START;
	idx = id / 32;
	mask = 1 << (id % 32);
	dprintf(dlevel,"id: %03x, mask: %08x, bitmaps[%d]: %08x\n", id, mask, idx, s->bitmaps[idx]);
	retries=5;
	while(retries--) {
		if ((s->bitmaps[idx] & mask) == 0) {
			dprintf(5,"bit not set, sleeping...\n");
			sleep(1);
			continue;
		}
		if (debug >= dlevel+1) {
			char temp[16];

			sprintf(temp,"%03x", id);
			bindump(temp,&s->frames[id],s->frames[idx].can_dlc);
		}
		memcpy(data,&s->frames[id],sizeof(struct can_frame));
		return sizeof(struct can_frame);
	}
	return 0;
}

static int dscan_write(void *handle, void *what, void *buf, int buflen) {
	dscan_session_t *s = handle;
	return can_driver.write(s->can_handle,what,buf,buflen);
}

static int dscan_close(void *handle) {
	dscan_session_t *s = handle;

	dprintf(1,"closing...\n");
	clear_state(s,DSCAN_OPENED);
	return can_driver.close(s->can_handle);
}

solard_driver_t dscan_driver = {
	"dscan",
	dscan_new,		/* New */
	dscan_destroy,		/* Destroy */
	dscan_open,		/* Open */
	dscan_close,		/* Close */
	dscan_read,		/* Read */
	dscan_write,		/* Write */
	0,			/* Config */
};
