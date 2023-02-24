#ifdef JS

/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 5
#include "debug.h"

#include "jk.h"

enum JK_HW_PROPERTY_ID {
	JK_HW_PROPERTY_ID_MF,
	JK_HW_PROPERTY_ID_MODEL,
	JK_HW_PROPERTY_ID_DATE,
	JK_HW_PROPERTY_ID_VER
};

static JSBool jk_hw_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	jk_hwinfo_t *hwinfo;

	hwinfo = JS_GetPrivate(cx,obj);
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case JK_HW_PROPERTY_ID_MF:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,hwinfo->manufacturer));
			break;
		case JK_HW_PROPERTY_ID_MODEL:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,hwinfo->model));
			break;
		case JK_HW_PROPERTY_ID_DATE:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,hwinfo->mfgdate));
			break;
		case JK_HW_PROPERTY_ID_VER:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,hwinfo->swvers));
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

static JSClass jk_hw_class = {
	"jk_hw",		/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	jk_hw_getprop,		/* getProperty */
	JS_PropertyStub,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *js_jk_hw_new(JSContext *cx, JSObject *parent, void *priv) {
	JSPropertySpec jk_hw_props[] = {
		{ "manufacturer", JK_HW_PROPERTY_ID_MF, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "model", JK_HW_PROPERTY_ID_MODEL, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "mfgdate", JK_HW_PROPERTY_ID_DATE, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "version", JK_HW_PROPERTY_ID_VER, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ 0 }
	};
	JSObject *obj;

	dprintf(dlevel,"defining %s object\n",jk_hw_class.name);
	obj = JS_InitClass(cx, parent, 0, &jk_hw_class, 0, 0, jk_hw_props, 0, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s", jk_hw_class.name);
		return 0;
	}
	JS_SetPrivate(cx,obj,priv);
	dprintf(dlevel,"done!\n");
	return obj;
}

/*************************************************************************/

static JSBool js_jk_data_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	jk_session_t *s;

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
		} else if (strcmp(p->name,"cellvolt") == 0) {
			dprintf(4,"ncells: %d\n", s->data.ncells);
			p->len = s->data.ncells;
		} else if (strcmp(p->name,"cellres") == 0) {
			dprintf(4,"ncells: %d\n", s->data.ncells);
			p->len = s->data.ncells;
		}
		*vp = type_to_jsval(cx,p->type,p->dest,p->len);
	}
	return JS_TRUE;
}

static JSBool js_jk_data_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	jk_session_t *s;

	s = JS_GetPrivate(cx,obj);
	return js_config_common_setprop(cx, obj, id, vp, s->ap->cp, s->data_props);
}

static JSClass jk_data_class = {
	"jk_data",		/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	js_jk_data_getprop,	/* getProperty */
	js_jk_data_setprop,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *js_jk_data_new(JSContext *cx, JSObject *parent, jk_session_t *s) {
	JSAliasSpec jk_data_aliases[] = {
		{ 0 }
	};
	JSObject *obj;

	if (!s->data_props) {
		s->data_props = js_config_to_props(s->ap->cp, cx, "jk_data", 0);
		dprintf(dlevel,"info->props: %p\n",s->data_props);
		if (!s->data_props) {
			log_error("unable to create jk_data props: %s\n", config_get_errmsg(s->ap->cp));
			return 0;
		}
	}

	dprintf(dlevel,"defining %s object\n",jk_data_class.name);
	obj = JS_InitClass(cx, parent, 0, &jk_data_class, 0, 0, s->data_props, 0, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s", jk_data_class.name);
		return 0;
	}
	dprintf(dlevel,"defining %s aliases\n",jk_data_class.name);
	if (!JS_DefineAliases(cx, obj, jk_data_aliases)) {
		JS_ReportError(cx,"unable to define aliases");
		return 0;
	}
	JS_SetPrivate(cx,obj,s);
	dprintf(dlevel,"done!\n");
	return obj;
}

/*************************************************************************/

enum JK_PROPERTY_ID {
	JK_PROPERTY_ID_DATA=1024,
	JK_PROPERTY_ID_HW,
	JK_PROPERTY_ID_AGENT,
};

static JSBool jk_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	jk_session_t *s;
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
		case JK_PROPERTY_ID_DATA:
			*rval = s->data_val;
			break;
		case JK_PROPERTY_ID_HW:
			*rval = s->hw_val;
			break;
		case JK_PROPERTY_ID_AGENT:
			*rval = s->agent_val;
			break;
		default:
			p = CONFIG_GETMAP(s->ap->cp,prop_id);
			if (!p) p = config_get_propbyid(s->ap->cp,prop_id);
			if (!p) {
				JS_ReportError(cx, "property not found");
				return JS_FALSE;
			}
//			dprintf(1,"p: type: %d(%s), name: %s\n", p->type, typestr(p->type), p->name);
			if (strcmp(p->name,"temps") == 0) p->len = s->data.ntemps;
			else if (strcmp(p->name,"cellvolt") == 0) p->len = s->data.ncells;
			else if (strcmp(p->name,"cellres") == 0) p->len = s->data.ncells;
			*rval = type_to_jsval(cx,p->type,p->dest,p->len);
			break;
		}
	}
	return JS_TRUE;
}

