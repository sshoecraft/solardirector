
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 3

#include "battery.h"

/******************************
*
*** Battery Storage Device
*
******************************/

static void _get_arr(void *ctx, char *name,void *dest, int len,json_value_t *v) {
	solard_battery_t *bp = dest;
	json_array_t *a;
	int i,type;

	type = json_value_get_type(v);
	dprintf(dlevel,"type: %d(%s)\n", type, json_typestr(type));
	if (type != JSON_TYPE_ARRAY) return;
 
	a = json_value_array(v);
	if (!a->count) return;
	/* must be numbers */
	if (json_value_get_type(a->items[0]) != JSON_TYPE_NUMBER) return;
	if (strcmp(name,"temps")==0) {
		for(i=0; i < a->count; i++) bp->temps[i] = json_value_get_number(a->items[i]);
		bp->ntemps = a->count;
	} else if (strcmp(name,"cellvolt")==0) {
//		for(i=0; i < a->count; i++) bp->cells[i].voltage = json_value_get_number(a->items[i]);
		for(i=0; i < a->count; i++) bp->cellvolt[i] = json_value_get_number(a->items[i]);
		bp->ncells = a->count;
	} else if (strcmp(name,"cellres")==0) {
//		for(i=0; i < a->count; i++) bp->cells[i].resistance = json_value_get_number(a->items[i]);
		for(i=0; i < a->count; i++) bp->cellres[i] = json_value_get_number(a->items[i]);
	}
	return;
}

static void _set_arr(void *ctx, char *name,void *dest, int len,json_value_t *v) {
	solard_battery_t *bp = dest;
	json_array_t *a;
	int i;

	a = json_create_array();
	if (!a) return;
	dprintf(dlevel,"len: %d\n", len);
	if (strcmp(name,"temps")==0) {
		for(i=0; i < len; i++) json_array_add_number(a,bp->temps[i]);
	} else if (strcmp(name,"cellvolt")==0) {
//		for(i=0; i < len; i++) json_array_add_number(a,bp->cells[i].voltage);
		for(i=0; i < len; i++) json_array_add_number(a,bp->cellvolt[i]);
	} else if (strcmp(name,"cellres")==0) {
//		for(i=0; i < len; i++) json_array_add_number(a,bp->cells[i].resistance);
		for(i=0; i < len; i++) json_array_add_number(a,bp->cellres[i]);
	}
	json_object_set_array(json_value_object(v),name,a);
	return;
}

static void _flat_arr(void *ctx, char *name,void *dest, int len,json_value_t *v) {
	solard_battery_t *bp = dest;
	json_object_t *o;
	char label[32];
	int i;

	o = json_value_object(v);
	dprintf(dlevel+1,"len: %d\n", len);
	if (strcmp(name,"temps")==0) {
		for(i=0; i < len; i++) {
			sprintf(label,"temp_%02d",i + bp->one);
			json_object_set_number(o,label,bp->temps[i]);
		}
	} else if (strcmp(name,"cellvolt")==0) {
		for(i=0; i < len; i++) {
			sprintf(label,"cell_%02d",i + bp->one);
//			json_object_set_number(o,label,bp->cells[i].voltage);
			json_object_set_number(o,label,bp->cellvolt[i]);
		}
	} else if (strcmp(name,"cellres")==0) {
		for(i=0; i < len; i++) {
			sprintf(label,"res_%02d",i + bp->one);
//			json_object_set_number(o,label,bp->cells[i].resistance);
			json_object_set_number(o,label,bp->cellres[i]);
		}
	}
	return;
}

static struct battery_states {
	int mask;
	char *label;
} states[] = {
	{ BATTERY_STATE_CHARGING, "Charging" },
	{ BATTERY_STATE_DISCHARGING, "Discharging" },
	{ BATTERY_STATE_BALANCING, "Balancing" },
};
#define NSTATES (sizeof(states)/sizeof(struct battery_states))

