
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "jsclass.h"
#include "jsnavigator.h"
#include "jsnavigator_defs.h"

#if 0
Navigator Object Properties
Property        Description
appCodeName     Returns browser code name
appName Returns browser name
appVersion      Returns browser version
cookieEnabled   Returns true if browser cookies are enabled
geolocation     Returns a geolocation object for the users location
language        Returns browser language
onLine  Returns true if the browser is online
platform        Returns browser platform
product Returns browser engine name
userAgent       Returns browser user-agent header
Navigator Object Methods
Method  Description
javaEnabled()   Returns true if the browser has Java enabled
#endif

JSBool js_navigator_javaEnabled(JSContext *cx, uintN argc, jsval *rval) {
	*rval = BOOLEAN_TO_JSVAL(false);
	return JS_TRUE;
}

#define JSNAVIGATOR_FUNCSPEC \
	JS_FN("javaEnabled",js_navigator_javaEnabled,0,0,0),

mkstdclass(navigator,JSNAVIGATOR,jsnavigator_t);
