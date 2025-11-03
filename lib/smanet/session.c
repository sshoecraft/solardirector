
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 4
#include "debug.h"

#include "smanet_internal.h"
#include "transports.h"
#ifndef WINDOWS
#include <signal.h>
#endif
#include <ctype.h>
#ifdef __APPLE__
#include <sys/fcntl.h>
#else
#include <sys/file.h>
#endif
#include <libgen.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

static solard_driver_t *smanet_transports[] = { &serial_driver, &rdev_driver, 0 };

#if SMANET_AUTO_CLOSE || SMANET_RECV_THREAD
static void *smanet_recv_thread(void *handle) {
	smanet_session_t *s = handle;
#if SMANET_AUTO_CLOSE
	time_t now,diff;
#else
	struct rdata rdata;
#endif
#if !defined(WINDOWS) && !defined(__APPLE__)
	sigset_t set;

	/* Ignore SIGPIPE */
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	sigprocmask(SIG_BLOCK, &set, NULL);
#elif defined(__APPLE__)
	/* On macOS, use signal() instead of sigprocmask */
	signal(SIGPIPE, SIG_IGN);
#endif

	/* Enable us to be cancelled immediately */
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,0);

	dprintf(dlevel,"thread started!\n");
	smanet_set_state(s,SMANET_STATE_STARTED);
	while(smanet_check_state(s,SMANET_STATE_RUN)) {
		pthread_mutex_lock(&s->lock);
		dprintf(dlevel+2,"opened: %d\n", s->opened);
#if SMANET_AUTO_CLOSE
		if (s->opened && s->auto_close) {
			time(&now);
			diff = now - s->last_command;
#ifdef __APPLE__
			dprintf(dlevel+2,"diff: %ld\n", (long)diff);
#else
			dprintf(dlevel+2,"diff: %d\n", (int)diff);
#endif
			if (diff > s->auto_close_timeout) {
				dprintf(dlevel,"SMANET: closing transport.\n");
				s->tp->close(s->tp_handle);
				s->opened = false;
			}
		}
		pthread_mutex_unlock(&s->lock);
//		usleep(5000);
		sleep(1);
#else
		if (!s->opened) {
			sleep(1);
			pthread_mutex_unlock(&s->lock);
			continue;
		}
		pthread_mutex_unlock(&s->lock);
		/* read frame provides our sleep/wait */
		rdata.len = smanet_read_frame(s,rdata.data,sizeof(rdata.data),-1);
		if (rdata.len < 1) continue;
//		bindump("recv frame",rdata.data,rdata.len);
		list_add(s->frames,&rdata,sizeof(rdata));
#endif
	}
	dprintf(dlevel,"returning!\n");
	smanet_clear_state(s,SMANET_STATE_STARTED);
	return 0;
}
#endif

#if SMANET_USE_BUFFER
static int tp_get(void *handle, uint8_t *buffer, int buflen) {
	smanet_session_t *s = handle;
	int bytes;

	bytes = s->tp->read(s->tp_handle,0,buffer,buflen);
	dprintf(dlevel+2,"bytes: %d\n", bytes);
	return bytes;
}
#endif

int smanet_open(smanet_session_t *s) {
	int r,ldlevel;

	ldlevel = dlevel;

	r = 1;
#if SMANET_AUTO_CLOSE || SMANET_RECV_THREAD
	dprintf(ldlevel,"locking...\n");
	pthread_mutex_lock(&s->lock);
#endif
	dprintf(ldlevel,"opened: %d\n", s->opened);
	if (s->opened) {
		r = 0;
		goto smanet_open_error;
	}
	dprintf(ldlevel,"tp->open: %p\n", s->tp->open);
	if (!s->tp->open) {
		sprintf(s->errmsg,"smanet_open: transport does not have open func!");
		goto smanet_open_error;
	}
//	if (!smanet_lock_target(s)) goto smanet_open_error;
	dprintf(ldlevel,"opening...\n");
	if (s->tp->open(s->tp_handle)) {
		sprintf(s->errmsg,"smanet_open: unable to open transport");
		goto smanet_open_error;
	}
	s->opened = true;

	r = 0;
smanet_open_error:
#if SMANET_AUTO_CLOSE || SMANET_RECV_THREAD
	dprintf(ldlevel,"unlocking...\n");
	pthread_mutex_unlock(&s->lock);
#endif
	dprintf(ldlevel,"returning: %d\n", r);
	return r;
}

