
#ifndef __SDJS_H
#define __SDJS_H

#include "client.h"
#include "jsengine.h"

struct sdjs_private {
	int argc;
	char **argv;
	solard_client_t *c;
};
typedef struct sdjs_private sdjs_private_t;

#endif