static void _get_state(void *ctx, char *name, void *dest, int len, json_value_t *v) {
	solard_battery_t *bp = dest;
	char *p,*sp;
	int i,j;

	p = json_value_get_string(v);
	dprintf(dlevel,"value: %s\n", p);
	for(i=0; i < 99; i++) {
		sp = strele(i,",",p);
		if (!strlen(sp)) break;
		dprintf(dlevel,"sp: %s\n", sp);
		for(j=0; j < NSTATES; j++) {
			dprintf(dlevel,"states[j].label: %s\n", states[j].label);
			if (strcmp(sp,states[j].label) == 0) {
				dprintf(dlevel,"found\n");
				bp->state |= states[j].mask;
				break;
			}
		}
	}
	dprintf(dlevel,"state: %x\n", bp->state);
}

static void _set_state(void *ctx, char *name, void *dest, int len, json_value_t *v) {
	solard_battery_t *bp = dest;
	char temp[128],*p;
	int i,j;

	dprintf(dlevel+1,"state: %x\n", bp->state);

	/* States */
	temp[0] = 0;
	p = temp;
	for(i=j=0; i < NSTATES; i++) {
		if (check_state(bp,states[i].mask)) {
			if (j) p += sprintf(p,",");
			p += sprintf(p,states[i].label);
			j++;
		}
	}
	dprintf(dlevel+1,"temp: %s\n", temp);
	json_object_set_string(json_value_object(v), name, temp);
}

#define BATTERY_TAB(NTEMP,NBAT,ACTION,STATE) \
		{ "name",DATA_TYPE_STRING,&bp->name,sizeof(bp->name)-1,0 }, \
		{ "capacity",DATA_TYPE_DOUBLE,&bp->capacity,0,0 }, \
		{ "voltage",DATA_TYPE_DOUBLE,&bp->voltage,0,0 }, \
		{ "current",DATA_TYPE_DOUBLE,&bp->current,0,0 }, \
		{ "power",DATA_TYPE_DOUBLE,&bp->power,0,0 }, \
		{ "ntemps",DATA_TYPE_INT,&bp->ntemps,0,0 }, \
		{ "temps",0,bp,NTEMP,ACTION }, \
		{ "ncells",DATA_TYPE_INT,&bp->ncells,0,0 }, \
		{ "cellvolt",0,bp,NBAT,ACTION }, \
		{ "cellres",0,bp,NBAT,ACTION }, \
		{ "cell_min",DATA_TYPE_DOUBLE,&bp->cell_min,0,0 }, \
		{ "cell_max",DATA_TYPE_DOUBLE,&bp->cell_max,0,0 }, \
		{ "cell_diff",DATA_TYPE_DOUBLE,&bp->cell_diff,0,0 }, \
		{ "cell_avg",DATA_TYPE_DOUBLE,&bp->cell_avg,0,0 }, \
		{ "cell_total",DATA_TYPE_DOUBLE,&bp->cell_total,0,0 }, \
		{ "errcode",DATA_TYPE_INT,&bp->errcode,0,0 }, \
		{ "errmsg",DATA_TYPE_STRING,&bp->errmsg,sizeof(bp->errmsg)-1,0 }, \
		{ "state",0,bp,0,STATE }, \
		JSON_PROCTAB_END

static void _dump_arr(void *ctx, char *name,void *dest, int flen,json_value_t *v) {
	solard_battery_t *bp = dest;
	char format[16];
	int i,*level = (int *) v;

#ifdef DEBUG
	if (debug >= *level) {
		dprintf(dlevel,"flen: %d\n", flen);
#endif
		if (strcmp(name,"temps")==0) {
			sprintf(format,"%%%ds: %%.1f\n",flen);
			for(i=0; i < bp->ntemps; i++) printf(format,name,bp->temps[i]);
		} else if (strcmp(name,"cellvolt")==0) {
			sprintf(format,"%%%ds: %%.3f\n",flen);
//			for(i=0; i < bp->ncells; i++) printf(format,name,bp->cells[i].voltage);
			for(i=0; i < bp->ncells; i++) printf(format,name,bp->cellvolt[i]);
		} else if (strcmp(name,"cellres")==0) {
			sprintf(format,"%%%ds: %%.3f\n",flen);
//			for(i=0; i < bp->ncells; i++) printf(format,name,bp->cell[i].resistance);
			for(i=0; i < bp->ncells; i++) printf(format,name,bp->cellres[i]);
		}
#ifdef DEBUG
	}
#endif
	return;
}

