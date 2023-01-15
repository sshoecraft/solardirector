
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __CONFIG_H
#define __CONFIG_H

#include "cfg.h"
#include "json.h"

enum CONFIG_FILE_FORMAT {
	CONFIG_FILE_FORMAT_AUTO,
	CONFIG_FILE_FORMAT_INI,
	CONFIG_FILE_FORMAT_JSON,
	CONFIG_FILE_FORMAT_CUSTOM,
};

#define CONFIG_FLAG_READONLY	0x0001	/* Do not update */
#define CONFIG_FLAG_NOSAVE	0x0002	/* Do not save */
#define CONFIG_FLAG_NOID	0x0004	/* Do not assign an ID */
#define CONFIG_FLAG_FILE	0x0008	/* Exists in storage (file/mqtt) */
#define CONFIG_FLAG_FILEONLY	0x0010	/* Exists in file only (not visible by mqtt/js) */
#define CONFIG_FLAG_ALLOC	0x0020	/* Property is malloc'd */
#define CONFIG_FLAG_NOPUB	0x0040	/* Do not publish */
#define CONFIG_FLAG_NODEF	0x0080	/* Do set default */
#define CONFIG_FLAG_ALLOCDEST	0x0100	/* Just dest is allocated */
#define CONFIG_FLAG_NOINFO	0x0200	/* Do not include in info */
#define CONFIG_FLAG_PUB		0x0400	/* Publish (one time) */
#define CONFIG_FLAG_NOWARN	0x1000	/* Don't warn dup props */
#define CONFIG_FLAG_NOTRIG	0x2000	/* Don't Call trigger func during addprop */
#define CONFIG_FLAG_VALUE	0x4000	/* Value has been set */
#define CONFIG_FLAG_PRIVATE	(CONFIG_FLAG_NOSAVE | CONFIG_FLAG_NOPUB | CONFIG_FLAG_NOINFO)

struct config_property;
typedef struct config_property config_property_t;
typedef int (config_trigger_func_t)(void *ctx, config_property_t *p, void *old_value);

struct config;
struct config_property {
	char *name;
	int type;			/* Data type */
	void *dest;			/* Ptr to storage */
	int dsize;			/* Size of storage */
	void *def;			/* Default value */
	uint16_t flags;			/* Flags */
	char *scope;			/* Scope (select/range/etc) */
	char *values;			/* Scope values */
	char *labels;			/* Labels */
	char *units;			/* Units */
	float scale;			/* Scale for numerics */
	int precision;			/* Decimal places */
	config_trigger_func_t *trigger;	/* Function to call when prop is updated */
	void *ctx;			/* Context to pass function */
	/* Not exposed */
	int len;			/* actual length of storage */
	int id;				/* Property ID */
	int dirty;			/* Has been updated since last write */
#ifdef JS
	jsval jsval;			/* JSVal of this property object */
	jsval arg;			/* Trigger arg */
#endif
	struct config *cp;		/* backlink to config */
};

typedef int (config_funccall_t)(void *ctx, list args, char *errmsg, json_object_t *results);
struct config_function {
	char *name;
	config_funccall_t *func;
	void *ctx;
	int nargs;
#ifdef JS
	jsval jsval;
#endif
};
typedef struct config_function config_function_t;

#define CONFIG_SECTION_NAME_SIZE 64
struct config_section {
	char name[CONFIG_SECTION_NAME_SIZE];
	list items;
	time_t items_updated;
	uint32_t flags;
#ifdef JS
	jsval jsval;			/* JSVal of this property object */
#endif
};
typedef struct config_section config_section_t;

struct config;
typedef int (config_rw_t)(void *ctx);

#ifdef JS
struct config_rootinfo {
	JSContext *cx;
	void *vp;
	char *name;
};
#endif

#define CONFIG_FILENAME_SIZE 256
struct config {
	int id;
	list sections;
	list funcs;
	char filename[CONFIG_FILENAME_SIZE];
	enum CONFIG_FILE_FORMAT file_format;
	config_rw_t *reader;
	void *reader_ctx;
	config_rw_t *writer;
	void *writer_ctx;
	char errmsg[256];
	config_property_t **map;		/* ID map */
	int map_maxid;
	union {
		cfg_info_t *cfg;
		json_value_t *v;
	};
	int dirty;
	int triggers;
	config_trigger_func_t *trigger;	/* Function to call when config is updated */
	void *trigger_ctx;		/* Context to pass function */
#ifdef JS
	list roots;
	list fctx;
#endif
};
typedef struct config config_t;

config_t *config_init(char *, config_property_t *,config_function_t *);
char *config_get_errmsg(config_t *);
config_property_t *config_combine_props(config_property_t *p1, config_property_t *p2);
void config_add_props(config_t *, char *, config_property_t *, int flags);
config_property_t *config_get_props(config_t *, char *);
config_function_t *config_combine_funcs(config_function_t *f1, config_function_t *f2);
void config_add_funcs(config_t *, config_function_t *);
list config_get_funcs(config_t *);
int config_add_info(config_t *, json_object_t *);

