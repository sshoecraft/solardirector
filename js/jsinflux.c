
#ifdef INFLUX
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "jsapi.h"
#include "jsobj.h"
#include "transports.h"

JSObject *JSINFLUX(JSContext *cx, void *priv);

struct influx_private {
	char *transport;
	char *target;
	char *topts;
#if 0
	char transport[DRIVER_TRANSPORT_SIZE];
	char target[DRIVER_TARGET_SIZE];
	char topts[DRIVER_TOPTS_SIZE];
#endif
	solard_driver_t *tp;
	void *tp_handle;
	bool connected;
};
typedef struct influx_private influx_private_t;

//solard_driver_t *influx_transports[] = { &http_driver, &rdev_driver, 0 };

enum INFLUX_PROPERTY_ID {
	INFLUX_PROPERTY_ID_CONNECTED,
	INFLUX_PROPERTY_ID_TRANSPORT,
	INFLUX_PROPERTY_ID_TARGET,
	INFLUX_PROPERTY_ID_TOPTS,
};

static JSBool influx_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	influx_private_t *p;
//	register int i;

	p = JS_GetPrivate(cx,obj);
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(4,"prop_id: %d\n", prop_id);
		switch(prop_id) {
                case INFLUX_PROPERTY_ID_CONNECTED:
                        *rval = BOOLEAN_TO_JSVAL(p->connected);
                        break;
#if 0
                case INFLUX_PROPERTY_ID_AUTOCONNECT:
                        *rval = BOOLEAN_TO_JSVAL(p->autoconnect);
                        break;
		case INFLUX_PROPERTY_ID_NAME:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,bp->name));
			*rval = type_to_jsval(cx, DATA_TYPE_STRING, bp->name, strlen(bp->name));
			break;
		case INFLUX_PROPERTY_ID_CAPACITY:
			JS_NewDoubleValue(cx, bp->capacity, rval);
			break;
		case INFLUX_PROPERTY_ID_VOLTAGE:
			JS_NewDoubleValue(cx, bp->voltage, rval);
			break;
		case INFLUX_PROPERTY_ID_CURRENT:
			JS_NewDoubleValue(cx, bp->current, rval);
			break;
		case INFLUX_PROPERTY_ID_NTEMPS:
			dprintf(4,"ntemps: %d\n", bp->ntemps);
			*rval = INT_TO_JSVAL(bp->ntemps);
			break;
		case INFLUX_PROPERTY_ID_TEMPS:
		       {
				JSObject *rows;
				jsval val;

//				dprintf(4,"ntemps: %d\n", bp->ntemps);
				rows = JS_NewArrayObject(cx, bp->ntemps, NULL);
				for(i=0; i < bp->ntemps; i++) {
//					dprintf(4,"temps[%d]: %f\n", i, bp->temps[i]);
					JS_NewDoubleValue(cx, bp->temps[i], &val);
					JS_SetElement(cx, rows, i, &val);
				}
				*rval = OBJECT_TO_JSVAL(rows);
			}
			break;
		case INFLUX_PROPERTY_ID_NCELLS:
			dprintf(4,"ncells: %d\n", bp->ncells);
			*rval = INT_TO_JSVAL(bp->ncells);
			break;
		case INFLUX_PROPERTY_ID_CELLVOLT:
		       {
				JSObject *rows;
				jsval val;

				rows = JS_NewArrayObject(cx, bp->ncells, NULL);
				for(i=0; i < bp->ncells; i++) {
					JS_NewDoubleValue(cx, bp->cells[i].voltage, &val);
					JS_SetElement(cx, rows, i, &val);
				}
				*rval = OBJECT_TO_JSVAL(rows);
			}
			break;
		case INFLUX_PROPERTY_ID_CELLRES:
		       {
				JSObject *rows;
				jsval val;

				rows = JS_NewArrayObject(cx, bp->ncells, NULL);
				for(i=0; i < bp->ncells; i++) {
					JS_NewDoubleValue(cx, bp->cells[i].resistance, &val);
					JS_SetElement(cx, rows, i, &val);
				}
				*rval = OBJECT_TO_JSVAL(rows);
			}
			break;
		case INFLUX_PROPERTY_ID_CELL_MIN:
			JS_NewDoubleValue(cx, bp->cell_min, rval);
			break;
		case INFLUX_PROPERTY_ID_CELL_MAX:
			JS_NewDoubleValue(cx, bp->cell_max, rval);
			break;
		case INFLUX_PROPERTY_ID_CELL_DIFF:
			JS_NewDoubleValue(cx, bp->cell_diff, rval);
			break;
		case INFLUX_PROPERTY_ID_CELL_AVG:
			JS_NewDoubleValue(cx, bp->cell_avg, rval);
			break;
		case INFLUX_PROPERTY_ID_CELL_TOTAL:
			JS_NewDoubleValue(cx, bp->cell_total, rval);
			break;
		case INFLUX_PROPERTY_ID_BALANCEBITS:
			*rval = INT_TO_JSVAL(bp->balancebits);
			break;
		case INFLUX_PROPERTY_ID_ERRCODE:
			*rval = INT_TO_JSVAL(bp->errcode);
			break;
		case INFLUX_PROPERTY_ID_ERRMSG:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,bp->errmsg));
			break;
		case INFLUX_PROPERTY_ID_STATE:
			*rval = INT_TO_JSVAL(bp->state);
			break;
		case INFLUX_PROPERTY_ID_LAST_UPDATE:
			*rval = INT_TO_JSVAL(bp->last_update);
			break;
