
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 4 
#include "debug.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "types.h"
#include "log.h"

#if TYPES_NEW_METHOD
static char *typenames[DATA_TYPE_MAX] = {
        "DATA_TYPE_UNKNOWN",
        "DATA_TYPE_NULL",
        "DATA_TYPE_VOIDP",
        "DATA_TYPE_MLIST",
        "DATA_TYPE_STRING",
        "DATA_TYPE_STRINGP",
        "DATA_TYPE_STRING_LIST",
        "DATA_TYPE_STRING_ARRAY",
        "DATA_TYPE_BOOL",
        "DATA_TYPE_BOOL_ARRAY",
        "DATA_TYPE_S8",
        "DATA_TYPE_S8_ARRAY",
        "DATA_TYPE_S16",
        "DATA_TYPE_S16_ARRAY",
        "DATA_TYPE_S32",
        "DATA_TYPE_S32_ARRAY",
        "DATA_TYPE_S64",
        "DATA_TYPE_S64_ARRAY",
        "DATA_TYPE_U8",
        "DATA_TYPE_U8_ARRAY",
        "DATA_TYPE_U16",
        "DATA_TYPE_U16_ARRAY",
        "DATA_TYPE_U32",
        "DATA_TYPE_U32_ARRAY",
        "DATA_TYPE_U64",
        "DATA_TYPE_U64_ARRAY",
        "DATA_TYPE_F32",
        "DATA_TYPE_F32_ARRAY",
        "DATA_TYPE_F64",
        "DATA_TYPE_F64_ARRAY",
        "DATA_TYPE_F128",
};

char *typestr(int type) {
	return (type >= 0 && type <= DATA_TYPE_MAX ? typenames[type] : "UNKNOWN");
}

static int typesizes[DATA_TYPE_MAX] = {
	0,			// DATA_TYPE_UNKNOWN,
	0,			// DATA_TYPE_NULL,
	0,			// DATA_TYPE_VOIDP,
	sizeof(void *),		// DATA_TYPE_MLIST,
	0,			// DATA_TYPE_STRING,
	0,			// DATA_TYPE_STRINGP,
	0,			// DATA_TYPE_STRING_LIST,
	0,			// DATA_TYPE_STRING_ARRAY,
	sizeof(int),		// DATA_TYPE_BOOL,
	0,			// DATA_TYPE_BOOL_ARRAY,
	sizeof(int8_t),		// DATA_TYPE_S8,
	0,			// DATA_TYPE_S8_ARRAY,
	sizeof(int16_t),	// DATA_TYPE_S16,
	0,			// DATA_TYPE_S16_ARRAY,
	sizeof(int32_t),	// DATA_TYPE_S32,
	0,			// DATA_TYPE_S32_ARRAY,
	sizeof(int64_t),        // DATA_TYPE_S64,
	0,			// DATA_TYPE_S64_ARRAY,
	sizeof(uint8_t),        // DATA_TYPE_U8,
	0,			// DATA_TYPE_U8_ARRAY,
	sizeof(uint16_t),       // DATA_TYPE_U16,
	0,			// DATA_TYPE_U16_ARRAY,
	sizeof(uint32_t),       // DATA_TYPE_U32,
	0,			// DATA_TYPE_U32_ARRAY,
	sizeof(uint64_t),	// DATA_TYPE_U64,
	0,			// DATA_TYPE_U64_ARRAY,
	sizeof(float),		// DATA_TYPE_F32,
	0,			// DATA_TYPE_F32_ARRAY,
	sizeof(double),		// DATA_TYPE_F64,
	0,			// DATA_TYPE_F64_ARRAY,
	sizeof(long double),	// DATA_TYPE_F128,
};

int typesize(int type) {
	return (type >= 0 && type <= DATA_TYPE_MAX ? typesizes[type] : 0);
}

#else
			