static void _dump_state(void *ctx, char *name, void *dest, int flen, json_value_t *v) {
	solard_battery_t *bp = dest;
	char format[16];
	int *level = (int *) v;

	sprintf(format,"%%%ds: %%d\n",flen);
#ifdef DEBUG
	if (debug >= *level) printf(format,name,bp->state);
#else
	printf(format,name,bp->state);
#endif
}

void battery_dump(solard_battery_t *bp, int level) {
	json_proctab_t tab[] = { BATTERY_TAB(bp->ntemps,bp->ncells,_dump_arr,_dump_state) }, *p;
	char format[16],temp[1024];
	int flen;

	flen = 0;
	for(p=tab; p->field; p++) {
		if (strlen(p->field) > flen)
			flen = strlen(p->field);
	}
	flen++;
	sprintf(format,"%%%ds: %%s\n",flen);
	dprintf(level,"battery:\n");
	for(p=tab; p->field; p++) {
		if (p->cb) p->cb(bp,p->field,p->ptr,flen,(void *)&level);
		else {
			conv_type(DATA_TYPE_STRING,&temp,sizeof(temp)-1,p->type,p->ptr,p->len);
#ifdef DEBUG
			if (debug >= level) printf(format,p->field,temp);
#else
			printf(format,p->field,temp);
#endif
		}
	}
}

json_value_t *battery_to_json(solard_battery_t *bp) {
	json_proctab_t battery_tab[] = { BATTERY_TAB(bp->ntemps,bp->ncells,_set_arr,_set_state) };

	return json_from_tab(battery_tab);
}

json_value_t *battery_to_flat_json(solard_battery_t *bp) {
	json_proctab_t battery_tab[] = { BATTERY_TAB(bp->ntemps,bp->ncells,_flat_arr,_set_state) };

	return json_from_tab(battery_tab);
}

