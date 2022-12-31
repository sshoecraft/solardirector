
#ifndef __JSDOCUMENT_H
#define __JSDOCUMENT_H

#include "jsclass.h"

#if 0
Property / Method	Description
activeElement	Returns the currently focused element in the document
anchors	Deprecated
applets	Deprecated
baseURI	Returns the absolute base URI of a document
body	Sets or returns the documents body (the <body> element)
charset	Deprecated
characterSet	Returns the character encoding for the document
cookie	Returns all name/value pairs of cookies in the document
defaultView	Returns the window object associated with a document, or null if none is available.
designMode	Controls whether the entire document should be editable or not.
doctype	Returns the Document Type Declaration associated with the document
documentElement	Returns the Document Element of the document (the <html> element)
documentMode	Deprecated
documentURI	Sets or returns the location of the document
domain	Returns the domain name of the server that loaded the document
domConfig	Deprecated
embeds	Returns a collection of all <embed> elements the document
forms	Returns a collection of all <form> elements in the document
head	Returns the <head> element of the document
images	Returns a collection of all <img> elements in the document
implementation	Returns the DOMImplementation object that handles this document
inputEncoding	Deprecated
lastModified	Returns the date and time the document was last modified
links	Returns a collection of all <a> and <area> elements in the document that have a href attribute
readyState	Returns the (loading) status of the document
referrer	Returns the URL of the document that loaded the current document
scripts	Returns a collection of <script> elements in the document
strictErrorChecking	Deprecated
title	Sets or returns the title of the document
URL	Returns the full URL of the HTML document

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

struct jsdocument {
	char *compatMode;
};
typedef struct jsdocument jsdocument_t;

#if 0
	void *activeElement;
	void *anchors;
	void *applets;
	void *baseURI;
	void *body;
	void *charset;
	void *characterSet;
	void *ookie;
	void *defaultView;
	bool designMode;
	void *doctype;
	void *documentElement;
	bool documentMode;
	void *documentURI;
	void *domain;
	void *domConfig;
	void *embeds;
	void *forms;
	void *head;
	void *images;
	void *implementation;
	void *inputEncoding;
	void *lastModified;
	void *links;
	void *readyState;
	void *referrer;
	void *scripts;
	bool *strictErrorChecking;
	void *title;
	void *URL;
#endif
extern JSObject *js_document_new(JSContext *cx, JSObject *parent, void *priv);

#endif /* __JS_DOUMENT_H */
