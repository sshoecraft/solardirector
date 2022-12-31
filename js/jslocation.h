
#ifndef __JSLOCATION_H
#define __JSLOCATION_H

#if 0
Location Object Properties
Property	Description
hash	Sets or returns the anchor part (#) of a URL
host	Sets or returns the hostname and port number of a URL
hostname	Sets or returns the hostname of a URL
href	Sets or returns the entire URL
origin	Returns the protocol, hostname and port number of a URL
pathname	Sets or returns the path name of a URL
port	Sets or returns the port number of a URL
protocol	Sets or returns the protocol of a URL
search	Sets or returns the querystring part of a URL
#endif

#include "types.h"

struct jslocation {
	char *hash;
	char *host;
	char *hostname;
	char *href;
	char *origin;
	char *pathname;
	char *protocol;
	char *search;
};
typedef struct jslocation jslocation_t;

extern JSObject *js_location_new(JSContext *cx, JSObject *parent, void *priv);

#endif
