
#ifdef JS

#define DEBUG_JSFUNCS 0
#define dlevel 4

#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG DEBUG_JSFUNCS
#include "debug.h"
#include "sb.h"
#include "jsobj.h"
#include "jsarray.h"

enum SMA_OBJECT_PROPERTY_ID {
	SMA_OBJECT_PROPERTY_ID_KEY,
	SMA_OBJECT_PROPERTY_ID_LABEL,
	SMA_OBJECT_PROPERTY_ID_PATH,
	SMA_OBJECT_PROPERTY_ID_UNIT,
	SMA_OBJECT_PROPERTY_ID_MULT
};

static JSBool sb_object_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	sb_object_t *o;

	o = JS_GetPrivate(cx,obj);
	if (!o) return JS_FALSE;
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
                case SMA_OBJECT_PROPERTY_ID_KEY:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,o->key,strlen(o->key));
                        break;
                case SMA_OBJECT_PROPERTY_ID_LABEL:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,o->label,strlen(o->label));
                        break;
                case SMA_OBJECT_PROPERTY_ID_PATH:
			if (!o->path_val) {
				JSObject *arr;
				jsval element;
				int i,count;

				for(i=0; i < 8; i++) { if (!o->hier[i]) break; }
				count = i;
				arr = JS_NewArrayObject(cx, count, NULL);
				for(i=0; i < count; i++) {
					element = type_to_jsval(cx,DATA_TYPE_STRING,o->hier[i],strlen(o->hier[i]));
					JS_SetElement(cx, arr, i, &element);
				}
				o->path_val = OBJECT_TO_JSVAL(arr);
			}
			*rval = o->path_val;
			break;
                case SMA_OBJECT_PROPERTY_ID_UNIT:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,o->label,strlen(o->label));
			break;
                case SMA_OBJECT_PROPERTY_ID_MULT:
                        JS_NewDoubleValue(cx, o->mult, rval);
                        break;
		default:
			dprintf(0,"unhandled id: %d\n", prop_id);
			*rval = JSVAL_NULL;
			break;
		}
	}
	return JS_TRUE;
}

static JSClass sb_object_class = {
	"sb_object",		/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	sb_object_getprop,	/* getProperty */
	JS_PropertyStub,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *js_sb_object_new(JSContext *cx, JSObject *parent, sb_object_t *o) {
	JSPropertySpec sb_object_props[] = {
		{ "key",SMA_OBJECT_PROPERTY_ID_KEY,JSPROP_ENUMERATE },
		{ "label",SMA_OBJECT_PROPERTY_ID_LABEL,JSPROP_ENUMERATE },
		{ "path",SMA_OBJECT_PROPERTY_ID_PATH,JSPROP_ENUMERATE },
		{ "unit",SMA_OBJECT_PROPERTY_ID_UNIT,JSPROP_ENUMERATE },
		{ "mult",SMA_OBJECT_PROPERTY_ID_MULT,JSPROP_ENUMERATE },
		{ 0 }
	};
	JSObject *obj;

	dprintf(dlevel,"defining %s object\n",sb_object_class.name);
	obj = JS_InitClass(cx, parent, 0, &sb_object_class, 0, 0, sb_object_props, 0, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s class", sb_object_class.name);
		return 0;
	}
	JS_SetPrivate(cx,obj,o);
	dprintf(dlevel,"done!\n");
	return obj;
}


enum SB_RESULTS_PROPERTY_ID {
	SB_RESULTS_PROPERTY_ID_OBJECT,
	SB_RESULTS_PROPERTY_ID_LOW,
	SB_RESULTS_PROPERTY_ID_HIGH,
	SB_RESULTS_PROPERTY_ID_SELECTIONS,
	SB_RESULTS_PROPERTY_ID_VALUES
};

static JSBool sb_results_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	sb_result_t *res;
	JSObject *newobj;

	res = JS_GetPrivate(cx,obj);
	if (!res) return JS_FALSE;
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case SB_RESULTS_PROPERTY_ID_OBJECT:
			if (!res->obj_val) {
				newobj = js_sb_object_new(cx,obj,res->obj);
				if (!newobj) return JS_FALSE;
				res->obj_val = OBJECT_TO_JSVAL(newobj);
			}
			*rval = res->obj_val;
			break;
		case SB_RESULTS_PROPERTY_ID_LOW:
			*rval = INT_TO_JSVAL(res->low);
			break;
		case SB_RESULTS_PROPERTY_ID_HIGH:
			*rval = INT_TO_JSVAL(res->high);
			break;
		case SB_RESULTS_PROPERTY_ID_SELECTIONS:
			if (!res->selects_val)
				res->selects_val = type_to_jsval(cx,DATA_TYPE_STRING_LIST,res->selects,list_count(res->selects));
			*rval = res->selects_val;
			break;
		case SB_RESULTS_PROPERTY_ID_VALUES:
			if (!res->values_val) {
				JSObject *arr;
				jsval element;
				sb_value_t *vp;
				char value[1024];
				int i;

				arr = JS_NewArrayObject(cx, list_count(res->values), NULL);
				i = 0;
				list_reset(res->values);
				while((vp = list_get_next(res->values)) != 0) {
					element = type_to_jsval(cx,vp->type,vp->data,vp->len);
					*value = 0;
					conv_type(DATA_TYPE_STRING,value,sizeof(value),vp->type,vp->data,vp->len);
					dprintf(dlevel,"value: %s\n", value);
					JS_SetElement(cx, arr, i++, &element);
				}
				res->values_val = OBJECT_TO_JSVAL(arr);
			}
			*rval = res->values_val;
			break;
		default:
			dprintf(0,"unhandled id: %d\n", prop_id);
			*rval = JSVAL_NULL;
			break;
		}
	}
	return JS_TRUE;
}