static struct _type_info {
        int type;
        char *name;
	int size;
} type_info[] = {
        { DATA_TYPE_UNKNOWN,"UNKNOWN",0 },
        { DATA_TYPE_NULL,"NULL",0 },
        { DATA_TYPE_VOID,"VOID",0 },
	{ DATA_TYPE_STRING,"STRING",0 },
        { DATA_TYPE_S8,"S8",1 },
        { DATA_TYPE_S16,"S16",sizeof(int16_t) },
        { DATA_TYPE_S32,"S32",sizeof(int32_t) },
        { DATA_TYPE_INT,"INT",sizeof(int) },
        { DATA_TYPE_LONG,"LONG",sizeof(long) },
        { DATA_TYPE_S64,"S64",sizeof(int64_t) },
        { DATA_TYPE_QUAD,"QUAD",sizeof(long long) },
        { DATA_TYPE_U8,"U8",1 },
        { DATA_TYPE_U16,"U16",sizeof(uint16_t) },
        { DATA_TYPE_U32,"U32",sizeof(uint32_t) },
        { DATA_TYPE_U64,"U64",sizeof(uint64_t) },
        { DATA_TYPE_F32,"F32",sizeof(float) },
        { DATA_TYPE_FLOAT,"FLOAT",sizeof(float) },
        { DATA_TYPE_F64,"F64",sizeof(double) },
        { DATA_TYPE_DOUBLE,"DOUBLE",sizeof(double) },
        { DATA_TYPE_F128,"F128",sizeof(long double) },
        { DATA_TYPE_BOOL,"BOOL",sizeof(int) },
        { DATA_TYPE_STRINGP,"STRING PTR",sizeof(void *) },
        { DATA_TYPE_VOIDP,"VOID PTR",0 },
        { DATA_TYPE_S8P,"S8 PTR",0 },
        { DATA_TYPE_S16P,"S16 PTR",0 },
        { DATA_TYPE_S32P,"S32 PTR",0 },
        { DATA_TYPE_S64P,"S64 PTR",0 },
        { DATA_TYPE_U8P,"U8 PTR",0 },
        { DATA_TYPE_U16P,"U16 PTR",0 },
        { DATA_TYPE_U32P,"U32 PTR",0 },
        { DATA_TYPE_U64P,"U64 PTR",0 },
        { DATA_TYPE_F32P,"F32 PTR",0 },
        { DATA_TYPE_F64P,"F64 PTR",0 },
        { DATA_TYPE_F128P,"F128 PTR",0 },
        { DATA_TYPE_BOOLP,"BOOL PTR",0 },
        { DATA_TYPE_S8_ARRAY,"S8_ARRAY",0 },
        { DATA_TYPE_S16_ARRAY,"S16_ARRAY",0 },
        { DATA_TYPE_S32_ARRAY,"S32_ARRAY",0 },
        { DATA_TYPE_S64_ARRAY,"S64_ARRAY",0 },
        { DATA_TYPE_U8_ARRAY,"U8_ARRAY",0 },
        { DATA_TYPE_U16_ARRAY,"U16_ARRAY",0 },
        { DATA_TYPE_U32_ARRAY,"U32_ARRAY",0 },
        { DATA_TYPE_U64_ARRAY,"U64_ARRAY",0 },
        { DATA_TYPE_F32_ARRAY,"F32_ARRAY",0 },
        { DATA_TYPE_F64_ARRAY,"F64_ARRAY",0 },
        { DATA_TYPE_F128_ARRAY,"F128_ARRAY",0 },
        { DATA_TYPE_BOOL_ARRAY,"BOOL_ARRAY",0 },
        { DATA_TYPE_STRING_ARRAY,"STRING_ARRAY",0 },
        { DATA_TYPE_STRING_LIST,"STRING_LIST",0 },
        { DATA_TYPE_NUMBER,"NUMBER",0 },
        { DATA_TYPE_ARRAY,"ARRAY",0 },
        { DATA_TYPE_MLIST,"MLIST",0 },
        { 0, 0 }
};
#define NUM_TYPES (sizeof(type_info)/sizeof(struct _type_info))

char *typestr(int type) {
	struct _type_info *t;

	for(t = type_info; t->name; t++) {
		break;
		if (t->type == type)
			return t->name;
	}
	return "";
}

int typesize(int type) {
	register struct _type_info *t;

	for(t = type_info; t->name; t++) {
		if (t->type == type)
			return t->size;
	}
        return(0);
}

int name2dt(char *name) {
	register struct _type_info *t;

	for(t = type_info; t->name; t++) {
		if (strcmp(t->name,name) == 0)
			return t->type;
        }
        return DATA_TYPE_UNKNOWN;
}
#endif

#ifdef JS
#include "jsapi.h"
#include "jsengine.h"
#include "jsstr.h"
static JSBool jstypes_typestr(JSContext *cx, uintN argc, jsval *vp) {
	int type;
	char *str;
	jsval *argv = vp + 2;

	if (argc != 1) {
		JS_ReportError(cx, "typestr: 1 argument required (type:number)");
		return JS_FALSE;
	}
	type = JSVAL_TO_INT(argv[0]);
//dprintf(-1,"type: %x\n", type);
	str = typestr(type);
//dprintf(-1,"typestr: %s\n", str);
	*vp = type_to_jsval(cx, DATA_TYPE_STRING, str, strlen(str));
	return JS_TRUE;
}

