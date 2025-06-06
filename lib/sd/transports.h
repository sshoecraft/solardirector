
#ifndef __SDTRANSPORTS_H
#define __SDTRANSPORTS_H

#include "common.h"
#include "driver.h"

extern solard_driver_t null_driver;
#ifdef BLUETOOTH
extern solard_driver_t bt_driver;
#endif
#if !defined(__WIN32) && !defined(__WIN64)
extern solard_driver_t can_driver;
#endif
extern solard_driver_t ip_driver;
extern solard_driver_t rdev_driver;
extern solard_driver_t serial_driver;
#ifdef CURL
extern solard_driver_t http_driver;
extern solard_driver_t https_driver;
#endif

#ifdef JS
#include "jsengine.h"
int transports_jsinit(JSEngine *e);
#endif

#endif
