
#include "common.h"
#include "config.h"
#include "jsapi.h"
#include "jsobj.h"
#include "jsstr.h"

#define dlevel 6

#if 0
/* All of these classes are defined here */
Int8Array		 Int8		1	ToInt8		8-bit twos complement signed integer
Uint8Array		 Uint8		1	ToUint8		8-bit unsigned integer
Uint8ClampedArray	 Uint8C		1	ToUint8Clamp	8-bit unsigned integer (clamped conversion)
Int16Array		 Int16		2	ToInt16		16-bit twos complement signed integer
Uint16Array		 Uint16		2	ToUint16	16-bit unsigned integer
Int32Array		 Int32		4	ToInt32		32-bit twos complement signed integer
Uint32Array		 Uint32		4	ToUint32	32-bit unsigned integer
BigInt64Array		 BigInt64	8 	ToBigInt64	64-bit twos complement signed integer
BigUint64Array		 BigUint64	8 	ToBigUint64	64-bit unsigned integer
Float32Array		 Float32	4 			32-bit IEEE floating point
Float64Array		 Float64	8 			64-bit IEEE floating point
https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/ArrayBuffer
https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/DataView
#endif

struct arraybuf_private {
	void *buffer;
	int bufsize;
};
typedef struct arraybuf_private arraybuf_private_t;

static JSBool arraybuf_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	arraybuf_private_t *p;

	dprintf(dlevel,"getprop called!\n");
	p = JS_GetPrivate(cx,obj);
	if (!p) {
		JS_ReportError(cx,"private is null!");
		return JS_FALSE;
	}
	return JS_FALSE;
}

static JSBool arraybuf_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	arraybuf_private_t *p;

	dprintf(dlevel,"setprop called!\n");
	p = JS_GetPrivate(cx,obj);
	if (!p) {
		JS_ReportError(cx,"private is null!");
		return JS_FALSE;
	}
	return JS_FALSE;
}

static JSClass arraybuf_class = {
	"ArrayBuffer",		/* Name */
	JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	arraybuf_getprop,	/* getProperty */
	arraybuf_setprop,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool arraybuf_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
#if 0
	arraybuf_private_t *p;
	JSObject *newobj,*objp,*gobj;
	JSClass *newclassp,*ccp = &arraybuf_class;
	char *namep;
	int flags;

	if (argc < 2) {
		JS_ReportError(cx, "ArrayBuffer requires 1 arguments (size:integer)\n");
		return JS_FALSE;
	}
	namep =0;
	objp = 0;
	flags = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "s o / u", &namep, &objp, &flags)) return JS_FALSE;
	dprintf(dlevel,"name: %s, objp: %p, flags: %x\n", namep, objp, flags);

	p = JS_malloc(cx,sizeof(*p));
	if (!p) {
		JS_ReportError(cx,"error allocating memory");
		return JS_FALSE;
	}

	/* Create a new instance of Class */
	newclassp = JS_malloc(cx,sizeof(JSClass));
	*newclassp = *ccp;
	newclassp->name = p->name;

	gobj = JS_GetGlobalObject(cx);
	newobj = JS_InitClass(cx, gobj, gobj, newclassp, 0, 0, 0, 0, 0, 0);
	JS_SetPrivate(cx,newobj,p);
	*rval = OBJECT_TO_JSVAL(newobj);
#endif
	return JS_TRUE;
}

JSObject *js_InitClassClass(JSContext *cx, JSObject *gobj) {
	JSObject *obj;

	dprintf(dlevel,"Defining %s object\n",arraybuf_class.name);
	obj = JS_InitClass(cx, gobj, gobj, &arraybuf_class, arraybuf_ctor, 2, 0, 0, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize Class class");
		return 0;
	}
	dprintf(dlevel,"done!\n");
	return obj;
}
