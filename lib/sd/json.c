
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 6
#include "debug.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "json.h"
#include "utils.h"

#include "parson.c"


#define VAL(v) ((JSON_Value *)(v))
#define OBJ(o) ((JSON_Object *)(o))
#define OBJV(o) parson_object_get_wrapping_value(OBJ(o))
#define ARR(a) ((JSON_Array *)(a))
#define ARRV(a) parson_array_get_wrapping_value(ARR(a))

char *json_typestr(int type) {
	char *typenames[] = { "None","Null","String","Number","Object","Array","Boolean" };
	if (type < JSONNull || type > JSONBoolean) return "unknown";
	return typenames[type];
}
json_value_t *json_parse(char *str) {
	return (json_value_t *)parson_parse_string(str);
}

json_value_t *json_parse_file(char *filename) {
	return (json_value_t *)parson_parse_file(filename);
}

/* Values */
enum JSON_TYPE json_value_get_type(json_value_t *v) { return(VAL(v)->type); }
char *json_value_get_string(json_value_t *v) { return (char *)parson_string(VAL(v)); }
double json_value_get_number(json_value_t *v) { return parson_number(VAL(v)); }
json_object_t *json_value_object(json_value_t *v) { return (json_object_t *)parson_object(VAL(v)); }
json_object_t *json_value_toobject(json_value_t *v) { return (json_object_t *)parson_object(VAL(v)); }
json_array_t *json_value_array(json_value_t *v) { return (json_array_t *) parson_array(VAL(v)); }
int json_value_get_boolean(json_value_t *v) { return parson_boolean(VAL(v)); }
int json_destroy_value(json_value_t *v);

json_value_t *json_value_dotget_value(json_value_t *v, char *spec) {
	json_object_t *o;
	json_array_t *a;
	int i,type;

	if (!spec) return v;
	dprintf(dlevel,"spec: %s\n", spec);
	type = json_value_get_type(v);
	dprintf(dlevel,"type: %s\n", json_typestr(type));
	switch(type) {
	case JSON_TYPE_OBJECT:
		o = json_value_object(v);
		for(i=0; i < o->count; o++) {
			dprintf(dlevel,"type{%d]: %s\n", i, json_typestr(json_value_get_type(o->values[i])));
		}
		break;
	case JSON_TYPE_ARRAY:
		a = json_value_array(v);
		for(i=0; i < a->count; o++) {
			dprintf(dlevel,"type{%d]: %s\n", i, json_typestr(json_value_get_type(a->items[i])));
		}
		break;
	}
	return 0;
}

/* Objects */
/*****************************************************/
/* json_object */
json_object_t *json_create_object(void);
json_value_t *json_object_value(json_object_t *o) { return (json_value_t *)OBJV(o); }
json_value_t *json_object_tovalue(json_object_t *o) { return (json_value_t *)OBJV(o); }
char *json_object_get_string(json_object_t *o,char *name) { return (char *)parson_object_get_string(OBJ(o),name); }
int json_object_get_boolean(json_object_t *,char *,int);
double json_object_get_number(json_object_t *o,char *n) { return parson_object_get_number(OBJ(o),n); }
json_object_t *json_object_get_object(json_object_t *o,char *n) { return (json_object_t *)parson_object_get_object(OBJ(o),n); }
json_array_t *json_object_get_array(json_object_t *o,char *n) { return (json_array_t *)parson_object_get_array(OBJ(o),n); }
json_value_t *json_object_get_value(json_object_t *o,char *n) { return (json_value_t *)parson_object_get_value(OBJ(o),n); }
char *json_object_dotget_string(json_object_t *o,char *n) { return (char *)parson_object_dotget_string(OBJ(o),n); }
int json_object_dotget_boolean(json_object_t *o,char *n) { return parson_object_dotget_boolean(OBJ(o),n); }
double json_object_dotget_number(json_object_t *o,char *n) { return parson_object_dotget_number(OBJ(o),n); }
json_object_t *json_object_dotget_object(json_object_t *o,char *n) {
	return (json_object_t *)parson_object_dotget_object(OBJ(o), n);
}
json_array_t *json_object_dotget_array(json_object_t *o,char *n) {
	return (json_array_t *)parson_object_dotget_array(OBJ(o), n);
}
json_value_t *json_object_dotget_value(json_object_t *o, char *n) {
	return (json_value_t *)parson_object_dotget_value(OBJ(o), n);
}

