/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifdef JS
#define DEBUG_JSFUNCS 1
#define dlevel 3

#include "rheem.h"
#include "jsobj.h"

enum RHEEM_DEVICE_PROPERTY_ID {
	RHEEM_DEVICE_PROPERTY_ID_ID=2048,
	RHEEM_DEVICE_PROPERTY_ID_SERIAL,
	RHEEM_DEVICE_PROPERTY_ID_TYPE,
	RHEEM_DEVICE_PROPERTY_ID_ENABLED,
	RHEEM_DEVICE_PROPERTY_ID_RUNNING,
	RHEEM_DEVICE_PROPERTY_ID_MODES,
	RHEEM_DEVICE_PROPERTY_ID_MODE,
	RHEEM_DEVICE_PROPERTY_ID_TEMP,
	RHEEM_DEVICE_PROPERTY_ID_LEVEL,
	RHEEM_DEVICE_PROPERTY_ID_STATUS,
};

static JSBool rheem_device_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	rheem_device_t *d;
	rheem_session_t *s;
	config_property_t *p;

	dprintf(dlevel+1,"obj: %p\n", obj);
	d = JS_GetPrivate(cx,obj);
	dprintf(dlevel+1,"d: %p\n", d);
	if (!d) {
		JS_ReportError(cx,"private is null!");
		return JS_FALSE;
	}
	s = d->s;

	dprintf(dlevel+1,"id type: %s\n", jstypestr(cx,id));
	p = 0;
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
//		dprintf(dlevel+1,"prop_id: %d\n", prop_id);
		dprintf(dlevel,"prop_id: %d, enabled: %d\n", prop_id, RHEEM_DEVICE_PROPERTY_ID_ENABLED);
		switch(prop_id) {
		case RHEEM_DEVICE_PROPERTY_ID_ID:
			*rval = type_to_jsval(cx, DATA_TYPE_STRING, d->id, strlen(d->id));
			break;
		case RHEEM_DEVICE_PROPERTY_ID_SERIAL:
			*rval = type_to_jsval(cx, DATA_TYPE_STRING, d->serial, strlen(d->serial));
			break;
		case RHEEM_DEVICE_PROPERTY_ID_TYPE:
			*rval = type_to_jsval(cx, DATA_TYPE_INT, &d->type, 0);
			break;
		case RHEEM_DEVICE_PROPERTY_ID_ENABLED:
			*rval = type_to_jsval(cx, DATA_TYPE_BOOL, &d->enabled, 0);
			break;
		case RHEEM_DEVICE_PROPERTY_ID_RUNNING:
			*rval = type_to_jsval(cx, DATA_TYPE_BOOL, &d->running, 0);
			break;
		case RHEEM_DEVICE_PROPERTY_ID_MODES:
			if (!d->modes_val) {
				JSObject *rows;
				jsval val;
				int i;

				rows = JS_NewArrayObject(cx, d->modecount, NULL);
				if (!rows) return JS_FALSE;
				for(i=0; i < d->modecount; i++) {
					val = type_to_jsval(cx, DATA_TYPE_STRING, d->modes[i], strlen(d->modes[i]));
					JS_SetElement(cx, rows, i, &val);
				}
				d->modes_val = OBJECT_TO_JSVAL(rows);
			}
			*rval = d->modes_val;
			break;
		case RHEEM_DEVICE_PROPERTY_ID_MODE:
			*rval = type_to_jsval(cx, DATA_TYPE_STRING, d->modes[d->mode], strlen(d->modes[d->mode]));
			break;
		case RHEEM_DEVICE_PROPERTY_ID_TEMP:
			JS_NewDoubleValue(cx, d->temp, rval);
			break;
		case RHEEM_DEVICE_PROPERTY_ID_LEVEL:
			JS_NewDoubleValue(cx, d->level, rval);
			break;
		case RHEEM_DEVICE_PROPERTY_ID_STATUS:
			*rval = type_to_jsval(cx, DATA_TYPE_STRING, d->status, strlen(d->status));
			break;
		default:
			p = CONFIG_GETMAP(s->ap->cp,prop_id);
			dprintf(dlevel+1,"p: %p\n", p);
			if (!p) p = config_get_propbyid(s->ap->cp,prop_id);
			dprintf(dlevel+1,"p: %p\n", p);
			if (!p) {
				JS_ReportError(cx, "internal error: property %d not found", prop_id);
				return JS_FALSE;
			}
			break;
		}
	} else if (JSVAL_IS_STRING(id)) {
		char *sname, *name;
		JSClass *classp = OBJ_GET_CLASS(cx, obj);

		sname = (char *)classp->name;
		name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
		dprintf(dlevel+1,"sname: %s, name: %s\n", sname, name);
		if (sname && name) p = config_get_property(s->ap->cp, sname, name);
//		if (!p) dlevel,"prop not found: %s\n", name);
		if (name) JS_free(cx,name);
	}
	dprintf(dlevel+1,"p: %p\n", p);
	if (p && p->dest) {
//		config_dump_property(p,0);
		dprintf(dlevel+1,"p: type: %d(%s), name: %s\n", p->type, typestr(p->type), p->name);
		*rval = type_to_jsval(cx,p->type,p->dest,p->len);
	}
	return JS_TRUE;
}

