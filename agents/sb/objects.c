
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 6
#include "debug.h"

#include "sb.h"

static void _getstr(void *ctx, char *name,void *dest, int len,json_value_t *v) {
	register int tag;

	tag = json_value_get_number(v);
	if (!tag) {
//		dprintf(1,"name: %s, tag: %d\n", name, tag);
		*((char **)dest) = "";
	}
	else *((char **)dest) = sb_get_string((sb_session_t *)ctx,tag);
}

static void _gethier(void *ctx, char *name,void *dest, int len,json_value_t *v) {
	json_array_t *a;
	char **d = dest;
	int i;
	register int tag;

	a = json_value_array(v);
	for(i=0; i < a->count; i++) {
		tag = json_value_get_number(a->items[i]);
		if (!tag) continue;
//		dprintf(1,"tag: %d\n", tag);
		d[i] = sb_get_string((sb_session_t *)ctx,tag);
	}
}

static int object_cmp(const void *v1, const void *v2) {
	const sb_object_t *c1 = v1;
	const sb_object_t *c2 = v2;

	return strcmp(c1->key, c2->key);
}

int sb_load_objects(sb_session_t *s, json_value_t *v) {
	json_object_t *o;
	sb_object_t *objects, *newobj;
	int objsize,i,r;

	dprintf(dlevel,"v: %p\n", v);
	if (!v) return 1;

	r = 1;
	if (json_value_get_type(v) != JSON_TYPE_OBJECT) goto sb_load_objects_done;
	o = json_value_object(v);
	dprintf(dlevel,"count: %d\n", o->count);
	objsize = o->count * sizeof(struct sb_object);
	objects = malloc(objsize);
	if (!objects) {
		log_syserror("sb_get_objects: malloc objects(%d)",objsize);
		goto sb_load_objects_done;
	}
	memset(objects,0,objsize);
	for(i=0; i < o->count; i++) {
//		dprintf(dlevel,"names[%d]: %s\n", i, o->names[i]);
		newobj = &objects[i];
		newobj->key = malloc(strlen(o->names[i])+1);
		if (!newobj->key) {
			log_syserror("sb_get_objects: malloc newobj->key(%d)",strlen(o->names[i])+1);
			break;
		}
		strcpy(newobj->key,o->names[i]);
//		dprintf(dlevel,"values[%d] type: %s\n", i, json_typestr(json_value_get_type(o->values[i])));
		if (json_value_get_type(o->values[i]) == JSON_TYPE_OBJECT) {
			json_proctab_t tab[] = {
//				{ "Prio",DATA_TYPE_INT,&newobj->prio,0,0 },
				{ "TagId",DATA_TYPE_INT,&newobj->labeltag,0,0 },
				{ "TagId",DATA_TYPE_NULL,&newobj->label,0,_getstr,s },
				{ "TagIdEvtMsg",DATA_TYPE_INT,&newobj->evtmsgtag,0,0 },
				{ "TagIdEvtMsg",DATA_TYPE_NULL,&newobj->evtmsg,0,_getstr,s },
				{ "Unit",DATA_TYPE_INT,&newobj->unittag,0,0 },
				{ "Unit",DATA_TYPE_NULL,&newobj->unit,0,_getstr,s },
				{ "DataFrmt",DATA_TYPE_INT,&newobj->format,0,0 },
				{ "Scale",DATA_TYPE_DOUBLE,&newobj->mult,0,0 },
//				{ "Typ",DATA_TYPE_INT,&newobj->type,0,0 },
//				{ "WriteLevel",DATA_TYPE_INT,&newobj->wrlevel,0,0 },
				{ "TagHier",DATA_TYPE_S32_ARRAY,&newobj->hiertags,8,0 },
				{ "TagHier",DATA_TYPE_NULL,&newobj->hier,0,_gethier,s },
				{ "Min",DATA_TYPE_BOOLEAN,&newobj->min,0,0 },
				{ "Max",DATA_TYPE_BOOLEAN,&newobj->max,0,0 },
				{ "Sum",DATA_TYPE_BOOLEAN,&newobj->sum,0,0 },
				{ "Avg",DATA_TYPE_BOOLEAN,&newobj->avg,0,0 },
				{ "Cnt",DATA_TYPE_BOOLEAN,&newobj->cnt,0,0 },
				{ "SumD",DATA_TYPE_BOOLEAN,&newobj->sumd,0,0 },
				{0}
			};
			json_to_tab(tab,o->values[i]);
			dprintf(dlevel+1,"newobj: key: %s, label: %s, unit: %s, mult: %f, format: %d, min: %d, max: %d, sum: %d, avg: %d, cnt: %d, sumd: %d\n", newobj->key, newobj->label, newobj->unit, newobj->mult, newobj->format, newobj->min, newobj->max, newobj->sum, newobj->avg, newobj->cnt, newobj->sumd);
		}
	}
	s->objects = objects;
	s->objsize = objsize;
	s->objects_count = i;
	qsort (s->objects, s->objects_count, sizeof (struct sb_object), object_cmp);
	dprintf(dlevel,"done!\n");
	r = 0;
sb_load_objects_done:
	json_destroy_value(v);
	return r;
}

