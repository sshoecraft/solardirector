
#include "jsclass.h"
#include "types.h"
#include "jslocation.h"
#include "jswindow.h"
#include "jswindow_defs.h"
#include "debug.h"

#define dlevel 0

#if 0
Window Object Methods
Method	Description
alert()	Displays an alert box with a message and an OK button
atob()	Decodes a base-64 encoded string
blur()	Removes focus from the current window
btoa()	Encodes a string in base-64
clearInterval()	Clears a timer set with setInterval()
clearTimeout()	Clears a timer set with setTimeout()
close()	Closes the current window
confirm()	Displays a dialog box with a message and an OK and a Cancel button
focus()	Sets focus to the current window
getComputedStyle()	Gets the current computed CSS styles applied to an element
getSelection()	Returns a Selection object representing the range of text selected by the user
matchMedia()	Returns a MediaQueryList object representing the specified CSS media query string
moveBy()	Moves a window relative to its current position
moveTo()	Moves a window to the specified position
open()	Opens a new browser window
print()	Prints the content of the current window
prompt()	Displays a dialog box that prompts the visitor for input
requestAnimationFrame()	Requests the browser to call a function to update an animation before the next repaint
resizeBy()	Resizes the window by the specified pixels
resizeTo()	Resizes the window to the specified width and height
scroll()	Deprecated. This method has been replaced by the scrollTo() method.
scrollBy()	Scrolls the document by the specified number of pixels
scrollTo()	Scrolls the document to the specified coordinates
setInterval()	Calls a function or evaluates an expression at specified intervals (in milliseconds)
setTimeout()	Calls a function or evaluates an expression after a specified number of milliseconds
stop()	Stops the window from loading
#endif

/* Extend the props */
enum EWINDOW {
	JSWINDOW_PROPIDS,
	EWINDOW_PROPERTY_ID_LOCATION
};

#define EWINDOW_GETPROP \
	JSWINDOW_GETPROP \
	case EWINDOW_PROPERTY_ID_LOCATION:\
		dprintf(0,"returning location...\n");\
		if (!s->location_val) { \
			location_t *loc = malloc(sizeof(*loc)); \
			if (loc) { \
				memset(loc,0,sizeof(*loc)); \
				loc->protocol = "file:"; \
				JSObject *locobj = location_new(cx,obj,loc); \
				if (locobj) s->location_val = OBJECT_TO_JSVAL(locobj); \
			} \
		} \
		*rval = s->location_val; \
		break;

#define EWINDOW_PROPSPEC \
	{ "location",EWINDOW_PROPERTY_ID_LOCATION,JSPROP_ENUMERATE| JSPROP_READONLY },\
	JSWINDOW_PROPSPEC

JSBool js_window_setInterval(JSContext *cx, uintN argc, jsval *rval) {
	dprintf(0,"setInterval called\n");
	return JS_TRUE;
}

#define EWINDOW_FUNCSPEC \
	JS_FN("setInterval",js_window_setInterval,0,0,0),

mkstdclass(jswindow,EWINDOW);

/* Add the window props/funcs to the global object */
JSBool JS_DefineWindow(JSContext *cx, JSObject *obj) {
	JSPropertySpec props[] = {
		EWINDOW_PROPSPEC,
		{ 0 }
	};
	JSFunctionSpec funcs[] = {
		EWINDOW_FUNCSPEC
		{ 0 }
	};

	dprintf(dlevel,"Defining window props...\n");
	if(!JS_DefineProperties(cx, obj, props)) {
		dprintf(dlevel,"error defining window properties\n");
		return JS_FALSE;
	}

	dprintf(dlevel,"Defining window funcs...\n");
	if(!JS_DefineFunctions(cx, obj, funcs)) {
		dprintf(dlevel,"error defining window functions\n");
		return JS_FALSE;
	}

	dprintf(dlevel,"Defining window property...\n");
	jsval val = OBJECT_TO_JSVAL(obj);
	if (!JS_DefineProperty(cx, obj, "window", val, 0, 0, 0)) {
		dprintf(dlevel,"error defining window property\n");
		return JS_FALSE;
	}

	return JS_TRUE;
}
