
	debug = 4;

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "old2/old/sma_strings.c"
//#include "s1.h"
#define NODEFSTRINGS 1
#include "strings.c"
#include "list.h"

char *sb_agent_version_string = "t";

int main(void) {
	sb_session_t *s;
	sb_string_t *newstr, *sp;
	char temp[4096];
	int found,newstr_count;
	register int i,j;
	list l;
	FILE *fp;
	char *p,*p2;

	s = sb_driver.new(0,0);
	if (!s) return 1;


	s->data = json_parse_file("old2/en-US.json");
	if (sb_load_strings(s)) return 1;
	printf("count: %d\n", s->strings_count);
//	return 0;

	l = list_create();
	for(i=0; i < sma_strings_count; i++) {
#if !INT_TAGS
		sprintf(temp,"%d",sma_strings[i].tag);
#endif
		found = 0;
		for(j=0; j < s->strings_count; j++) {
			if (!s->strings[j].tag) continue;
#if INT_TAGS
			if (s->strings[j].tag == sma_strings[i].tag) {
#else
			if (strcmp(s->strings[j].tag,temp) == 0) {
#endif
//				printf("found\n");
				list_add(l,&s->strings[j],sizeof(struct sb_string));
				found = 1;
				break;
			}
		}
		if (!found) {
			sb_string_t ns;
//			printf("NOT found: %d\n", sma_strings[i].tag);
#if INT_TAGS
			ns.tag = sma_strings[i].tag;
#else
			ns.tag = malloc(strlen(temp)+1);
			strcpy(ns.tag,temp);
#endif
			ns.string = sma_strings[i].string;
			printf("adding: %d = %s\n", ns.tag, ns.string);
			list_add(l,&ns,sizeof(ns));
		}
	}
	for(j=0; j < s->strings_count; j++) {
		if (!s->strings[j].tag) continue;
		found = 0;
		for(i=0; i < sma_strings_count; i++) {
#if INT_TAGS
			if (s->strings[j].tag == sma_strings[i].tag) {
#else
			sprintf(temp,"%d",sma_strings[i].tag);
			if (strcmp(s->strings[j].tag,temp) == 0) {
#endif
				found = 1;
				break;
			}
		}
		if (!found) {
			printf("adding: %d = %s\n", s->strings[j].tag, s->strings[j].string);
			list_add(l,&s->strings[j],sizeof(struct sb_string));
		}
	}
	printf("sorting...\n");
	newstr_count = list_count(l);
	printf("newstr_count: %d\n", newstr_count);
	newstr = malloc(newstr_count*sizeof(struct sb_string));
	if (!newstr) return 1;
	list_reset(l);
	i = 0;
	while((sp = list_get_next(l)) != 0) newstr[i++] = *sp;
	qsort(newstr, newstr_count, sizeof(struct sb_string), string_cmp);
	fp = fopen("en_strings.h","w+");
	if (!fp) return 1;
	fprintf(fp,"sb_string_t en_strings[] = {\n");
	for(i=0; i < newstr_count; i++) {
		sp = &newstr[i];
		printf("newstr[%d]: tag: %d, string: %s\n", i, sp->tag, sp->string);
		p2 = temp;
		for(p=sp->string; *p; p++) {
			if (*p == '\"') *p2++ = '\\';
			else if (*p == '\n' || *p == '\r') continue;
			else if (*p < 0 || *p > 127) continue;
			*p2++ = *p;
		}
		*p2 = 0;
#if INT_TAGS
		fprintf(fp,"\t{ %d,\"%s\" },\n", sp->tag, temp);
#else
		fprintf(fp,"\t{ \"%s\",\"%s\" },\n", sp->tag, temp);
#endif
	}
	fprintf(fp,"};\n");
	fprintf(fp,"#define en_strings_count (sizeof(en_strings)/sizeof(struct sb_string))\n");
	return 0;
}
