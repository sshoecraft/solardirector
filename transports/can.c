
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define DEBUG_CAN 1
#define dlevel 7

#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG DEBUG_CAN
#include "debug.h"

#include "transports.h"
#ifndef WINDOWS
#include "can.h"
#include <sys/ioctl.h>
#include <fcntl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>
#include <linux/netlink.h>
#include <linux/can/raw.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <pthread.h>

#define DEFAULT_BITRATE 250000
#define CAN_INTERFACE_LEN 16

struct can_session {
	int fd;
	char errmsg[128];
	char interface[CAN_INTERFACE_LEN+1];
	int bitrate;
	struct can_filter *filters;
	int filters_size;
	canid_t min,max;
	int (*read)(struct can_session *,uint32_t *control,void *,int);
	pthread_t th;
	struct can_frame *frames;
	int frames_size;
	canid_t start,end;
	uint32_t *bitmaps;
	int bitmaps_size;
};
typedef struct can_session can_session_t;

static int can_start_buffer(can_session_t *s, canid_t start, canid_t end);
static int can_read_direct(can_session_t *s, uint32_t *control, void *buf, int bufsize);
static int can_read_buffer(can_session_t *s, uint32_t *control, void *buf, int bufsize);
static int can_stop_buffer(can_session_t *s);

static int can_config_clear_filter(can_session_t *s) {
	dprintf(dlevel,"s->filters: %p\n", s->filters);
	if (s->filters) {
		int r;

		r = setsockopt(s->fd, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0);
		dprintf(dlevel,"r: %d\n", r);
		free(s->filters);
		s->filters = 0;
		if (r < 0) return 1;
	}
	return 0;
}

static int can_config_set_filter(can_session_t *s, struct can_config_filter *fs) {
	register int i;
	canid_t id;

	dprintf(dlevel,"fs: %p, s->filters: %p\n", fs, s->filters);
	if (s->filters) {
		sprintf(s->errmsg,"unable to set filters: filters already defined");
		return 1;
	}

	if (!fs) {
		can_config_clear_filter(s);
		return 0;
	}

	dprintf(dlevel,"fs->count: %d\n",fs->count);
	for(i=0; i < fs->count; i++) dprintf(dlevel,"id[%d]: %03x\n", i, fs->id[i]);

	if (!fs->count) return 0;

	s->filters_size = fs->count * sizeof(struct can_filter);
	s->filters = malloc(s->filters_size);
	if (!s->filters) {
		log_syserror("can_config_set_filter: malloc(%d)", s->filters_size);
		return 1;
	}
	s->min = 0xFFFFFFFF;
	s->max = 0;
	for(i=0; i < fs->count; i++) {
		id = fs->id[i];
//		s->filters[i].can_id = id | CAN_INV_FILTER;
		s->filters[i].can_id = id;
		s->filters[i].can_mask = CAN_SFF_MASK;
		dprintf(dlevel,"can_id: %x, min: %x, max: %x\n", id, s->min, s->max);
		if (id < s->min) s->min = id;
		if (id > s->max) s->max = id;
	}
	if (s->min > s->max) s->min = s->max;
	return 0;
}

static int can_config_set_range(can_session_t *s, canid_t start, canid_t end) {
	struct can_config_filter fs;
	int i,r,id;

	dprintf(dlevel,"start: %03x, end: %03x\n", start, end);

	fs.count = end - start;
	dprintf(dlevel,"count: %d\n", fs.count);
	fs.id = malloc(fs.count * sizeof(canid_t));
	if (!fs.id) {
		log_syserror("can_config_set_range: malloc(%d)", fs.count * sizeof(canid_t));
		return 1;
	}
	id = start;
	for(i=0; i < fs.count; i++) fs.id[i] = id++;
	r = can_config_set_filter(s,&fs);
	free(fs.id);

	return r;
}

static void _add_range(list ids, int start, int end) {
	canid_t id;

	for(id=start; id <= end; id++) {
		dprintf(dlevel,"adding: 0x%03x\n", id);
		list_add(ids,&id,sizeof(id));
	}
}

static void can_parse_filter(can_session_t *s, char *inspec) {
	char spec[256], temp[256];
	int have_start, have_plus;
	struct can_config_filter fs;
	canid_t id,start,end,*idp;
	register char *p;
	register int i;
	list ids;

	dprintf(dlevel,"inspec: %s\n", inspec);
	strncpy(spec,stredit(inspec,"COLLAPSE,LOWERCASE,TRIM"),sizeof(spec)-1);
	dprintf(dlevel,"spec: %s\n", spec);

	have_start = have_plus = 0;
	i = 0;
	p = spec;
	ids = list_create();
	while(*p) {
		dprintf(dlevel,"p: %c, i: %d, have_start: %d\n", *p, i, have_start, have_plus);
		if (*p == '-') {
			temp[i] = 0;
			start = strtol(temp,0,0);
			have_start = 1;
			i = 0;
		} else if (*p == '+') {
			temp[i] = 0;
			if (have_start) {
				end = strtol(temp,0,0);
				_add_range(ids,start,end);
				have_start = 0;
			} else {
				id = strtol(temp,0,0);
				dprintf(dlevel,"adding: 0x%03x\n", id);
				list_add(ids,&id,sizeof(id));
			}
			i = 0;
		} else {
			temp[i++] = *p;
		}
		p++;
	}
	dprintf(dlevel,"i: %d\n", i);
	if (i) {
		temp[i] = 0;
		if (have_start) {
			end = strtol(temp,0,0);
			_add_range(ids,start,end);
		} else {
			id = strtol(temp,0,0);
			dprintf(dlevel,"adding: 0x%03x\n", id);
			list_add(ids,&id,sizeof(id));
		}
	}
	fs.count = list_count(ids);
	dprintf(dlevel,"ids count: %d\n", fs.count);
	fs.id = malloc(fs.count * sizeof(canid_t));
	if (!fs.id) {
		log_syserror("can_config_set_range: malloc(%d)",fs.count * sizeof(canid_t));
		return;
	}
	i = 0;
	dprintf(dlevel,"adding IDs...\n");
	list_reset(ids);
	while((idp = list_get_next(ids)) != 0) fs.id[i++] = *idp;
	list_destroy(ids);
	dprintf(dlevel,"setting filter...\n");
	i = can_config_set_filter(s, &fs);
	free(fs.id);
}