#endif
		default:
			JS_ReportError(cx, "not a property");
			*rval = JSVAL_NULL;
		}
	}
	return JS_TRUE;
}

static JSClass influx_class = {
	"INFLUX",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,
	JS_PropertyStub,
	influx_getprop,
	JS_PropertyStub,
	JS_EnumerateStub,
	JS_ResolveStub,
	JS_ConvertStub,
	JS_FinalizeStub,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

#if 0
static JSBool jsinflux_read(JSContext *cx, uintN argc, jsval *vp) {
#if 0
	influx_private_t *p;
	int id,len,r;
	jsval *argv = vp + 2;
	JSObject *obj;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	p = JS_GetPrivate(cx, obj);

	if (argc != 2) {
		JS_ReportError(cx,"influx_read requires 2 arguments (id: number, length: number)\n");
		return JS_FALSE;
	}

	if (!jsval_to_type(DATA_TYPE_INT,&id,0,cx,argv[0])) return JS_FALSE;
	if (!jsval_to_type(DATA_TYPE_INT,&len,0,cx,argv[1])) return JS_FALSE;
	dprintf(0,"id: %03x, len: %d\n", id, len);
	if (len > 8) len = 8;
//	r = p->tp->influx_read(s,id,data,len);
//	memcpy(&frame.data,data,len);
	r = p->tp->read(p->tp_handle,&frame,id);
	printf("r: %d\n", r);
	*vp = type_to_jsval(cx,DATA_TYPE_U8_ARRAY,frame.data,frame.influx_dlc);
	return JS_TRUE;
#endif
	return JS_FALSE;
}

static JSBool jsinflux_write(JSContext *cx, uintN argc, jsval *vp) {
#if 0
	influx_private_t *p;
	uint8_t data[8];
	struct influx_frame frame;
	int id,len,bytes;
	jsval *argv = vp + 2;
	JSObject *obj, *array = 0;
	JSClass *classp;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	p = JS_GetPrivate(cx, obj);

	if (argc != 2) {
		JS_ReportError(cx,"influx_write requires 2 arguments (id: number, data: array)\n");
		return JS_FALSE;
	}

	printf("isobj: %d\n", JSVAL_IS_OBJECT(argv[1]));
	if (!JSVAL_IS_OBJECT(argv[1])) {
		JS_ReportError(cx, "influx_write: 2nd argument must be array\n");
		return JS_FALSE;
	}
	obj = JSVAL_TO_OBJECT(argv[1]);
	classp = OBJ_GET_CLASS(cx,obj);
	if (!classp) {
		JS_ReportError(cx, "influx_write: 2nd argument must be array\n");
		return JS_FALSE;
	}
	printf("class: %s\n", classp->name);
	if (classp && strcmp(classp->name,"Array")) {
		JS_ReportError(cx, "influx_write: 2nd argument must be array\n");
		return JS_FALSE;
	}

	dprintf(1,"id: %d, array: %p\n", id, array);

	if (!jsval_to_type(DATA_TYPE_INT,&id,0,cx,argv[0])) return JS_FALSE;
	len = jsval_to_type(DATA_TYPE_U8_ARRAY,data,8,cx,argv[1]);
	dprintf(0,"id: %03x, len: %d\n", len);
	bindump("data",data,len);

        memset(&frame,0,sizeof(frame));
        frame.influx_id = id;
        frame.influx_dlc = len;
        memcpy(&frame.data,data,len);
        bytes = p->tp->write(p->tp_handle,&frame,sizeof(frame));
        dprintf(0,"bytes: %d\n", bytes);
	*vp = INT_TO_JSVAL(bytes);
	return JS_TRUE;
#endif
}

static int do_connect(JSContext *cx, influx_private_t *p) {
#if 0
       /* Find the driver */
	p->tp = find_driver(influx_transports,p->transport);
	if (!p->tp) {
		JS_ReportError(cx,"unable to find transport: %s", p->transport);
		return JS_FALSE;
	}

	/* Create a new driver instance */
	p->tp_handle = p->tp->new(0, p->target, p->topts);
	if (!p->tp_handle) {
		JS_ReportError(cx, "%s_new: %s", p->transport, strerror(errno));
		return JS_FALSE;
	}

	/* Open the transport */
	dprintf(0,"opening...\n");
	if (!p->tp->open) {
		JS_ReportError(cx, "internal error: transport has no open function");
		return JS_FALSE;
	}
	if (p->tp->open(p->tp_handle) == 0) p->connected = 1;

	return JS_TRUE;
#endif
	return JS_FALSE;
}

static JSBool jsinflux_connect(JSContext *cx, uintN argc, jsval *vp) {
	influx_private_t *p;
//	jsval *argv = JS_ARGV(cx,vp);
	JSObject *obj;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	p = JS_GetPrivate(cx, obj);

	if (argc > 2) {
		if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "s s s", &p->transport, &p->target, &p->topts))
			return JS_FALSE;
	}
	dprintf(0,"transport: %s, target: %s, topts:%s\n", p->transport, p->target, p->topts);

	if (!p->transport) {
		JS_ReportError(cx, "influx_connect: transport not set");
		return JS_FALSE;
	}

	return do_connect(cx,p);
}
#endif

