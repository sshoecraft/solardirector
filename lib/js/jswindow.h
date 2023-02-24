
#ifndef __JSWINDOW_H
#define __JSWINDOW_H

#include "jsdocument.h"
#include "jslocation.h"
#include "jsnavigator.h"

#if 0
Property	Description
closed	Returns a boolean true if a window is closed.
console	Returns the Console Object for the window.  See also The Console Object.
defaultStatus	Deprecated.
document	Returns the Document object for the window.  See also The Document Object.
frameElement	Returns the frame in which the window runs.
frames	Returns all window objects running in the window.
history	Returns the History object for the window.  See also The History Object.
innerHeight	Returns the height of the windows content area (viewport) including scrollbars
innerWidth	Returns the width of a windows content area (viewport) including scrollbars
length	Returns the number of <iframe> elements in the current window
localStorage	Allows to save key/value pairs in a web browser. Stores the data with no expiration date
location	Returns the Location object for the window.  See also the The Location Object.
name	Sets or returns the name of a window
navigator	Returns the Navigator object for the window.  See also The Navigator object.
opener	Returns a reference to the window that created the window
outerHeight	Returns the height of the browser window, including toolbars/scrollbars
outerWidth	Returns the width of the browser window, including toolbars/scrollbars
pageXOffset	Returns the pixels the current document has been scrolled (horizontally) from the upper left corner of the window
pageYOffset	Returns the pixels the current document has been scrolled (vertically) from the upper left corner of the window
parent	Returns the parent window of the current window
screen	Returns the Screen object for the window See also The Screen object
screenLeft	Returns the horizontal coordinate of the window relative to the screen
screenTop	Returns the vertical coordinate of the window relative to the screen
screenX	Returns the horizontal coordinate of the window relative to the screen
screenY	Returns the vertical coordinate of the window relative to the screen
sessionStorage	Allows to save key/value pairs in a web browser. Stores the data for one session
scrollX	An alias of pageXOffset
scrollY	An alias of pageYOffset
self	Returns the current window
status	Deprecated. Avoid using it.
top	Returns the topmost browser window
#endif

typedef int bool;

struct jswindow {
	bool closed;
	void *console;
	void *See;
	void *defaultStatus;
	JSObject *document;
	void *frameElement;
	void *frames;
	void *history;
	int innerHeight;
	int innerWidth;
	int length;
	void *localStorage;
	JSObject *location;
	jsval location_val;
	void *name;
	JSObject *navigator;
	void *opener;
	int outerHeight;
	int outerWidth;
	int pageXOffset;
	int pageYOffset;
	void *parent;
	JSObject *screen;
	int screenLeft;
	int screenTop;
	int screenX;
	int screenY;
	void *sessionStorage;
	int scrollX;
	int scrollY;
	void *self;
	void *status;
	void *top;
//	void *undefined;
	JSBool (*global_getprop)(JSContext *cx, JSObject *obj, jsval id, jsval *rval);
};
typedef struct jswindow jswindow_t;

extern JSObject *jswindow_new(JSContext *cx, JSObject *parent, void *priv);
extern JSBool JS_InitWindow(JSContext *cx, JSObject *parent, void *jsengine);

#endif