static void *can_new(void *target, void *topts) {
	can_session_t *s;
	char *p;

	dprintf(dlevel,"target: %s, topts: %s\n", target, topts);
	if (!target) return 0;

	s = calloc(sizeof(*s),1);
	if (!s) {
		log_syserror("can_new: malloc");
		return 0;
	}
	s->fd = -1;
	s->read = can_read_direct;

	strncat(s->interface,strele(0,",",(char *)target),sizeof(s->interface)-1);
	if (topts) {
		p = strele(0,",",(char *)topts);
		s->bitrate = strtol(p,0,0);
		if (!s->bitrate) s->bitrate = DEFAULT_BITRATE;
		p = strele(1,",",(char *)topts);
		dprintf(dlevel,"filter: %s\n", p);
		can_parse_filter(s,p);
		p = strele(2,",",(char *)topts);
		dprintf(dlevel,"buffer: %s\n", p);
		if ((strcasecmp(p,"yes") == 0 || strcasecmp(p,"true") == 0) && s->filters) can_start_buffer(s,s->min,s->max);
	}
	dprintf(dlevel,"interface: %s, bitrate: %d\n",s->interface,s->bitrate);

        return s;
}

/* The below get/set speed code was pulled from libsocketcan
https://git.pengutronix.de/cgit/tools/libsocketcan

*/

/* libsocketcan.c
 *
 * (C) 2009 Luotao Fu <l.fu@pengutronix.de>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define IF_UP 1
#define IF_DOWN 2

#define GET_STATE 1
#define GET_RESTART_MS 2
#define GET_BITTIMING 3
#define GET_CTRLMODE 4
#define GET_CLOCK 5
#define GET_BITTIMING_CONST 6
#define GET_BERR_COUNTER 7
#define GET_XSTATS 8
#define GET_LINK_STATS 9

/*
 * CAN bit-timing parameters
 *
 * For further information, please read chapter "8 BIT TIMING
 * REQUIREMENTS" of the "Bosch CAN Specification version 2.0"
 * at http://www.semiconductors.bosch.de/pdf/can2spec.pdf.
 */
struct can_bittiming {
	__u32 bitrate;		/* Bit-rate in bits/second */
	__u32 sample_point;	/* Sample point in one-tenth of a percent */
	__u32 tq;		/* Time quanta (TQ) in nanoseconds */
	__u32 prop_seg;		/* Propagation segment in TQs */
	__u32 phase_seg1;	/* Phase buffer segment 1 in TQs */
	__u32 phase_seg2;	/* Phase buffer segment 2 in TQs */
	__u32 sjw;		/* Synchronisation jump width in TQs */
	__u32 brp;		/* Bit-rate prescaler */
};

/*
 * CAN harware-dependent bit-timing constant
 *
 * Used for calculating and checking bit-timing parameters
 */
struct can_bittiming_const {
	char name[16];		/* Name of the CAN controller hardware */
	__u32 tseg1_min;	/* Time segement 1 = prop_seg + phase_seg1 */
	__u32 tseg1_max;
	__u32 tseg2_min;	/* Time segement 2 = phase_seg2 */
	__u32 tseg2_max;
	__u32 sjw_max;		/* Synchronisation jump width */
	__u32 brp_min;		/* Bit-rate prescaler */
	__u32 brp_max;
	__u32 brp_inc;
};

/*
 * CAN clock parameters
 */
struct can_clock {
	__u32 freq;		/* CAN system clock frequency in Hz */
};

/*
 * CAN operational and error states
 */
enum can_state {
	CAN_STATE_ERROR_ACTIVE = 0,	/* RX/TX error count < 96 */
	CAN_STATE_ERROR_WARNING,	/* RX/TX error count < 128 */
	CAN_STATE_ERROR_PASSIVE,	/* RX/TX error count < 256 */
	CAN_STATE_BUS_OFF,		/* RX/TX error count >= 256 */
	CAN_STATE_STOPPED,		/* Device is stopped */
	CAN_STATE_SLEEPING,		/* Device is sleeping */
	CAN_STATE_MAX
};

/*
 * CAN bus error counters
 */
struct can_berr_counter {
	__u16 txerr;
	__u16 rxerr;
};

/*
 * CAN controller mode
 */
struct can_ctrlmode {
	__u32 mask;
	__u32 flags;
};

