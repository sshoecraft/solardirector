
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __SOLARD_JSON_H
#define __SOLARD_JSON_H

#include "cfg.h"
#include "types.h"

struct json_value;
typedef struct json_value json_value_t;

typedef void (json_ptcallback_t)(void *,char *,void *,int,json_value_t *);
struct json_proctab {
	char *field;
	int type;
	void *ptr;
	int len;
	json_ptcallback_t *cb;
	void *ctx;
};
typedef struct json_proctab json_proctab_t;

#define JSON_PROCTAB_END { 0, 0, 0, 0, 0 }

/* All of these Must match underlying lib */
struct json_object {
    json_value_t  *wrapping_value;
    size_t        *cells;
    unsigned long *hashes;
    char         **names;
    json_value_t  **values;
    size_t        *cell_ixs;
    size_t         count;
    size_t         item_capacity;
    size_t         cell_capacity;
};
typedef struct json_object json_object_t;

struct json_array {
    json_value_t *wrapping_value;
    json_value_t **items;
    size_t       count;
    size_t       capacity;
};
typedef struct json_array json_array_t;

enum JSON_TYPE {
	JSON_TYPE_NULL    = 1,
	JSON_TYPE_STRING  = 2,
	JSON_TYPE_NUMBER  = 3,
	JSON_TYPE_OBJECT  = 4,
	JSON_TYPE_ARRAY   = 5,
	JSON_TYPE_BOOLEAN = 6
};

char *json_typestr(int);
json_value_t *json_parse(char *);
json_value_t *json_parse_file(char *filename);

/* Values */
enum JSON_TYPE json_value_get_type(json_value_t *);
char *json_value_get_string(json_value_t *);
double json_value_get_number(json_value_t *);
json_object_t *json_value_object(json_value_t *);
json_object_t *json_value_toobject(json_value_t *);
json_array_t *json_value_array(json_value_t *);
json_array_t *json_value_toarray(json_value_t *);
int json_value_get_boolean(json_value_t *);
int json_destroy_value(json_value_t *);
json_value_t *json_value_dotget_value(json_value_t *, char *);

/* Objects */
json_object_t *json_create_object(void);
json_value_t *json_object_value(json_object_t *);
char *json_object_get_string(json_object_t *,char *);
int json_object_get_boolean(json_object_t *,char *,int);
double json_object_get_number(json_object_t *,char *);
json_object_t *json_object_get_object(json_object_t *,char *);
json_array_t *json_object_get_array(json_object_t *,char *);
json_value_t *json_object_get_value(json_object_t *,char *);
char *json_object_dotget_string(json_object_t *,char *);
int json_object_dotget_boolean(json_object_t *,char *);
double json_object_dotget_number(json_object_t *,char *);
json_value_t *json_object_dotget_value(json_object_t *, char *);
json_object_t *json_object_dotget_object(json_object_t *o,char *n);
json_array_t *json_object_dotget_array(json_object_t *o,char *n);
int json_object_delete_value(json_object_t *, char *);
int json_object_add_string(json_object_t *,char *,char *);
int json_object_set_string(json_object_t *,char *,char *);
int json_object_set_boolean(json_object_t *,char *,int);
int json_object_set_number(json_object_t *,char *,double);
int json_object_set_object(json_object_t *,char *,json_object_t *);
int json_object_set_array(json_object_t *,char *,json_array_t *);
int json_object_set_value(json_object_t *,char *,json_value_t *);
int json_object_add_array(json_object_t *o,char *n,json_array_t *a);
int json_object_add_value(json_object_t *o,char *n,json_value_t *v);
//double json_object_dotget_number(json_object_t *, char *);
int json_object_dotset_value(json_object_t *o, char *label, json_value_t *v);
int json_combine_objects(json_object_t *,json_object_t *);

int json_object_remove(json_object_t *, char *);
int json_destroy_object(json_object_t *);

/* Arrays */
json_array_t *json_create_array(void);
json_value_t *json_array_get_value(json_array_t *);
int json_array_add_string(json_array_t *,char *);
int json_array_add_strings(json_array_t *,char *);
int json_array_add_boolean(json_array_t *,int);
int json_array_add_number(json_array_t *,double);
int json_array_add_object(json_array_t *,json_object_t *);
int json_array_add_array(json_array_t *,json_array_t *);
int json_array_add_value(json_array_t *,json_value_t *);
int json_array_remove(json_object_t *, int);
int json_destroy_array(json_array_t *);
int json_combine_arrays(json_array_t *,json_array_t *);

int json_tostring(json_value_t *,char *,int,int);
char *json_dumps(json_value_t *,int);
int json_dumps_r(json_value_t *,char *,int);
//const char *json_string(const JSON_Value *value);
void json_free_serialized_string(char *string);
#define json_free_string(s) json_free_serialized_string(s)

int json_to_type(int, void *, int, json_value_t *);
json_value_t *json_from_type(int, void *, int);

json_value_t *json_from_tab(json_proctab_t *);
int json_to_tab(json_proctab_t *, json_value_t *);
int json_string_from_tab(char *dest, int dsize, json_proctab_t *tab, int pretty);

typedef int (json_ifunc_t)(void *,char *,char *);
int json_iter(char *name, json_value_t *v, json_ifunc_t *func, void *ctx);


//json_value_t *json_from_cfgtab(cfg_proctab_t *);

#ifdef JS
JSObject *js_json_toObject(JSContext *cx, JSObject *parent, json_value_t *v);
JSObject *js_InitmyJSONClass(JSContext *cx, JSObject *parent);
jsval json2jsval(json_value_t *v, JSContext *cx);
#endif /* JS */
#endif /* __SOLARD_JSON_H */
