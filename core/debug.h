
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __DEBUG_H
#define __DEBUG_H

#include <stdio.h>
#include "utils.h"

extern int debug;

#if defined(DEBUG) && DEBUG > 0
#define dprintf(level, format, args...) { if (debug >= level) log_info("%s(%d) %s: " format,__FILE__,__LINE__, __FUNCTION__, ## args); }
#else
#ifdef DEBUG_STDOUT
#define dprintf(level,format,args...) printf("%s(%d) %s: " format,__FILE__,__LINE__, __FUNCTION__, ## args)
#else
#define dprintf(level,format,args...) /* noop */
#endif
#endif

#ifdef DEBUG_MEM
#include "debugmem.h"
#endif

#endif /* __DEBUG_H */