int sb_get_objects(sb_session_t *s) {
	if (sb_driver.open(s)) return 1;
	return sb_load_objects(s,sb_request(s,"data/ObjectMetadata_Istl.json",0));
}

int sb_read_objects(sb_session_t *s, char *filename) {
#if 0
	dprintf(dlevel,"filename: %s\n", filename);
	if (sb_read_file(s,filename,"sb_read_objects")) return 1;
	return sb_load_objects(s);
#endif
	return sb_load_objects(s,json_parse_file(filename));
}

int sb_write_objects(sb_session_t *s, char *filename) {
	FILE *fp;
	json_object_t *ro;
	char *str;
	sb_object_t *newobj;
	register int i;

	dprintf(1,"objects: %p\n", s->objects);
	if (!s->objects) {
		log_error("unable to write objects: not loaded!\n");
		return 1;
	}
	ro = json_create_object();
	dprintf(1,"objects_count: %d\n", s->objects_count);
	for(i=0; i < s->objects_count; i++) {
		newobj = &s->objects[i];
		dprintf(1,"key: %s\n",newobj->key);
		json_proctab_t tab[] = {
//			{ "Prio",DATA_TYPE_INT,&newobj->prio,0,0 },
			{ "TagId",DATA_TYPE_INT,&newobj->labeltag,0,0 },
			{ "TagIdEvtMsg",DATA_TYPE_INT,&newobj->evtmsgtag,0,0 },
			{ "Unit",DATA_TYPE_INT,&newobj->unittag,0,0 },
			{ "DataFrmt",DATA_TYPE_INT,&newobj->format,0,0 },
			{ "Scale",DATA_TYPE_DOUBLE,&newobj->mult,0,0 },
//			{ "Typ",DATA_TYPE_INT,&newobj->type,0,0 },
//			{ "WriteLevel",DATA_TYPE_INT,&newobj->wrlevel,0,0 },
			{ "TagHier",DATA_TYPE_S32_ARRAY,&newobj->hiertags,8,0 },
			{ "Min",DATA_TYPE_BOOLEAN,&newobj->min,0,0 },
			{ "Max",DATA_TYPE_BOOLEAN,&newobj->max,0,0 },
			{ "Sum",DATA_TYPE_BOOLEAN,&newobj->sum,0,0 },
			{ "Avg",DATA_TYPE_BOOLEAN,&newobj->avg,0,0 },
			{ "Cnt",DATA_TYPE_BOOLEAN,&newobj->cnt,0,0 },
			{ "SumD",DATA_TYPE_BOOLEAN,&newobj->sumd,0,0 },
			{0}
		};
		json_object_add_value(ro,newobj->key,json_from_tab(tab));
	}
	str = json_dumps(json_object_value(ro),1);
//	json_destroy_object(ro);
	if (!str) {
		log_error("unable to write strings: error generating json string!\n");
		return 1;
	}
	fp = fopen(filename,"w+");
	if (!fp) {
		log_syserror("sb_write_strings: fopen(%s)",filename);
		return 1;
	}
	fprintf(fp,"%s\n",str);
	fclose(fp);
exit(0);
	return 0;
}


sb_object_t *sb_get_object(sb_session_t *s, char *key) {
	sb_object_t target, *result;

	dprintf(dlevel,"s->objects: %p\n", s->objects);
	if (!s->objects) return 0;

	target.key = key;
	result = bsearch (&target, s->objects, s->objects_count, sizeof (struct sb_object), object_cmp);
	if (!result) log_error("OBJECT NOT FOUND: %s\n", key);
	return result;

#if 0
	register int i;

	dprintf(dlevel,"s->objects: %p\n", s->objects);
	if (!s->objects) return 0;

//	dprintf(dlevel,"key: %s\n", key);

	for(i=0; i < s->objects_count; i++) {
		if (strcmp(s->objects[i].key,key) == 0) {
//			dprintf(dlevel,"found\n");
			return &s->objects[i];
		}
	}
	log_error("OBJECT NOT FOUND: %s\n", key);
//	dprintf(dlevel,"NOT found\n");
	return 0;
#endif
}

int sb_get_object_path(sb_session_t *s, char *dest, int destsize, sb_object_t *o) {
	char path[256], *p;
	int i;

	if (!dest) return 1;
	*dest = 0;

	p = path;
	for(i=0; i < 8; i++) {
		if (!o->hier[i]) break;
		if (i) p += sprintf(p," -> ");
		p += sprintf(p,"%s",o->hier[i]);
	}
	p += sprintf(p," -> %s",o->label);
	dprintf(dlevel,"path: %s\n",path);
	strncpy(dest,path,destsize);
	return 0;
}
