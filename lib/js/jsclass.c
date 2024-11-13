
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "common.h"
#include "config.h"
#include "jsapi.h"
#include "jsobj.h"
#include "jsstr.h"

struct class_private {
	char name[64];
	config_t *cp;
	config_section_t *sec;
	jsval config_val;
};
typedef struct class_private class_private_t;

enum CLASS_PROPERTY_ID {
	CLASS_PROPERTY_ID_CONFIG=1,
};

static JSBool class_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	class_private_t *p;

	p = JS_GetPrivate(cx,obj);
	if (!p) {
		JS_ReportError(cx,"internal error: class_getprop: private is null!");
		return JS_FALSE;
	}
	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	if (JSVAL_IS_INT(id)) {
		int prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case CLASS_PROPERTY_ID_CONFIG:
			dprintf(dlevel,"config_val: %x, cp: %p\n", p->config_val, p->cp);
			if (!p->config_val && p->cp) {
				JSObject *newobj = js_config_new(cx,obj,p->cp);
				if (newobj) p->config_val = OBJECT_TO_JSVAL(obj);
			}
			*rval = p->config_val;
			break;
		default:
			if (p->cp) return js_config_common_getprop(cx, obj, id, rval, p->cp, 0);
			break;
		}
	} else {
#if 0
		if (JSVAL_IS_STRING(id)) {
			char *sname, *name;
			JSClass *classp = OBJ_GET_CLASS(cx, obj);

			sname = (char *)classp->name;
			name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
			dprintf(dlevel,"sname: %s, name: %s\n", sname, name);
			if (name) JS_free(cx, name);
		}
#endif
		dprintf(dlevel,"cp: %p\n", p->cp);
		if (p->cp) return js_config_common_getprop(cx, obj, id, rval, p->cp, 0);
	}
	return JS_TRUE;
}

static JSBool class_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	class_private_t *p;

	dprintf(dlevel,"setprop called!\n");
	p = JS_GetPrivate(cx,obj);
	if (!p) {
		JS_ReportError(cx,"internal error: class_setprop: private is null!");
		return JS_FALSE;
	}
	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	if (JSVAL_IS_INT(id)) {
		int prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case CLASS_PROPERTY_ID_CONFIG:
			p->config_val = *vp;
			p->cp = JS_GetPrivate(cx,JSVAL_TO_OBJECT(*vp));
			dprintf(dlevel,"cp: %p\n", p->cp);
			break;
		default:
			dprintf(dlevel,"cp: %p\n", p->cp);
			if (p->cp) return js_config_common_setprop(cx, obj, id, vp, p->cp, 0);
			break;
		}
	} else {
#if 0
		if (JSVAL_IS_STRING(id)) {
			char *sname, *name;
			JSClass *classp = OBJ_GET_CLASS(cx, obj);

			sname = (char *)classp->name;
			name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
			dprintf(dlevel,"sname: %s, name: %s\n", sname, name);
			if (name) JS_free(cx, name);
		}
#endif
		dprintf(dlevel,"cp: %p\n", p->cp);
		if (p->cp) return js_config_common_setprop(cx, obj, id, vp, p->cp, 0);
	}
	return JS_TRUE;
}

static JSClass class_class = {
	"Class",		/* Name */
	JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	class_getprop,		/* getProperty */
	class_setprop,		/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool class_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	class_private_t *p;
	JSObject *newobj;
	JSClass *newclassp;
//	,*ccp = &class_class;
	char *namep;
	JSPropertySpec props[] = {
		{ "config",CLASS_PROPERTY_ID_CONFIG,JSPROP_ENUMERATE },
		{0}
	};
	JSFunctionSpec funcs[] = {
		{0}
	};

	if (argc < 1) {
		JS_ReportError(cx, "Class requires 1 argument: (name:string)");
		return JS_FALSE;
	}
	namep =0;
	if (!JS_ConvertArguments(cx, argc, argv, "s", &namep)) return JS_FALSE;
	dprintf(dlevel,"name: %s\n", namep);

	p = JS_malloc(cx,sizeof(*p));
	if (!p) {
		JS_ReportError(cx,"error allocating memory");
		return JS_FALSE;
	}
	memset(p,0,sizeof(*p));
	strncpy(p->name,namep,sizeof(p->name)-1);

	/* Create a new instance of Class */
	newclassp = JS_malloc(cx,sizeof(JSClass));
//	memset(newclassp,0,sizeof(*newclassp));
	memcpy(newclassp,&class_class,sizeof(class_class));
//	*newclassp = *ccp;
	newclassp->name = p->name;

	newobj = JS_InitClass(cx, obj, 0, newclassp, 0, 0, props, funcs, 0, 0);
//	newobj = JS_NewObject(cx, newclassp, 0, obj);
	if (!newobj) {
		JS_ReportError(cx,"error allocating memory");
		return JS_FALSE;
	}
	JS_SetPrivate(cx,newobj,p);
	*rval = OBJECT_TO_JSVAL(newobj);
	return JS_TRUE;
}

JSObject *js_InitClassClass(JSContext *cx, JSObject *parent) {
	JSObject *obj;

	dprintf(dlevel,"Defining %s object\n",class_class.name);
	obj = JS_InitClass(cx, parent, 0, &class_class, class_ctor, 2, 0, 0, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize Class class");
		return 0;
	}
	dprintf(dlevel,"done!\n");
	return obj;
}