#if 1
int battery_from_json(solard_battery_t *bp, char *str) {
#else
int battery_from_json(solard_battery_t *bp, json_value_t *v) {
#endif
	json_proctab_t battery_tab[] = { BATTERY_TAB(BATTERY_MAX_TEMPS,BATTERY_MAX_CELLS,_get_arr,_get_state) };
#if 1
	json_value_t *v;

	dprintf(dlevel,"parsing...\n");
	v = json_parse(str);
	dprintf(dlevel,"v: %p\n", v);
	if (!v) return 1;
#endif
	memset(bp,0,sizeof(*bp));
	dprintf(dlevel,"getting tab...\n");
	json_to_tab(battery_tab,v);
//	battery_dump(bp,3);
	json_destroy_value(v);
	dprintf(dlevel,"done!\n");
	return 0;
};

#ifdef JS
enum BATTERY_PROPERTY_ID {
	BATTERY_PROPERTY_ID_NAME=1,
	BATTERY_PROPERTY_ID_CAPACITY,
	BATTERY_PROPERTY_ID_VOLTAGE,
	BATTERY_PROPERTY_ID_CURRENT,
	BATTERY_PROPERTY_ID_POWER,
	BATTERY_PROPERTY_ID_NTEMPS,
	BATTERY_PROPERTY_ID_TEMPS,
	BATTERY_PROPERTY_ID_TEMP,
	BATTERY_PROPERTY_ID_NCELLS,
	BATTERY_PROPERTY_ID_CELLVOLT,
	BATTERY_PROPERTY_ID_CELLRES,
	BATTERY_PROPERTY_ID_CELL_MIN,
	BATTERY_PROPERTY_ID_CELL_MAX,
	BATTERY_PROPERTY_ID_CELL_DIFF,
	BATTERY_PROPERTY_ID_CELL_AVG,
	BATTERY_PROPERTY_ID_CELL_TOTAL,
	BATTERY_PROPERTY_ID_BALANCEBITS,
	BATTERY_PROPERTY_ID_ERRCODE,
	BATTERY_PROPERTY_ID_ERRMSG,
	BATTERY_PROPERTY_ID_STATE,
	BATTERY_PROPERTY_ID_LAST_UPDATE,
};

static JSBool battery_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	solard_battery_t *bp;
	register int i;

	bp = JS_GetPrivate(cx,obj);
	dprintf(dlevel,"bp: %p\n", bp);
	if (!bp) {
		JS_ReportError(cx, "private is null!");
		return JS_FALSE;
	}

	dprintf(dlevel,"is_int: %d\n", JSVAL_IS_INT(id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case BATTERY_PROPERTY_ID_NAME:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,bp->name));
			*rval = type_to_jsval(cx, DATA_TYPE_STRING, bp->name, strlen(bp->name));
			break;
		case BATTERY_PROPERTY_ID_CAPACITY:
			JS_NewDoubleValue(cx, bp->capacity, rval);
			break;
		case BATTERY_PROPERTY_ID_VOLTAGE:
			JS_NewDoubleValue(cx, bp->voltage, rval);
			break;
		case BATTERY_PROPERTY_ID_CURRENT:
			JS_NewDoubleValue(cx, bp->current, rval);
			break;
		case BATTERY_PROPERTY_ID_NTEMPS:
			dprintf(dlevel,"ntemps: %d\n", bp->ntemps);
			*rval = INT_TO_JSVAL(bp->ntemps);
			break;
		case BATTERY_PROPERTY_ID_TEMPS:
		       {
				JSObject *rows;
				jsval val;

//				dprintf(dlevel,"ntemps: %d\n", bp->ntemps);
				rows = JS_NewArrayObject(cx, bp->ntemps, NULL);
				for(i=0; i < bp->ntemps; i++) {
//					dprintf(dlevel,"temps[%d]: %f\n", i, bp->temps[i]);
					JS_NewDoubleValue(cx, bp->temps[i], &val);
					JS_SetElement(cx, rows, i, &val);
				}
				*rval = OBJECT_TO_JSVAL(rows);
			}
			break;
		case BATTERY_PROPERTY_ID_NCELLS:
			dprintf(dlevel,"ncells: %d\n", bp->ncells);
			*rval = INT_TO_JSVAL(bp->ncells);
			break;
		case BATTERY_PROPERTY_ID_CELLVOLT:
		       {
				JSObject *rows;
				jsval val;

				rows = JS_NewArrayObject(cx, bp->ncells, NULL);
				for(i=0; i < bp->ncells; i++) {
//					JS_NewDoubleValue(cx, bp->cells[i].voltage, &val);
					JS_NewDoubleValue(cx, bp->cellvolt[i], &val);
					JS_SetElement(cx, rows, i, &val);
				}
				*rval = OBJECT_TO_JSVAL(rows);
			}
			break;
		case BATTERY_PROPERTY_ID_CELLRES:
		       {
				JSObject *rows;
				jsval val;

				rows = JS_NewArrayObject(cx, bp->ncells, NULL);
				for(i=0; i < bp->ncells; i++) {
//					JS_NewDoubleValue(cx, bp->cells[i].resistance, &val);
					JS_NewDoubleValue(cx, bp->cellres[i], &val);
					JS_SetElement(cx, rows, i, &val);
				}
				*rval = OBJECT_TO_JSVAL(rows);
			}
			break;
		case BATTERY_PROPERTY_ID_CELL_MIN:
			JS_NewDoubleValue(cx, bp->cell_min, rval);
			break;
		case BATTERY_PROPERTY_ID_CELL_MAX:
			JS_NewDoubleValue(cx, bp->cell_max, rval);
			break;
		case BATTERY_PROPERTY_ID_CELL_DIFF:
			JS_NewDoubleValue(cx, bp->cell_diff, rval);
			break;
		case BATTERY_PROPERTY_ID_CELL_AVG:
			JS_NewDoubleValue(cx, bp->cell_avg, rval);
			break;
		case BATTERY_PROPERTY_ID_CELL_TOTAL:
			JS_NewDoubleValue(cx, bp->cell_total, rval);
			break;
		case BATTERY_PROPERTY_ID_BALANCEBITS:
			*rval = INT_TO_JSVAL(bp->balancebits);
			break;
		case BATTERY_PROPERTY_ID_ERRCODE:
			*rval = INT_TO_JSVAL(bp->errcode);
			break;
		case BATTERY_PROPERTY_ID_ERRMSG:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,bp->errmsg));
			break;
		case BATTERY_PROPERTY_ID_STATE:
			*rval = INT_TO_JSVAL(bp->state);
			break;
		case BATTERY_PROPERTY_ID_LAST_UPDATE:
			*rval = INT_TO_JSVAL(bp->last_update);
			break;
		default:
			JS_ReportError(cx, "not a property");
			*rval = JSVAL_NULL;
		}
	}
	return JS_TRUE;
}

