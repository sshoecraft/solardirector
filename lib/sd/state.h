
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __SD_STATE_H
#define __SD_STATE_H

#include "common.h"

#define set_state(c,v)	set_bit((c)->state,v)
#define clear_state(c,v)	clear_bit((c)->state,v)
#define check_state(c,v)	check_bit((c)->state,v)

#endif
