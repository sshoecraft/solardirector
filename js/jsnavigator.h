
#ifndef __JSNAVIGATOR_H
#define __JSNAVIGATOR_H

#include "jsapi.h"
typedef int bool;

struct jsnavigator {
	char *appCodeName;	/* Returns browser code name */
	char *appName;		/* Returns browser name */
	char *appVersion;	/* Returns browser version */
	char *cookieEnabled;	/* Returns true if browser cookies are enabled */
	void *geolocation;	/* Returns a geolocation object for the user's location */
	bool onLine;		/* Returns true if the browser is online */
	char *platform;		/* Returns browser platform */
	char *product;		/* Returns browser engine name */
	char *userAgent;	/* Returns browser user-agent header */
};
typedef struct jsnavigator jsnavigator_t;

extern JSObject *js_navigator_new(JSContext *cx, JSObject *parent, void *priv);

#endif /* __JSNAVIGATOR_H */