#define CAN_CTRLMODE_LOOPBACK		0x01	/* Loopback mode */
#define CAN_CTRLMODE_LISTENONLY		0x02	/* Listen-only mode */
#define CAN_CTRLMODE_3_SAMPLES		0x04	/* Triple sampling mode */
#define CAN_CTRLMODE_ONE_SHOT		0x08	/* One-Shot mode */
#define CAN_CTRLMODE_BERR_REPORTING	0x10	/* Bus-error reporting */
#define CAN_CTRLMODE_FD			0x20	/* CAN FD mode */
#define CAN_CTRLMODE_PRESUME_ACK	0x40	/* Ignore missing CAN ACKs */

/*
 * CAN device statistics
 */
struct can_device_stats {
	__u32 bus_error;	/* Bus errors */
	__u32 error_warning;	/* Changes to error warning state */
	__u32 error_passive;	/* Changes to error passive state */
	__u32 bus_off;		/* Changes to bus off state */
	__u32 arbitration_lost; /* Arbitration lost errors */
	__u32 restarts;		/* CAN controller re-starts */
};

/*
 * CAN netlink interface
 */
enum {
	IFLA_CAN_UNSPEC,
	IFLA_CAN_BITTIMING,
	IFLA_CAN_BITTIMING_CONST,
	IFLA_CAN_CLOCK,
	IFLA_CAN_STATE,
	IFLA_CAN_CTRLMODE,
	IFLA_CAN_RESTART_MS,
	IFLA_CAN_RESTART,
	IFLA_CAN_BERR_COUNTER,
	IFLA_CAN_DATA_BITTIMING,
	IFLA_CAN_DATA_BITTIMING_CONST,
	__IFLA_CAN_MAX
};

#define IFLA_CAN_MAX	(__IFLA_CAN_MAX - 1)
struct get_req {
	struct nlmsghdr n;
	struct ifinfomsg i;
};

struct set_req {
	struct nlmsghdr n;
	struct ifinfomsg i;
	char buf[1024];
};

struct req_info {
	__u8 restart;
	__u8 disable_autorestart;
	__u32 restart_ms;
	struct can_ctrlmode *ctrlmode;
	struct can_bittiming *bittiming;
};

#define parse_rtattr_nested(tb, max, rta) \
        (parse_rtattr((tb), (max), RTA_DATA(rta), RTA_PAYLOAD(rta)))

