
#include "sb.h"
#include <ctype.h>
#ifndef NODEFSTRINGS
#include "en_strings.h"
#endif

static int string_cmp(const void *v1, const void *v2) {
	const sb_string_t *c1 = v1;
	const sb_string_t *c2 = v2;

#if INT_TAGS
	if (c1->tag > c2->tag) return 1;
	else if (c1->tag < c2->tag) return -1;
	else return 0;
#else
	return strcmp(c1->tag, c2->tag);
#endif
}

static int sb_load_strings(sb_session_t *s, json_value_t *v) {
	json_object_t *o;
	sb_string_t *strings, *newstr;
	int strsize,i,count;
	register char *p;
	int r;

	dprintf(0,"v: %p\n", v);
	if (!v) return 1;

	r = 1;
	if (json_value_get_type(v) != JSON_TYPE_OBJECT) goto sb_load_strings_error;
	o = json_value_object(v);
	dprintf(1,"count: %d\n", o->count);
	strsize = o->count * sizeof(struct sb_string);
	strings = malloc(strsize);
	if (!strings) {
		log_syserror("sb_get_strings: malloc strings(%d)",strsize);
		goto sb_load_strings_error;
	}
	memset(strings,0,strsize);
	count = 0;
	for(i=0; i < o->count; i++) {
		p = o->names[i];
		newstr = &strings[i];
#if INT_TAGS
		newstr->tag = atoi(p);
		if (!newstr->tag) continue;
#else
		newstr->tag = malloc(strlen(p)+1);
		if (!newstr->tag) {
			log_syserror("sb_get_objects: malloc newstr->tag(%d)",strlen(p)+1);
			break;
		}
		strcpy(newstr->tag,p);
#endif
		if (json_value_get_type(o->values[i]) != JSON_TYPE_STRING) {
			log_error("sb_get_strings: json type %d(%s) is not string?\n",
				json_value_get_type(o->values[i]),
				json_typestr(json_value_get_type(o->values[i])));
			goto sb_load_strings_error;
		}
		p = json_value_get_string(o->values[i]);
		if (!p) continue;
		newstr->string = malloc(strlen(p)+1);
		if (!newstr->string) {
			log_syserror("sb_get_objects: malloc newstr->string(%d)",strlen(p)+1);
			break;
		}
		strcpy(newstr->string,p);
//		dprintf(8,"newstr[%d]: %s = %s\n", i, newstr->tag, newstr->string);
		count++;
	}
	s->strings = strings;
	s->strsize = strsize;
	s->strings_count = count;

	/* Now sort the array */
	qsort (s->strings, s->strings_count, sizeof (struct sb_string), string_cmp);
	dprintf(1,"done!\n");
	r = 0;

sb_load_strings_error:
	json_destroy_value(v);
	return r;
}

int sb_get_strings(sb_session_t *s) {
	char func[32];

#ifndef NODEFSTRINGS
	if (strcmp(s->lang,"en-US")==0 && s->use_internal_strings) {
		s->strings = en_strings;
		s->strings_count = en_strings_count;
		return 0;
	} else
#endif
	{
		json_value_t *v;

		if (sb_driver.open(s)) return 1;
		sprintf(func,"data/l10n/%s.json",s->lang);
#if 0
		if (sb_request(s,func,0)) return 1;
		if (!s->data) return 1;
#endif
		v = sb_request(s,func,0);
		if (!v) return 1;
		return sb_load_strings(s,v);
	}
}

int sb_read_strings(sb_session_t *s, char *filename) {
//	if (sb_read_file(s,filename,"sb_read_strings")) return 1;
	return sb_load_strings(s,json_parse_file(filename));
}

int sb_write_strings(sb_session_t *s, char *filename) {
	FILE *fp;
	register int i;

	if (!s->strings) {
		log_error("unable to write strings: not loaded!\n");
		return 1;
	}

	fp = fopen(filename,"w+");
	if (!fp) {
		log_syserror("sb_write_strings: fopen(%s)",filename);
		return 1;
	}

	if (s->inc_flag) {
		char temp[4096];
		register char *p,*p2;

		fprintf(fp,"#include \"sb.h\"\n");
		fprintf(fp,"sb_string_t strings[] = {\n");
		for(i=0; i < s->strings_count; i++) {
			p2 = temp;
			for(p=s->strings[i].string; *p; p++) {
				if (*p == '\"') *p2++ = '\\';
				else if (*p == '\n' || *p == '\r') continue;
				else if (*p < 0 || *p > 127) continue;
				*p2++ = *p;
			}
			*p2 = 0;
#if INT_TAGS
			fprintf(fp,"\t{ %d,\"%s\" },\n", s->strings[i].tag, temp);
#else
			fprintf(fp,"\t{ \"%s\",\"%s\" },\n", s->strings[i].tag, temp);
#endif
		}
		fprintf(fp,"};\n");
		fprintf(fp,"#define strings_count (sizeof(strings/sizeof(sb_string_t))\n");
	} else {
		json_object_t *o;
		char *str;
#if INT_TAGS
		char temp[32];
#endif

		o = json_create_object();
		for(i=0; i < s->strings_count; i++) {
#if INT_TAGS
			sprintf(temp,"%d",s->strings[i].tag);
			json_object_add_string(o,temp,s->strings[i].string);
#else
			json_object_add_string(o,s->strings[i].tag,s->strings[i].string);
#endif
		}
		str = json_dumps(json_object_value(o),1);
		json_destroy_object(o);
		if (!str) {
			log_error("unable to write strings: error generating json string!\n");
			return 1;
		}
		fprintf(fp,"%s\n",str);
	}
	fclose(fp);
	return 0;
}

char *sb_get_string(sb_session_t *s, int itag) {
	sb_string_t target, *result;
#if !INT_TAGS
	char temp[32];
#endif

//	dprintf(1,"s->strings: %p\n", s->strings);
	if (!s->strings) return 0;

//	dprintf(5,"itag: %d\n", itag);

#if INT_TAGS
	target.tag = itag;
#else
	sprintf(temp,"%d",itag);
	target.tag = temp;
#endif
	result = bsearch (&target, s->strings, s->strings_count, sizeof (struct sb_string), string_cmp);
	if (!result) log_error("TAG NOT FOUND: %d\n", itag);
	return (result ? result->string : 0);
}
