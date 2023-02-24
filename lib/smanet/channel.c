
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 6
#include "debug.h"

#include "smanet_internal.h"
#include <sys/stat.h>
#ifdef __WIN32
#include <windows.h>
#endif
#include <errno.h>

#define CHANFILE_SIG1		0xd5
#define CHANFILE_SIG2 		0xaa
#define CHANFILE_VERSION 	2

int smanet_destroy_channels(smanet_session_t *s) {
	register int i;

	if (!s->chans) return 0;

	for(i=0; i < s->chancount; i++) {
		if (s->chans[i].mask & CH_STATUS)
			list_destroy(s->chans[i].strings);
	}
	free(s->chans);
	s->chans = 0;
	return 0;
}

static int parse_channels(smanet_session_t *s, uint8_t *data, int data_len) {
	smanet_channel_t newchan,*c;
	register uint8_t *sptr, *eptr;
	int type,format,i;
	list channels;

	/* Create a temp list to hold the channels */
	channels = list_create();

	/* Now parse the structure into channels */
	dprintf(dlevel,"data_len: %d\n", data_len);
	sptr = data;
	eptr = data + data_len;
	i = 0;
	while(sptr < eptr) {
		memset(&newchan,0,sizeof(newchan));
		newchan.id = i++;
		newchan.index = *sptr++;
		newchan.mask = getshort(sptr);
		sptr += 2;
		newchan.format = getshort(sptr);
		sptr += 2;
		newchan.level = getshort(sptr);
		sptr += 2;
		memcpy(newchan.name,sptr,16);
		sptr += 16;
		type = newchan.mask & 0x0f;
		dprintf(dlevel,"type: %x\n",type);
		switch(type) {
		case CH_ANALOG:
			memcpy(newchan.unit,sptr,8);
			sptr += 8;
			newchan.gain = _getf32(sptr);
			sptr += 4;
			newchan.offset = _getf32(sptr);
			sptr += 4;
			dprintf(dlevel,"unit: %s, gain: %f, offset: %f\n", newchan.unit, newchan.gain, newchan.offset);
			break;
		case CH_DIGITAL:
			memcpy(newchan.txtlo,sptr,16);
			sptr += 16;
			memcpy(newchan.txthi,sptr,16);
			sptr += 16;
			dprintf(dlevel,"txtlo: %s, txthi: %s\n", newchan.txtlo, newchan.txthi);
			break;
		case CH_COUNTER:
			memcpy(newchan.unit,sptr,8);
			sptr += 8;
			newchan.gain = _getf32(sptr);
			sptr += 4;
			dprintf(dlevel,"unit: %s, gain: %f\n", newchan.unit, newchan.gain);
			break;
		case CH_STATUS:
			newchan.size = _getu16(sptr);
			dprintf(dlevel,"newchan.size: %d\n",newchan.size);
			sptr += 2;
			{
				register int i;
				for(i=0; i < newchan.size - 1; i++) {
					if (sptr[i] == 0)
						sptr[i] = ',';
				}
			}
			dprintf(dlevel,"fixed status: %s\n", sptr);
			newchan.strings = list_create();
			conv_type(DATA_TYPE_STRING_LIST,newchan.strings,0,DATA_TYPE_STRING,sptr,strlen((char *)sptr));
			dprintf(dlevel,"count: %d\n", list_count(newchan.strings));
			sptr += newchan.size;
			break;
		default:
			log_error("parse_channels: unhandled type: %x\n", type);
			sptr = eptr;
			break;
		}
		/* Set the type */
		format = newchan.format & 0x0f;
		dprintf(dlevel,"format: %x\n",format);
		switch(format) {
		case CH_BYTE:
			newchan.type = DATA_TYPE_BYTE;
			break;
		case CH_SHORT:
			newchan.type = DATA_TYPE_SHORT;
			break;
		case CH_LONG:
			newchan.type = DATA_TYPE_LONG;
			break;
		case CH_FLOAT:
			newchan.type = DATA_TYPE_FLOAT;
			break;
		case CH_DOUBLE:
			newchan.type = DATA_TYPE_DOUBLE;
			break;
		case CH_BYTE | CH_ARRAY:
			newchan.type = DATA_TYPE_BYTE_ARRAY;
			break;
		case CH_SHORT | CH_ARRAY:
			newchan.type = DATA_TYPE_SHORT_ARRAY;
			break;
		case CH_LONG | CH_ARRAY:
			newchan.type = DATA_TYPE_LONG_ARRAY;
			break;
		case CH_FLOAT | CH_ARRAY:
			newchan.type = DATA_TYPE_FLOAT_ARRAY;
			break;
		case CH_DOUBLE | CH_ARRAY:
			newchan.type = DATA_TYPE_DOUBLE_ARRAY;
			break;
		default:
			newchan.type = DATA_TYPE_UNKNOWN;
			log_error("parse_channels: unhandled format: %x\n", format);
			break;
		}
		if (format & CH_ARRAY) {
			newchan.count = (newchan.format & 0xf0) >> 8;
			printf("===> channel %s count: %d\n", newchan.name, newchan.count);
		}
		dprintf(dlevel,"newchan: id: %d, name: %s, index: %02x, mask: %04x, format: %04x, level: %d, type: %d(%s), count: %d\n",
			newchan.id, newchan.name, newchan.index, newchan.mask, newchan.format, newchan.level, newchan.type, typestr(newchan.type), newchan.count);
		list_add(channels,&newchan,sizeof(newchan));
	}

	/* Now that we know how many channels we have, alloc the mem and copy the list */
	dprintf(dlevel,"count: %d\n", list_count(channels));
	s->chans = malloc(sizeof(smanet_channel_t)*list_count(channels));
	if (!s->chans) {
		log_syserror("smanet_parse_channels: malloc");
		return 1;
	}
	i = 0;
	list_reset(channels);
	while((c = list_get_next(channels)) != 0) s->chans[i++] = *c;
	s->chancount = i;
	list_destroy(channels);
	return 0;
}