int smanet_close(smanet_session_t *s) {
#if SMANET_AUTO_CLOSE || SMANET_RECV_THREAD
	pthread_mutex_lock(&s->lock);
#endif
	if (s->tp) s->tp->close(s->tp_handle);
	s->opened = 0;
//	smanet_unlock_target(s);
#if SMANET_AUTO_CLOSE || SMANET_RECV_THREAD
	pthread_mutex_unlock(&s->lock);
#endif
	s->connected = 0;
	return 0;
}

static int smanet_configure(smanet_session_t *s) {
	if (smanet_get_net_start(s,&s->serial,s->type,sizeof(s->type)-1)) {
		sprintf(s->errmsg,"smanet_connect: unable to start network");
		log_error("%s\n",s->errmsg);
		return 1;
	}
#ifdef __APPLE__
	dprintf(dlevel,"serial: %lld, type: %s\n", (long long)s->serial, s->type);
#else
	dprintf(dlevel,"serial: %ld, type: %s\n", s->serial, s->type);
#endif
	sleep(1);
	if (smanet_cfg_net_adr(s,0)) {
		sprintf(s->errmsg,"smanet_connect: unable to configure network address");
		log_error("%s\n",s->errmsg);
		return 1;
	}
	return 0;
}

int smanet_connect(smanet_session_t *s, char *transport, char *target, char *topts) {

	dprintf(dlevel,"s: %p\n", s);
	if (!s) return 1;

	dprintf(dlevel,"transport: %s, target: %s, topts: %s\n", transport, target, topts);
	if (!transport || !target) return 0;
	if (!strlen(transport) || !strlen(target)) {
		sprintf(s->errmsg,"smanet_connect: transport or target cannot be empty");
		return 1;
	}

	if (s->connected) smanet_disconnect(s);

	/* if the transport changed, get a new driver */
	dprintf(dlevel,"new transport: %s, old transport: %s\n", transport, s->transport);
	if (strcmp(s->transport,transport) != 0) {
		dprintf(dlevel,"handle: %p, destroy: %p\n", s->tp_handle, (s->tp ? s->tp->destroy : 0));
		if (s->tp_handle && s->tp->destroy) s->tp->destroy(s->tp_handle);
		s->tp_handle = 0;
	}

	strncpy(s->transport,transport,sizeof(s->transport)-1);
	strncpy(s->target,target,sizeof(s->target)-1);
	if (topts) strncpy(s->topts,topts,sizeof(s->topts)-1);
	else *s->topts = 0;

	dprintf(dlevel,"tp_handle: %p\n", s->tp_handle);
	if (!s->tp_handle) {
		/* Find the driver */
		s->tp = find_driver(smanet_transports,transport);
		if (!s->tp) {
			sprintf(s->errmsg,"unable to find smanet transport: %s", transport);
			log_error("%s\n",s->errmsg);
			return 1;
		}

		/* Create a new driver instance */
		s->tp_handle = s->tp->new(target, topts ? topts : "");
		if (!s->tp_handle) {
			sprintf(s->errmsg,"%s_new: %s", transport, strerror(errno));
			log_error("%s\n",s->errmsg);
			return 1;
		}
	}

	dprintf(dlevel,"opening transport...\n");
	if (smanet_open(s)) return 1;
	if (smanet_configure(s)) return 1;
	s->connected = 1;
	return 0;
}

int smanet_disconnect(smanet_session_t *s) {
	smanet_close(s);
	s->connected = 0;
	return 0;
}

int smanet_reconnect(smanet_session_t *s) {
	if (smanet_close(s)) return 1;
	if (smanet_open(s)) return 1;
	if (smanet_configure(s)) return 1;
	s->connected = 1;
	return 0;
}

int smanet_connected(smanet_session_t *s) {
	if (!s) return 0;
	return s->connected;
}

