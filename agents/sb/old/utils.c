
#include "sb.h"
#include <sys/stat.h>

#if 0
int sb_read_file(sb_session_t *s, char *filename, char *what) {
	FILE *fp;
	struct stat sb;
	char *buffer;
	int bytes;

	if (stat(filename,&sb) < 0) {
		log_syserror("%s: stat(%s)",what,filename);
		return 1;
	}
	dprintf(1,"sb.st_size: %d\n", sb.st_size);
	buffer = malloc(sb.st_size);
	if (!buffer) {
		log_syserror("%s: malloc buffer(%d)",what,sb.st_size);
		return 1;
	}

	dprintf(1,"filename: %s\n", filename);
	fp = fopen(filename,"r");
	if (!fp) {
		log_syserror("%s: fopen(%s)",what,filename);
		free(buffer);
		return 1;
	}
	bytes = fread(buffer,1,sb.st_size,fp);
	dprintf(1,"bytes: %d\n", bytes);

	s->data = json_parse(buffer);
	free(buffer);
	return (s->data ? 0 : 1);
}
#endif

char *sb_mkfields(char **keys, int count) {
	json_object_t *o;
	json_array_t *a;
	char *fields;
	int i;

	dprintf(1,"keys: %p, count: %d\n",keys, count);
	if (!keys) return "{\"destDev\":[]}";
	for(i=0; i < count; i++) {
		dprintf(1,"key[%d]: %s\n", i, keys[i]);
	}

//	{"destDev":[],"keys":["6400_00260100","6400_00262200","6100_40263F00"]}
//	{"destDev":[],"keys":["6800_08822000"]}

	o = json_create_object();
	a = json_create_array();
	json_object_set_array(o,"destDev",a);
	if (keys && count) {
		json_array_t *ka;

		ka = json_create_array();
		for(i=0; i < count; i++) json_array_add_string(ka,keys[i]);
		json_object_set_array(o,"keys",ka);
	}
	fields = json_dumps(json_object_value(o),0);
	if (!fields) {
		log_syserror("sb_mkfields: json_dumps");
		return 0;
	}
	dprintf(1,"fields: %s\n", fields);
	return fields;
}

void sb_destroy_results(sb_session_t *s) {
	sb_result_t *res;
	sb_value_t *vp;

	list_reset(s->results);
	while((res = list_get_next(s->results)) != 0) {
		if (res->selects) list_destroy(res->selects);
		if (res->values) {
			list_reset(res->values);
			while((vp = list_get_next(res->values)) != 0) {
				if (vp->data) free(vp->data);
			}
			list_destroy(res->values);
		}
	}
	list_destroy(s->results);
	s->results = 0;
#ifdef JS
	s->results_val = 0;
#endif
}

/* Get a single result by key */
int sb_get_result(sb_session_t *s, int dt, void *dest, int dl, char *key) {
	sb_result_t *rp;
	sb_value_t *vp;
	int found, i, r, l;
//, count;
	char temp[1024];
//, w[5];

	dprintf(1,"key: %s\n", key);

	found = 0;
	list_reset(s->results);
	while((rp = list_get_next(s->results)) != 0) {
		dprintf(1,"rp->obj->key: %s\n", rp->obj->key);
		if (strcmp(rp->obj->key, key) == 0) {
//			count = list_count(rp->values);
        		i = 0;
			r = dl;
			list_reset(rp->values);
       			while((vp = list_get_next(rp->values)) != 0) {
//				if (count > 1) sprintf(w," [%c]",'A' + i++);
//				else *w = 0;
				conv_type(DATA_TYPE_STRING,temp,sizeof(temp)-1,vp->type,vp->data,vp->len);
				l = strlen(temp);
				dprintf(1,"l: %d, r: %d\n", l, r);
				if (i) strcat(dest,",");
				strncat(dest,temp,r);
				r -= l;
				if (r < 0) break;
			}
			found = 1;
			break;
		}
	}

	dprintf(1,"found: %d\n", found);
	return (found ? 0 : 1);
}