#define NLMSG_TAIL(nmsg) \
        ((struct rtattr *) (((void *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

#define IFLA_CAN_MAX    (__IFLA_CAN_MAX - 1)

static int send_dump_request(int fd, const char *name, int family, int type)
{
	struct get_req req;

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = sizeof(req);
	req.n.nlmsg_type = type;
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_pid = 0;
	req.n.nlmsg_seq = 0;

	req.i.ifi_family = family;
	/*
	 * If name is null, set flag to dump link information from all
	 * interfaces otherwise, just dump specified interface's link
	 * information.
	 */
	if (name == NULL) {
		req.n.nlmsg_flags |= NLM_F_DUMP;
	} else {
		req.i.ifi_index = if_nametoindex(name);
		if (req.i.ifi_index == 0) {
			log_error("Cannot find device \"%s\"\n", name);
			return -1;
		}
	}

	return send(fd, (void *)&req, sizeof(req), 0);
}
static void parse_rtattr(struct rtattr **tb, int max, struct rtattr *rta, int len) {
	memset(tb, 0, sizeof(*tb) * (max + 1));
	while (RTA_OK(rta, len)) {
		if (rta->rta_type <= max) {
			tb[rta->rta_type] = rta;
		}

		rta = RTA_NEXT(rta, len);
	}
}

static int do_get_nl_link(int fd, __u8 acquire, const char *name, void *res) {
	struct sockaddr_nl peer;

	char cbuf[64];
	char nlbuf[1024 * 8];

	int ret = -1;
	int done = 0;

	struct iovec iov = {
		.iov_base = (void *)nlbuf,
		.iov_len = sizeof(nlbuf),
	};

	struct msghdr msg = {
		.msg_name = (void *)&peer,
		.msg_namelen = sizeof(peer),
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = &cbuf,
		.msg_controllen = sizeof(cbuf),
		.msg_flags = 0,
	};
	struct nlmsghdr *nl_msg;
	ssize_t msglen;

	struct rtattr *linkinfo[IFLA_INFO_MAX + 1];
	struct rtattr *can_attr[IFLA_CAN_MAX + 1];

	if (send_dump_request(fd, name, AF_PACKET, RTM_GETLINK) < 0) {
		log_error("do_get_nl_link: cannot send dump request");
		return ret;
	}

	while (!done && (msglen = recvmsg(fd, &msg, 0)) > 0) {
		size_t u_msglen = (size_t) msglen;
		/* Check to see if the buffers in msg get truncated */
		if (msg.msg_namelen != sizeof(peer) ||
		    (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC))) {
			log_error("Uhoh... truncated message.\n");
			return -1;
		}

		for (nl_msg = (struct nlmsghdr *)nlbuf;
		     NLMSG_OK(nl_msg, u_msglen);
		     nl_msg = NLMSG_NEXT(nl_msg, u_msglen)) {
			int type = nl_msg->nlmsg_type;
			int len;

			if (type == NLMSG_DONE) {
				done++;
				continue;
			}
			if (type != RTM_NEWLINK)
				continue;

			struct ifinfomsg *ifi = NLMSG_DATA(nl_msg);
			struct rtattr *tb[IFLA_MAX + 1];

			len =
				nl_msg->nlmsg_len - NLMSG_LENGTH(sizeof(struct ifaddrmsg));
			parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), len);

			/* Finish process if the reply message is matched */
			if (strcmp((char *)RTA_DATA(tb[IFLA_IFNAME]), name) == 0)
				done++;
			else
				continue;

			if (acquire == GET_LINK_STATS) {
				if (!tb[IFLA_STATS64]) {
					log_error("no link statistics (64-bit) found\n");
				} else {
					memcpy(res, RTA_DATA(tb[IFLA_STATS64]), sizeof(struct rtnl_link_stats64));
					ret = 0;
				}
				continue;
			}

			if (tb[IFLA_LINKINFO])
				parse_rtattr_nested(linkinfo,
						    IFLA_INFO_MAX, tb[IFLA_LINKINFO]);
			else
				continue;

			if (acquire == GET_XSTATS) {
				if (!linkinfo[IFLA_INFO_XSTATS])
					log_error("no can statistics found\n");
				else {
					memcpy(res, RTA_DATA(linkinfo[IFLA_INFO_XSTATS]),
					       sizeof(struct can_device_stats));
					ret = 0;
				}
				continue;
			}

			if (!linkinfo[IFLA_INFO_DATA]) {
#if 0
				log_error("no link data found\n");
#endif
				return ret;
			}

			parse_rtattr_nested(can_attr, IFLA_CAN_MAX, linkinfo[IFLA_INFO_DATA]);

			switch (acquire) {
			case GET_STATE:
				if (can_attr[IFLA_CAN_STATE]) {
					*((int *)res) = *((__u32 *)
							  RTA_DATA(can_attr
								   [IFLA_CAN_STATE]));
					ret = 0;
#if 0
				} else {
					log_error("no state data found\n");
#endif
				}

				break;
			case GET_RESTART_MS:
				if (can_attr[IFLA_CAN_RESTART_MS]) {
					*((__u32 *) res) = *((__u32 *)
							     RTA_DATA(can_attr
								      [IFLA_CAN_RESTART_MS]));
					ret = 0;
				}
#if 0
				else
					log_error("no restart_ms data found\n");
#endif

				break;
			case GET_BITTIMING:
				if (can_attr[IFLA_CAN_BITTIMING]) {
					memcpy(res,
					       RTA_DATA(can_attr[IFLA_CAN_BITTIMING]),
					       sizeof(struct can_bittiming));
					ret = 0;
				}
#if 0
				else log_error("no bittiming data found\n");
#endif

				break;
			case GET_CTRLMODE:
				if (can_attr[IFLA_CAN_CTRLMODE]) {
					memcpy(res,
					       RTA_DATA(can_attr[IFLA_CAN_CTRLMODE]),
					       sizeof(struct can_ctrlmode));
					ret = 0;
				}
#if 0
				else log_error("no ctrlmode data found\n");
#endif

				break;
			case GET_CLOCK:
				if (can_attr[IFLA_CAN_CLOCK]) {
					memcpy(res,
					       RTA_DATA(can_attr[IFLA_CAN_CLOCK]),
					       sizeof(struct can_clock));
					ret = 0;
				}
#if 0
				else log_error("no clock parameter data found\n");
#endif

				break;
			case GET_BITTIMING_CONST:
				if (can_attr[IFLA_CAN_BITTIMING_CONST]) {
					memcpy(res,
					       RTA_DATA(can_attr[IFLA_CAN_BITTIMING_CONST]),
					       sizeof(struct can_bittiming_const));
					ret = 0;
				}
#if 0
				else log_error("no bittiming_const data found\n");
#endif

				break;
			case GET_BERR_COUNTER:
				if (can_attr[IFLA_CAN_BERR_COUNTER]) {
					memcpy(res,
					       RTA_DATA(can_attr[IFLA_CAN_BERR_COUNTER]),
					       sizeof(struct can_berr_counter));
					ret = 0;
				}
#if 0
				else log_error("no berr_counter data found\n");
#endif

				break;

#if 0
			default:
				log_error("unknown acquire mode\n");
#endif
			}
		}
	}

	return ret;
}

static int open_nl_sock()
{
	int fd;
	int sndbuf = 32768;
	int rcvbuf = 32768;
	unsigned int addr_len;
	struct sockaddr_nl local;

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (fd < 0) {
		log_syserror("open_nl_sock: cannot open netlink socket");
		return -1;
	}

	setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&sndbuf, sizeof(sndbuf));

	setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&rcvbuf, sizeof(rcvbuf));

	memset(&local, 0, sizeof(local));
	local.nl_family = AF_NETLINK;
	local.nl_groups = 0;

	if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
		log_syserror("open_nl_sock: Cannot bind netlink socket");
		return -1;
	}

	addr_len = sizeof(local);
	if (getsockname(fd, (struct sockaddr *)&local, &addr_len) < 0) {
		log_syserror("open_nl_sock: Cannot getsockname");
		return -1;
	}
	if (addr_len != sizeof(local)) {
		log_error("Wrong address length %u\n", addr_len);
		return -1;
	}
	if (local.nl_family != AF_NETLINK) {
		log_error("Wrong address family %d\n", local.nl_family);
		return -1;
	}
	return fd;
}

static int get_link(const char *name, __u8 acquire, void *res) {
	int err, fd;

	fd = open_nl_sock();
	if (fd < 0)
		return -1;

	err = do_get_nl_link(fd, acquire, name, res);
	close(fd);

	return err;

}

int can_get_bittiming(const char *name, struct can_bittiming *bt) {
	return get_link(name, GET_BITTIMING, bt);
}