extern char SOLARD_LIBDIR[256];

int smanet_read_channels(smanet_session_t *s) {
	smanet_packet_t *p;
	char name[320];
	FILE *fp;
	struct stat sb;

//	sprintf(name,"%s/%s.dat",SOLARD_LIBDIR,s->type);
	sprintf(name,"/tmp/%s.dat",s->type);
	dprintf(dlevel,"name: %s\n", name);
	if (stat(name,&sb) == 0) {
		uint8_t *data;
		int r;

		fp = fopen(name,"rb");
		if (fp) {
			data = malloc(sb.st_size);
			if (data) {
				fread(data,1,sb.st_size,fp);
				r = parse_channels(s,data,sb.st_size);
				free(data);
				return r;
			}
			fclose(fp);
		}
	}

	p = smanet_alloc_packet(8192);
	if (!p) return 1;
	if (smanet_command(s,CMD_GET_CINFO,p,0,0)) return 1;
	dprintf(dlevel,"p->dataidx: %d\n", p->dataidx);
//	if (debug > 0) bindump("channel data",p->data,p->dataidx);

	fp = fopen(name,"wb+");
	if (fp) {
		fwrite(p->data,1,p->dataidx,fp);
		fclose(fp);
	}

	return parse_channels(s,p->data,p->dataidx);
}

int smanet_save_channels(smanet_session_t *s, char *filename) {
	json_value_t *v;
	json_object_t *o;
	json_array_t *a;
	register int i;
	smanet_channel_t *c;
	char *j;
	FILE *fp;

	/* make sure we can open the file first */
	fp = fopen(filename,"w+");
	if (!fp) {
		log_syserror("fopen(%s)",filename);
		return 1;
	}

	a = json_create_array();
	for(i=0; i < s->chancount; i++) {
		o = json_create_object();
		c = &s->chans[i];
		json_object_set_number(o,"id",c->id);
		json_object_set_number(o,"index",c->index);
		json_object_set_number(o,"mask",c->mask);
		json_object_set_number(o,"format",c->format);
		json_object_set_number(o,"type",c->type);
		json_object_set_number(o,"count",c->count);
		json_object_set_number(o,"level",c->level);
		json_object_set_string(o,"name",c->name);
		json_object_set_string(o,"unit",c->unit);
		switch(c->mask & 0x0f) {
		case CH_ANALOG:
			json_object_set_number(o,"gain",c->gain);
			json_object_set_number(o,"offset",c->offset);
			break;
		case CH_DIGITAL:
			json_object_set_string(o,"txtlo",c->txtlo);
			json_object_set_string(o,"txthi",c->txthi);
			break;
		case CH_COUNTER:
			json_object_set_number(o,"gain",c->gain);
			break;
		case CH_STATUS:
			v = json_from_type(DATA_TYPE_STRING_LIST,c->strings,list_count(c->strings));
			json_object_add_value(o,"strings",v);
			break;
		default:
			log_error("smanet_save_channels: unhandled type: %x\n", c->mask & 0x0f);
			break;
		}
		json_array_add_object(a,o);
	}

	j = json_dumps(json_array_get_value(a),1);
	if (!j) {
		log_error("smanet_save_channels: error getting json string");
		json_destroy_array(a);
		fclose(fp);
		return 1;
	}

	fwrite(j, 1, strlen(j), fp);
	fclose(fp);
	free(j);
//	json_destroy_array(a);
	return 0;
}