static JSBool influx_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
#if 0
	influx_private_t *p;
	JSObject *newobj;
	int r;

	p = JS_malloc(cx,sizeof(*p));
	if (!p) {
		JS_ReportError(cx,"error allocating memory");
		return JS_FALSE;
	}
	memset(p,0,sizeof(*p));
	if (argc == 3) {
		if (!JS_ConvertArguments(cx, argc, argv, "s s s", &p->transport, &p->target, &p->topts))
			return JS_FALSE;
	} else if (argc > 1) {
		JS_ReportError(cx, "INFLUX requires 3 arguments (transport, target, topts)");
		return JS_FALSE;
	}

	dprintf(0,"transport: %s, target: %s, topts:%s\n", p->transport, p->target, p->topts);

	r = do_connect(cx,p);

	newobj = js_InitINFLUXClass(cx,JS_GetGlobalObject(cx));
	JS_SetPrivate(cx,newobj,p);
	*rval = OBJECT_TO_JSVAL(newobj);

	return r;
#endif
	return JS_FALSE;
}

#if 0
JSObject *jsinflux_new(JSContext *cx, void *tp, void *handle, char *transport, char *target, char *topts) {
	influx_private_t *p;
	JSObject *newobj;

	p = JS_malloc(sizeof(*p));
	if (!p) {
		JS_ReportError(cx,"jsinflux_new: error allocating memory");
		return 0;
	}
	memset(p,0,sizeof(*p));
	p->tp = tp;
	p->tp_handle = handle;
	p->transport = transport;
	p->target = target;
	p->topts = topts;

	newobj = js_InitINFLUXClass(cx,JS_GetGlobalObject(cx));
	JS_SetPrivate(cx,newobj,p);
	return newobj;
}
#endif

JSObject *js_InitINFLUXClass(JSContext *cx, JSObject *gobj) {
	JSPropertySpec influx_props[] = { 
		{ "connected", INFLUX_PROPERTY_ID_CONNECTED, JSPROP_ENUMERATE | JSPROP_READONLY },
//		{ "autoconnect", INFLUX_PROPERTY_ID_AUTOCONNECT, JSPROP_ENUMERATE },
		{ "transport",INFLUX_PROPERTY_ID_TRANSPORT,JSPROP_ENUMERATE },
		{ "target",INFLUX_PROPERTY_ID_TARGET,JSPROP_ENUMERATE | JSPROP_READONLY  },
		{ "topts",INFLUX_PROPERTY_ID_TOPTS,JSPROP_ENUMERATE | JSPROP_READONLY  },
		{0}
	};
	JSFunctionSpec influx_funcs[] = {
#if 0
		JS_FN("connect",jsinflux_connect,3,3,0),
		JS_FN("read",jsinflux_read,1,1,0),
		JS_FN("write",jsinflux_write,2,2,0),
#endif
		{ 0 }
	};
	JSObject *obj;

	dprintf(5,"defining %s object\n",influx_class.name);
	obj = JS_InitClass(cx, gobj, gobj, &influx_class, influx_ctor, 3, influx_props, influx_funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize influx class");
		return 0;
	}
	dprintf(5,"done!\n");
	return obj;
}
#endif