static JSClass sb_results_class = {
	"sb_results",		/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	sb_results_getprop,	/* getProperty */
	JS_PropertyStub,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSObject *js_sb_results_new(JSContext *cx, JSObject *parent, sb_result_t *res) {
	JSPropertySpec sb_results_props[] = {
		{ "object",SB_RESULTS_PROPERTY_ID_OBJECT,JSPROP_ENUMERATE },
		{ "low",SB_RESULTS_PROPERTY_ID_LOW,JSPROP_ENUMERATE },
		{ "high",SB_RESULTS_PROPERTY_ID_HIGH,JSPROP_ENUMERATE },
		{ "selections",SB_RESULTS_PROPERTY_ID_SELECTIONS,JSPROP_ENUMERATE },
		{ "values",SB_RESULTS_PROPERTY_ID_VALUES,JSPROP_ENUMERATE },
		{ 0 }
	};
	JSObject *obj;

	dprintf(dlevel,"defining %s object\n",sb_results_class.name);
	obj = JS_InitClass(cx, parent, 0, &sb_results_class, 0, 0, sb_results_props, 0, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s", sb_results_class.name);
		return 0;
	}
	JS_SetPrivate(cx,obj,res);
	dprintf(dlevel,"done!\n");
	return obj;
}

/*************************************************************************/

enum SB_PROPERTY_ID {
	SB_PROPERTY_ID_AGENT=1024,
	SB_PROPERTY_ID_RESULTS,
};

static JSBool sb_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	sb_session_t *s;
//	sb_result_t *res;
	config_property_t *p;

	s = JS_GetPrivate(cx,obj);
	dprintf(0,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx,"internal error: private is null!");
		return JS_FALSE;
	}

	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case SB_PROPERTY_ID_AGENT:
			*rval = s->agent_val;
			break;