int json_object_delete_value(json_object_t *o, char *name) {
	return parson_object_remove_internal(OBJ(o), name, 1);
}
int json_object_set_string(json_object_t *o,char *name,char *value) {
	return parson_object_set_string(OBJ(o),name,value);
}
int json_object_add_string(json_object_t *o,char *name,char *value) {
	return parson_object_set_string(OBJ(o),name,value);
}
int json_object_set_boolean(json_object_t *,char *,int);
int json_object_set_number(json_object_t *,char *,double);
int json_object_set_object(json_object_t *o,char *name,json_object_t *no) {
	return parson_object_set_value(OBJ(o),name,OBJV(no));
}
int json_object_add_array(json_object_t *o,char *n,json_array_t *a) {
	return parson_object_add(OBJ(o),n,ARRV(a));
}
int json_object_add_value(json_object_t *o,char *n,json_value_t *v) {
	return parson_object_add(OBJ(o),n,VAL(v));
}
int json_object_set_array(json_object_t *,char *,json_array_t *);
int json_object_set_value(json_object_t *,char *,json_value_t *);
int json_destroy_object(json_object_t *);
int json_object_dotset_value(json_object_t *o, char *label, json_value_t *v) {
	return parson_object_dotset_value((JSON_Object *)o, label, (JSON_Value *)v);
}
int json_object_set_number(json_object_t *o, char *name, double value) {
	dprintf(dlevel,"object: %p, name: %s, value: %f\n", o, name, value);
	return parson_object_set_number(OBJ(o), name, value);
}

int json_object_set_boolean(json_object_t *o, char *name, int value) {
	dprintf(dlevel,"object: %p, name: %s, value: %d\n", o, name, value);
	return parson_object_set_boolean(OBJ(o), name, value);
}

int json_object_set_value(json_object_t *o, char *name, json_value_t *value) {
//	XXX json_object_set_value does not copy passed value so it shouldn't be freed afterwards.
	dprintf(dlevel,"object: %p, name: %s, value: %p\n", o, name, value);
	return parson_object_set_value(OBJ(o), name, VAL(value));
}

int json_object_set_array(json_object_t *o, char *name, json_array_t *a) {
	return parson_object_set_value(OBJ(o), name, ARRV(a));
}

int json_combine_objects(json_object_t *o1, json_object_t *o2) {
	int i;

	for(i=0; i < o2->count; i++) json_object_set_value(o1,o2->names[i],o2->values[i]);
	return 0;
}

/* Arrays */
json_array_t *json_create_array(void);
json_value_t *json_array_get_value(json_array_t *a) { return (json_value_t *)ARRV(a); }
int json_array_add_string(json_array_t *,char *);
int json_array_add_strings(json_array_t *,char *);
int json_array_add_boolean(json_array_t *,int);
int json_array_add_number(json_array_t *,double);
int json_array_add_object(json_array_t *,json_object_t *);
int json_array_add_array(json_array_t *a,json_array_t *aa) {
	return parson_array_add(ARR(a),ARRV(aa));
}
int json_array_add_value(json_array_t *,json_value_t *);
int json_destroy_array(json_array_t *);

json_object_t *json_create_object(void) {
	return (json_object_t *)parson_object(parson_value_init_object());
}

json_array_t *json_create_array(void) {
	return (json_array_t *)parson_array(parson_value_init_array());
}

int json_destroy_value(json_value_t *v) {
	parson_value_free((JSON_Value *)v);
	return 0;
}

int json_destroy_object(json_object_t *o) {
	parson_value_free(parson_object_get_wrapping_value((JSON_Object *)o));
	return 0;
}

int json_destroy_array(json_array_t *a) {
	parson_value_free(parson_array_get_wrapping_value((JSON_Array *)a));
	return 0;
}


