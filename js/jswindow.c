
#include "jsclass.h"
#include "jswindow.h"
#include "jswindow_defs.h"
#include "jsengine.h"

#define dlevel 1

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
		dprintf(dlevel,"returning location...\n");\
		if (!((jswindow_t *)s->private)->location_val) { \
			jslocation_t *loc = malloc(sizeof(*loc)); \
			if (loc) { \
				memset(loc,0,sizeof(*loc)); \
				loc->protocol = "file:"; \
				JSObject *locobj = js_location_new(cx,obj,loc); \
				if (locobj) ((jswindow_t *)s->private)->location_val = OBJECT_TO_JSVAL(locobj); \
			} \
		} \
		*rval = ((jswindow_t *)s->private)->location_val; \
		break; \
	default: \
		return global_getprop(cx,obj,id,rval);

#define EWINDOW_PROPSPEC \
	{ "location",EWINDOW_PROPERTY_ID_LOCATION,JSPROP_ENUMERATE| JSPROP_READONLY },\
	JSWINDOW_PROPSPEC

JSBool js_window_setInterval(JSContext *cx, uintN argc, jsval *rval) {
	dprintf(dlevel,"setInterval called\n");
	return JS_TRUE;
}

#define EWINDOW_FUNCSPEC \
	JS_FN("setInterval",js_window_setInterval,0,0,0),

static JSBool (*global_getprop)(JSContext *cx, JSObject *obj, jsval id, jsval *rval);

mkstdclass(window,EWINDOW,JSEngine);

/* Need to use our own getprop here - 2 different contexts */
#if 0
static JSBool sdjs_window_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	dprintf(0,"sdjs_window_getprop called!\n");
	return global_getprop(cx,obj,id,rval);
	return JS_FALSE;
}
#endif

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

/******************************* init ****************************************/
static jsnavigator_t nav = { };

static jsdocument_t doc = { };

static jswindow_t win = { };

typedef JSObject *(js_newobj_t)(JSContext *cx, JSObject *parent, void *priv);
static void newgobj(JSContext *cx, char *name, js_newobj_t func, void *priv) {
	JSObject *newobj, *global = JS_GetGlobalObject(cx);
	jsval newval;

	newobj = func(cx, global, priv);
	dprintf(1,"%s newobj: %p\n", name, newobj);
	if (newobj) {
		newval = OBJECT_TO_JSVAL(newobj);
		JS_DefineProperty(cx, global, name, newval, 0, 0, JSPROP_ENUMERATE);
	}
}

JSBool JS_InitWindow(JSContext *cx, JSObject *parent, void *priv) {
	JSEngine *e = priv;
	JSClass *classp = OBJ_GET_CLASS(cx, parent);

//	printf("parent name: %s\n", classp->name);

	nav.userAgent = "Mozilla/5.0 (X11; Linux x86_64; rv:102.0) Gecko/20100101 Firefox/102.0 debugger eval code:1:9";
	newgobj(cx,"navigator",js_navigator_new,&nav);
	doc.compatMode = "BackCompat";
	newgobj(cx,"document",js_document_new,&doc);

	/* Window is a special case - we highjack global and define all windows props/funcs in global */
	e->private = &win;
	global_getprop = classp->getProperty;
	classp->getProperty = js_window_getprop;
	JS_DefineWindow(cx,parent);
	return JS_TRUE;
}

