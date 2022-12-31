
#include "jsclass.h"
#include "types.h"
#include "jslocation.h"
#include "jslocation_defs.h"

#if 0
Location Object Methods
Method	Description
assign()	Loads a new document
reload()	Reloads the current document
replace()	Replaces the current document with a new one
#endif

JSBool js_location_assign(JSContext *cx, uintN argc, jsval *rval) {
	printf("location.assign called\n");
	return JS_TRUE;
}

JSBool js_location_reload(JSContext *cx, uintN argc, jsval *rval) {
	printf("location.reload called\n");
	return JS_TRUE;
}

JSBool js_location_replace(JSContext *cx, uintN argc, jsval *rval) {
	printf("location.replace called\n");
	return JS_TRUE;
}

#define LOCATION_FUNCSPEC \
	JS_FN("assign",js_location_assign,0,0,0),\
	JS_FN("reload",js_location_assign,0,0,0),\
	JS_FN("replace",js_location_assign,0,0,0),

mkstdclass(location,LOCATION);