static int addattr32(struct nlmsghdr *n, size_t maxlen, int type, __u32 data)
{
	int len = RTA_LENGTH(4);
	struct rtattr *rta;

	if (NLMSG_ALIGN(n->nlmsg_len) + len > maxlen) {
		log_error("addattr32: Error! max allowed bound %zu exceeded\n", maxlen);
		return -1;
	}

	rta = NLMSG_TAIL(n);
	rta->rta_type = type;
	rta->rta_len = len;
	memcpy(RTA_DATA(rta), &data, 4);
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + len;

	return 0;
}

static int addattr_l(struct nlmsghdr *n, size_t maxlen, int type,
		     const void *data, int alen)
{
	int len = RTA_LENGTH(alen);
	struct rtattr *rta;

	if (NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len) > maxlen) {
		log_error("addattr_l ERROR: message exceeded bound of %zu\n", maxlen);
		return -1;
	}

	rta = NLMSG_TAIL(n);
	rta->rta_type = type;
	rta->rta_len = len;
	memcpy(RTA_DATA(rta), data, alen);
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len);

	return 0;
}

static int send_mod_request(int fd, struct nlmsghdr *n)
{
	int status;
	struct sockaddr_nl nladdr;
	struct nlmsghdr *h;

	struct iovec iov = {
		.iov_base = (void *)n,
		.iov_len = n->nlmsg_len
	};
	struct msghdr msg = {
		.msg_name = &nladdr,
		.msg_namelen = sizeof(nladdr),
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};
	char buf[16384];

	memset(&nladdr, 0, sizeof(nladdr));

	nladdr.nl_family = AF_NETLINK;
	nladdr.nl_pid = 0;
	nladdr.nl_groups = 0;

	n->nlmsg_seq = 0;
	n->nlmsg_flags |= NLM_F_ACK;

	status = sendmsg(fd, &msg, 0);

	if (status < 0) {
		log_syserror("send_mod_request: Cannot talk to rtnetlink");
		return -1;
	}

	iov.iov_base = buf;
	while (1) {
		iov.iov_len = sizeof(buf);
		status = recvmsg(fd, &msg, 0);
		for (h = (struct nlmsghdr *)buf; (size_t) status >= sizeof(*h);) {
			int len = h->nlmsg_len;
			int l = len - sizeof(*h);
			if (l < 0 || len > status) {
				if (msg.msg_flags & MSG_TRUNC) {
					log_error("Truncated message\n");
					return -1;
				}
				log_error("!!!malformed message: len=%d\n", len);
				return -1;
			}

			if (h->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *err =
				    (struct nlmsgerr *)NLMSG_DATA(h);
				if ((size_t) l < sizeof(struct nlmsgerr)) {
					log_error("ERROR truncated\n");
				} else {
					errno = -err->error;
					if (errno == 0)
						return 0;

					log_syserror("send_mod_request: RTNETLINK answers");
				}
				return -1;
			}
			status -= NLMSG_ALIGN(len);
			h = (struct nlmsghdr *)((char *)h + NLMSG_ALIGN(len));
		}
	}

	return 0;
}

static int do_set_nl_link(int fd, __u8 if_state, const char *name, struct req_info *req_info)
{
	struct set_req req;

	const char *type = "can";

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	req.n.nlmsg_type = RTM_NEWLINK;
	req.i.ifi_family = 0;

	req.i.ifi_index = if_nametoindex(name);
	if (req.i.ifi_index == 0) {
		log_error("Cannot find device \"%s\"\n", name);
		return -1;
	}

	if (if_state) {
		switch (if_state) {
		case IF_DOWN:
			req.i.ifi_change |= IFF_UP;
			req.i.ifi_flags &= ~IFF_UP;
			break;
		case IF_UP:
			req.i.ifi_change |= IFF_UP;
			req.i.ifi_flags |= IFF_UP;
			break;
		default:
			log_error("unknown state\n");
			return -1;
		}
	}

	if (req_info != NULL) {
		/* setup linkinfo section */
		struct rtattr *linkinfo = NLMSG_TAIL(&req.n);
		addattr_l(&req.n, sizeof(req), IFLA_LINKINFO, NULL, 0);
		addattr_l(&req.n, sizeof(req), IFLA_INFO_KIND, type, strlen(type));
		/* setup data section */
		struct rtattr *data = NLMSG_TAIL(&req.n);
		addattr_l(&req.n, sizeof(req), IFLA_INFO_DATA, NULL, 0);

		if (req_info->restart_ms > 0 || req_info->disable_autorestart)
			addattr32(&req.n, 1024, IFLA_CAN_RESTART_MS, req_info->restart_ms);

		if (req_info->restart) addattr32(&req.n, 1024, IFLA_CAN_RESTART, 1);

		if (req_info->bittiming != NULL) {
			addattr_l(&req.n, 1024, IFLA_CAN_BITTIMING, req_info->bittiming, sizeof(struct can_bittiming));
		}

		if (req_info->ctrlmode != NULL) {
			addattr_l(&req.n, 1024, IFLA_CAN_CTRLMODE, req_info->ctrlmode, sizeof(struct can_ctrlmode));
		}

		/* mark end of data section */
		data->rta_len = (void *)NLMSG_TAIL(&req.n) - (void *)data;

		/* mark end of link info section */
		linkinfo->rta_len = (void *)NLMSG_TAIL(&req.n) - (void *)linkinfo;
	}

	return send_mod_request(fd, &req.n);
}

static int set_link(const char *name, __u8 if_state, struct req_info *req_info)
{
	int err, fd;

	fd = open_nl_sock();
	if (fd < 0)
		return -1;

	err = do_set_nl_link(fd, if_state, name, req_info);
	close(fd);

	return err;
}

