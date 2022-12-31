
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "sdconfig.h"
#include "client.h"
#include "uuid.h"

#define dlevel 2

#define NAME_FORMAT "%-20.20s  "
#define TYPE_FORMAT "%-10.10s  "
#define SCOPE_FORMAT "%-10.10s  "
#define VALUE_FORMAT "%s  "
#define LABEL_FORMAT "%s"
#define UNITS_FORMAT " >%s<"

static int do_list_item(char *target, int level, json_object_t *o, int hdr) {
//	json_array_t *labels;
	char *name,*dtype,*scope;
	char pad[16],line[1024],*p;
	int i;
//	int j;
//	int type;
	char *values,*labels,*units;

	dprintf(dlevel,"target: %s, level: %d, obj: %p, hdr: %d\n", target, level, o, hdr);

	pad[0] = 0;
	for(i=0; i < level; i++) strcat(pad,"  ");

	if (hdr) printf("%s"NAME_FORMAT""TYPE_FORMAT""SCOPE_FORMAT""VALUE_FORMAT""LABEL_FORMAT""UNITS_FORMAT"\n",pad,"Name","Type","Scope","Values","(Labels)","Units");
	name = json_object_get_string(o,"name");
	dprintf(dlevel,"name: %s\n", name);
	dtype = json_object_get_string(o,"type");
	dprintf(dlevel,"dtype: %p\n", dtype);

		dtype = json_object_get_string(o,"type");
		dprintf(dlevel,"dtype: %p\n", dtype);
		if (!dtype) return 1;
		dprintf(dlevel,"dtype: %s\n", dtype);
		p = line;
		p += sprintf(p,"%s"NAME_FORMAT""TYPE_FORMAT,pad,name,dtype);
		scope = json_object_get_string(o,"scope");
		if (scope) {
			p += sprintf(p,SCOPE_FORMAT,scope);
			values = json_object_get_string(o,"values");
			if (values) p += sprintf(p,VALUE_FORMAT,values);
			labels = json_object_get_string(o,"labels");
			if (labels) p += sprintf(p,LABEL_FORMAT,labels);
			units = json_object_get_string(o,"units");
			if (units) p += sprintf(p,UNITS_FORMAT,units);
		} else {
			p += sprintf(p,SCOPE_FORMAT,dtype);
			if (strcmp(dtype,"bol")==0) p += sprintf(p,"true/false");
		}
#if 0
		if (scope) { 
			dprintf(dlevel,"scope: %s\n", scope);
			p += sprintf(p,SCOPE_FORMAT,scope);
			if (strcmp(scope,"select")==0 || strcmp(scope,"mselect")==0) {
				json_array_t *values;
				float num;
//				int inum;

				values = json_object_get_array(o,"values");
				dprintf(dlevel,"values: %p\n", values);
				if (values) {
					for(j=0; j < values->count; j++) {
						if (j) p += sprintf(p,",");
						type = json_value_get_type(values->items[j]);
						dprintf(dlevel,"type: %d\n", type);
						switch(type) {
						case JSON_TYPE_STRING:
							p += sprintf(p,"%s",json_value_get_string(values->items[j]));
							break;
						case JSON_TYPE_NUMBER:
							num = json_value_get_number(values->items[j]);
							dprintf(0,"num: %f, isint: %d\n", num, double_isint(num));
							if (double_isint(num)) p += sprintf(p,"%d",(int)num);
							else p += sprintf(p,"%f",num);
							break;
						default:
							p += sprintf(p,"unhandled type: %d",type);
							break;
						}
					}
				}
			} else if (strcmp(scope,"range")==0) {
				json_array_t *values;
				double min,max,step;
				int imin,imax,istep;

				/* values array should have 3 items */
				values = json_object_get_array(o,"values");
				dprintf(dlevel,"values: %p\n", values);
				if (!values) {
					printf("  %s: invalid info format(range specified but no values\n",name);
					return 1;
				}
				if (values->count != 3) {
					printf("  %s: invalid info format(range specified but values count != 3\n",name);
					return 1;
				}
				min = json_value_get_number(values->items[0]);
#define DTEST(a,b) !((a>b)||(a<b))
				imin = min;
				if (imin == min || DTEST(min,0.0)) p += sprintf(p,"min: %d, ",imin);
				else p += sprintf(p,"min: %lf, ",min);
				max = json_value_get_number(values->items[1]);
				imax = max;
				if (imax == max || DTEST(max,0.0)) p += sprintf(p,"max: %d, ",imax);
				else p += sprintf(p,"max: %lf, ",max);
				step = json_value_get_number(values->items[2]);
				istep = step;
				if (istep == step || DTEST(step,0.0)) p += sprintf(p,"step: %d",istep);
				else p += sprintf(p,"step: %lf",step);
			}
			labels = json_object_get_array(o,"labels");
			dprintf(dlevel,"labels: %p\n", labels);
			if (labels && labels->count) {
				p += sprintf(p," (");
				for(j=0; j < labels->count; j++) {
					if (j) p += sprintf(p,",");
					p += sprintf(p,"%s",json_value_get_string(labels->items[j]));
				}
				p += sprintf(p,")");
			}
			units = json_object_get_string(o,"units");
			if (units && strlen(units))  p += sprintf(p," >%s<", units);
		} else {
			p += sprintf(p,SCOPE_FORMAT,dtype);
			if (strcmp(dtype,"bol")==0) p += sprintf(p,"true/false");
		}
#endif
		printf("%s\n",line);
//	}
	return 0;
}