static JSBool jk_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	jk_session_t *s;

	s = JS_GetPrivate(cx,obj);
	if (!s) {
		JS_ReportError(cx,"private is null!");
		return JS_FALSE;
	}
	return js_config_common_setprop(cx, obj, id, rval, s->ap->cp, s->props);
}

static JSClass jk_class = {
	"jk",			/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	jk_getprop,		/* getProperty */
	jk_setprop,		/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

int js_init_jk(JSContext *cx, JSObject *parent, void *priv) {
	JSPropertySpec jk_props[] = {
		{ "data", JK_PROPERTY_ID_DATA, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "hw", JK_PROPERTY_ID_HW, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "agent", JK_PROPERTY_ID_AGENT, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ 0 }
	};
	JSConstantSpec jk_consts[] = {
		JS_NUMCONST(JK_STATE_CHARGING),
		JS_NUMCONST(JK_STATE_DISCHARGING),
		JS_NUMCONST(JK_STATE_BALANCING),
		JS_NUMCONST(JK_MAX_TEMPS),
		JS_NUMCONST(JK_MAX_CELLS),
		{ 0 }
	};
	JSObject *obj;
	jk_session_t *s = priv;

	if (s->ap && s->ap->cp && !s->props) {
		s->props = js_config_to_props(s->ap->cp, cx, s->ap->section_name, jk_props);
		if (!s->props) {
			log_error("unable to create props: %s\n", config_get_errmsg(s->ap->cp));
			return 1;
		}
	} else {
		s->props = jk_props;
	}

	dprintf(dlevel,"defining %s object\n",jk_class.name);
	obj = JS_InitClass(cx, parent, 0, &jk_class, 0, 0, s->props, 0, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize si class");
		return 1;
	}
	JS_SetPrivate(cx,obj,priv);
	if (!JS_DefineConstants(cx, parent, jk_consts)) {
		JS_ReportError(cx,"unable to add jk constants");
		return 1;
	}

	/* Create jsvals */
//	s->data_val = OBJECT_TO_JSVAL(js_jk_data_new(cx,obj,s));
	s->data_val = OBJECT_TO_JSVAL(js_battery_new(cx,obj,&s->data));
	s->hw_val = OBJECT_TO_JSVAL(js_jk_hw_new(cx,obj,&s->hwinfo));
	s->agent_val = OBJECT_TO_JSVAL(js_agent_new(cx,obj,s->ap));

	/* Create the parent convenience objects */
	JS_DefineProperty(cx, parent, "data", s->data_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "agent", s->agent_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "config", s->ap->js.config_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "mqtt", s->ap->js.mqtt_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "influx", s->ap->js.influx_val, 0, 0, JSPROP_ENUMERATE);

	dprintf(dlevel,"done!\n");
	return 0;
}

int jk_jsinit(jk_session_t *s) {
	return JS_EngineAddInitFunc(s->ap->js.e, s->ap->section_name, js_init_jk, s);
}

#endif