int config_load_json(config_t *cp, char *data);
int config_parse_json(config_t *cp, json_value_t *v);
config_section_t *config_get_section(config_t *, char *);
int config_read(config_t *cp);
int config_write(config_t *cp);
int config_set_filename(config_t *, char *, enum CONFIG_FILE_FORMAT);
int config_read_file(config_t *cp, char *filename);
int config_set_reader(config_t *, config_rw_t *, void *);
int config_set_writer(config_t *, config_rw_t *, void *);

config_property_t *config_section_dup_property(config_section_t *, config_property_t *, char *);

config_section_t *config_get_section(config_t *,char *);
config_section_t *config_create_section(config_t *,char *,int);
config_property_t *config_get_property(config_t *cp, char *sname, char *name);
int config_delete_property(config_t *cp, char *sname, char *name);
int config_set_property(config_t *cp, char *sname, char *name, int type, void *src, int len);
config_property_t *config_section_get_property(config_section_t *s, char *name);
int config_section_get_properties(config_section_t *s, config_property_t *props);
int config_section_set_property(config_section_t *s, config_property_t *p);
int config_section_set_properties(config_t *cp, config_section_t *s, config_property_t *props, int add, int flags);
int config_section_add_property(config_t *cp, config_section_t *s, config_property_t *p, int flags);
config_property_t *config_section_add_props(config_t *cp, config_section_t *s, config_property_t *p);

config_property_t *config_new_property(config_t *cp, char *name, int type, void *dest, int dsize, int len, char *def, int flags, char *scope, char *values, char *labels, char *units, float scale, int precision);
int config_property_set_value(config_property_t *p, int type, void *src, int size, int dirty, int trigger);

config_function_t *config_function_get(config_t *, char *name);
int config_function_set(config_t *, char *name, config_function_t *);

json_value_t *config_to_json(config_t *cp, int noflags, int dirty);
int config_from_json(config_t *cp, json_value_t *v);
int config_add(config_t *cp,char *section, char *label, char *value);
int config_del(config_t *cp,char *section, char *label, char *value);
int config_get(config_t *cp,char *section, char *label, char *value);
//int config_set(config_t *cp,char *section, char *label, char *value);
//int config_pub(config_t *cp, mqtt_session_t *m);
config_property_t *config_find_property(config_t *cp, char *name);
config_property_t *config_get_propbyid(config_t *cp, int);

//typedef int (config_process_callback_t)(void *,char *,char *,char *,char *);
int config_process_request(config_t *cp, char *req, json_object_t *res);

void config_build_propmap(config_t *cp);
void config_dump_property(config_property_t *p, int level);
void config_dump(config_t *cp);
void config_dump_props(config_property_t *props);
void config_dump_function(config_function_t *f, int level);

void config_destroy_props(config_property_t *);
void config_destroy_funcs(config_function_t *);
void config_destroy(config_t *cp);

int config_service_get_value(void *ctx, list args, char *errmsg, json_object_t *results);
int config_service_set_value(void *ctx, list args, char *errmsg, json_object_t *results);
int config_service_clear_value(void *ctx, list args, char *errmsg, json_object_t *results);

#define CONFIG_GETMAP(cp,id) ((cp && cp->map && id >= 0 && id < cp->map_maxid && cp->map[id]) ? cp->map[id] : 0)

int js_config_calltrigger(void *ctx, config_property_t *p);

#ifdef JS
#include "jsapi.h"
#include "jsengine.h"
JSObject * js_InitConfigClass(JSContext *cx, JSObject *global_object);
JSPropertySpec *js_config_to_props(config_t *cp, JSContext *cx, char *name, JSPropertySpec *add);
JSBool js_config_callfunc(config_t *cp, JSContext *cx, uintN argc, jsval *vp);
typedef JSBool (js_config_callfunc_t)(JSContext *cx, uintN argc, jsval *vp);
JSFunctionSpec *js_config_to_funcs(config_t *cp, JSContext *cx, js_config_callfunc_t *func, JSFunctionSpec *add);
JSBool js_config_common_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval, config_t *, JSPropertySpec *);
JSBool js_config_common_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval, config_t *, JSPropertySpec *);
JSObject *js_config_new(JSContext *cx, JSObject *parent, config_t *cp);
config_property_t *js_config_obj2props(JSContext *cx, JSObject *obj, JSObject *addto);
config_function_t *js_config_obj2funcs(JSContext *cx, JSObject *obj);
JSBool js_config_add_props(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool js_config_add_funcs(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
int config_jsinit(JSEngine *e);
#endif

#endif