static int do_list_section(char *target, json_object_t *o) {
	json_array_t *a;
	int i,type;

	/* A section is a single object with array of items */
	printf("  %s\n", o->names[0]);
	dprintf(dlevel,"target: %s, type: %s\n", target, json_typestr(json_value_get_type(o->values[0])));
	if (json_value_get_type(o->values[0]) != JSON_TYPE_ARRAY) return 1;
	a = json_value_array(o->values[0]);
	dprintf(dlevel,"a->count: %d\n", a->count);
	for(i = 0; i < a->count; i++) {
		type = json_value_get_type(a->items[i]);
		dprintf(dlevel,"item[%d]: type: %s\n", i, json_typestr(type));
		if (json_value_get_type(a->items[i]) != JSON_TYPE_OBJECT) continue;
		if (do_list_item(target,2,json_value_object(a->items[i]),i == 0)) return 1;
	}
	return 0;
}

void do_list(client_agentinfo_t *ap) {
	json_object_t *o,*o2;
	json_array_t *a,*a2;
	int i,j,k,sec,type;

	if (!ap->info) return;
	o = json_value_object(ap->info);
	if (!o) return ;
	for(i=0; i < o->count; i++) {
		dprintf(dlevel,"label[%d]: %s\n",i,o->names[i]);
		if (strcmp(o->names[i],"configuration")!=0) continue;
		/* Configuration is an array of objects */
		if (json_value_get_type(o->values[i]) != JSON_TYPE_ARRAY) {
			printf("  invalid info format(configuration section is not an array)\n");
			return;
		}
		/* Check array for sections */
		sec = 0;
		a = json_value_array(o->values[i]);
		dprintf(dlevel,"a->count: %d\n", a->count);
		for(j=0; j < a->count; j++) {
			/* Better be an object */
			type = json_value_get_type(a->items[j]);
			dprintf(dlevel,"[%d]: type: %d(%s)\n", j, type, json_typestr(type));
			if (type != JSON_TYPE_OBJECT) {
				printf("  invalid info format (configuration not array of objects)\n");
				return;
			}
			/* if this object has 1 entry and it's an array, it's a section */
			o2 = json_value_object(a->items[j]);
			type = json_value_get_type(o2->values[0]);
			dprintf(dlevel,"o2->count: %d, type: %d(%s)\n", o2->count, type, json_typestr(type));
			if (o2->count == 1 && type == JSON_TYPE_ARRAY) {
				dprintf(dlevel,"setting section flag...\n");
				sec = 1;
			}
		}
		/* If 1 entry is a section, they must all be sections */
		dprintf(dlevel,"sec: %d\n", sec);
		if (sec) {
			for(j=0; j < a->count; j++) {
				o2 = json_value_object(a->items[j]);
				type = json_value_get_type(o2->values[0]);
				if (o2->count != 1 || type != JSON_TYPE_ARRAY) {
					printf("  invalid info format (mixed sections and non sections)\n");
					return;
				}
				/* All array values must be objects */
				a2 = json_value_array(o2->values[0]);
//				dprintf(dlevel,"a2->count: %d\n", a2->count);
				for(k=0; k < a2->count; k++) {
					if (json_value_get_type(a2->items[k]) != JSON_TYPE_OBJECT) {
						printf("  invalid info format (configuration item not an object)\n");
						return;
					}
				}
			}
		}

		for(j=0; j < a->count; j++) {
			o2 = json_value_object(a->items[j]);
			if (sec) {
				do_list_section(ap->target,o2);
			} else {
				do_list_item(ap->target,1,o2,j==0);
			}
		}
	}
}