#if 0
int json_add_string(json_value_t *o, char *name, char *value) {
	dprintf(dlevel,"object: %p, name: %s, value: %s\n", o, name, value);
	return json_object_set_string(json_object(o), name, value);
}

int json_add_number(json_value_t *o, char *name, double value) {
	dprintf(dlevel,"object: %p, name: %s, value: %f\n", o, name, value);
	return json_object_set_number(json_object(o), name, value);
}

int json_add_boolean(json_value_t *o, char *name, int value) {
	dprintf(dlevel,"object: %p, name: %s, value: %d\n", o, name, value);
	return json_object_set_boolean(json_object(o), name, value);
}

int json_add_value(json_value_t *v, char *name, json_value_t *value) {
//	XXX json_object_set_value does not copy passed value so it shouldn't be freed afterwards.
	dprintf(dlevel,"object: %p, name: %s, value: %p\n", v, name, value);
	return json_object_set_value(json_object(v), name, value);
}

int json_add_array(json_value_t *v, char *name, json_value_t *array) {
	return json_object_set_value(json_object(v), name, array);
}

int json_add_list(json_object_t *j, char *label, list values) {
	JSON_Value *value;
	JSON_Array *array;
	char *p;

	value = json_value_init_array();
	array = json_array(value);
	list_reset(values);
	while((p = list_get_next(values)) != 0) {
		dprintf(dlevel,"adding: %s\n", p);
		parson_array_append_string(array,p);
	}
	return 0;
}

#endif


/*****************************************************/
/* json_array */
int json_array_add_string(json_array_t *a,char *value) {
	return parson_array_append_string(ARR(a),value);
}

int json_array_add_strings(json_array_t *a,char *value) {
	register char *p;
	char temp[128];
	int i;

	i = 0;
	for(p = value; *p; p++) {
		if (*p == ',') {
			dprintf(dlevel,"temp: %s\n", temp);
			parson_array_append_string(ARR(a),temp);
			i = 0;
		} else
			temp[i++] = *p;
	}
	if (i > 0) {
		temp[i] = 0;
		dprintf(dlevel,"temp: %s\n", temp);
		parson_array_append_string(ARR(a),temp);
	}
	return 0;
}

int json_array_add_number(json_array_t *a,double value) {
	return parson_array_append_number(ARR(a),value);
}

int json_array_add_value(json_array_t *a,json_value_t *v) {
	return parson_array_append_value(ARR(a),VAL(v));
}

int json_array_add_object(json_array_t *a,json_object_t *o) {
	return parson_array_append_value(ARR(a),OBJV(o));
}

/*****************************************************/
int json_tostring(json_value_t *j, char *dest, int dest_len, int pretty) {
	return pretty ? parson_serialize_to_buffer_pretty(VAL(j),dest,dest_len) : parson_serialize_to_buffer(VAL(j),dest,dest_len);
}

char *json_dumps(json_value_t *o, int pretty) {
	return (pretty ? parson_serialize_to_string_pretty(VAL(o)) : parson_serialize_to_string(VAL(o)));
}

int json_dumps_r(json_value_t *v, char *buf, int buflen) {
	return parson_serialize_to_buffer(VAL(v),buf,buflen);
}

int json_string_from_tab(char *dest, int dsize, json_proctab_t *tab, int pretty) {
	json_proctab_t *tp;
	char *p;
	int remain,n,count;

	dprintf(dlevel,"dest: %p, dsize: %d, tab: %p, pretty: %d\n", dest, dsize, tab, pretty);
	if (!dest || !tab) return -1;

	p = dest;
	remain = dsize;
#define OUT(fmt,...) { n = snprintf(p,remain,fmt,##__VA_ARGS__); p += n; remain -= n; }
	OUT("{");
	count = 0;
	for(tp = tab; tp->field; tp++) {
		dprintf(dlevel,"type: %s\n", typestr(tp->type));
		if (count) OUT(",");
		if (pretty) OUT("\n    ");
		OUT("\"%s\":",tp->field);
		switch(tp->type) {
		case DATA_TYPE_STRING:
//			n = snprintf(p,remain,"%s",(char *)tp->ptr);
			OUT("\"%s\"",(char *)tp->ptr);
			break;
		case DATA_TYPE_INT:
			OUT("%d",*((int *)tp->ptr));
			break;
		case DATA_TYPE_BOOLEAN:
			OUT("%d",*((int *)tp->ptr));
			break;
		case DATA_TYPE_FLOAT:
			OUT("%f",*((float *)tp->ptr));
			break;
		case DATA_TYPE_DOUBLE:
			OUT("%lf",*((double *)tp->ptr));
			break;
		default:
			log_error("json_string_from_tab: unhandled type: %s\n", json_typestr(tp->type));
			n = 0;
			break;
		}
		count++;
	}
	OUT("%s}",pretty ? "\n" : "");
	return dsize - remain;
}

