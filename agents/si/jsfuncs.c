
#ifdef JS
#define DEBUG_JSFUNCS 1
#define dlevel 5

#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG DEBUG_JSFUNCS
#include "debug.h"
#include "si.h"
#include "jsobj.h"
#include "jsjson.h"
#include "jsstr.h"
#include "jsprintf.h"

static JSBool si_data_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	si_session_t *s;

	s = JS_GetPrivate(cx,obj);
	return js_config_common_getprop(cx, obj, id, vp, s->ap->cp, s->data_props);
}

static JSBool si_data_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	si_session_t *s;

	s = JS_GetPrivate(cx,obj);
	return js_config_common_setprop(cx, obj, id, vp, s->ap->cp, s->data_props);
}

static JSClass si_data_class = {
	"si_data",		/* Name */
	JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	si_data_getprop,	/* getProperty */
	si_data_setprop,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool refresh_si_data(JSContext *cx, uintN argc, jsval *vp) {
	si_session_t *s = JS_GetPrivate(cx,JS_THIS_OBJECT(cx, vp));

	if (s->can_connected) si_can_get_data(s);
	return JS_TRUE;
}

JSObject *jssi_data_new(JSContext *cx, JSObject *parent, si_session_t *s) {
	JSAliasSpec si_data_aliases[] = {
//		{ "battery_soc", "battery_level" },
		{ "ac1_voltage", "output_voltage" },
		{ "ac1_frequency", "output_frequency" },
		{ "ac1_current", "output_current" },
		{ "ac1_power", "output_power" },
		{ "ac2_voltage", "input_voltage" },
		{ "ac2_frequency", "input_frequency" },
		{ "ac2_current", "input_current" },
		{ "ac2_power", "input_power" },
		{ "TotLodPwr", "load_power" },
		{ 0 }
	};
	JSFunctionSpec si_data_funcs[] = {
		JS_FN("refresh",refresh_si_data,0,0,0),
		{ 0 }
	};
	JSObject *obj;

	if (s->ap && s->ap->cp && !s->data_props) {
		s->data_props = js_config_to_props(s->ap->cp, cx, "si_data", 0);
		dprintf(dlevel,"data->props: %p\n",s->data_props);
		if (!s->data_props) {
			log_error("unable to create si_data props: %s\n", config_get_errmsg(s->ap->cp));
			return 0;
		}
	}

	dprintf(dlevel,"defining %s object\n",si_data_class.name);
	obj = JS_InitClass(cx, parent, 0, &si_data_class, 0, 0, s->data_props, si_data_funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s", si_data_class.name);
		return 0;
	}
	if (s->ap->cp) {
		dprintf(dlevel,"defining %s aliases\n",si_data_class.name);
		if (!JS_DefineAliases(cx, obj, si_data_aliases)) {
			JS_ReportError(cx,"unable to define aliases");
			return 0;
		}
	}

	JS_SetPrivate(cx,obj,s);
	dprintf(dlevel,"done!\n");
	return obj;
}

/*************************************************************************/

enum SI_PROPERTY_ID {
	SI_PROPERTY_ID_DATA=1024,
	SI_PROPERTY_ID_INFO,
	SI_PROPERTY_ID_AGENT,
	SI_PROPERTY_ID_CAN,
	SI_PROPERTY_ID_SMANET,
	SI_PROPERTY_ID_SMANET_CON,
};

static JSBool si_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	si_session_t *s;
	config_property_t *p;

	dprintf(dlevel,"obj: %p\n", obj);
	s = JS_GetPrivate(cx,obj);
	dprintf(dlevel,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx,"private is null!");
		return JS_FALSE;
	}

	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	p = 0;
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case SI_PROPERTY_ID_DATA:
			dprintf(dlevel,"getting data..\n");
			*rval = s->data_val;
			break;
		case SI_PROPERTY_ID_INFO:
		    {
			json_value_t *v;
			char *j;
			JSString *str;
			jsval rootVal;
			JSONParser *jp;
			jsval reviver = JSVAL_NULL;
			JSBool ok;

			dprintf(dlevel,"getting info..\n");
			/* Convert from C JSON type to JS JSON type */
			v = si_get_info(s);
			dprintf(dlevel,"v: %p\n", v);
			if (!v) {
				JS_ReportError(cx, "unable to create info\n");
				return JS_FALSE;
			}
			j = json_dumps(v,0);
			if (!j) {
				JS_ReportError(cx, "unable to stringify info\n");
				json_destroy_value(v);
				return JS_FALSE;
			}
			dprintf(dlevel,"j: %p\n", j);
			json_destroy_value(v);
			jp = js_BeginJSONParse(cx, &rootVal);
			dprintf(dlevel,"jp: %p\n", jp);
			if (!jp) {
				JS_ReportError(cx, "unable init JSON parser\n");
				free(j);
				return JS_FALSE;
			}
			str = JS_NewStringCopyZ(cx,j);
        		ok = js_ConsumeJSONText(cx, jp, JS_GetStringChars(str), JS_GetStringLength(str));
			dprintf(dlevel,"ok: %d\n", ok);
			ok = js_FinishJSONParse(cx, jp, reviver);
//			ok = js_FinishJSONParse(cx, jp);
			dprintf(dlevel,"ok: %d\n", ok);
			free(j);
			*rval = rootVal;
		    }
		    break;
		case SI_PROPERTY_ID_AGENT:
			dprintf(dlevel,"getting agent..\n");
			*rval = s->agent_val;
			break;
		case SI_PROPERTY_ID_CAN:
			*rval = s->can_val;
			break;
		case SI_PROPERTY_ID_SMANET:
			*rval = s->smanet_val;
			break;
		case SI_PROPERTY_ID_SMANET_CON:
			*rval = BOOLEAN_TO_JSVAL(s->smanet ? smanet_connected(s->smanet) : false);
			break;
		default:
			p = CONFIG_GETMAP(s->ap->cp,prop_id);
			dprintf(dlevel,"p: %p\n", p);
			if (!p) p = config_get_propbyid(s->ap->cp,prop_id);
			dprintf(dlevel,"p: %p\n", p);
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
		dprintf(dlevel,"sname: %s, name: %s\n", sname, name);
		if (sname && name) p = config_get_property(s->ap->cp, sname, name);
