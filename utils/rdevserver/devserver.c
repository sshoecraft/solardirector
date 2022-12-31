
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "common.h"
#include "rdevserver.h"
#include "socket.h"

#define DEVSERVER_MAX_UNITS	8
#define DS_WHAT_SIZE		256
#define DS_DATA_SIZE		65536

struct devserver_session {
	rdev_config_t *conf;
	socket_t c;
	uint8_t data[DS_DATA_SIZE],opcode,unit;
	rdev_device_t *units[DEVSERVER_MAX_UNITS];
	int unit_count;
};
typedef struct devserver_session devserver_session_t;

#define dlevel 4

static devserver_session_t *devserver_new(rdev_config_t *conf, int c) {
	devserver_session_t *s;

	/* Create a session */
	s = malloc(sizeof(*s));
	if (!s) {
		log_syserror("devserver: malloc(%d)\n",sizeof(*s));
		return 0;
	}
	s->conf = conf;
	s->c = c;

	return s;
}

/* Send a reply */
static int devserver_reply(socket_t fd, uint8_t status, uint8_t unit, uint16_t control, uint8_t *data, int len) {
	int bytes;

	dprintf(dlevel,"status: %d, unit: %d, len: %d\n", status, unit, len);

	if (data && len && debug >= dlevel+1) bindump("==> REPLY",data,len);
	bytes = rdev_send(fd,status,unit,control,data,len);
	dprintf(dlevel,"bytes sent: %d\n", bytes);
	return (bytes < 0 ? 1 : 0);
}

/* Send error reply */
int devserver_error(socket_t fd, uint8_t code) {
	int bytes;

	dprintf(dlevel,"code: %d\n", code);
	bytes = rdev_send(fd,code,0,0,0,0);
	dprintf(dlevel,"bytes sent: %d\n", bytes);
	return (bytes < 0 ? 1 : 0);
}

static int devserver_open(devserver_session_t *s) {
	char *name = (char *)s->data;
	rdev_device_t *dev;
	int i,found;

	dprintf(dlevel,"name: %s\n", name);

	found = 0;
	for(i=0; i < s->conf->device_count; i++) {
		dprintf(dlevel,"s->conf->devices[%d].name: %s\n", i, s->conf->devices[i].name);
		if (strcmp(s->conf->devices[i].name,name) == 0) {
			dev = &s->conf->devices[i];
			dprintf(dlevel,"found!\n");
			found = 1;
			break;
		}
	}
	if (!found) {
		dprintf(dlevel,"NOT found!\n");
		return devserver_error(s->c,ENOENT);
	}

	/* Shared device? */
	dprintf(dlevel,"shared: %d\n", dev->shared);
	if (!dev->shared) {
		/* Try the lock */
		if (pthread_mutex_trylock(&dev->lock) != 0) {
			dprintf(dlevel,"lock failed!\n");
			return devserver_error(s->c,EBUSY);
		}
	}

	if (!dev->handle) {
		dev->handle = dev->driver->new(dev->target, dev->topts);
		if (!dev->handle) return devserver_error(s->c,EBUSY);
	}

	/* Open the device */
	dprintf(dlevel,"opening: %s\n", dev->name);
	if (dev->driver->open(dev->handle)) {
		if (!dev->shared) pthread_mutex_unlock(&dev->lock);
		return devserver_error(s->c,EIO);
	}

	/* Add the unit */
	s->units[s->unit_count++] = dev;

	return devserver_reply(s->c,RDEV_STATUS_SUCCESS,s->unit_count-1,0,(uint8_t *)dev->driver->name,strlen(dev->driver->name)+1);
}

static int devserver_close(devserver_session_t *s, uint8_t unit) {
	rdev_device_t *dev;
	int r;

	dprintf(dlevel,"unit: %d, unit_count: %d\n", unit, s->unit_count);
	if (unit < 0 || unit >= s->unit_count ) return devserver_error(s->c,EBADF);

	dev = s->units[unit];
	r = dev->driver->close(dev->handle);
	dprintf(dlevel,"r: %d\n", r);
	return r < 0 ? devserver_error(s->c,EIO) : devserver_reply(s->c,RDEV_STATUS_SUCCESS,unit,0,0,0);
}


static int devserver_read(devserver_session_t *s, uint8_t unit, uint32_t control) {
	rdev_device_t *dev;
	uint16_t rdlen;
	int bytes;

	dprintf(dlevel,"unit: %d, unit_count: %d\n", unit, s->unit_count);
	if (unit < 0 || unit >= s->unit_count ) return devserver_error(s->c,EBADF);

	/* bytes to read in the sent data */
	rdlen = _getu16(s->data);
	dprintf(dlevel,"rdlen: %d\n", rdlen);
	dev = s->units[unit];
	bytes = dev->driver->read(dev->handle,&control,s->data,rdlen);
	dprintf(dlevel,"bytes read: %d\n", bytes);
	return bytes < 0 ? devserver_error(s->c,EIO) : devserver_reply(s->c,RDEV_STATUS_SUCCESS,unit,control,s->data,bytes);
}

static int devserver_write(devserver_session_t *s, uint8_t unit, uint32_t control, int bytes) {
	rdev_device_t *dev;
	int r;

	dprintf(dlevel,"unit: %d, unit_count: %d, bytes: %d\n", unit, s->unit_count, bytes);
	if (unit < 0 || unit >= s->unit_count ) return devserver_error(s->c,EBADF);

	dev = s->units[unit];
	r = dev->driver->write(dev->handle,&control,s->data,bytes);
	dprintf(dlevel,"bytes read: %d\n", r);
	return r < 0 ? devserver_error(s->c,EIO) : devserver_reply(s->c,RDEV_STATUS_SUCCESS,unit,control,0,0);
}

int devserver(rdev_config_t *conf, socket_t c) {
	devserver_session_t *s;
	uint8_t opcode,unit;
	uint32_t control;
	int r,bytes,error;

	dprintf(dlevel,"************************************ DEVSERVER START *************************************\n");

	s = devserver_new(conf,c);
	if (!s) goto devserver_done;

	error = 0;
	while(!error) {
		dprintf(dlevel,"waiting for command...\n");
		opcode = unit = 0;
		bytes = rdev_recv(c,&opcode,&unit,&control,s->data,sizeof(s->data),-1);
		dprintf(dlevel,"bytes: %d\n", bytes);
		if (bytes < 0) break;
		dprintf(dlevel,"opcode: %d, unit: %d, control: %x\n", opcode, unit, control);
		switch(opcode) {
		case RDEV_OPCODE_OPEN:
			r = devserver_open(s);
			break;
		case RDEV_OPCODE_CLOSE:
			r = devserver_close(s,unit);
			break;
		case RDEV_OPCODE_READ:
			r = devserver_read(s,unit,control);
			break;
		case RDEV_OPCODE_WRITE:
			r = devserver_write(s,unit,control,bytes);
			break;
		default:
			r = devserver_error(c,ENOENT);
			break;
		}
		if (r) break;
	}

devserver_done:
	if (s->unit_count) {
		rdev_device_t *dev;

		for(r=0; r < s->unit_count; r++) {
			dprintf(dlevel,"closing: %s\n", dev->name);
			dev = s->units[r];
			dev->driver->close(dev->handle);
			dprintf(dlevel,"destroying: %s\n", dev->name);
			dev->driver->destroy(dev->handle);
		}
	}
	free(s);
	dprintf(dlevel,"closing socket\n");
	close(c);
	dprintf(dlevel,"done!\n");
	dprintf(dlevel,"************************************ DEVSERVER END *************************************\n");
	return 0;
}
