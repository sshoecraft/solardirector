
#include "jsclass.h"
#include "jsdocument.h"
#include "jsdocument_defs.h"

#define dlevel 4

/******************************* document ****************************************/
#if 0
addEventListener()	Attaches an event handler to the document
adoptNode()	Adopts a node from another document
close()	Closes the output stream previously opened with document.open()
createAttribute()	Creates an attribute node
createComment()	Creates a Comment node with the specified text
createDocumentFragment()	Creates an empty DocumentFragment node
createElement()	Creates an Element node
createEvent()	Creates a new event
createTextNode()	Creates a Text node
execCommand()	Deprecated
getElementById()	Returns the element that has the ID attribute with the specified value
getElementsByClassName()	Returns an HTMLCollection containing all elements with the specified class name
getElementsByName()	Returns an live NodeList containing all elements with the specified name
getElementsByTagName()	Returns an HTMLCollection containing all elements with the specified tag name
hasFocus()	Returns a Boolean value indicating whether the document has focus
importNode()	Imports a node from another document
normalize()	Removes empty Text nodes, and joins adjacent nodes
normalizeDocument()	Deprecated
open()	Opens an HTML output stream to collect output from document.write()
querySelector()	Returns the first element that matches a specified CSS selector(s) in the document
querySelectorAll()	Returns a static NodeList containing all elements that matches a specified CSS selector(s) in the document
removeEventListener()	Removes an event handler from the document (that has been attached with the addEventListener() method)
renameNode()	Deprecated
write()	Writes HTML expressions or JavaScript code to a document
writeln()	Same as write(), but adds a newline character after each statement
#endif

JSBool js_document_addEventListener(JSContext *cx, uintN argc, jsval *rval) {
	dprintf(dlevel,"called!\n");
	return JS_TRUE;
}

JSBool js_document_createElement(JSContext *cx, uintN argc, jsval *rval) {
	dprintf(dlevel,"called!\n");
	return JS_TRUE;
}

JSBool js_document_getElementsByTagName(JSContext *cx, uintN argc, jsval *rval) {
	dprintf(dlevel,"called!\n");
	return JS_TRUE;
}

#define JSDOCUMENT_FUNCSPEC \
	JS_FN("addEventListener",js_document_addEventListener,0,0,0), \
	JS_FN("createElement",js_document_createElement,0,0,0), \
	JS_FN("getElementsByTagName",js_document_getElementsByTagName,0,0,0),

mkstdclass(document,JSDOCUMENT,jsdocument_t);