//		if (!p) dprintf(0,"prop not found: %s\n", name);
		if (name) JS_free(cx,name);
	}
	dprintf(dlevel,"p: %p\n", p);
	if (p && p->dest) {
//		config_dump_property(p,0);
		dprintf(dlevel,"p: type: %d(%s), name: %s\n", p->type, typestr(p->type), p->name);
		*rval = type_to_jsval(cx,p->type,p->dest,p->len);
	}
	return JS_TRUE;
}

static JSBool si_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	si_session_t *s;

	s = JS_GetPrivate(cx,obj);
	if (!s) {
		JS_ReportError(cx,"private is null!");
		return JS_FALSE;
	}
#if 0
	if (JSVAL_IS_STRING(id)) {
		config_property_t *p;
		char *sname, *name;
		JSClass *classp = OBJ_GET_CLASS(cx, obj);

		sname = (char *)classp->name;
		name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
		dprintf(dlevel,"sname: %s, name: %s\n", sname, name);
		if (sname && name) p = config_get_property(s->ap->cp, sname, name);
//		if (!p) dprintf(0,"prop not found: %s\n", name);
		if (name) JS_free(cx,name);
	}
#endif
	return js_config_common_setprop(cx, obj, id, rval, s->ap->cp, 0);
}


static JSClass si_class = {
	"SI",		/* Name */
	JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	si_getprop,		/* getProperty */
	si_setprop,		/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool jssi_can_read(JSContext *cx, uintN argc, jsval *vp) {
	si_session_t *s;
	uint8_t data[8];
	int id,len,r;
	jsval *argv = vp + 2;
	JSObject *obj;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	s = JS_GetPrivate(cx, obj);
	dprintf(1,"s: %p\n", s);

	if (argc != 2) {
		JS_ReportError(cx,"can_read requires 2 arguments (id: number, length: number)\n");
		return JS_FALSE;
	}

	if (!jsval_to_type(DATA_TYPE_INT,&id,0,cx,argv[0])) return JS_FALSE;
	dprintf(dlevel,"id: 0x%03x\n", id);
	if (!jsval_to_type(DATA_TYPE_INT,&len,0,cx,argv[1])) return JS_FALSE;
	dprintf(dlevel,"len: %d\n", len);
	if (len > 8) len = 8;
	dprintf(1,"****** id: %03x, len: %d\n", id, len);
	if (!s->can) return JS_FALSE;
	r = s->can_read(s,id,data,len);
	dprintf(4,"r: %d\n", r);
	if (r) return JS_FALSE;
	*vp = type_to_jsval(cx,DATA_TYPE_U8_ARRAY,data,len);
	return JS_TRUE;
}

static JSBool jssi_can_write(JSContext *cx, uintN argc, jsval *vp) {
	si_session_t *s;
	uint8_t data[8];
	struct can_frame frame;
	int len,r;
	uint32_t can_id;
	JSObject *obj;
	JSClass *classp;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx, "can_write: internal error: private is null!\n");
		return JS_FALSE;
	}

	if (argc != 2) {
		JS_ReportError(cx,"can_write requires 2 arguments (id: number, data: array)\n");
		return JS_FALSE;
	}

	/* Get the args*/
	if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "u o", &can_id, &obj)) return JS_FALSE;
	dprintf(dlevel,"can_id: %d, obj: %p\n", can_id, obj);

	/* Make sure the object is an array */
	classp = OBJ_GET_CLASS(cx,obj);
	if (!classp) {
		JS_ReportError(cx, "can_write: 2nd argument must be array\n");
		return JS_FALSE;
	}
	dprintf(dlevel,"class: %s\n", classp->name);
	if (classp && strcmp(classp->name,"Array")) {
		JS_ReportError(cx, "can_write: 2nd argument must be array\n");
		return JS_FALSE;
	}

	/* Convert the JS array to C array */
	len = jsval_to_type(DATA_TYPE_U8_ARRAY,data,8,cx,OBJECT_TO_JSVAL(obj));

        memset(&frame,0,sizeof(frame));
        frame.can_id = can_id;
        frame.can_dlc = len;
        memcpy(&frame.data,data,len);
	r = si_can_write(s,can_id,data,len);
	dprintf(dlevel,"r: %d\n", r);
	*vp = INT_TO_JSVAL(r);
	return JS_TRUE;
}