int js_types_init(JSContext *cx, JSObject *parent, void *priv) {
	JSObject *obj = JS_GetGlobalObject(cx);
//	char *name;
//	int namesz;
//	char temp[64];
	JSBool ok;
	int i,value;
//	int count;
	struct _alias_info {
		char *name;
		int value;
	} aliases[] = {
		{ "DATA_TYPE_VOID",DATA_TYPE_NULL },
		{ "DATA_TYPE_BYTE",DATA_TYPE_S8 },
		{ "DATA_TYPE_BYTE_ARRAY",DATA_TYPE_S8_ARRAY },
		{ "DATA_TYPE_SHORT",DATA_TYPE_S16 },
		{ "DATA_TYPE_SHORT_ARRAY",DATA_TYPE_S16_ARRAY },
		{ "DATA_TYPE_INT",DATA_TYPE_S32 },
		{ "DATA_TYPE_INT_ARRAY",DATA_TYPE_S32_ARRAY },
		{ "DATA_TYPE_LONG",DATA_TYPE_S32 },
		{ "DATA_TYPE_LONG_ARRAY",DATA_TYPE_S32_ARRAY },
		{ "DATA_TYPE_QUAD",DATA_TYPE_S64 },
		{ "DATA_TYPE_QUAD_ARRAY",DATA_TYPE_S64_ARRAY },
		{ "DATA_TYPE_UBYTE",DATA_TYPE_U8 },
		{ "DATA_TYPE_USHORT",DATA_TYPE_U16 },
		{ "DATA_TYPE_UINT",DATA_TYPE_U32 },
		{ "DATA_TYPE_ULONG",DATA_TYPE_U32 },
		{ "DATA_TYPE_UQUAD",DATA_TYPE_U64 },
		{ "DATA_TYPE_FLOAT",DATA_TYPE_F32 },
		{ "DATA_TYPE_DOUBLE",DATA_TYPE_F64 },
		{ "DATA_TYPE_BOOLEAN",DATA_TYPE_BOOL },
		{ "DATA_TYPE_LOGICAL",DATA_TYPE_BOOL },
		{ "DATA_TYPE_FLOAT_ARRAY",DATA_TYPE_F32_ARRAY },
		{ "DATA_TYPE_DOUBLE_ARRAY",DATA_TYPE_F64_ARRAY },
		{ 0, 0 }
	};
	struct _alias_info *ap;

	JSFunctionSpec funcs[] = {
		JS_FN("typestr",jstypes_typestr,1,1,0),
		{ 0 }
	};
//	JSConstantSpec *consts;

	if (!JS_DefineFunctions(cx, obj, funcs)) {
		dprintf(1,"error defining types funcs\n");
		return 1;
	}

#if TYPES_NEW_METHOD
	for(i=0; i < DATA_TYPE_MAX; i++) {
		/* all floats are double */
		value = (i == DATA_TYPE_FLOAT ? DATA_TYPE_DOUBLE : i);
                ok = JS_DefineProperty(cx, obj, typenames[i], INT_TO_JSVAL(value), 0, 0,
                        JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT|JSPROP_EXPORTED);
		if (!ok) {
			log_error("js_types_init: define %s(%d)\n",typenames[i],i);
			return 1;
		}
        }
	for(ap = aliases; ap->name; ap++) {
                ok = JS_DefineProperty(cx, obj, ap->name, INT_TO_JSVAL(ap->value), 0, 0,
                        JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT|JSPROP_EXPORTED);
		if (!ok) {
			log_error("js_types_init: define %s(%d)\n",ap->name,ap->value);
			return 1;
		}
	}

#else
	for(i=count=0; type_info[i].name; i++) count++;
	consts = malloc((count+1)*sizeof(JSConstantSpec));
	if (!consts) {
		log_syserror("JSTypes: malloc");
		return 1;
	}
#define PREFIX "DATA_TYPE_"
	for(i=0; i < count; i++) {
		snprintf(temp,sizeof(temp)-1,"%s%s",PREFIX,type_info[i].name);
//		dprintf(0,"temp: %s\n", temp);
		consts[i].name = JS_EncodeString(cx, JS_InternString(cx,temp));
		if (type_info[i].type == DATA_TYPE_FLOAT || type_info[i].type == DATA_TYPE_F32)
			consts[i].value = NUMBER_TO_JSVAL(DATA_TYPE_DOUBLE);
		else
			consts[i].value = NUMBER_TO_JSVAL(type_info[i].type);
	}
	consts[i].name = 0;
	if(!JS_DefineConstants(cx, obj, consts)) {
		dprintf(1,"error defining types constants\n");
		return 1;
	}
	for(i=0; i < count; i++) JS_free(cx,(void *)consts[i].name);
	free(consts);
#endif
	return 0;
}
#if 0
int types_jsinit(void *e) {
	return JS_EngineAddInitFunc((JSEngine *)e, "types", jstypes_init, 0);
}
#endif
#endif
