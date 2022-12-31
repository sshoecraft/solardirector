
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __RDEVSERVER_H
#define __RDEVSERVER_H

#include "common.h"
#include "driver.h"
#include "rdev.h"
#include <pthread.h>
#include "can.h"

#define DEVSERVER_MAX_DEVICES 8
#define DEVSERVER_NAME_SIZE 16

struct rdev_device {
	char name[DEVSERVER_NAME_SIZE];
	char target[SOLARD_TARGET_LEN];
	char topts[SOLARD_TOPTS_LEN];
	solard_driver_t *driver;
	void *handle;
	bool shared;
	pthread_mutex_t lock;
};
typedef struct rdev_device rdev_device_t;

struct rdev_config {
	cfg_info_t *cfg;
	int port;
	uint8_t state;
	rdev_device_t devices[DEVSERVER_MAX_DEVICES];
	int device_count;
};
typedef struct rdev_config rdev_config_t;

#define RDEVSERVER_RUNNING 0x0001

int rdev_get_config(rdev_config_t *conf, char *configfile);
int server(rdev_config_t *);
int devserver(rdev_config_t *,socket_t);

#endif