static JSBool rheem_device_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	int prop_id;
	rheem_device_t *d;
	rheem_session_t *s;
	config_property_t *p;

	dprintf(dlevel+1,"obj: %p\n", obj);
	d = JS_GetPrivate(cx,obj);
	dprintf(dlevel+1,"d: %p\n", d);
	if (!d) {
		JS_ReportError(cx,"private is null!");
		return JS_FALSE;
	}
	s = d->s;

	dprintf(dlevel+1,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
//		dprintf(dlevel+1,"prop_id: %d\n", prop_id);
		dprintf(dlevel,"prop_id: %d, enabled: %d\n", prop_id, RHEEM_DEVICE_PROPERTY_ID_ENABLED);
		switch(prop_id) {
		case RHEEM_DEVICE_PROPERTY_ID_ENABLED:
			{
				bool b;

				jsval_to_type(DATA_TYPE_BOOL,&b,sizeof(b),cx,*vp);
				p = config_get_property(s->ap->cp,d->id,"enabled");
				dprintf(dlevel,"p: %p\n", p);
				if (p) config_property_set_value(p,DATA_TYPE_BOOL,&b,sizeof(b),true,true);
			}
			break;
		case RHEEM_DEVICE_PROPERTY_ID_MODE:
			{
				char *str;

				jsval_to_type(DATA_TYPE_STRINGP,&str,sizeof(str),cx,*vp);
				p = config_get_property(s->ap->cp,d->id,"mode");
				dprintf(dlevel,"p: %p\n", p);
				if (p) config_property_set_value(p,DATA_TYPE_STRING,str,strlen(str),true,true);
			}
			break;
		case RHEEM_DEVICE_PROPERTY_ID_TEMP:
			{
				int i;

				jsval_to_type(DATA_TYPE_INT,&i,sizeof(i),cx,*vp);
				p = config_get_property(s->ap->cp,d->id,"temp");
				dprintf(dlevel,"p: %p\n", p);
				if (p) config_property_set_value(p,DATA_TYPE_INT,&i,sizeof(i),true,true);
			}
			break;
		case RHEEM_DEVICE_PROPERTY_ID_LEVEL:
			break;
		}
	}
	return JS_TRUE;
}