json_value_t *json_from_tab(json_proctab_t *tab) {
	json_proctab_t *p;
	json_object_t *o;
	int *ip;
	bool *bp;
	float *fp;
	double *dp;

	o = json_create_object();
	if (!o) return 0;
	for(p=tab; p->field; p++) {
		dprintf(dlevel,"p: field: %s, ptr: %p, len: %d, cb: %p\n", p->field, p->ptr, p->len, p->cb);
		if (p->cb) p->cb(p->ctx, p->field,p->ptr,p->len,json_object_value(o));
		else {
			dprintf(dlevel,"p->type: %d\n", p->type);
			switch(p->type) {
			case DATA_TYPE_STRING:
				json_object_set_string(o,p->field,p->ptr);
				break;
			case DATA_TYPE_INT:
				ip = p->ptr;
				json_object_set_number(o,p->field,*ip);
				break;
			case DATA_TYPE_BOOLEAN:
				bp = p->ptr;
				json_object_set_boolean(o,p->field,*bp);
				break;
			case DATA_TYPE_FLOAT:
				fp = p->ptr;
				json_object_set_number(o,p->field,*fp);
				break;
			case DATA_TYPE_DOUBLE:
				dp = p->ptr;
				json_object_set_number(o,p->field,*dp);
				break;
			case DATA_TYPE_INT_ARRAY:
				{
					register int i;
					json_array_t *a;

					ip = p->ptr;
					a = json_create_array();
					for(i=0; i < p->len; i++) json_array_add_number(a,ip[i]);
					json_object_add_array(o,p->field,a);
				}
				break;
			case DATA_TYPE_STRING_ARRAY:
			case DATA_TYPE_FLOAT_ARRAY:
			case DATA_TYPE_DOUBLE_ARRAY:
			default:
				log_error("json_from_tab: unhandled type: %s\n", typestr(p->type));
				break;
			}
		}
	}
	return json_object_value(o);
}

json_value_t *json_from_cfgtab(cfg_proctab_t *ct) {
	json_value_t *v;
	cfg_proctab_t *ctp;
	json_proctab_t *jt;
	int i,count;

	count = 0;
	for(ctp = ct; ctp->keyword; ctp++) count++;
	jt = malloc(sizeof(*jt)*count);
	memset(jt,0,sizeof(*jt)*count);
	i = 0;
	for(ctp = ct; ctp->keyword; ctp++) {
		jt[i].field = ctp->keyword;
		jt[i].type = ctp->type;
		jt[i].ptr = ctp->dest;
		jt[i].len = ctp->dlen;
		i++;
	}
	v = json_from_tab(jt);
	free(jt);
	return v;
}


