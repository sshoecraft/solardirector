#ifdef JS

/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 5
#include "debug.h"

#include "jbd.h"

enum JBD_HW_PROPERTY_ID {
	JBD_HW_PROPERTY_ID_MF,
	JBD_HW_PROPERTY_ID_MODEL,
	JBD_HW_PROPERTY_ID_DATE,
	JBD_HW_PROPERTY_ID_VER
};

static JSBool jbd_hw_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	jbd_hwinfo_t *hwinfo;

	hwinfo = JS_GetPrivate(cx,obj);
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case JBD_HW_PROPERTY_ID_MF:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,hwinfo->manufacturer,strlen(hwinfo->manufacturer));
			break;
		case JBD_HW_PROPERTY_ID_MODEL:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,hwinfo->model,strlen(hwinfo->model));
			break;
		case JBD_HW_PROPERTY_ID_DATE:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,hwinfo->mfgdate,strlen(hwinfo->mfgdate));
			break;
		case JBD_HW_PROPERTY_ID_VER:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,hwinfo->version,strlen(hwinfo->version));
			break;
		default:
			JS_ReportError(cx, "property not found");
			*rval = JSVAL_VOID;
			return JS_FALSE;
			break;
		}
	}
	return JS_TRUE;
}

static JSClass jbd_hw_class = {
	"jbd_hw_info",		/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	jbd_hw_getprop,		/* getProperty */
	JS_PropertyStub,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *js_jbd_hw_new(JSContext *cx, JSObject *parent, void *priv) {
	JSPropertySpec jbd_hw_props[] = {
		{ "manufacturer", JBD_HW_PROPERTY_ID_MF, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "model", JBD_HW_PROPERTY_ID_MODEL, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "mfgdate", JBD_HW_PROPERTY_ID_DATE, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "version", JBD_HW_PROPERTY_ID_VER, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ 0 }
	};
	JSObject *obj;

	dprintf(dlevel,"defining %s object\n",jbd_hw_class.name);
	obj = JS_InitClass(cx, parent, 0, &jbd_hw_class, 0, 0, jbd_hw_props, 0, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s", jbd_hw_class.name);
		return 0;
	}
	JS_SetPrivate(cx,obj,priv);
	dprintf(dlevel,"done!\n");
	return obj;
}

#if 0
/*************************************************************************/

static JSBool jbd_data_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	jbd_session_t *s;

	s = JS_GetPrivate(cx,obj);
	if (JSVAL_IS_INT(id)) {
		int prop_id = JSVAL_TO_INT(id);
		config_property_t *p;

		dprintf(dlevel,"prop_id: %d\n", prop_id);
		p = CONFIG_GETMAP(s->ap->cp,prop_id);
		if (!p) p = config_get_propbyid(s->ap->cp,prop_id);
		if (!p) {
			JS_ReportError(cx, "property not found");
			return JS_FALSE;
		}
		dprintf(4,"p: name: %s, type: %d(%s)\n", p->name, p->type, typestr(p->type));
		if (strcmp(p->name,"temps") == 0) {
			dprintf(4,"ntemps: %d\n", s->data.ntemps);
			p->len = s->data.ntemps;
		}
		else if (strcmp(p->name,"cellvolt") == 0) {
			dprintf(4,"ncells: %d\n", s->data.ncells);
			p->len = s->data.ncells;
		}
		*vp = type_to_jsval(cx,p->type,p->dest,p->len);
	}
	return JS_TRUE;
}

static JSBool jbd_data_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	jbd_session_t *s;

	s = JS_GetPrivate(cx,obj);
	return js_config_common_setprop(cx, obj, id, vp, s->ap->cp, s->data_propspec);
}