int can_set_bittiming(const char *name, struct can_bittiming *bt)
{
	struct req_info req_info = {
		.bittiming = bt,
	};

	return set_link(name, 0, &req_info);
}

int can_set_bitrate(const char *name, __u32 bitrate)
{
	struct can_bittiming bt;

	memset(&bt, 0, sizeof(bt));
	bt.bitrate = bitrate;

	return can_set_bittiming(name, &bt);
}

/* End of libsocketcan code */

static int up_down_up(int fd, char *interface) {
	struct ifreq ifr;

	memset(&ifr,0,sizeof(ifr));
	strcpy(ifr.ifr_name,interface);

	/* Bring up the IF */
	ifr.ifr_flags |= (IFF_UP | IFF_RUNNING | IFF_NOARP);
	if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
		log_syserror("up_down_up: SIOCSIFFLAGS IFF_UP");
		return 1;
	}

	/* Bring down the IF */
	ifr.ifr_flags = 0;
	if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
		log_syserror("up_down_up: SIOCSIFFLAGS IFF_DOWN");
		return 1;
	}

	/* Bring up the IF */
	ifr.ifr_flags |= (IFF_UP | IFF_RUNNING | IFF_NOARP);
	if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
		log_syserror("up_down_up: SIOCSIFFLAGS IFF_UP");
		return 1;
	}
	return 0;
}

