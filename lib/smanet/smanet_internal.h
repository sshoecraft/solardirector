
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __SMANET_INTERNAL_H
#define __SMANET_INTERNAL_H

/* Automatically close transport */
#define SMANET_AUTO_CLOSE 1
#define SMANET_AUTO_CLOSE_TIMEOUT 60

/* Use a receive thread for out-of-band frames */
#define SMANET_RECV_THREAD 0

/* Better to use buffering here vs at the serial driver layer */
#define SMANET_USE_BUFFER 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "types.h"
#include "smanet.h"
#include "debug.h"
#include "driver.h"
#include "buffer.h"
#if SMANET_AUTO_CLOSE || SMANET_RECV_THREAD
#include <pthread.h>
#endif

#define PACKET_DATA_SIZE 284

struct smanet_packet {
	uint16_t src;
	uint16_t dest;
	uint8_t control;
	unsigned group: 1;
	unsigned response: 1;
	unsigned block: 1;
	uint8_t count;
	uint8_t command;
	uint8_t *data;
	int datasz;
	int dataidx;
};
typedef struct smanet_packet smanet_packet_t;

/* Commands */
#define CMD_GET_NET		0x01	/* Request for network configuration */
#define CMD_SEARCH_DEV		0x02	/* Find Devices */
#define CMD_CFG_NETADR		0x03	/* Assign network address */
#define CMD_SET_GRPADR		0x04	/* Assign group address */
#define CMD_DEL_GRPADR		0x05	/* Delete group address */
#define CMD_GET_NET_START	0x06	/* Start of configuration */

#define CMD_GET_CINFO		0x09	/* Request for channel information */
#define CMD_SYN_ONLINE		0x0A	/* Synchronize online data */
#define CMD_GET_DATA		0x0B	/* Data request */
#define CMD_SET_DATA		0x0C	/* Transmit data */
#define CMD_GET_SINFO		0x0D	/* Query storge config */
#define CMD_SET_MPARA		0x0F	/* Parameterize data acquisition */

#define CMD_GET_MTIME		0x14	/* Read storage intervals */
#define CMD_SET_MTIME		0x15	/* Set storage intervals */

#define CMD_GET_BINFO		0x1E	/* Request binary range information */
#define CMD_GET_BIN		0x1F	/* Binary data request */
#define CMD_SET_BIN		0x20	/* Send binary data */

#define CMD_TNR_VERIFY		0x32	/* Verify participant number */
#define CMD_VAR_VALUE		0x33	/* Request system variables */
#define CMD_VAR_FIND		0x34	/* Owner of a variable */
#define CMD_VAR_STATUS_OUT	0x35	/* Allocation variable - channel */
#define CMD_VAR_DEFINE_OUT	0x36	/* Allocate variable - channel */
#define CMD_VAR_STATUS_IN	0x37	/* Allocation input parameter - status */
#define CMD_VAR_DEFINE_IN	0x38	/* Allocate input parameter - variable */

#define CMD_PDELIMIT		0x28	/* Limitation of device power */
#define CMD_TEAM_FUNCTION	0x3C	/* Team function for PV inverters */

#if SMANET_AUTO_CLOSE && SMANET_RECV_THREAD
#undef SMANET_RECV_THREAD
#define SMANET_RECV_THREAD 0
#endif

#if SMANET_RECV_THREAD
#error 1
struct smanet_frame {
	int len;
	uint8_t data[284];
};
typedef struct smanet_frame smanet_frame_t;
#endif

struct smanet_session {
	uint16_t state;
	time_t last_command;
#if SMANET_USE_BUFFER
	buffer_t *b;
#endif
	int lockfd;
	char lockfile[256];
	int param_timeout;
//	list channels;
	smanet_channel_t *chans;
	int chancount;
//	smanet_value_t *values;
	smanet_value_t values[1024];
	solard_driver_t *tp;
	void *tp_handle;
//	char *transport;
	char transport[32];
//	char *target;
	char target[128];
//	char *topts;
	char topts[64];
	bool opened;
	bool connected;
	uint32_t serial;
	char type[32];
	uint16_t src;
	uint16_t dest;
	int timeouts;
	int commands;
	bool doarray;
	bool readonly;
	char errmsg[256];
#if SMANET_AUTO_CLOSE || SMANET_RECV_THREAD
#ifdef SMANET_AUTO_CLOSE
	bool auto_close;
	int auto_close_timeout;
#endif
	pthread_mutex_t lock;
	pthread_t tid;
#endif
#if SMANET_RECV_THREAD
	list frames;
#endif
};
typedef struct smanet_session smanet_session_t;

#define SMANET_STATE_OPENED	0x0001
#define SMANET_STATE_CONNECTED	0x0002
#if SMANET_AUTO_CLOSE || SMANET_RECV_THREAD
#define SMANET_STATE_RUN	0x1000
#define SMANET_STATE_STARTED	0x2000
#endif

/* Global val */
//extern smanet_session_t *smanet_session;

#define getshort(v) (short)( (((v)[1]) << 8) | (v)[0] )
#define putshort(p,v) { float tmp; *((p+1)) = ((int)(tmp = v) >> 8); *((p)) = (int)(tmp = v); }
#define putlong(p,v) { *((p+3)) = ((int)(v) >> 24); *((p)+2) = ((int)(v) >> 16); *((p+1)) = ((int)(v) >> 8); *((p+0)) = ((int)(v) & 0xff); }
#define getlong(v) (uint32_t)( (((v)[3]) << 24) | (((v)[2]) << 16) | (((v)[1]) << 8) | (((v)[0]) & 0xff) )

/* internal funcs */
int smanet_open(smanet_session_t *);
int smanet_close(smanet_session_t *);
int smanet_read_frame(smanet_session_t *, uint8_t *, int, int);
int smanet_write_frame(smanet_session_t *, uint8_t *, int);

smanet_packet_t *smanet_alloc_packet(int);
void smanet_free_packet(smanet_packet_t *);
int smanet_recv_packet(smanet_session_t *, uint8_t, int, smanet_packet_t *, int);
int smanet_send_packet(smanet_session_t *s, uint16_t src, uint16_t dest, uint8_t ctrl, uint8_t cnt, uint8_t cmd, uint8_t *buffer, int buflen);

int smanet_command(smanet_session_t *s, int cmd, smanet_packet_t *p, uint8_t *buf, int buflen);
int smanet_get_net(smanet_session_t *s);
int smanet_get_net_start(smanet_session_t *s, uint32_t *sn, char *type, int len);
int smanet_cfg_net_adr(smanet_session_t *s, int);
int smanet_syn_online(smanet_session_t *s);

#define smanet_set_state(s,v)     (s->state |= (v))
#define smanet_clear_state(s,v)   (s->state &= (~v))
#define smanet_check_state(s,v)   ((s->state & v) != 0)

#endif