int smanet_load_channels(smanet_session_t *s, char *filename) {
	json_value_t *rv,*v;
	json_array_t *a;
	json_object_t *o;
	int size,i,j;
	smanet_channel_t *c;
	char *n;

	dprintf(dlevel,"filename: %s\n", filename);
	s->chancount = 0;
	rv = json_parse_file(filename);
	dprintf(dlevel,"rv: %p\n", rv);
	if (!rv) {
		sprintf(s->errmsg,"unable to load SMANET channels(%s): %s", filename, strerror(errno));
		return 1;
	}
	dprintf(dlevel,"rv type: %s\n", json_typestr(json_value_get_type(rv)));
	if (json_value_get_type(rv) != JSON_TYPE_ARRAY) {
		sprintf(s->errmsg,"invalid json format");
		json_destroy_value(rv);
		return 1;
	}
	a = json_value_array(rv);
	size = sizeof(smanet_channel_t)*a->count;
	dprintf(dlevel,"size: %d\n", size);
	s->chans = malloc(size);
	dprintf(dlevel,"chans: %p\n", s->chans);
	if (!s->chans) {
		log_syserror("smanet_load_channels: malloc(%d)", size);
		sprintf(s->errmsg,"memory allocation error");
		json_destroy_value(rv);
		return 1;
	}
	memset(s->chans,0,size);
	dprintf(dlevel,"a->count: %d\n", a->count);
	for(i=0; i < a->count; i++) {
//		dprintf(dlevel,"item[%d] type: %s\n", i, json_typestr(json_value_get_type(a->items[i])));
		if (json_value_get_type(a->items[i]) != JSON_TYPE_OBJECT) {
			sprintf(s->errmsg,"invalid json format");
			free(s->chans);
			s->chans = 0;
			json_destroy_value(rv);
			return 1;
		}
		o = json_value_object(a->items[i]);
		c = &s->chans[i];
		for(j=0; j < o->count; j++) {
			n = o->names[j];
			v = o->values[j];
			if (strcmp(n,"id") == 0) json_to_type(DATA_TYPE_U16,&c->id,0,v);
			else if (strcmp(n,"index") == 0) json_to_type(DATA_TYPE_U8,&c->index,0,v);
			else if (strcmp(n,"mask") == 0) json_to_type(DATA_TYPE_U16,&c->mask,0,v);
			else if (strcmp(n,"format") == 0) json_to_type(DATA_TYPE_U16,&c->format,0,v);
			else if (strcmp(n,"type") == 0) json_to_type(DATA_TYPE_INT,&c->type,0,v);
			else if (strcmp(n,"count") == 0) json_to_type(DATA_TYPE_INT,&c->count,0,v);
			else if (strcmp(n,"level") == 0) json_to_type(DATA_TYPE_U16,&c->level,0,v);
			else if (strcmp(n,"name") == 0) json_to_type(DATA_TYPE_STRING,c->name,sizeof(c->name)-1,v);
			else if (strcmp(n,"unit") == 0) json_to_type(DATA_TYPE_STRING,c->unit,sizeof(c->unit)-1,v);
			else if (strcmp(n,"gain") == 0) json_to_type(DATA_TYPE_F32,&c->gain,0,v);
			else if (strcmp(n,"offset") == 0) json_to_type(DATA_TYPE_F32,&c->offset,0,v);
			else if (strcmp(n,"txtlo") == 0) json_to_type(DATA_TYPE_STRING,c->txtlo,sizeof(c->txtlo)-1,v);
			else if (strcmp(n,"txthi") == 0) json_to_type(DATA_TYPE_STRING,c->txthi,sizeof(c->txthi)-1,v);
			else if (strcmp(n,"strings") == 0) {
				c->strings = list_create();
				json_to_type(DATA_TYPE_STRING_LIST,c->strings,0,v);
			}
		}
//		printf("chan: id: %d, name: %s, index: %02x, mask: %04x, format: %04x, level: %d, type: %d(%s), count: %d\n", c->id, c->name, c->index, c->mask, c->format, c->level, c->type, typestr(c->type), c->count);
	}
	s->chancount = a->count;
	dprintf(dlevel,"new chancount: %d\n", s->chancount);
	dprintf(dlevel,"destroying rv\n");
	json_destroy_value(rv);
	return 0;
}

smanet_channel_t *smanet_get_channel(smanet_session_t *s, char *name) {
	register int i;

	dprintf(dlevel,"name: %s\n", name);

	dprintf(dlevel,"s->chans: %p, s->chancount: %d\n", s->chans, s->chancount);
	if (!s->chans || !s->chancount) {
		sprintf(s->errmsg, "SMANET channels not loaded");
		return 0;
	}

	for(i=0; i < s->chancount; i++) {
		if (strcmp(s->chans[i].name,name) == 0) {
			dprintf(dlevel,"found!\n");
			return &s->chans[i];
		}
	}
	dprintf(dlevel,"NOT found!\n");
	return 0;
}

int smanet_get_chaninfo(smanet_session_t *s, smanet_chaninfo_t *info) {
	dprintf(dlevel,"connected: %d\n", s->connected);
	if (!s->connected) {
		strcpy(s->errmsg,"not connected");
		return 1;
	}
	dprintf(dlevel,"chancount: %d\n",s->chancount);
	info->channels = s->chans;
	info->chancount = s->chancount;
	return 0;
}