static JSClass rheem_device_class = {
	NULL,			/* Name */
	JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	rheem_device_getprop,	/* getProperty */
	rheem_device_setprop,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *js_rheem_device_new(JSContext *cx, JSObject *parent, rheem_device_t *d) {
	JSPropertySpec rheem_device_props[] = {
		{ "id", RHEEM_DEVICE_PROPERTY_ID_ID, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "serial", RHEEM_DEVICE_PROPERTY_ID_SERIAL, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "type", RHEEM_DEVICE_PROPERTY_ID_TYPE, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "enabled", RHEEM_DEVICE_PROPERTY_ID_ENABLED, JSPROP_ENUMERATE },
		{ "running", RHEEM_DEVICE_PROPERTY_ID_RUNNING, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "status", RHEEM_DEVICE_PROPERTY_ID_STATUS, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "modes", RHEEM_DEVICE_PROPERTY_ID_MODES, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "mode", RHEEM_DEVICE_PROPERTY_ID_MODE, JSPROP_ENUMERATE },
		{ "temp", RHEEM_DEVICE_PROPERTY_ID_TEMP, JSPROP_ENUMERATE },
		{ "level", RHEEM_DEVICE_PROPERTY_ID_LEVEL, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ 0 }
	};
	JSFunctionSpec rheem_device_funcs[] = {
		{ 0 }
	};
	JSObject *obj;

	if (d->obj) return d->obj;

	rheem_device_class.name = strdup(d->id);
	dprintf(dlevel,"defining %s object\n",rheem_device_class.name);
	obj = JS_InitClass(cx, parent, 0, &rheem_device_class, 0, 0, rheem_device_props, rheem_device_funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s", rheem_device_class.name);
		return 0;
	}

	JS_SetPrivate(cx,obj,d);
	dprintf(dlevel,"done!\n");
	d->obj = obj;
	return obj;
}

/*************************************************************************/

enum RHEEM_PROPERTY_ID {
	RHEEM_PROPERTY_ID_AGENT=1024,
	RHEEM_PROPERTY_ID_CLIENT,
	RHEEM_PROPERTY_ID_DEVICES,
};

static JSBool rheem_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	rheem_session_t *s;
	config_property_t *p;

	dprintf(dlevel+1,"obj: %p\n", obj);
	s = JS_GetPrivate(cx,obj);
	dprintf(dlevel+1,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx,"private is null!");
		return JS_FALSE;
	}

	dprintf(dlevel+1,"id type: %s\n", jstypestr(cx,id));
	p = 0;
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel+1,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case RHEEM_PROPERTY_ID_AGENT:
			*rval = s->agent_val;
			break;
		case RHEEM_PROPERTY_ID_CLIENT:
			*rval = s->client_val;
			break;
		case RHEEM_PROPERTY_ID_DEVICES:
			{
				JSObject *rows;
				JSObject *dobj;
				jsval val;
				int i;

				rows = JS_NewArrayObject(cx, s->devcount, NULL);
				if (!rows) return JS_FALSE;
				for(i=0; i < s->devcount; i++) {
					dobj = js_rheem_device_new(cx, rows, &s->devices[i]);
					if (dobj) {
						val = OBJECT_TO_JSVAL(dobj);
						JS_SetElement(cx, rows, i, &val);
					}
				}
				*rval = OBJECT_TO_JSVAL(rows);
			}
			break;
		default:
			p = CONFIG_GETMAP(s->ap->cp,prop_id);
			dprintf(dlevel+1,"p: %p\n", p);
			if (!p) p = config_get_propbyid(s->ap->cp,prop_id);
			dprintf(dlevel+1,"p: %p\n", p);
			if (!p) {
				JS_ReportError(cx, "internal error: property %d not found", prop_id);
				return JS_FALSE;
			}
			break;
		}
	} else if (JSVAL_IS_STRING(id)) {
		char *sname, *name;
		JSClass *classp = OBJ_GET_CLASS(cx, obj);

		sname = (char *)classp->name;
		name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
		dprintf(dlevel+1,"sname: %s, name: %s\n", sname, name);
		if (sname && name) p = config_get_property(s->ap->cp, sname, name);
//		if (!p) dlevel,"prop not found: %s\n", name);
		if (name) JS_free(cx,name);
	}
	dprintf(dlevel+1,"p: %p\n", p);
	if (p && p->dest) {
//		config_dump_property(p,0);
		dprintf(dlevel+1,"p: type: %d(%s), name: %s\n", p->type, typestr(p->type), p->name);
		*rval = type_to_jsval(cx,p->type,p->dest,p->len);
	}
	return JS_TRUE;
}

static JSBool rheem_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	rheem_session_t *s;

	s = JS_GetPrivate(cx,obj);
	if (!s) {
		JS_ReportError(cx,"private is null!");
		return JS_FALSE;
	}
	return js_config_common_setprop(cx, obj, id, rval, s->ap->cp, 0);
}