int json_to_tab(json_proctab_t *tab, json_value_t *v) {
	json_proctab_t *p;
	json_object_t *o;
	json_array_t *a;
	char *s;
	double d;
	int i,n,size;

	/* must be object */
	dprintf(dlevel,"v: %p\n", v);
	if (!v) return 1;
	if (json_value_get_type(v) != JSON_TYPE_OBJECT) return 1;

	o = json_value_object(v);
        for (i = 0; i < o->count; i++) {
		dprintf(dlevel,"name: %s\n", o->names[i]);
		for(p=tab; p->field; p++) {
			if (strcmp(p->field,o->names[i])==0) {
				if (p->cb) p->cb(p->ctx, p->field,p->ptr,p->len,o->values[i]);
				else {
					json_value_t *vv = o->values[i];
					switch(json_value_get_type(vv)) {
					case JSON_TYPE_STRING:
						s = json_value_get_string(vv);
						dprintf(dlevel,"s: %s\n", s);
						conv_type(p->type,p->ptr,p->len,DATA_TYPE_STRING,s,strlen(s));
						break;
					case JSON_TYPE_NUMBER:
						d =  json_value_get_number(vv);
						dprintf(dlevel,"d: %f\n", d);
						conv_type(p->type,p->ptr,p->len,DATA_TYPE_DOUBLE,&d,0);
						break;
					case JSON_TYPE_BOOLEAN:
						n = json_value_get_boolean(vv);
						dprintf(dlevel,"n: %d\n", n);
						conv_type(p->type,p->ptr,p->len,DATA_TYPE_LOGICAL,&n,0);
						break;
					case JSON_TYPE_ARRAY:
						a = json_value_array(vv);
						if (a->count) {
							/* get array type (only string or number supported) */
							int j,atype;

							atype = json_value_get_type(a->items[0]);
							dprintf(dlevel,"atype: %s\n", json_typestr(atype));
							switch(atype) {
							case JSON_TYPE_STRING:
								{
									char **sa;

									size = a->count * sizeof(char *);
									sa = malloc(size);
									if (!sa) return 1;
									for(j=0; j < a->count; j++) sa[j] = json_value_get_string(a->items[j]);
									conv_type(p->type,p->ptr,p->len,DATA_TYPE_STRING_ARRAY,sa,a->count);
									free(sa);
								}
								break;
							case JSON_TYPE_NUMBER:
								{
									double *da;

									size = a->count * sizeof(double);
									da = malloc(size);
									if (!da) return 1;
									for(j=0; j < a->count; j++) da[j] = json_value_get_number(a->items[j]);
									conv_type(p->type,p->ptr,p->len,DATA_TYPE_F64_ARRAY,da,a->count);
									free(da);
								}
								break;
							case JSON_TYPE_BOOLEAN:
								{
									bool *ba;

									size = a->count * sizeof(bool);
									ba = malloc(size);
									if (!ba) return 1;
									for(j=0; j < a->count; j++) ba[j] = json_value_get_boolean(a->items[j]);
									conv_type(p->type,p->ptr,p->len,DATA_TYPE_BOOL_ARRAY,ba,a->count);
									free(ba);
								}
								break;
							default:
								log_error("json_to_tab: unhandled array type: %s\n", json_typestr(atype));
								break;
							}
						}
						break;
					default:
						log_error("json_to_tab: unhandled type: %s\n", json_typestr(json_value_get_type(vv)));
						break;
					}
				}
			}
		}
        }
	return 0;
}

json_value_t *json_from_type(int type, void *src, int len) {
	double d;
	json_value_t *v;

	if (!src) return (json_value_t *)parson_value_init_null();

	v = 0;
	dprintf(dlevel,"type: %d(%s)\n", type, typestr(type));
	if (DATA_TYPE_ISNUMBER(type)) {
		conv_type(DATA_TYPE_DOUBLE,&d,0,type,src,len);
		dprintf(dlevel,"d: %f\n", d);
		if (isnan(d)) d = 0.0;
		v = (json_value_t *)parson_value_init_number(d);
	} else {
		switch(type) {
		case DATA_TYPE_BOOLEAN:
			v = (json_value_t *)parson_value_init_boolean(*((int *)src));
			break;
		case DATA_TYPE_STRING:
			v = (json_value_t *)parson_value_init_string(src);
			break;
		case DATA_TYPE_STRING_LIST:
			{
#if 0
				char value[4096];
				dprintf(dlevel,"src: %p, len: %d\n", src, len);
				conv_type(DATA_TYPE_STRING,value,sizeof(value)-1,type,src,len);
				dprintf(dlevel,"value: %s\n", value);
				v = (json_value_t *)parson_value_init_string(value);
#endif
				list l = (list) src;
				char *p;
				json_array_t *a;

				v = (json_value_t *)parson_value_init_array();
				a = json_value_array(v);
				list_reset(l);
				while((p = list_get_next(l)) != 0) {
					dprintf(-1,"p: %s\n", p);
					parson_array_append_string(ARR(a),p);
				}
			}
			break;
		case DATA_TYPE_STRING_ARRAY:
			{
				char **p;
				json_array_t *a;
				int i;

				p = (char **) src;
				dprintf(-1,"src: %s\n", (char *)src);
				v = (json_value_t *)parson_value_init_array();
				a = json_value_array(v);
				for(i=0; i < 999; i++) {
					dprintf(-1,"p[%d]: %p\n", i, p[i]);
					if (!p[i]) break;
					dprintf(-1,"p[%d]: %s\n", i, p[i]);
					parson_array_append_string(ARR(a),p[i]);
				}
			}
			break;
		default:
			log_error("json_from_type: unhandled type(%x): %s\n",type,typestr(type));
			break;
		}
	}
	return v;
}