static int can_open(void *handle) {
	can_session_t *s = handle;
	struct can_bittiming bt;
	struct sockaddr_can addr;
	struct ifreq ifr;
	int if_up;

	/* If already open, dont try again */
	dprintf(dlevel,"fd: %d\n", s->fd);
	if (s->fd >= 0) return 0;

	/* Open socket */
	dprintf(dlevel,"opening socket...\n");
	s->fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (s->fd < 0) {
		log_syserror("can_open: socket");
		return 1;
	}
	dprintf(dlevel,"fd: %d\n", s->fd);

	/* Get the state */
	memset(&ifr,0,sizeof(ifr));
	strcpy(ifr.ifr_name,s->interface);
	if (ioctl(s->fd, SIOCGIFFLAGS, &ifr) < 0) {
		log_syserror("can_open: SIOCGIFFLAGS");
		goto can_open_error;
	}
	if_up = ((ifr.ifr_flags & IFF_UP) != 0 ? 1 : 0);

	dprintf(dlevel,"if_up: %d\n",if_up);
	if (!if_up) {
		/* Set the bitrate */
		can_set_bitrate(s->interface, s->bitrate);
#if 0
		if (can_set_bitrate(s->interface, s->bitrate) < 0) {
			printf("ERROR: unable to set bitrate on %s!\n", s->interface);
			goto can_open_error;
		}
#endif
		up_down_up(s->fd,s->interface);
	} else {
		/* Get the current timing */
		if (can_get_bittiming(s->interface,&bt) < 0) {
//			log_error("can_open: unable to get bittiming");
//			goto can_open_error;
			bt.bitrate = s->bitrate;
		}

		/* Check the bitrate */
		dprintf(dlevel,"current bitrate: %d, wanted bitrate: %d\n", bt.bitrate, s->bitrate);
		if (bt.bitrate != s->bitrate) {
			/* Bring down the IF */
			ifr.ifr_flags = 0;
			dprintf(dlevel,"can_open: SIOCSIFFLAGS clear\n");
			if (ioctl(s->fd, SIOCSIFFLAGS, &ifr) < 0) {
				log_syserror("can_open: SIOCSIFFLAGS IFF_DOWN");
				goto can_open_error;
			}

			/* Set the bitrate */
			dprintf(dlevel,"setting bitrate...\n");
			if (can_set_bitrate(s->interface, s->bitrate) < 0) {
				printf("ERROR: unable to set bitrate on %s!\n", s->interface);
				goto can_open_error;
			}
			up_down_up(s->fd,s->interface);

		}
	}

	/* Get IF index */
	strcpy(ifr.ifr_name, s->interface);
	if (ioctl(s->fd, SIOCGIFINDEX, &ifr) < 0) {
		log_syserror("can_open: SIOCGIFINDEX");
		goto can_open_error;
	}

	/* Bind the socket */
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if (bind(s->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		log_syserror("can_open: bind");
		goto can_open_error;
	}

	/* If a filter specified, set it */
	if (s->filters) {
#if 0
		int join_filter = 1;

		dprintf(dlevel,"setting join_filter = 1\n");
		if (setsockopt(s->fd, SOL_CAN_RAW, CAN_RAW_JOIN_FILTERS, &join_filter, sizeof(join_filter)) < 0) {
			log_error("setsockopt CAN_RAW_JOIN_FILTERS not supported by your Linux Kernel");
		}
#endif
#if 0
		{
			int i,count;
			count = s->filters_size / sizeof(struct can_filter);
			dprintf(dlevel,"count: %d\n", count);
			for(i=0; i < s->filters_size / sizeof(struct can_filter); i++) {
				printf("filter[%d].can_id: %x, can_mask: %x\n", i, s->filters[i].can_id, s->filters[i].can_mask);
			}
		}
#endif
		dprintf(dlevel,"setting filters... size: %d\n", s->filters_size);
		if (setsockopt(s->fd, SOL_CAN_RAW, CAN_RAW_FILTER, s->filters, s->filters_size) < 0) {
			log_syserror("setsockopt CAN_RAW_FILTER");
			goto can_open_error;
		}
	}

//	fcntl(s->fd, F_SETFL, O_NONBLOCK);

#if 0
	{
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		setsockopt(s->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	}
#endif

	dprintf(dlevel,"done!\n");
	return 0;
can_open_error:
	close(s->fd);
	s->fd = -1;
	return 1;
}

static int can_read(void *handle, uint32_t *control, void *buf, int buflen) {
	can_session_t *s = handle;
	int bytes;

	/* If not open, error */
	if (s->fd < 0) return -1;

	/* buflen MUST be frame size */
	dprintf(dlevel,"buf: %p, buflen: %d\n", buf, buflen);
	if (buflen != sizeof(struct can_frame)) {
//		sprintf(s->errmsg,"can_read: buflen < frame size");
		return -1;
	}

	dprintf(dlevel,"fd: %d\n", s->fd);
	bytes = s->read(s, control, buf, buflen);
	dprintf(dlevel,"bytes read: %d\n", bytes);

#ifdef DEBUG
	if (bytes == sizeof(struct can_frame)) {
		if (bytes > 0 && debug >= 8) bindump("FROM DEVICE",buf,bytes);
	}
#endif
	dprintf(dlevel,"returning: %d\n", bytes);
	return (bytes);
}

static int can_write(void *handle, uint32_t *control, void *buf, int buflen) {
	can_session_t *s = handle;
	int bytes;

	if (s->fd < 0) return -1;

	dprintf(dlevel,"buf: %p, buflen: %d\n", buf, buflen);
	if (buflen != sizeof(struct can_frame)) {
//		sprintf(s->errmsg,"can_read: buflen < frame size");
		return -1;
	}

#ifdef DEBUG
	if (debug >= 5) bindump("TO DEVICE",buf,buflen);
#endif
	dprintf(dlevel,"id: %03x\n", ((struct can_frame *)buf)->can_id);
	bytes = write(s->fd, buf, buflen);
	dprintf(dlevel,"fd: %d, returning: %d\n", s->fd, bytes);
	return bytes;
}

static int can_close(void *handle) {
	can_session_t *s = handle;

	if (s->fd >= 0) {
		dprintf(dlevel,"closing %d\n", s->fd);
		close(s->fd);
		s->fd = -1;
	}
	return 0;
}

static void *can_recv_thread(void *handle) {
	can_session_t *s = handle;
	struct can_frame frame;
	int bytes,id,idx;
	uint32_t mask;
#ifndef __WIN32
	sigset_t set;

	/* Ignore SIGPIPE */
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	sigprocmask(SIG_BLOCK, &set, NULL);
#endif

	/* Enable us to be cancelled immediately */
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,0);

	dprintf(dlevel,"thread started!\n");
//	while(s->flags & CAN_FLAG_RUNNING)) {
	while(1) {
		/* If not open, clear the bitmaps and loop */
		dprintf(8,"fd: %d\n", s->fd);
		if (s->fd < 0) {
			memset(s->bitmaps,0,s->bitmaps_size);
			sleep(1);
			continue;
		}

		/* Read a frame */
		bytes = read(s->fd,&frame,sizeof(frame));
		dprintf(8,"bytes: %d\n", bytes);
		if (bytes < 1) continue;

		/* In range? */
		dprintf(8,"frame.can_id: %03x\n",frame.can_id);
		if (frame.can_id < s->start || frame.can_id >= s->end) continue;
//		bindump("frame",&frame,sizeof(frame));

		/* Yes, store the frame and set the bitmap */
		id = frame.can_id - s->start;
		idx = id / 32;
		mask = 1 << (id % 32);
		dprintf(8,"id: %03x, idx: %d, mask: %08x\n", id, idx, mask);
		memcpy(&s->frames[id],&frame,sizeof(frame));
		s->bitmaps[idx] |= mask;
	}
	dprintf(dlevel,"returning!\n");
	return 0;
}

static int can_read_direct(can_session_t *s, uint32_t *id, void *buf, int buflen) {
	struct can_frame *frame = buf;
	int bytes;

	dprintf(dlevel,"fd: %d\n", s->fd);
	if (s->fd < 0) return -1;

	if (!id) return 0;
	dprintf(dlevel,"id: %03x\n", *id);

	/* Keep reading until we get our ID */
	do {
		dprintf(8,"fd: %d\n", s->fd);
		bytes = read(s->fd, frame, sizeof(*frame));
		dprintf(8,"bytes: %d\n", bytes);
		if (bytes < 0) {
			if (errno != EAGAIN) bytes = -1;
			break;
		}
		if (bytes != sizeof(struct can_frame)) {
			log_error("can_read_direct: incomplete CAN frame\n");
			continue;
		}
		dprintf(8,"id: %x, frame->can_id: %x\n", id, frame->can_id);
	} while(*id != 0xFFFF && frame->can_id != *id);
#ifdef DEBUG
	if (bytes > 0 && debug >= 8) bindump("FROM DEVICE",buf,sizeof(struct can_frame));
#endif
	dprintf(dlevel,"returning: %d\n", bytes);
	return bytes;
}

static int can_read_buffer(can_session_t *s, uint32_t *control, void *data, int datasz) {
	uint32_t mask;
	canid_t can_id;
	int id,idx,retries;

	if (!control) return 0;
	can_id = *control;
	dprintf(dlevel,"id: %03x\n", can_id);

	dprintf(dlevel,"id: %03x, start: %03x, end: %03x\n", can_id, s->min, s->max);
	if (can_id < s->start || can_id >= s->end) return 0;

	dprintf(dlevel,"datasz: %d\n", datasz);
	if (datasz != sizeof(struct can_frame)) return -1;

	id = can_id - s->start;
	idx = id / 32;
	mask = 1 << (id % 32);
	dprintf(dlevel,"id: %03x, mask: %08x, bitmaps[%d]: %08x\n", id, mask, idx, s->bitmaps[idx]);
	retries=5;
	while(retries--) {
		if ((s->bitmaps[idx] & mask) == 0) {
			dprintf(dlevel,"bit not set, sleeping...\n");
			sleep(1);
			continue;
		}
//		if (debug >= 5) bindump("FRAME",&s->frames[id],sizeof(struct can_frame));
		dprintf(dlevel,"returning data!\n");
		memcpy(data,&s->frames[id],datasz);
		return sizeof(struct can_frame);
	}
	return 0;
}

static int can_start_buffer(can_session_t *s, canid_t start, canid_t end) {
	pthread_attr_t attr;
	int num;

	dprintf(dlevel,"start: %03x, end: %03x\n", start, end);

	/* if we already have buffer set, exit */
	if (s->frames) {
		sprintf(s->errmsg,"unable to start buffer: buffer already defined");
		return 1;
	}

	/* Create the frame buffer and bitmaps */
	num = end-start;
	dprintf(dlevel,"num: %d\n", num);

	/* Alloc frames */
	s->frames_size = num * sizeof(struct can_frame);
	dprintf(dlevel,"frames_size: %d\n", s->frames_size);
	s->frames = calloc(s->frames_size,1);
	if (!s->frames) {
		log_syserror("can_start_buffer: malloc frames(%d)",s->frames_size);
		return 1;
	}

	/* Alloc bitmaps */
	s->bitmaps_size = num / 32;
	if (!s->bitmaps_size) s->bitmaps_size = 1;
	dprintf(dlevel,"bitmaps_size: %d\n", s->bitmaps_size);
	s->bitmaps = calloc(s->bitmaps_size,1);
	if (!s->bitmaps) {
		log_syserror("can_start_buffer: malloc bitmaps(%d)",s->bitmaps_size);
		return 1;
	}

	s->start = start;
	s->end = end;

	/* Create a detached thread */
	dprintf(dlevel,"creating thread...\n");
	if (pthread_attr_init(&attr)) {
		sprintf(s->errmsg,"pthread_attr_init: %s",strerror(errno));
		goto can_set_reader_error;
	}
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) {
		sprintf(s->errmsg,"pthread_attr_setdetachstate: %s",strerror(errno));
		goto can_set_reader_error;
	}
	if (pthread_create(&s->th,&attr,&can_recv_thread,s)) {
		sprintf(s->errmsg,"pthread_create: %s",strerror(errno));
		goto can_set_reader_error;
	}
	pthread_attr_destroy(&attr);

	dprintf(dlevel,"setting func to buffer\n");
	s->read = can_read_buffer;
	return 0;

can_set_reader_error:
	s->read = can_read_direct;
	return 1;
}

