
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "pvinverter.h"

#if 0
/* States */
#define SOLARD_PVINVERTER_STATE_OPEN 	0x01
#define SOLARD_PVINVERTER_STATE_UPDATED	0x02
#define SOLARD_PVINVERTER_STATE_RUNNING	0x04

static struct pvinverter_states {
	int mask;
	char *label;
} states[] = {
	{ SOLARD_PVINVERTER_STATE_RUNNING, "Running" },
};
#define NSTATES (sizeof(states)/sizeof(struct pvinverter_states))

static void _get_state(void *ctx, char *name, void *dest, int len, json_value_t *v) {
	solard_pvinverter_t *inv = dest;
	char *p,*sp;
	int i,j;

	p = json_value_get_string(v);
	dprintf(4,"value: %s\n", p);
	for(i=0; i < 99; i++) {
		sp = strele(i,",",p);
		if (!strlen(sp)) break;
		dprintf(4,"sp: %s\n", sp);
		for(j=0; j < NSTATES; j++) {
			dprintf(4,"states[j].label: %s\n", states[j].label);
			if (strcmp(sp,states[j].label) == 0) {
				dprintf(4,"found\n");
				inv->state |= states[j].mask;
				break;
			}
		}
	}
	dprintf(4,"state: %x\n", inv->state);
}

static void _set_state(void *ctx, char *name, void *dest, int len, json_value_t *v) {
	solard_pvinverter_t *inv = dest;
	char temp[128],*p;
	int i,j;

	dprintf(4,"state: %x\n", inv->state);

	/* States */
	temp[0] = 0;
	p = temp;
	for(i=j=0; i < NSTATES; i++) {
		if (check_state(inv,states[i].mask)) {
			if (j) p += sprintf(p,",");
			p += sprintf(p,states[i].label);
			j++;
		}
	}
	dprintf(4,"temp: %s\n", temp);
	json_object_set_string(json_value_object(v), "states", temp);
}


static void _dump_state(void *ctx, char *name, void *dest, int flen, json_value_t *v) {
	solard_pvinverter_t *inv = dest;
	char format[16];
	int *dlevel = (int *) v;

	sprintf(format,"%%%ds: %%d\n",flen);
	if (debug >= *dlevel) printf(format,name,inv->state);
}
#endif

//		{ "state",0,inv,0,ACTION }, 
//		{ "errmsg",DATA_TYPE_STRING,&inv->errmsg,sizeof(inv->errmsg)-1,0 },
#define PVINVERTER_TAB(ACTION) \
		{ "name",DATA_TYPE_STRING,&inv->name,sizeof(inv->name)-1,0 }, \
		{ "input_voltage",DATA_TYPE_DOUBLE,&inv->input_voltage,0,0 }, \
		{ "input_current",DATA_TYPE_DOUBLE,&inv->input_current,0,0 }, \
		{ "input_power",DATA_TYPE_DOUBLE,&inv->input_power,0,0 }, \
		{ "output_voltage",DATA_TYPE_DOUBLE,&inv->output_voltage,0,0 }, \
		{ "output_frequency",DATA_TYPE_DOUBLE,&inv->output_frequency,0,0 }, \
		{ "output_current",DATA_TYPE_DOUBLE,&inv->output_current,0,0 }, \
		{ "output_power",DATA_TYPE_DOUBLE,&inv->output_power,0,0 }, \
		{ "total_yield",DATA_TYPE_DOUBLE,&inv->total_yield,0,0 }, \
		{ "daily_yield",DATA_TYPE_DOUBLE,&inv->daily_yield,0,0 }, \
		{ "errcode",DATA_TYPE_INT,&inv->errcode,0,0 }, \
		JSON_PROCTAB_END


void pvinverter_dump(solard_pvinverter_t *inv, int dump_level) {
	json_proctab_t tab[] = { PVINVERTER_TAB(_dump_state) }, *p;
	char format[32],temp[1024];
	int flen;

	flen = 0;
	for(p=tab; p->field; p++) {
		if (strlen(p->field) > flen)
			flen = strlen(p->field);
	}
	flen++;
	sprintf(format,"%%%ds: %%s\n",flen);
	dprintf(dump_level,"pvinverter:\n");
	for(p=tab; p->field; p++) {
		if (p->cb) p->cb(inv,p->field,p->ptr,flen,(void *)&dump_level);
		else {
			conv_type(DATA_TYPE_STRING,&temp,sizeof(temp)-1,p->type,p->ptr,p->len);
			if (debug >= dump_level) printf(format,p->field,temp);
		}
	}
}

int pvinverter_from_json(solard_pvinverter_t *inv, char *str) {
	json_proctab_t pvinverter_tab[] = { PVINVERTER_TAB(_get_state) };
	json_value_t *v;

	v = json_parse(str);
	memset(inv,0,sizeof(*inv));
	json_to_tab(pvinverter_tab,v);
//	pvinverter_dump(inv,3);
	json_destroy_value(v);
	return 0;
};

json_value_t *pvinverter_to_json(solard_pvinverter_t *inv) {
	json_proctab_t pvinverter_tab[] = { PVINVERTER_TAB(_set_state) };

	return json_from_tab(pvinverter_tab);
}

#if 0
#ifdef JS
enum PVINVERTER_PROPERTY_ID {
	SOLARD_PVINVERTER_PROPIDS=1,
};

static JSBool pvinverter_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	int prop_id;
	solard_pvinverter_t *inv;

	inv = JS_GetPrivate(cx,obj);
	dprintf(4,"inv: %p\n", inv);
	if (!inv) {
		JS_ReportError(cx, "private is null!");
		return JS_FALSE;
	}

	dprintf(4,"is_int: %d\n", JSVAL_IS_INT(id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(4,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		SOLARD_PVINVERTER_GETPROP
		default:
			JS_ReportError(cx, "not a property");
			*rval = JSVAL_NULL;
		}
	}
	return JS_TRUE;
}

static JSClass pvinverter_class = {
	"pvinverter",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,
	JS_PropertyStub,
	pvinverter_getprop,
	JS_PropertyStub,
	JS_EnumerateStub,
	JS_ResolveStub,
	JS_ConvertStub,
	JS_FinalizeStub,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *JSInverter(JSContext *cx, solard_pvinverter_t *bp) {
	JSPropertySpec pvinverter_props[] = { 
		{0}
	};
	JSFunctionSpec pvinverter_funcs[] = {
		{ 0 }
	};
	JSObject *obj;

	dprintf(5,"defining %s object\n",pvinverter_class.name);
#if 0
	obj = JS_InitClass(cx, JS_GetGlobalObject(cx), 0, &pvinverter_class, 0, 0, pvinverter_props, pvinverter_funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize pvinverter class");
		return 0;
	}
#endif
	if ((obj = JS_DefineObject(cx, JS_GetGlobalObject(cx), pvinverter_class.name, &pvinverter_class, NULL, JSPROP_ENUMERATE)) == 0) return 0;
	if (!JS_DefineProperties(cx, obj, pvinverter_props)) return 0;
	if (!JS_DefineFunctions(cx, obj, pvinverter_funcs)) return 0;

	/* Pre-create the JSVALs */
	JS_SetPrivate(cx,obj,bp);
	dprintf(5,"done!\n");
	return obj;
}
#endif
#endif
