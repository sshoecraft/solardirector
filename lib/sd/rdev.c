
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 7
#include "debug.h"

#ifdef __WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/signal.h>
#endif
#include <fcntl.h>
#include <ctype.h>
#include "transports.h"
#include "rdev.h"

#define RDEV_HEADER_SIZE 8
#define RDEV_TARGET_LEN 128
#define RDEV_DEFAULT_PORT 3930

struct rdev_session {
	int fd;
	char target[RDEV_TARGET_LEN];
	int port;
	char name[RDEV_NAME_LEN];
	char type[RDEV_TYPE_LEN];
	uint8_t unit;
};
typedef struct rdev_session rdev_session_t;

static int rdev_init = 0;

/*************************************** Global funcs ***************************************/

int rdev_send(socket_t fd, uint8_t opcode, uint8_t unit, uint32_t control, void *buf, uint16_t buflen) {
	uint8_t header[RDEV_HEADER_SIZE];
	int bytes,sent;

	dprintf(dlevel,"fd: %d\n", fd);
	if (fd < 0) return -1;

	sent = 0;

	/* Send the header */
	dprintf(dlevel,"opcode: %d, unit: %d, control: %x, buflen: %d\n", opcode, unit, control, buflen);
	header[0] = opcode;
	header[1] = unit;
	_putu32(&header[2],control);
	_putu16(&header[6],buflen);
//	bindump("SENDING",header,sizeof(header));
	bytes = send(fd,header,RDEV_HEADER_SIZE, 0);
	dprintf(dlevel,"bytes: %d\n", bytes);
	if (bytes < 0) return -1;
	sent += bytes;

	/* Send the buf */
	if (buf && buflen) {
//		if (debug >= 1) bindump("SENDING",buf,buflen);
		bytes = send(fd,buf,buflen,0);
		dprintf(dlevel,"bytes: %d\n", bytes);
		if (bytes < 0) return -1;
		sent += bytes;
	}

	dprintf(dlevel,"returning: %d\n", sent);
	return sent;
}

int rdev_recv(socket_t fd, uint8_t *opcode, uint8_t *unit, uint32_t *control, void *buf, int bufsz, int timeout) {
	uint8_t header[RDEV_HEADER_SIZE];
	uint32_t len;
	int bytes,readlen;
	uint8_t ch;
	register int i;

	dprintf(dlevel,"fd: %d, timeout: %d\n", fd, timeout);
	if (fd < 0) return -1;

	if (timeout > 0) {
		struct timeval tv;
		fd_set fds;
		int num;

		FD_ZERO(&fds);
		FD_SET(fd,&fds);
		tv.tv_usec = 0;
		tv.tv_sec = 5;
		dprintf(dlevel,"waiting...\n");
		num = select(fd+1,&fds,0,0,&tv);
		dprintf(dlevel,"num: %d\n", num);
		if (num < 1) return -1;
	}

	/* Read the header */
	bytes = recv(fd, header, RDEV_HEADER_SIZE, 0);
	dprintf(dlevel,"bytes: %d\n", bytes);
	if (bytes < 0) return -1;
//	if (bytes > 0 && debug >= dlevel +1) bindump("RECEIVED",header,sizeof(header));
	*opcode = header[0];
	*unit = header[1];
	*control = _getu32(&header[2]);
	len = _getu16(&header[6]);

	dprintf(dlevel,"header: opcode: %02x, unit: %d, control: %x, len: %d\n", *opcode, *unit, *control, len);

	/* Read the data */
	dprintf(dlevel,"len: %d, bufsz: %d\n",len,bufsz);
	readlen = (len > bufsz ? bufsz : len);
	dprintf(dlevel,"readlen: %d\n", readlen);
	if (buf && len) {
		bytes = recv(fd, buf, readlen, 0);
		dprintf(dlevel,"bytes: %d\n", bytes);
		if (bytes < 0) return -1;
//		if (debug >= 1) bindump("RECEIVED",buf,bytes);
		/* XXX */
//		if (bytes != readlen) return 0;
	} else {
		bytes = 0;
	}
	dprintf(dlevel,"bytes: %d\n", bytes);

	/* drain any remaining bytes */
	readlen = len - bytes;
	dprintf(dlevel,"bytes: %d, len: %d, readlen: %d\n", bytes, len, readlen);
	for(i=0; i < readlen; i++) recv(fd, (char *)&ch, 1, 0);

	/* Return bytes received */
	dprintf(dlevel,"returning: %d\n", bytes);
	return bytes;
}