static JSClass jbd_data_class = {
	"jbd_data",		/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	jbd_data_getprop,	/* getProperty */
	jbd_data_setprop,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *js_jbd_data_new(JSContext *cx, JSObject *parent, jbd_session_t *s) {
	JSAliasSpec jbd_data_aliases[] = {
		{ 0 }
	};
	JSObject *obj;

	if (!s->data_propspec && s->ap->cp) {
		/* section name must match used in jbd_config_add_jbd_data() */
		s->data_propspec = js_config_to_props(s->ap->cp, "jbd_data", 0);
		dprintf(dlevel,"data_propspec: %p\n",s->data_propspec);
		if (!s->data_propspec) {
			log_error("unable to create jbd_data props: %s\n", config_get_errmsg(s->ap->cp));
			return 0;
		}
	}

	dprintf(dlevel,"defining %s object\n",jbd_data_class.name);
	obj = JS_InitClass(cx, parent, 0, &jbd_data_class, 0, 0, s->data_propspec, 0, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s", jbd_data_class.name);
		return 0;
	}
	dprintf(dlevel,"defining %s aliases\n",jbd_data_class.name);
	if (!JS_DefineAliases(cx, obj, jbd_data_aliases)) {
		JS_ReportError(cx,"unable to define aliases");
		return 0;
	}
	JS_SetPrivate(cx,obj,s);
	dprintf(dlevel,"done!\n");
	return obj;
}
#endif

/*************************************************************************/

enum JBD_PROPERTY_ID {
	JBD_PROPERTY_ID_DATA=1024,
	JBD_PROPERTY_ID_HW,
	JBD_PROPERTY_ID_AGENT,
};

static JSBool jbd_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	jbd_session_t *s;
	config_property_t *p;

	s = JS_GetPrivate(cx,obj);
        if (!s) {
                JS_ReportError(cx,"private is null!");
                return JS_FALSE;
        }
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case JBD_PROPERTY_ID_DATA:
			*rval = s->data_val;
			break;
		case JBD_PROPERTY_ID_HW:
			*rval = s->hw_val;
			break;
		case JBD_PROPERTY_ID_AGENT:
			*rval = s->agent_val;
			break;
		default:
			p = CONFIG_GETMAP(s->ap->cp,prop_id);
			if (!p) p = config_get_propbyid(s->ap->cp,prop_id);
			if (!p) {
				JS_ReportError(cx, "property not found");
				return JS_FALSE;
			}
//			dprintf(4,"p: name: %s, type: %d(%s)\n", p->name, p->type, typestr(p->type));
			*rval = type_to_jsval(cx,p->type,p->dest,p->len);
			break;
		}
	}
	return JS_TRUE;
}

static JSBool jbd_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	jbd_session_t *s;

	s = JS_GetPrivate(cx,obj);
	if (!s) {
		JS_ReportError(cx,"private is null!");
		return JS_FALSE;
	}
	return js_config_common_setprop(cx, obj, id, rval, s->ap->cp, s->propspec);
}

static JSClass jbd_class = {
	"jbd",			/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	jbd_getprop,		/* getProperty */
	jbd_setprop,		/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

int js_jbd_init(JSContext *cx, JSObject *parent, void *priv) {
	JSPropertySpec jbd_props[] = {
		{ "data", JBD_PROPERTY_ID_DATA, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "hw", JBD_PROPERTY_ID_HW, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "agent", JBD_PROPERTY_ID_AGENT, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ 0 }
	};
	JSConstantSpec jbd_consts[] = {
		JS_NUMCONST(JBD_STATE_CHARGING),
		JS_NUMCONST(JBD_STATE_DISCHARGING),
		JS_NUMCONST(JBD_STATE_BALANCING),
		JS_NUMCONST(JBD_MAX_TEMPS),
		JS_NUMCONST(JBD_MAX_CELLS),
		{ 0 }
	};
	JSObject *obj;
	jbd_session_t *s = priv;

	if (!s->propspec && s->ap->cp) {
		s->propspec = js_config_to_props(s->ap->cp, cx, s->ap->section_name, jbd_props);
		if (!s->propspec) {
			log_error("unable to create props: %s\n", config_get_errmsg(s->ap->cp));
			return 1;
		}
	}
	if (!s->propspec) s->propspec = jbd_props;

	dprintf(dlevel,"defining %s object\n",jbd_class.name);
	obj = JS_InitClass(cx, parent, 0, &jbd_class, 0, 0, s->propspec, 0, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s class, jbd_class.name");
		return 1;
	}
	JS_SetPrivate(cx,obj,priv);
	if (!JS_DefineConstants(cx, parent, jbd_consts)) {
		JS_ReportError(cx,"unable to add jbd constants");
		return 1;
	}

//	s->data_val = OBJECT_TO_JSVAL(js_jbd_data_new(cx,obj,s));
	s->data_val = OBJECT_TO_JSVAL(js_battery_new(cx,obj,&s->data));
	s->hw_val = OBJECT_TO_JSVAL(js_jbd_hw_new(cx,obj,&s->hwinfo));
	s->agent_val = OBJECT_TO_JSVAL(js_agent_new(cx,obj,s->ap));

	/* Create the convenience objects */
//	JS_DefineProperty(cx, parent, "jbd", OBJECT_TO_JSVAL(obj), 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "data", s->data_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "agent", s->agent_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "config", s->ap->js.config_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "mqtt", s->ap->js.mqtt_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "influx", s->ap->js.influx_val, 0, 0, JSPROP_ENUMERATE);

	dprintf(dlevel,"done!\n");
	return 0;
}

int jbd_jsinit(jbd_session_t *s) {
	return JS_EngineAddInitFunc(s->ap->js.e, s->ap->section_name, js_jbd_init, s);
}

#endif