static JSClass js_battery_class = {
	"battery",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,
	JS_PropertyStub,
	battery_getprop,
	JS_PropertyStub,
	JS_EnumerateStub,
	JS_ResolveStub,
	JS_ConvertStub,
	JS_FinalizeStub,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *js_InitBatteryClass(JSContext *cx, JSObject *parent) {
	JSPropertySpec battery_props[] = { 
		{ "name",		BATTERY_PROPERTY_ID_NAME,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "capacity",		BATTERY_PROPERTY_ID_CAPACITY,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "voltage",		BATTERY_PROPERTY_ID_VOLTAGE,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "current",		BATTERY_PROPERTY_ID_CURRENT,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "power",		BATTERY_PROPERTY_ID_POWER,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "ntemps",		BATTERY_PROPERTY_ID_NTEMPS,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "temps",		BATTERY_PROPERTY_ID_TEMPS,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "ncells",		BATTERY_PROPERTY_ID_NCELLS,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "cellvolt",		BATTERY_PROPERTY_ID_CELLVOLT,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "cellres",		BATTERY_PROPERTY_ID_CELLRES,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "cell_min",		BATTERY_PROPERTY_ID_CELL_MIN,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "cell_max",		BATTERY_PROPERTY_ID_CELL_MAX,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "cell_diff",		BATTERY_PROPERTY_ID_CELL_DIFF,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "cell_avg",		BATTERY_PROPERTY_ID_CELL_AVG,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "cell_total",		BATTERY_PROPERTY_ID_CELL_TOTAL,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "balancebits",	BATTERY_PROPERTY_ID_BALANCEBITS,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "errcode",		BATTERY_PROPERTY_ID_ERRCODE,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "errmsg",		BATTERY_PROPERTY_ID_ERRMSG,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "state",		BATTERY_PROPERTY_ID_STATE,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "last_update",	BATTERY_PROPERTY_ID_LAST_UPDATE,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{0}
	};
	JSFunctionSpec battery_funcs[] = {
		{ 0 }
	};
	JSObject *obj;

	dprintf(dlevel,"defining %s object\n",js_battery_class.name);
	obj = JS_InitClass(cx, parent, 0, &js_battery_class, 0, 0, battery_props, battery_funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize battery class");
		return 0;
	}
	dprintf(dlevel,"done!\n");
	return obj;
}

JSObject *js_battery_new(JSContext *cx, JSObject *parent, solard_battery_t *bp) {
	JSObject *newobj;

	/* Create the new object */
	newobj = JS_NewObject(cx, &js_battery_class, 0, parent);
	if (!newobj) return 0;

	JS_SetPrivate(cx,newobj,bp);

	return newobj;
}
#endif
