/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 4

#include "sb.h"

#define SB_INIT_BUFSIZE 4096

static char *gettag(sb_session_t *s, json_value_t *rv) {
	json_object_t *o;
	json_value_t *v;
	int tag;
	char *p;

	o = json_value_object(rv);
	dprintf(dlevel,"object count: %d\n", o->count);
	if (o->count < 1) return 0;
	/* The only option we support here is "tag" (number) */
	v = json_object_dotget_value(o,"tag");
	dprintf(dlevel,"v: %p\n", v);
	if (!v) return 0;
	tag = json_value_get_number(v);
	dprintf(dlevel,"tag: %d\n", tag);
	p = sb_get_string(s, tag);
	if (!p) p = "";
	return p;
}

static void getval(sb_session_t *s, sb_result_t *res, json_value_t *rv) {
	sb_value_t newval;
	json_array_t *a;
	int type;
	register int i;
	register char *p;

	dprintf(dlevel,"id: %s\n", res->obj->key);

	memset(&newval,0,sizeof(newval));

	type = json_value_get_type(rv);
	dprintf(dlevel,"type: %d (%s)\n", type, json_typestr(type));
	switch(type) {
	case JSON_TYPE_NULL:
		newval.type = DATA_TYPE_NULL;
		break;
	case JSON_TYPE_NUMBER:
	    {
		double d;

		newval.type = DATA_TYPE_DOUBLE;
		newval.data = malloc(sizeof(double));
		if (!newval.data) {
			log_syserror("getval: malloc(%d)",sizeof(double));
			return;
		}
		d = json_value_get_number(rv);
		dprintf(dlevel,"newval.d(1): %f, mult: %f\n", d, res->obj->mult);
		if (!double_equals(res->obj->mult,0.0)) d *= res->obj->mult;
		dprintf(dlevel,"newval.d(2): %f\n", d);
		*((double *)newval.data) = d;
	    }
	    break;
	case JSON_TYPE_STRING:
		newval.type = DATA_TYPE_STRING;
		p = json_value_get_string(rv);
		newval.len = strlen(p);
		newval.data = malloc(newval.len+1);
		if (!newval.data) {
			log_syserror("_getval: malloc(%d)\n", newval.len+1);
			return;
		}
		strcpy((char *)newval.data,p);
		break;
	case JSON_TYPE_ARRAY:
		a = json_value_array(rv);
		dprintf(dlevel,"array count: %d\n", a->count);
		if (a->count == 1 && json_value_get_type(a->items[0]) == JSON_TYPE_OBJECT) {
			newval.type = DATA_TYPE_STRING;
			p = gettag(s,a->items[0]);
			dprintf(dlevel,"p: %s\n", p);
			if (!p) return;
			newval.len = strlen(p);
			newval.data = malloc(newval.len+1);
			if (!newval.data) {
				log_syserror("_getval: malloc(%d)\n", newval.len+1);
				return;
			}
			strcpy((char *)newval.data,p);
		} else {
			newval.type = DATA_TYPE_STRING_LIST;
			newval.data = list_create();
			for(i=0; i < a->count; i++) {
				type = json_value_get_type(a->items[i]);
				dprintf(dlevel,"type: %s\n", json_typestr(type));
				switch(type) {
				case JSON_TYPE_NUMBER:
					dprintf(dlevel,"num: %f\n", json_value_get_number(a->items[i]));
					break;
				case JSON_TYPE_OBJECT:
					p = gettag(s,a->items[i]);
					dprintf(dlevel,"adding: %s\n", p);
					list_add((list)newval.data,p,strlen(p)+1);
					break;
				default:
					dprintf(dlevel,"unhandled type: %d (%s)\n", type, json_typestr(type));
					return;
				}
			}
		}
		break;
	default:
		log_error("_getval: unhandled type: %d(%s)\n", type, json_typestr(type));
		return;
		break;
	}
	list_add(res->values,&newval,sizeof(newval));
}

static void getsel(sb_session_t *s, sb_result_t *res, json_array_t *a) {
	int i;
	char *p;

	dprintf(dlevel,"count: %d\n", a->count);
	for(i=0; i < a->count; i++) {
		p = sb_get_string(s,json_value_get_number(a->items[i]));
		if (!p) continue;
		dprintf(dlevel,"select: %s\n", p);
		list_add(res->selects,p,strlen(p)+1);
	}
}