static int can_stop_buffer(can_session_t *s) {
	dprintf(dlevel,"frames: %p\n", s->frames);
	if (!s->frames) return 1;
	s->read = can_read_direct;
	pthread_cancel(s->th);
	if (s->frames) {
		free(s->frames);
		s->frames = 0;
	}
	if (s->bitmaps) {
		free(s->bitmaps);
		s->bitmaps = 0;
	}
	return 0;
}

static int can_config(void *handle, int func, ...) {
	can_session_t *s = handle;
	va_list va;
	int r;

	r = 1;
	va_start(va,func);
	switch(func) {
	case CAN_CONFIG_GET_FD:
	    {
		int *fdptr = va_arg(va,int *);

		*fdptr = s->fd;
	    }
	    break;
	case CAN_CONFIG_SET_RANGE:
	    {
		canid_t start, end;

		start = va_arg(va,canid_t);
		end = va_arg(va,canid_t);
		r = can_config_set_range(s,start,end);
	    }
	    break;
	case CAN_CONFIG_SET_FILTER:
		r = can_config_set_filter(s,va_arg(va,struct can_config_filter *));
		break;
	case CAN_CONFIG_PARSE_FILTER:
		can_parse_filter(s,va_arg(va,char *));
		r = 0;
		break;
		r = can_config_set_filter(s,va_arg(va,struct can_config_filter *));
	case CAN_CONFIG_CLEAR_FILTER:
		r = can_config_clear_filter(s);
		break;
	case CAN_CONFIG_START_BUFFER:
	    {
		canid_t start, end;

		start = va_arg(va,canid_t);
		end = va_arg(va,canid_t);
		r = can_start_buffer(s,start,end);
	    }
	    break;
	case CAN_CONFIG_STOP_BUFFER:
		r = can_stop_buffer(s);
		break;
	default:
		dprintf(dlevel,"error: unhandled func: %d\n", func);
		break;
	}
	return r;
}

static int can_destroy(void *handle) {
	can_session_t *s = handle;

	can_config_clear_filter(s);
	can_stop_buffer(s);
        if (s->fd >= 0) can_close(s);
        free(s);
        return 0;
}

solard_driver_t can_driver = {
	"can",
	can_new,
	can_destroy,
	can_open,
	can_close,
	can_read,
	can_write,
	can_config
};
#else
solard_driver_t can_driver = { "can" };
#endif