int rdev_request(socket_t fd, uint8_t *opcode, uint8_t *unit, uint32_t *control, void *data, uint16_t len, int timeout) {
	int bytes;
	uint32_t ctemp = (control ? *control : 0);

	/* Send the req */
	bytes = rdev_send(fd,*opcode,*unit,ctemp,data,len);
	dprintf(dlevel,"bytes: %d\n", bytes);
	if (bytes < 0) return -1;

	/* Read the response */
	bytes = rdev_recv(fd,opcode,unit,&ctemp,data,len,timeout);
	dprintf(dlevel,"bytes: %d\n",bytes);
	if (bytes < 0) return -1;
	else if (control) *control = ctemp;
	return bytes;
}

/*************************************** Driver funcs ***************************************/

static void *rdev_new(void *target, void *topts) {
	rdev_session_t *s;
	char temp[128];
	char *p;

	s = calloc(sizeof(*s),1);
	if (!s) {
		log_syserror("rdev_new: malloc");
		return 0;
	}
	s->fd = -1;

	/* Format is addr:port,name */
	dprintf(dlevel,"target: %s\n", target);
	temp[0] = 0;
	strncat(temp,strele(0,",",(char *)target),sizeof(temp)-1);
	strncat(s->target,strele(0,":",temp),sizeof(s->target)-1);
	dprintf(dlevel,"s->target: %s\n", s->target);
	p = strele(1,":",temp);
	if (strlen(p)) s->port = atoi(p);
	if (!s->port) s->port = RDEV_DEFAULT_PORT;

	if (!topts) {
		log_write(LOG_ERROR,"rdev requires name in topts\n");
		return 0;
	}
	strncat(s->name,strele(0,",",topts),sizeof(s->name)-1);
	if (!strlen(s->name)) {
		log_write(LOG_ERROR,"rdev requires name in topts\n");
		return 0;
	}

#ifndef __WIN32
	if (!rdev_init) {
		sigset_t set;

		/* Ignore SIGPIPE */
		sigemptyset(&set);
		sigaddset(&set, SIGPIPE);
		sigprocmask(SIG_BLOCK, &set, NULL);
		rdev_init = 1;
	}
#endif

	dprintf(dlevel,"target: %s, port: %d, name: %s\n", s->target, s->port, s->name);
	return s;
}

static int rdev_open(void *handle) {
	rdev_session_t *s = handle;
	struct sockaddr_in addr;
	socklen_t sin_size;
	uint8_t status;
	uint32_t control;
	int bytes;
	char temp[SOLARD_TARGET_LEN];

	if (s->fd >= 0) return 0;

	dprintf(dlevel,"Creating socket...\n");
	s->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s->fd < 0) {
		dprintf(dlevel,"rdev_open: socket");
		return 1;
	}

	/* Try to resolve the target */
	dprintf(dlevel,"target: %s\n",s->target);
	*temp = 0;
	if (os_gethostbyname(temp,sizeof(temp),s->target)) strcpy(temp,s->target);
	if (!*temp) strcpy(temp,s->target);
	dprintf(dlevel,"temp: %s\n",temp);

	dprintf(dlevel,"Connecting to: %s\n",temp);
	memset(&addr,0,sizeof(addr));
	sin_size = sizeof(addr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(temp);
	addr.sin_port = htons(s->port);
	if (connect(s->fd,(struct sockaddr *)&addr,sin_size) < 0) {
		dprintf(dlevel,"rdev_open: connect");
		close(s->fd);
		s->fd = -1;
		return 1;
	}

	dprintf(dlevel,"sending open\n");
	bytes = rdev_send(s->fd,RDEV_OPCODE_OPEN,0,0,s->name,strlen(s->name)+1);
	dprintf(dlevel,"sent bytes: %d\n", bytes);

	/* Read the reply */
	bytes = rdev_recv(s->fd,&status,&s->unit,&control,s->type,sizeof(s->type)-1,10);
	if (bytes < 0) return -1;
	dprintf(dlevel,"recvd bytes: %d, status: %d, unit: %d\n", bytes, status, s->unit);
	dprintf(dlevel,"type: %s\n", s->type);
	if (status != 0) {
		close(s->fd);
		s->fd = -1;
		return 1;
	}

	return 0;
}

