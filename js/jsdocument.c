
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

#if 0
try.js:8 e: [object HTMLCollection]
src: http://192.168.1.5/try.js
type: text/javascript
noModule: false
charset: 
async: false
defer: false
crossOrigin: null
text: 
referrerPolicy: 
event: 
htmlFor: 
integrity: 
fetchPriority: auto
blocking: 
title: 
lang: 
translate: true
dir: 
hidden: false
accessKey: 
draggable: false
spellcheck: true
autocapitalize: 
contentEditable: inherit
enterKeyHint: 
isContentEditable: false
inputMode: 
virtualKeyboardPolicy: 
offsetParent: null
offsetTop: 0
offsetLeft: 0
offsetWidth: 0
offsetHeight: 0
innerText: 
outerText: 
onbeforexrselect: null
onabort: null
onbeforeinput: null
onblur: null
oncancel: null
oncanplay: null
oncanplaythrough: null
onchange: null
onclick: null
onclose: null
oncontextlost: null
oncontextmenu: null
oncontextrestored: null
oncuechange: null
ondblclick: null
ondrag: null
ondragend: null
ondragenter: null
ondragleave: null
ondragover: null
ondragstart: null
ondrop: null
ondurationchange: null
onemptied: null
onended: null
onerror: null
onfocus: null
onformdata: null
oninput: null
oninvalid: null
onkeydown: null
onkeypress: null
onkeyup: null
onload: null
onloadeddata: null
onloadedmetadata: null
onloadstart: null
onmousedown: null
onmouseenter: null
onmouseleave: null
onmousemove: null
onmouseout: null
onmouseover: null
onmouseup: null
onmousewheel: null
onpause: null
onplay: null
onplaying: null
onprogress: null
onratechange: null
onreset: null
onresize: null
onscroll: null
onsecuritypolicyviolation: null
onseeked: null
onseeking: null
onselect: null
onslotchange: null
onstalled: null
onsubmit: null
onsuspend: null
ontimeupdate: null
ontoggle: null
onvolumechange: null
onwaiting: null
onwebkitanimationend: null
onwebkitanimationiteration: null
onwebkitanimationstart: null
onwebkittransitionend: null
onwheel: null
onauxclick: null
ongotpointercapture: null
onlostpointercapture: null
onpointerdown: null
onpointermove: null
onpointerrawupdate: null
onpointerup: null
onpointercancel: null
onpointerover: null
onpointerout: null
onpointerenter: null
onpointerleave: null
onselectstart: null
onselectionchange: null
onanimationend: null
onanimationiteration: null
onanimationstart: null
ontransitionrun: null
ontransitionstart: null
ontransitionend: null
ontransitioncancel: null
oncopy: null
oncut: null
onpaste: null
dataset: [object DOMStringMap]
nonce: 
autofocus: false
tabIndex: -1
style: [object CSSStyleDeclaration]
attributeStyleMap: [object StylePropertyMap]
attachInternals: [function]
blur: [function]
click: [function]
focus: [function]
inert: false
oncontentvisibilityautostatechange: null
onbeforematch: null
namespaceURI: http://www.w3.org/1999/xhtml
prefix: null
localName: script
tagName: SCRIPT
id: 
className: 
classList: 
slot: 
attributes: [object NamedNodeMap]
shadowRoot: null
part: 
assignedSlot: null
innerHTML: 
outerHTML: <script src="try.js" type="text/javascript"></script>
scrollTop: 0
scrollLeft: 0
scrollWidth: 0
scrollHeight: 0
clientTop: 0
clientLeft: 0
clientWidth: 0
clientHeight: 0
onbeforecopy: null
onbeforecut: null
onbeforepaste: null
onsearch: null
elementTiming: 
onfullscreenchange: null
onfullscreenerror: null
onwebkitfullscreenchange: null
onwebkitfullscreenerror: null
role: null
ariaAtomic: null
ariaAutoComplete: null
ariaBusy: null
ariaBrailleLabel: null
ariaBrailleRoleDescription: null
ariaChecked: null
ariaColCount: null
ariaColIndex: null
ariaColSpan: null
ariaCurrent: null
ariaDescription: null
ariaDisabled: null
ariaExpanded: null
ariaHasPopup: null
ariaHidden: null
ariaInvalid: null
ariaKeyShortcuts: null
ariaLabel: null
ariaLevel: null
ariaLive: null
ariaModal: null
ariaMultiLine: null
ariaMultiSelectable: null
ariaOrientation: null
ariaPlaceholder: null
ariaPosInSet: null
ariaPressed: null
ariaReadOnly: null
ariaRelevant: null
ariaRequired: null
ariaRoleDescription: null
ariaRowCount: null
ariaRowIndex: null
ariaRowSpan: null
ariaSelected: null
ariaSetSize: null
ariaSort: null
ariaValueMax: null
ariaValueMin: null
ariaValueNow: null
ariaValueText: null
children: [object HTMLCollection]
firstElementChild: null
lastElementChild: null
childElementCount: 0
previousElementSibling: null
nextElementSibling: null
after: [function]
animate: [function]
append: [function]
attachShadow: [function]
before: [function]
closest: [function]
computedStyleMap: [function]
getAttribute: [function]
getAttributeNS: [function]
getAttributeNames: [function]
getAttributeNode: [function]
getAttributeNodeNS: [function]
getBoundingClientRect: [function]
getClientRects: [function]
getElementsByClassName: [function]
getElementsByTagName: [function]
getElementsByTagNameNS: [function]
getInnerHTML: [function]
hasAttribute: [function]
hasAttributeNS: [function]
hasAttributes: [function]
hasPointerCapture: [function]
insertAdjacentElement: [function]
insertAdjacentHTML: [function]
insertAdjacentText: [function]
matches: [function]
prepend: [function]
querySelector: [function]
querySelectorAll: [function]
releasePointerCapture: [function]
remove: [function]
removeAttribute: [function]
removeAttributeNS: [function]
removeAttributeNode: [function]
replaceChildren: [function]
replaceWith: [function]
requestFullscreen: [function]
requestPointerLock: [function]
scroll: [function]
scrollBy: [function]
scrollIntoView: [function]
scrollIntoViewIfNeeded: [function]
scrollTo: [function]
setAttribute: [function]
setAttributeNS: [function]
setAttributeNode: [function]
setAttributeNodeNS: [function]
setPointerCapture: [function]
toggleAttribute: [function]
webkitMatchesSelector: [function]
webkitRequestFullScreen: [function]
webkitRequestFullscreen: [function]
checkVisibility: [function]
getAnimations: [function]
nodeType: 1
nodeName: SCRIPT
baseURI: http://192.168.1.5/try3.html
isConnected: true
ownerDocument: [object HTMLDocument]
parentNode: [object HTMLBodyElement]
parentElement: [object HTMLBodyElement]
childNodes: [object NodeList]
firstChild: null
lastChild: null
previousSibling: [object Text]
nextSibling: null
nodeValue: null
textContent: 
ELEMENT_NODE: 1
ATTRIBUTE_NODE: 2
TEXT_NODE: 3
CDATA_SECTION_NODE: 4
ENTITY_REFERENCE_NODE: 5
ENTITY_NODE: 6
PROCESSING_INSTRUCTION_NODE: 7
COMMENT_NODE: 8
DOCUMENT_NODE: 9
DOCUMENT_TYPE_NODE: 10
DOCUMENT_FRAGMENT_NODE: 11
NOTATION_NODE: 12
DOCUMENT_POSITION_DISCONNECTED: 1
DOCUMENT_POSITION_PRECEDING: 2
DOCUMENT_POSITION_FOLLOWING: 4
DOCUMENT_POSITION_CONTAINS: 8
DOCUMENT_POSITION_CONTAINED_BY: 16
DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC: 32
appendChild: [function]
cloneNode: [function]
compareDocumentPosition: [function]
contains: [function]
getRootNode: [function]
hasChildNodes: [function]
insertBefore: [function]
isDefaultNamespace: [function]
isEqualNode: [function]
isSameNode: [function]
lookupNamespaceURI: [function]
lookupPrefix: [function]
normalize: [function]
removeChild: [function]
replaceChild: [function]
addEventListener: [function]
dispatchEvent: [function]
removeEventListener: [function]
#endif