static JSBool jssi_notify(JSContext *cx, uintN argc, jsval *vp) {
	jsval val;
	char *str;
	si_session_t *s;
	JSObject *obj;

	dprintf(0,"argc: %d\n", argc);
	if (!argc) return JS_TRUE;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	s = JS_GetPrivate(cx, obj);
	if (!s) {
		JS_ReportError(cx, "can_write: internal error: private is null!\n");
		return JS_FALSE;
	}

	JS_SPrintf(cx, JS_GetGlobalObject(cx), argc, JS_ARGV(cx, vp), &val);
	str = (char *)js_GetStringBytes(cx, JSVAL_TO_STRING(val));
	dprintf(0,"str: %s\n", str);
//	si_notify(s,"%s",str);
	return JS_TRUE;
}

static JSBool js_si_register(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	si_session_t *s;
	char *name, *func;

	s = JS_GetPrivate(cx, obj);
	if (!s) {
		JS_ReportError(cx, "can_write: internal error: private is null!\n");
		return JS_FALSE;
	}

	if (argc != 2) {
		JS_ReportError(cx,"register requires 2 arguments (script_name:string, function:string)\n");
		return JS_FALSE;
	}

	name = func = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "s s", &name, &func)) return JS_FALSE;
	dprintf(0,"name: %s, func: %s\n", name, func);

	s->eh.enabled = true;
	strncpy(s->eh.name,name,sizeof(s->eh.name)-1);
	strncpy(s->eh.func,func,sizeof(s->eh.func)-1);

	return JS_TRUE;
}

JSBool si_callfunc(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	si_session_t *s;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) {
		JS_ReportError(cx, "si_callfunc: internal error: object is null!\n");
		return JS_FALSE;
	}
	s = JS_GetPrivate(cx, obj);
	if (!s) {
		JS_ReportError(cx, "si_callfunc: internal error: private is null!\n");
		return JS_FALSE;
	}
	if (!s->ap) return JS_TRUE;
	if (!s->ap->cp) return JS_TRUE;

	return js_config_callfunc(s->ap->cp, cx, argc, vp);
}