static int rdev_read(void *handle, uint32_t *control, void *buf, int buflen) {
	rdev_session_t *s = handle;
	uint8_t rdlen[sizeof(uint32_t)],status,runit;
	uint32_t ctemp;
	int bytes;

	ctemp = (control ? *control : 0);
	dprintf(dlevel,"unit: %d, control: %x, buf: %p, buflen: %d\n", s->unit, ctemp, buf, buflen);

	memset(rdlen,0,sizeof(rdlen));
	_putu16(rdlen,((uint16_t)buflen));
	bytes = rdev_send(s->fd,RDEV_OPCODE_READ,s->unit,ctemp,rdlen,sizeof(rdlen));
	dprintf(dlevel,"bytes sent: %d\n", bytes);
	if (bytes < 0) return -1;

	/* Get the response */
	status = runit = 0;
	bytes = rdev_recv(s->fd,&status,&runit,&ctemp,buf,buflen,10);
	dprintf(dlevel,"bytes recvd: %d, status: %d, runit: %d\n", bytes, status, runit);
	if (bytes < 0) return -1;
	if (bytes > 0 && debug >= dlevel+1) bindump("==> READ",buf,bytes);
	if (control) *control = ctemp;
#if 0
	dprintf(dlevel,"returning: %d\n", status * (-1));
	return (status * (-1));
#else
	dprintf(dlevel,"returning: %d\n", bytes);
	return bytes;
#endif
}

static int rdev_write(void *handle, uint32_t *control, void *buf, int buflen) {
	rdev_session_t *s = handle;
	uint8_t status,runit;
	uint32_t ctemp;
	int bytes;

	ctemp = (control ? *control : 0);
	dprintf(dlevel,"unit: %d, control: %x, buf: %p, buflen: %d\n", s->unit, ctemp, buf, buflen);

	if (debug >= dlevel+1) bindump("==> WRITE",buf,buflen);
	bytes = rdev_send(s->fd,RDEV_OPCODE_WRITE,s->unit,ctemp,buf,buflen);
	dprintf(dlevel,"bytes: %d\n", bytes);
	if (bytes < 0) return -1;

	/* Read the response */
	status = runit = 0;
	bytes = rdev_recv(s->fd,&status,&runit,&ctemp,0,0,10);
	dprintf(dlevel,"bytes recvd: %d, status: %d, runit: %d, control: %x\n", bytes, status, runit, control);
	if (bytes < 0) return -1;
	if (control) *control = ctemp;
#if 0
	dprintf(dlevel,"returning: %d\n", status * (-1));
	return (status * (-1));
#else
	dprintf(dlevel,"returning: %d\n", bytes);
	return bytes;
#endif
}


static int rdev_close(void *handle) {
	rdev_session_t *s = handle;

	if (s->fd >= 0) {
		uint8_t opcode,unit;

		opcode = RDEV_OPCODE_CLOSE;
		unit = s->unit;
		rdev_request(s->fd,&opcode,&unit,0,0,0,0);
		close(s->fd);
		s->fd = -1;
	}
	return 0;
}

static int rdev_destroy(void *handle) {
        rdev_session_t *s = handle;

        if (s->fd >= 0) rdev_close(s);
	free(s);
	return 0;
}

static int rdev_config(void *h, int func, ...) {
	va_list ap;
	int r;

	r = 1;
	va_start(ap,func);
	switch(func) {
	default:
		dprintf(dlevel,"error: unhandled func: %d\n", func);
		break;
	}
	return r;
}

solard_driver_t rdev_driver = {
	"rdev",
	rdev_new,
	rdev_destroy,
	rdev_open,
	rdev_close,
	rdev_read,
	rdev_write,
	rdev_config
};