#if 0
		case SB_PROPERTY_ID_RESULTS:
		    {
			if (!s->results_val) {
				JSObject *arr,*newres;
				jsval element;
				int i,count;
				list results;
	
				count = list_count(s->results);
				arr = JS_NewArrayObject(cx, count, NULL);
				if (!arr) {
                                	JS_ReportError(cx, "unable to allocate results array");
	                                return JS_FALSE;
				}
				i = 0;
				list_reset(s->results);
				while((res = list_get_next(s->results)) != 0) {
					newres = js_sb_results_new(cx,obj,res);
					if (!newres) {
	                                	JS_ReportError(cx, "unable to allocate new sb_results object");
		                                return JS_FALSE;
					}
					element = OBJECT_TO_JSVAL(newres);
					JS_SetElement(cx, arr, i++, &element);
					if (i > count) {
	                                	JS_ReportError(cx,"sb_results index elemenet (%d) > count (%d)", i, count);
		                                return JS_FALSE;
					}
				}
				s->results_val = OBJECT_TO_JSVAL(arr);
			}
			*rval = s->results_val;
		    }
		    break;
#endif
		default:
			p = CONFIG_GETMAP(s->ap->cp,prop_id);
			if (!p) p = config_get_propbyid(s->ap->cp,prop_id);
			if (!p) {
				JS_ReportError(cx, "property %d not found", prop_id);
				return JS_FALSE;
			}
//			dprintf(dlevel,"p: type: %d(%s), name: %s\n", p->type, typestr(p->type), p->name);
			*rval = type_to_jsval(cx,p->type,p->dest,p->len);
			break;
		}
	}
	return JS_TRUE;
}

static JSBool sb_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	sb_session_t *s;
	int ok;

	s = JS_GetPrivate(cx,obj);
	ok = js_config_common_setprop(cx, obj, id, rval, s->ap->cp, s->props);
	return ok;
}


static JSClass sb_class = {
	"sb",			/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	sb_getprop,		/* getProperty */
	sb_setprop,		/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool js_sb_open(JSContext *cx, uintN argc, jsval *vp) {
	sb_session_t *s;
	JSObject *obj;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	s = JS_GetPrivate(cx, obj);
	*vp = BOOLEAN_TO_JSVAL(sb_driver.open(s));
	return JS_TRUE;
}

static JSBool js_sb_close(JSContext *cx, uintN argc, jsval *vp) {
	sb_session_t *s;
	JSObject *obj;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	s = JS_GetPrivate(cx, obj);
	*vp = BOOLEAN_TO_JSVAL(sb_driver.close(s));
	return JS_TRUE;
}

static JSBool js_sb_mkfields(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	JSClass *classp;
	jsval *argv;
	jsuint count;
	char **keys,*fields;
	int len,i;

	if (argc != 1) {
		JS_ReportError(cx,"mkfields requires 1 argument (keys:array)\n");
		return JS_FALSE;
	}

 	argv = JS_ARGV(cx,vp);
	obj = JSVAL_TO_OBJECT(argv[0]);
	classp = OBJ_GET_CLASS(cx,obj);
	if (!classp) {
		JS_ReportError(cx, "mkfields: argument must be array\n");
		return JS_FALSE;
	}
	dprintf(dlevel,"class: %s\n", classp->name);
	if (classp && strcmp(classp->name,"Array")) {
		JS_ReportError(cx, "mkfields: argument must be array\n");
		return JS_FALSE;
	}

	/* Get the array length */
	if (!js_GetLengthProperty(cx, obj, &count)) {
		JS_ReportError(cx,"unable to get array length");
		return 0;
	}
	dprintf(dlevel,"count: %d\n", count);

	keys = malloc(count * sizeof(char *));
	dprintf(dlevel,"keys: %p\n", keys);
	if (!keys) {
		JS_ReportError(cx,"mkfields: malloc(%d): %s\n", count * sizeof(char *),strerror(errno));
		return JS_FALSE;
	}
	memset(keys,0,count * sizeof(char *));
	len = jsval_to_type(DATA_TYPE_STRING_ARRAY,keys,count,cx,argv[0]);
	dprintf(dlevel,"len: %d\n", len);
	fields = sb_mkfields(keys,len);
	if (!fields) {
		JS_ReportError(cx,"mkfields: error!\n");
		return JS_FALSE;
	}
	/* string array dest must be freed */
	for(i=0; i < len; i++) free(keys[i]);
	free(keys);
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,fields));
	free(fields);
	return JS_TRUE;
}

