
#if 0
#include "jsclass.h"
#include "jsgeolocation.h"
#include "jsgeolocation_defs.h"
#include "jsutil.h"

#define dlevel 4

#if 0
Geolocation Object Methods
Method	Description
clearWatch()	Unregister location/error monitoring handlers previously installed using Geolocation.watchPosition()
getCurrentPosition()	Returns the current position of the device
watchPosition()	Returns a watch ID value that then can be used to unregister the handler by passing it to the Geolocation.clearWatch() method
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

#define JSLOCATION_FUNCSPEC \
        JS_FN("assign",js_location_assign,0,0,0),\
        JS_FN("reload",js_location_assign,0,0,0),\
        JS_FN("replace",js_location_assign,0,0,0),

mkstdclass(location,JSLOCATION,jslocation_t);
#endif
