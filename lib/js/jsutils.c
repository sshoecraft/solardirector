
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "jsapi.h"
#include "jsutils.h"

JSBool JS_DefineConstants(JSContext *cx, JSObject *obj, JSConstSpec *cs) {
	JSConstSpec *csp;
	JSBool ok;

	for(csp = cs; csp->name; csp++) {
		ok = JS_DefineProperty(cx, obj, csp->name, csp->value, 0, 0,
			JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT|JSPROP_EXPORTED);
		if (!ok) return JS_FALSE;
	}
	return JS_TRUE;
}

char *js_string(JSContext *cx, jsval val) {
	JSString *str;

	str = JS_ValueToString(cx, val);
	if (!str) return JS_FALSE;
	return JS_EncodeString(cx, str);
}

#if 0
char *JS_GetObjectName((JSContext *cx, JSObject *obj) {
        JSIdArray *ida;
        jsval *ids,val;
        int i;
        char *key;
	JSObject *parent;

	parent = JS_GetParent(cx,obj);
	if (!parent) {
		if (obj == JS_GetGlobalObject(cx))
			return "global";
		else
			return "unknown";
	}
        ida = JS_Enumerate(cx, parent);
        dprintf(0,"ida: %p, count: %d\n", ida, ida->length);
        ids = &ida->vector[0];
        for(i=0; i< ida->length; i++) {
                dprintf(dlevel+1,"ids[%d]: %s\n", i, jstypestr(cx,ids[i]));
                key = js_string(cx,ids[i]);
                if (!key) continue;
                dprintf(dlevel+1,"key: %s\n", key);
                if (!JS_GetProperty(cx,parent,key,&val)) {
                        JS_free(cx,key);
                        continue;
                }
                if (JSVAL_IS_OBJECT(val)) {
                	dprintf(0,"key: %s, IS_OBJECT: %d\n", key, JSVAL_IS_OBJECT(val));
                        JSObject *vobj = JSVAL_TO_OBJECT(val);
                        dprintf(0,"vobj: %p, obj: %p\n", vobj, obj);
                        if (vobj == obj) {
                                dprintf(0,"found: %s\n",key);
				break;
                        }
                }
                JS_free(cx,key);
        }

//        dprintf(0,"not found\n");
}
#endif