static JSClass rheem_class = {
	"RHEEM",		/* Name */
	JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	rheem_getprop,		/* getProperty */
	rheem_setprop,		/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSBool rheem_callfunc(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	rheem_session_t *s;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) {
		JS_ReportError(cx, "rheem_callfunc: internal error: object is null!\n");
		return JS_FALSE;
	}
	s = JS_GetPrivate(cx, obj);
	if (!s) {
		JS_ReportError(cx, "rheem_callfunc: internal error: private is null!\n");
		return JS_FALSE;
	}
	if (!s->ap) return JS_TRUE;
	if (!s->ap->cp) return JS_TRUE;

	return js_config_callfunc(s->ap->cp, cx, argc, vp);
}

static int js_rheem_init(JSContext *cx, JSObject *parent, void *priv) {
	rheem_session_t *s = priv;
	JSPropertySpec rheem_props[] = {
		{ "agent", RHEEM_PROPERTY_ID_AGENT, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "client", RHEEM_PROPERTY_ID_CLIENT, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "devices", RHEEM_PROPERTY_ID_DEVICES, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ 0 }
	};
	JSFunctionSpec rheem_funcs[] = {
		{ 0 }
	};
	JSAliasSpec rheem_aliases[] = {
		{ 0 }
	};
	JSConstantSpec rheem_constants[] = {
		{ 0 }
	};
	JSObject *obj;

	dprintf(dlevel,"parent: %p\n", parent);
	dprintf(dlevel,"s->props: %p, cp: %p\n",s->props,s->ap->cp);
	if (s->ap && s->ap->cp) {
		if (!s->props) {
			s->props = js_config_to_props(s->ap->cp, cx, "rheem", rheem_props);
			dprintf(dlevel,"s->props: %p\n",s->props);
			if (!s->props) {
				log_error("unable to create props: %s\n", config_get_errmsg(s->ap->cp));
				return 0;
			}
		}
		if (!s->funcs) {
			s->funcs = js_config_to_funcs(s->ap->cp, cx, rheem_callfunc, rheem_funcs);
			dprintf(dlevel,"s->funcs: %p\n",s->funcs);
			if (!s->funcs) {
				log_error("unable to create funcs: %s\n", config_get_errmsg(s->ap->cp));
				return 0;
			}
		}
	}
	dprintf(dlevel,"s->props: %p, s->funcs: %p\n", s->props, s->funcs);
	if (!s->props) s->props = rheem_props;
	if (!s->funcs) s->funcs = rheem_funcs;

	dprintf(dlevel,"Defining %s object\n",rheem_class.name);
	obj = JS_InitClass(cx, parent, 0, &rheem_class, 0, 0, s->props, s->funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s class", rheem_class.name);
		return 1;
	}

	dprintf(dlevel,"Defining %s aliases\n",rheem_class.name);
	if (!JS_DefineAliases(cx, obj, rheem_aliases)) {
		JS_ReportError(cx,"unable to define aliases");
		return 1;
	}
	dprintf(dlevel,"Defining global constants\n");
	if (!JS_DefineConstants(cx, parent, rheem_constants)) {
		JS_ReportError(cx,"unable to define constants");
		return 1;
	}
	dprintf(dlevel,"done!\n");
	JS_SetPrivate(cx,obj,s);

	/* Create the global convenience objects */
	JS_DefineProperty(cx, parent, "rheem", OBJECT_TO_JSVAL(obj), 0, 0, JSPROP_ENUMERATE);
	s->agent_val = OBJECT_TO_JSVAL(js_agent_new(cx,obj,s->ap));
	JS_DefineProperty(cx, parent, "agent", s->agent_val, 0, 0, JSPROP_ENUMERATE);

	if (s->ap) {
		JS_DefineProperty(cx, parent, "config", s->ap->js.config_val, 0, 0, JSPROP_ENUMERATE);
		JS_DefineProperty(cx, parent, "mqtt", s->ap->js.mqtt_val, 0, 0, JSPROP_ENUMERATE);
		JS_DefineProperty(cx, parent, "influx", s->ap->js.influx_val, 0, 0, JSPROP_ENUMERATE);
	}

	return 0;
}

int rheem_jsinit(rheem_session_t *s) {
	JS_EngineAddInitFunc(s->ap->js.e, "rheem", js_rheem_init, s);
	return 0;
}
#endif
