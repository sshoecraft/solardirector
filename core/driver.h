
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __SOLARD_DRIVER_H
#define __SOLARD_DRIVER_H

#include "list.h"
#include "json.h"

typedef void *(*solard_driver_new_t)(void);
typedef int (*solard_driver_destroy_t)(void *);
typedef int (*solard_driver_openclose_t)(void *);
typedef int (*solard_driver_readwrite_t)(void *handle, uint32_t *control, void *buf, int buflen);
typedef int (*solard_driver_config_t)(void *,int,...);

struct solard_driver {
	char *name;
	void *(*new)(void *,void *);
	solard_driver_destroy_t destroy;
	solard_driver_openclose_t open;
	solard_driver_openclose_t close;
	solard_driver_readwrite_t read;
	solard_driver_readwrite_t write;
	solard_driver_config_t config;
};
typedef struct solard_driver solard_driver_t;

/* Driver configuration requests */
enum SOLARD_CONFIG {
	SOLARD_CONFIG_INIT,		/* Init driver */
	SOLARD_CONFIG_GET_INFO,		/* Get driver info */
	SOLARD_CONFIG_GET_DRIVER,
	SOLARD_CONFIG_SET_DRIVER,
	SOLARD_CONFIG_GET_HANDLE,
	SOLARD_CONFIG_SET_HANDLE,
	SOLARD_CONFIG_GET_TARGET,
	SOLARD_CONFIG_SET_TARGET,
};

#define DRIVER_NAME_SIZE 16
#define DRIVER_TARGET_SIZE 128

solard_driver_t *find_driver(solard_driver_t **transports, char *name);

#ifdef JS
void *js_driver_get_handle(JSContext *cx, JSObject *obj);
int driver_jsinit(void *);
#define DRIVER_CP "__cp__"
solard_driver_t *js_driver_get_driver(char *name);
#endif

#endif /* __SOLARD_DRIVER_H */
