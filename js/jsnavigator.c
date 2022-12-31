
#include "jsclass.h"
#include "jsnavigator.h"
#include "jsnavigator_defs.h"

#define dlevel 4

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