extern const char *get_script_name(JSContext *cx);

static JSBool js_document_getElementById(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	return JS_TRUE;
}

static JSBool js_document_getElementsByTagName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	char *tag;

	tag = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "s", &tag)) return JS_FALSE;
	dprintf(0,"tag: %s\n", tag);
	if (strcmp(tag,"script") == 0) {
		dprintf(0,"adding script...\n");
		JSObject *newobj = JS_NewObject(cx, 0, 0, obj);
		jsval src = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,get_script_name(cx)));
		JS_DefineProperty(cx, newobj, "src", src, 0, 0, JSPROP_ENUMERATE);
		jsval val = OBJECT_TO_JSVAL(newobj);
		*rval = OBJECT_TO_JSVAL(JS_NewArrayObject(cx, 1, &val));
	} else {
		*rval = OBJECT_TO_JSVAL(JS_NewArrayObject(cx, 0, NULL));
	}
	return JS_TRUE;
}

extern JSBool JS_Printf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool js_document_write(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	return JS_Printf(cx, obj, argc, argv, rval);
}

#define JSDOCUMENT_FUNCSPEC \
	JS_FN("addEventListener",js_document_addEventListener,0,0,0), \
	JS_FN("createElement",js_document_createElement,0,0,0), \
	JS_FS("getElementsByTagName",js_document_getElementsByTagName,0,0,0), \
	JS_FS("getElementById",js_document_getElementById,0,0,0), \
	JS_FS("write",js_document_write,0,0,0),

mkstdclass(document,JSDOCUMENT,jsdocument_t);
