
#ifndef __JS_CLASS_H
#define __JS_CLASS_H

#include "jsapi.h"
#include "jsobj.h"
#include "jsclass.h"
#include <stdlib.h>
#include <string.h>
#include "debug.h"

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)

#if 0
        } else if (JSVAL_IS_STRING(id)) {
                char *sname, *name;
                JSClass *classp = OBJ_GET_CLASS(cx, obj);
                sname = (char *)classp->name;
                name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
                printf("sname: %s, name: %s\n", sname, name);
                if (name) JS_free(cx,name);
#endif

#define mkstdclass(__CLASS_NAME,__CLASS_UCNAME,__CLASS_PRIVATE) \
enum __CLASS_UCNAME##_PROPERTY_ID {  \
	__CLASS_UCNAME##_PROPIDS \
};  \
static JSBool js_##__CLASS_NAME##_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) { \
	int prop_id; \
	__CLASS_PRIVATE *s; \
\
	s = JS_GetPrivate(cx,obj); \
	if (!s) { \
		JS_ReportError(cx, STRINGIFY(js_##__CLASS_NAME##_getprop) ": internal error: private is null!"); \
		return JS_FALSE; \
	} \
	if (JSVAL_IS_INT(id)) { \
		prop_id = JSVAL_TO_INT(id); \
		dprintf(dlevel,"prop_id: %d\n", prop_id);\
		switch(prop_id) { \
		__CLASS_UCNAME##_GETPROP \
		} \
	} \
	return JS_TRUE; \
} \
static JSClass __CLASS_NAME##_class = { \
	STRINGIFY(__CLASS_NAME),/* Name */ \
	JSCLASS_HAS_PRIVATE,    /* Flags */ \
	JS_PropertyStub,        /* addProperty */ \
	JS_PropertyStub,        /* delProperty */ \
	js_##__CLASS_NAME##_getprop, /* getProperty */ \
	JS_PropertyStub,        /* setProperty */ \
	JS_EnumerateStub,       /* enumerate */ \
	JS_ResolveStub,         /* resolve */ \
	JS_ConvertStub,         /* convert */ \
	JS_FinalizeStub,        /* finalize */ \
	JSCLASS_NO_OPTIONAL_MEMBERS \
}; \
JSObject *js_##__CLASS_NAME##_new(JSContext *cx, JSObject *parent, void *priv) {\
	JSPropertySpec props[] = { \
		__CLASS_UCNAME##_PROPSPEC, \
		{ 0 } \
	}; \
	JSFunctionSpec funcs[] = { \
		__CLASS_UCNAME##_FUNCSPEC \
		{ 0 } \
	}; \
	JSObject *obj; \
	obj = JS_InitClass(cx, parent, 0, &__CLASS_NAME##_class, 0, 0, props, funcs, 0, 0); \
	if (!obj) { \
		JS_ReportError(cx,"unable to initialize %s", __CLASS_NAME##_class.name); \
		return 0; \
	} \
	JS_SetPrivate(cx,obj,priv); \
	return obj; \
}

#endif