int json_to_type(int dt, void *dest, int len, json_value_t *v) {
	bool b;
	int i;
	double d;
	char *s;

	dprintf(dlevel,"dt: %d(%s), json_type: %s\n", dt, typestr(dt), json_typestr(VAL(v)->type));
	switch(VAL(v)->type) {
	case JSONString:
		s = (char *)parson_string(VAL(v));
		return conv_type(dt,dest,len,DATA_TYPE_STRING,s,strlen(s));
		break;
	case JSONNumber:
		d = parson_number(VAL(v));
		if (double_isint(d)) {
			i = d;
			return conv_type(dt,dest,len,DATA_TYPE_INT,&i,0);
		} else {
			return conv_type(dt,dest,len,DATA_TYPE_DOUBLE,&d,0);
		}
		break;
	case JSONBoolean:
		b = parson_boolean(VAL(v));
		dprintf(dlevel,"b: %d\n", b);
		return conv_type(dt,dest,len,DATA_TYPE_BOOL,&b,sizeof(b));
		break;
	case JSONObject:
		{
			JSON_Object *o;
			int i,type;

			o = parson_object(VAL(v));
			for (i = 0; i < o->count; i++) {
				type = parson_value_get_type(o->values[i]);
				dprintf(-1,"name: %s, type: %s\n", o->names[i], json_typestr(type));
			}
			return 1;
		}
		break;
	case JSONArray:
		{
			JSON_Array *a;
			int i,size,r;
			char value[32768];
			char *str,*p;

	dprintf(-1,"dt: %d(%s), dest: %p, dsize: %d, json_type: %s\n", dt, typestr(dt), dest, len, json_typestr(VAL(v)->type));
			a = parson_array(VAL(v));
			for (i = size = 0; i < a->count; i++) {
				json_to_type(DATA_TYPE_STRING,value,sizeof(value),(json_value_t *)a->items[i]);
				size += strlen(value) + 1;
			}
			dprintf(-1,"size: %d\n", size);
			str = malloc(size);
			if (!str) {
				log_syserr("json_to_type: array malloc");
				return -1;
			}
			p = str;
			for (i = 0; i < a->count; i++) {
				json_to_type(DATA_TYPE_STRING,value,sizeof(value),(json_value_t *)a->items[i]);
				p += sprintf(p,"%s%s",value,i < a->count-1 ? "," : "");
			}
			dprintf(-1,"str: %s\n", str);
			r = conv_type(dt,dest,len,DATA_TYPE_STRING,str,strlen(str));
			dprintf(-1,"dest: %s\n", dest);
			free(str);
			dprintf(-1,"r: %d\n", r);
			return r;
		}
		break;
	default:
		dprintf(dlevel,"invalid type: %d (%s)\n", VAL(v)->type, json_typestr(VAL(v)->type));
		return -1;
		break;
	}
}

