
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "old2/old/sma_strings.c"
//#include "s1.h"
#define NODEFSTRINGS 1
#include "strings.c"
#include "list.h"

static int string_cmp(const void *v1, const void *v2) {
	const sb_string_t *c1 = v1;
	const sb_string_t *c2 = v2;

//	printf("c1: %s, c2: %s\n", c1->tag, c2->tag);
        return strcmp(c1->tag, c2->tag);
}

int main(void) {
	sb_string_t *newstr, *sp;
	char temp[4096];
	int found,newstr_count;
	register int i,j;
	list l;
	FILE *fp;
	char *p,*p2;

	l = list_create();
	for(i=0; i < sma_strings_count; i++) {
		sprintf(temp,"%d",sma_strings[i].tag);
		found = 0;
		for(j=0; j < strings_count; j++) {
			if (strcmp(strings[j].tag,temp) == 0) {
//				printf("found\n");
				list_add(l,&strings[j],sizeof(struct sb_string));
				found = 1;
				break;
			}
		}
		if (!found) {
			sb_string_t ns;
//			printf("NOT found: %d\n", sma_strings[i].tag);
			ns.tag = malloc(strlen(temp)+1);
			if (ns.tag) {
				strcpy(ns.tag,temp);
				ns.string = sma_strings[i].string;
				list_add(l,&ns,sizeof(ns));
			}
		}
	}
	for(j=0; j < strings_count; j++) {
		found = 0;
		for(i=0; i < sma_strings_count; i++) {
			sprintf(temp,"%d",sma_strings[i].tag);
			if (strcmp(strings[j].tag,temp) == 0) {
				found = 1;
				break;
			}
		}
		if (!found) list_add(l,&strings[j],sizeof(struct sb_string));
	}
	printf("sorting...\n");
	newstr_count = list_count(l);
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
		p2 = temp;
		for(p=sp->string; *p; p++) {
			if (*p == '\"') *p2++ = '\\';
			else if (*p == '\n' || *p == '\r') continue;
			else if (*p < 0 || *p > 127) continue;
			*p2++ = *p;
		}
		*p2 = 0;
		fprintf(fp,"\t{ \"%s\",\"%s\" },\n", sp->tag, temp);
	}
	fprintf(fp,"};\n");
	fprintf(fp,"#define en_strings_count (sizeof(en_strings)/sizeof(struct sb_string))\n");
	return 0;
}