static JSBool js_sb_request(JSContext *cx, uintN argc, jsval *vp) {
	sb_session_t *s;
	JSObject *obj;
	char *func, *fields;
	json_value_t *v;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	s = JS_GetPrivate(cx, obj);
	if (argc != 2) {
		JS_ReportError(cx,"request requires 2 arguments (func:string, fields:string)\n");
		return JS_FALSE;
	}

	func = fields = 0;
	if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "s s", &func, &fields)) return JS_FALSE;
	dprintf(1,"func: %s, fields: %s\n", func, fields);

	v = sb_request(s,func,fields);
	if (!v) {
		*vp = JSVAL_VOID;
		return JS_TRUE;
	}
	json_destroy_value(v);
	*vp = JSVAL_VOID;
	return JS_TRUE;
}

int js_sb_init(JSContext *cx, JSObject *parent, void *priv) {
	JSPropertySpec sb_props[] = {
		{ "agent", SB_PROPERTY_ID_AGENT, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "results", SB_PROPERTY_ID_RESULTS, JSPROP_ENUMERATE | JSPROP_READONLY },
		{ 0 }
	};
	JSFunctionSpec sb_funcs[] = {
		JS_FN("open",js_sb_open,0,0,0),
		JS_FN("close",js_sb_close,0,0,0),
		JS_FN("mkfields",js_sb_mkfields,0,0,0),
		JS_FN("request",js_sb_request,3,3,0),
		{ 0 }
	};
	JSAliasSpec sb_aliases[] = {
		{ 0 }
	};
	JSConstantSpec sb_constants[] = {
		JS_STRCONST(SB_LOGIN),
		JS_STRCONST(SB_LOGOUT),
		JS_STRCONST(SB_GETVAL),
		JS_STRCONST(SB_LOGGER),
		JS_STRCONST(SB_ALLVAL),
		JS_STRCONST(SB_ALLPAR),
		JS_STRCONST(SB_SETPAR),
		{ 0 }
	};
	JSObject *obj, *global = JS_GetGlobalObject(cx);
	sb_session_t *s = priv;

	dprintf(dlevel,"s->props: %p, cp: %p\n",s->props,s->ap->cp);
	if (!s->props) {

		/* Section name must be same as used in config */
		s->props = js_config_to_props(s->ap->cp, cx, "sb", sb_props);
		dprintf(dlevel,"s->props: %p\n",s->props);
		if (!s->props) {
			log_error("unable to create props: %s\n", config_get_errmsg(s->ap->cp));
			return 0;
		}
	}

	dprintf(dlevel,"Defining %s object\n",sb_class.name);
	obj = JS_InitClass(cx, parent, 0, &sb_class, 0, 0, s->props, sb_funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s class",sb_class.name);
		return 1;
	}
	dprintf(dlevel,"Defining %s aliases\n",sb_class.name);
	if (!JS_DefineAliases(cx, obj, sb_aliases)) {
		JS_ReportError(cx,"unable to define aliases");
		return 1;
	}
	dprintf(dlevel,"Defining %s constants\n",sb_class.name);
	if (!JS_DefineConstants(cx, global, sb_constants)) {
		JS_ReportError(cx,"unable to define constants");
		return 1;
	}
	JS_SetPrivate(cx,obj,priv);

	/* Get agent jsval */
	s->agent_val = OBJECT_TO_JSVAL(js_agent_new(cx,obj,s->ap));

	/* Create the global convenience objects */
	JS_DefineProperty(cx, global, "agent", s->agent_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, global, "config", s->ap->js.config_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, global, "mqtt", s->ap->js.mqtt_val, 0, 0, JSPROP_ENUMERATE);
	JS_DefineProperty(cx, global, "influx", s->ap->js.influx_val, 0, 0, JSPROP_ENUMERATE);

	dprintf(dlevel,"done!\n");
	return 0;
}

int sb_jsinit(sb_session_t *s) {
	return JS_EngineAddInitFunc(s->ap->js.e, "sb", js_sb_init, s);
}

#endif