int json_iter(char *name, json_value_t *v, json_ifunc_t *func, void *ctx) {
	JSON_Object *o;
	JSON_Array *a;
        int i;

        dprintf(dlevel,"type: %d (%s)\n", VAL(v)->type, json_typestr(VAL(v)->type));

        switch(VAL(v)->type) {
        case JSONObject:
                o = parson_object(VAL(v));
                dprintf(dlevel,"count: %d\n", o->count);
                for(i=0; i < o->count; i++) if (json_iter(o->names[i],(json_value_t *)o->values[i],func,ctx)) return 1;
                break;
        case JSONArray:
                a = parson_array(VAL(v));
                dprintf(dlevel,"count: %d\n", a->count);
                for(i=0; i < a->count; i++) if (json_iter(0,(json_value_t *)a->items[i],func,ctx)) return 1;
                break;
	default:
		{
			char value[256];
			json_to_type(DATA_TYPE_STRING,value,sizeof(value)-1,v);
			if (name) return func(ctx,name,value);
			else return func(ctx,value,0);
		}
		break;
        }
        return 0;
}

#ifdef JS
#include "jsapi.h"
#include "jsobj.h"
#include "jsjson.h"
static JSClass js_json_class = {
	"myJSON",			/* Name */
	0,			/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	JS_PropertyStub,	/* getProperty */
	JS_PropertyStub,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *js_json_toObject(JSContext *cx, JSObject *parent, json_value_t *v) {
	JSObject *obj;

	obj = JS_NewObject(cx, &js_ObjectClass, 0, parent); 

	return obj;
}

static JSBool js_json_parse(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	json_value_t *v;
	char *str;
	JSObject *newobj;

	str = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "s", &str)) return JS_FALSE;
	dprintf(0,"str: %s\n", str);

	v = json_parse(str);
	JS_free(cx,str);
	if (!v) {
		JS_ReportError(cx,"error parsing JSON string", js_json_class.name);
		return JS_FALSE;
	}
	newobj = js_json_toObject(cx, obj, v);
	if (!newobj) {
		JS_ReportError(cx,"error parsing JSON string", js_json_class.name);
		return JS_FALSE;
	}
	*rval = OBJECT_TO_JSVAL(newobj);
	return JS_TRUE;
}

JSObject *js_InitmyJSONClass(JSContext *cx, JSObject *parent) {
	JSFunctionSpec js_json_funcs[] = {
		JS_FS("parse",js_json_parse,1,1,0),
//		JS_FS("stringify",js_json_stringify,3,1,0),
		{0}
	};
	JSObject *newobj;

	dprintf(2,"Creating %s object...\n", js_json_class.name);
	newobj = JS_NewObject(cx, &js_json_class, NULL, parent);
	if (!newobj) {
		JS_ReportError(cx,"unable to initialize %s object", js_json_class.name);
		return 0;
	}
        if (!JS_DefineProperty(cx, parent, js_json_class.name, OBJECT_TO_JSVAL(newobj), JS_PropertyStub, JS_PropertyStub, 0)) return NULL;
        if (!JS_DefineFunctions(cx, newobj, js_json_funcs)) return NULL;

//	dprintf(dlevel,"done!\n");
	return newobj;
}

jsval json2jsval(json_value_t *v, JSContext *cx) {
	char *j;
	JSString *str;
	jsval rootVal;
	JSONParser *jp;
	jsval reviver = JSVAL_NULL;
	JSBool ok;

	/* Convert from C JSON type to JS JSON type */
	dprintf(dlevel,"v: %p\n", v);
	if (!v) {
		JS_ReportError(cx, "unable to create info\n");
		return 0;
	}
	j = json_dumps(v,0);
	if (!j) {
		JS_ReportError(cx, "unable to stringify info\n");
		json_destroy_value(v);
		return 0;
	}
	dprintf(dlevel,"j: %p\n", j);
	json_destroy_value(v);
	jp = js_BeginJSONParse(cx, &rootVal);
	dprintf(dlevel,"jp: %p\n", jp);
	if (!jp) {
		JS_ReportError(cx, "unable init JSON parser\n");
		free(j);
		return 0;
	}
	str = JS_NewStringCopyZ(cx,j);
	ok = js_ConsumeJSONText(cx, jp, JS_GetStringChars(str), JS_GetStringLength(str));
	dprintf(dlevel,"ok: %d\n", ok);
	ok = js_FinishJSONParse(cx, jp, reviver);
	dprintf(dlevel,"ok: %d\n", ok);
	return rootVal;
}

#endif