static int js_si_init(JSContext *cx, JSObject *parent, void *priv) {
	si_session_t *s = priv;
	JSPropertySpec si_props[] = {
		{ "data", SI_PROPERTY_ID_DATA, JSPROP_ENUMERATE },
		{ "info", SI_PROPERTY_ID_INFO, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "agent", SI_PROPERTY_ID_AGENT, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "can", SI_PROPERTY_ID_CAN, JSPROP_ENUMERATE | JSPROP_READONLY },
//		{ "smanet", SI_PROPERTY_ID_SMANET, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "smanet", SI_PROPERTY_ID_SMANET, JSPROP_ENUMERATE },
		{ "smanet_connected", SI_PROPERTY_ID_SMANET_CON, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ 0 }
	};
	JSFunctionSpec si_funcs[] = {
		JS_FN("can_read",jssi_can_read,1,1,0),
		JS_FN("can_write",jssi_can_write,2,2,0),
		JS_FS("register",js_si_register,2,2,0),
		JS_FN("notify",jssi_notify,0,0,0),
		{ 0 }
	};
	JSAliasSpec si_aliases[] = {
//		{ "charge_amps", "charge_current" },
		{ 0 }
	};
	JSConstantSpec si_constants[] = {
		JS_NUMCONST(SI_STATE_RUNNING),
		JS_NUMCONST(SI_VOLTAGE_MIN),
		JS_NUMCONST(SI_VOLTAGE_MAX),
		JS_NUMCONST(SI_CONFIG_FLAG_SMANET),
		JS_NUMCONST(CURRENT_SOURCE_NONE),
		JS_NUMCONST(CURRENT_SOURCE_CALCULATED),
		JS_NUMCONST(CURRENT_SOURCE_CAN),
		JS_NUMCONST(CURRENT_SOURCE_SMANET),
		JS_NUMCONST(CURRENT_TYPE_AMPS),
		JS_NUMCONST(CURRENT_TYPE_WATTS),
		{ 0 }
	};
	JSObject *obj;

	dprintf(dlevel,"parent: %p\n", parent);
	dprintf(dlevel,"s->props: %p, cp: %p\n",s->props,s->ap->cp);
	if (s->ap && s->ap->cp) {
		if (!s->props) {
			s->props = js_config_to_props(s->ap->cp, cx, "si", si_props);
			dprintf(dlevel,"s->props: %p\n",s->props);
			if (!s->props) {
				log_error("unable to create props: %s\n", config_get_errmsg(s->ap->cp));
				return 0;
			}
		}
		if (!s->funcs) {
			s->funcs = js_config_to_funcs(s->ap->cp, cx, si_callfunc, si_funcs);
			dprintf(dlevel,"s->funcs: %p\n",s->funcs);
			if (!s->funcs) {
				log_error("unable to create funcs: %s\n", config_get_errmsg(s->ap->cp));
				return 0;
			}
		}
	}
	dprintf(dlevel,"s->props: %p, s->funcs: %p\n", s->props, s->funcs);
	if (!s->props) s->props = si_props;
	if (!s->funcs) s->funcs = si_funcs;

	dprintf(dlevel,"Defining %s object\n",si_class.name);
	obj = JS_InitClass(cx, parent, 0, &si_class, 0, 0, s->props, s->funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize si class");
		return 1;
	}

	dprintf(dlevel,"Defining %s aliases\n",si_class.name);
	if (!JS_DefineAliases(cx, obj, si_aliases)) {
		JS_ReportError(cx,"unable to define aliases");
		return 1;
	}
	dprintf(dlevel,"Defining global constants\n");
	if (!JS_DefineConstants(cx, parent, si_constants)) {
		JS_ReportError(cx,"unable to define constants");
		return 1;
	}
	dprintf(dlevel,"done!\n");
	JS_SetPrivate(cx,obj,s);
	s->si_obj = obj;

	s->data_val = OBJECT_TO_JSVAL(jssi_data_new(cx,obj,s));
	s->agent_val = OBJECT_TO_JSVAL(js_agent_new(cx,obj,s->ap));
	s->can_obj = jscan_new(cx,parent,s->can,s->can_handle,s->can_transport,s->can_target,s->can_topts,&s->can_connected);
	if (s->can_obj) s->can_val = OBJECT_TO_JSVAL(s->can_obj);
#ifdef SMANET
	s->smanet_obj = jssmanet_new(cx,parent,s->smanet,s->smanet_transport,s->smanet_target,s->smanet_topts);
	if (s->smanet_obj) s->smanet_val = OBJECT_TO_JSVAL(s->smanet_obj);
#endif

	/* Create the global convenience objects */
	JS_DefineProperty(cx, parent, "si", OBJECT_TO_JSVAL(obj), 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "data", s->data_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "agent", s->agent_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, parent, "can", s->can_val, 0, 0, JSPROP_ENUMERATE);
#ifdef SMANET
	JS_DefineProperty(cx, parent, "smanet", s->smanet_val, 0, 0, JSPROP_ENUMERATE);
#endif

	if (s->ap) {
		JS_DefineProperty(cx, parent, "config", s->ap->js.config_val, 0, 0, JSPROP_ENUMERATE);
		JS_DefineProperty(cx, parent, "mqtt", s->ap->js.mqtt_val, 0, 0, JSPROP_ENUMERATE);
		JS_DefineProperty(cx, parent, "influx", s->ap->js.influx_val, 0, 0, JSPROP_ENUMERATE);
	}

	return 0;
}

int si_jsinit(si_session_t *s) {
	JS_EngineAddInitFunc(s->ap->js.e, "si", js_si_init, s);
	return 0;
}
#endif