//smanet_session_t *smanet_init(char *transport, char *target, char *topts) {
smanet_session_t *smanet_init(bool readonly) {
	smanet_session_t *s;
#if SMANET_AUTO_CLOSE || SMANET_RECV_THREAD
	pthread_attr_t attr;
#endif

//	dprintf(dlevel,"transport: %s, target: %s, topts: %s\n", transport, target, topts);

	s = calloc(sizeof(*s),1);
	if (!s) {
		log_write(LOG_SYSERR,"smanet_init: calloc");
		goto smanet_init_error;
	}
	dprintf(dlevel,"readonly: %d\n", readonly);
	s->readonly = readonly;
#if SMANET_USE_BUFFER
	s->b = buffer_init(256,tp_get,s);
	if (!s->b) {
		log_write(LOG_SYSERR,"smanet_init: buffer_init");
		goto smanet_init_error;
	}
#endif
#if SMANET_AUTO_CLOSE || SMANET_RECV_THREAD
#if SMANET_AUTO_CLOSE
	s->auto_close = true;
	s->auto_close_timeout = SMANET_AUTO_CLOSE_TIMEOUT;
#endif
#if SMANET_RECV_THREAD
	s->frames = list_create();
#endif
	time(&s->last_command);
	pthread_mutex_init(&s->lock, 0);
	/* Create a detached thread */
	dprintf(dlevel,"creating thread...\n");
	if (pthread_attr_init(&attr)) {
		sprintf(s->errmsg,"pthread_attr_init: %s",strerror(errno));
		goto smanet_init_error;
	}
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) {
		sprintf(s->errmsg,"pthread_attr_setdetachstate: %s",strerror(errno));
		goto smanet_init_error;
	}
	smanet_set_state(s,SMANET_STATE_RUN);
	if (pthread_create(&s->tid,&attr,&smanet_recv_thread,s)) {
		sprintf(s->errmsg,"pthread_create: %s",strerror(errno));
		goto smanet_init_error;
	}
	pthread_attr_destroy(&attr);
	sleep(1);
#endif

	/* We are addr 1 */
	s->src = 1;

	s->param_timeout = 0;

//	smanet_connect(s, transport, target, topts ? topts : "");
	return s;

smanet_init_error:
	free(s);
	return 0;
}

#if SMANET_AUTO_CLOSE || SMANET_RECV_THREAD
static void smanet_stop_thread(smanet_session_t *s) {
	dprintf(dlevel,"SMANET_STATE_RUN: %d\n", smanet_check_state(s,SMANET_STATE_RUN));
	if (smanet_check_state(s,SMANET_STATE_RUN)) {
		int i;

		/* wait 5s for thread to exit */
		smanet_clear_state(s,SMANET_STATE_RUN);
		for(i=0; i < 5; i++) {
			if (!smanet_check_state(s,SMANET_STATE_STARTED)) break;
			sleep(1);
		}
		if (smanet_check_state(s,SMANET_STATE_STARTED)) {
			log_error("smanet_stop_thread: timeout waiting for thread exit\n");
			pthread_cancel(s->tid);
		}
	}
}
#endif

void smanet_destroy(smanet_session_t *s) {

	if (!s) return;

#if SMANET_AUTO_CLOSE || SMANET_RECV_THREAD
	smanet_stop_thread(s);
#endif
	if (s->tp) {
		smanet_disconnect(s);
		if (s->tp->destroy) s->tp->destroy(s->tp_handle);
	}

	smanet_destroy_channels(s);
//	if (s->values) free(values);
#if SMANET_USE_BUFFER
	buffer_free(s->b);
#endif
	free(s);
}

char *smanet_get_errmsg(smanet_session_t *s) {
	return s->errmsg;
}

int smanet_get_info(smanet_session_t *s, smanet_info_t *info) {
	if (!s->connected) {
		strcpy(s->errmsg,"not connected");
		return 1;
	}
	strcpy(info->type,s->type);
	info->serial = s->serial;
	return 0;
}

int smanet_set_readonly(smanet_session_t *s, bool value) {
	dprintf(dlevel,"value: %d\n", value);
	s->readonly = value;
	return 0;
}

int smanet_set_auto_close(smanet_session_t *s, bool value) {
	dprintf(dlevel,"value: %d\n", value);
	s->auto_close = value;
	return 0;
}
int smanet_set_auto_close_timeout(smanet_session_t *s, int value) {
	dprintf(dlevel,"value: %d\n", value);
	s->auto_close_timeout = value;
	return 0;
}