static int getobject(sb_session_t *s,list results,json_object_t *o) {
	sb_result_t newres;
	json_value_t *v;
	json_object_t *oo;
	json_array_t *a;
	int i,j,k,type;

/*
      "6800_08822000": {
        "1": [
          {
            "validVals": [
              9439,
              9440,
              9441,
              9442,
              9443,
              9444
            ],
            "val": [
              {
                "tag": 9442
              }
            ]
          }
        ]
      },
*/
	dprintf(dlevel,"count: %d\n", o->count);
	for(i=0; i < o->count; i++) {
		/* ID */
		v = o->values[i];
		type = json_value_get_type(v);
		dprintf(dlevel,"o->names[%d]: %s, type: %d (%s)\n", i, o->names[i], type, json_typestr(type));
		if (type != JSON_TYPE_OBJECT) continue;

		/* Create a new result */
		memset(&newres,0,sizeof(newres));
		newres.obj = sb_get_object(s,o->names[i]);
		if (!newres.obj) continue;
		newres.selects = list_create();
		newres.values = list_create();

		/* "1" - array of 1 object */
		oo = json_value_object(v);
		if (oo->count < 1) continue;
		if (json_value_get_type(oo->values[0]) != JSON_TYPE_ARRAY) continue;

		a = json_value_array(oo->values[0]);
		dprintf(dlevel,"array count: %d\n", a->count);
		for(j=0; j < a->count; j++) {
			type = json_value_get_type(a->items[j]);
			dprintf(dlevel,"type: %s\n", json_typestr(type));
			switch(type) {
			case JSON_TYPE_NUMBER:
				dprintf(dlevel,"num: %f\n", json_value_get_number(a->items[j]));
				break;
			case JSON_TYPE_OBJECT:
				oo = json_value_object(a->items[j]);
				for(k=0; k < oo->count; k++) {
					type = json_value_get_type(oo->values[k]);
					dprintf(dlevel,"oo->names[%d]: %s, type: %d (%s)\n", k, oo->names[k], type, json_typestr(type));
					if (strcmp(oo->names[k],"val") == 0) {
						getval(s,&newres,oo->values[k]);
					} else if (strcmp(oo->names[k],"validVals") == 0 && type == JSON_TYPE_ARRAY) {
						getsel(s,&newres,json_value_array(oo->values[k]));
					} else if (strcmp(oo->names[k],"low") == 0 && type == JSON_TYPE_NUMBER) {
						newres.low = json_value_get_number(oo->values[k]);
					} else if (strcmp(oo->names[k],"high") == 0 && type == JSON_TYPE_NUMBER) {
						newres.high = json_value_get_number(oo->values[k]);
					}
				}
			}
		}
		list_add(results,&newres,sizeof(newres));
	}
	return 0;
}

list sb_get_results(sb_session_t *s, json_value_t *rv) {
	json_value_t *v,*vv;
	json_object_t *o,*oo;
	int i,j,type;
	bool bval;
	list results;

	if (json_value_get_type(rv) != JSON_TYPE_OBJECT) return 0;
	o = json_value_object(rv);
	results = list_create();
	for(i=0; i < o->count; i++) {
		dprintf(dlevel,"names[%d]: %s\n", i, o->names[i]);
		if (strcmp(o->names[i],"result") == 0) {
			v = o->values[i];
			dprintf(dlevel,"values[%d] type: %s\n", i, json_typestr(json_value_get_type(v)));
			if (json_value_get_type(v) != JSON_TYPE_OBJECT) continue;
			oo = json_value_object(v);
			for(j=0; j < oo->count; j++) {
				vv = oo->values[j];
				type = json_value_get_type(vv);
				dprintf(dlevel,"names[%d]: %s, type: %d(%s)\n", j, oo->names[j], type, json_typestr(type));
				if (strcmp(oo->names[j],"sid") == 0) {
					if (type != JSON_TYPE_STRING) continue;
					strncpy(s->session_id,json_value_get_string(vv),sizeof(s->session_id)-1);
				} else if (strcmp(oo->names[i],"isLogin") == 0) {
					json_to_type(DATA_TYPE_BOOLEAN,&bval,sizeof(bval),oo->values[i]);
					dprintf(dlevel,"isLogin: %d\n", bval);
					if (!bval) *s->session_id = 0;
				} else if (type == JSON_TYPE_OBJECT) {
					getobject(s,results,json_value_object(vv));
				}
			}
		} else if (strcmp(o->names[i],"err") == 0) {
			v = o->values[i];
			type = json_value_get_type(v);
//			dprintf(0,"err type: %s\n", json_typestr(type));
			if (type == JSON_TYPE_NUMBER) {
				s->errcode = json_value_get_number(v);
				goto sb_get_results_error;
			} else if (json_value_get_type(v) == JSON_TYPE_OBJECT) {
				oo = json_value_object(v);
				if (oo->count && json_value_get_type(oo->values[0]) == JSON_TYPE_NUMBER) {
					s->errcode = json_value_get_number(oo->values[0]);
				}
			} else {
				s->errcode = -1;
			}
			dprintf(dlevel,"errcode: %d\n", s->errcode);
			goto sb_get_results_error;
		}
	}

	return results;
sb_get_results_error:
	if (results) sb_destroy_results(results);
	return 0;
}

int sb_destroy_results(list results) {
	sb_result_t *res;
	sb_value_t *vp;

	if (!results) return 1;

	list_reset(results);
	while((res = list_get_next(results)) != 0) {
		if (res->selects) list_destroy(res->selects);
		if (res->values) {
			list_reset(res->values);
			while((vp = list_get_next(res->values)) != 0) {
				if (vp->data) free(vp->data);
			}
			list_destroy(res->values);
		}
	}
	list_destroy(results);
	return 0;
}

/* Get a single result by key */
sb_value_t *sb_get_result_value(list results, char *key) {
	sb_result_t *rp;

	dprintf(1,"key: %s\n", key);

	list_reset(results);
	while((rp = list_get_next(results)) != 0) {
		dprintf(1,"rp->obj->key: %s\n", rp->obj->key);
		if (strcmp(rp->obj->key, key) == 0) {
			/* only return the 1st value */
			list_reset(rp->values);
			dprintf(1,"found\n");
			return list_get_next(rp->values);
		}
	}

	dprintf(1,"NOT found\n");
	return 0;
}
