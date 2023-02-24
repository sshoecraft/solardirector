
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 1
#include "debug.h"

#include "sb.h"
#include "pvinverter.h"

#define SB_INIT_BUFSIZE 4096

static size_t getdata(void *ptr, size_t size, size_t nmemb, void *ctx) {
	sb_session_t *s = ctx;
	int bytes,newidx;

//	printf("%s\n",(char *)ptr);

	bytes = size*nmemb;
//	dprintf(dlevel,"bytes: %d, bufidx: %d, bufsize: %d\n", bytes, s->bufidx, s->bufsize);
	newidx = s->bufidx + bytes;
//	dprintf(dlevel,"newidx: %d\n", newidx);
	if (newidx >= s->bufsize) {
		char *newbuf;
		int newsize;

		newsize = (newidx / 4096) + 1;
		dprintf(dlevel+1,"newsize: %d\n", newsize);
		newsize *= 4096;
		dprintf(dlevel+1,"newsize: %d\n", newsize);
		newbuf = realloc(s->buffer,newsize);
		if (!newbuf) {
			log_syserror("sb_getdata: realloc(buffer,%d)",newsize);
			return 0;
		}
		s->buffer = newbuf;
		s->bufsize = newsize;
	}
	strcpy(s->buffer + s->bufidx,ptr);
	s->bufidx = newidx;

	return bytes;
}

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

json_value_t *sb_request(sb_session_t *s, char *func, char *fields) {
	char url[128],*p;
	CURLcode res;

	dprintf(dlevel,"func: %s, fields: %s\n", func, fields);
	dprintf(dlevel,"endpoint: %s\n", s->endpoint);
	if (!*s->endpoint) return 0;

	s->bufidx = 0;

	p = url;
	if (strncmp(s->endpoint,"http",4) != 0) p += sprintf(p,"https://");
	p += sprintf(p,"%s/%s",s->endpoint,func);
	if (*s->session_id) p += sprintf(p,"?sid=%s",s->session_id);
	dprintf(dlevel,"url: %s\n", url);
	curl_easy_setopt(s->curl, CURLOPT_URL, url);
	if (fields) curl_easy_setopt(s->curl, CURLOPT_POSTFIELDS, fields);
	else curl_easy_setopt(s->curl, CURLOPT_POSTFIELDS, "{}");
	res = curl_easy_perform(s->curl);
	if (res != CURLE_OK) {
		log_error("sb_request failed: %s\n", curl_easy_strerror(res));
		sb_driver.close(s);
		return 0;
	}

	/* Parse the buffer */
	dprintf(1,"bufidx: %d\n", s->bufidx);
	return json_parse(s->buffer);
}

static int curl_init(sb_session_t *s) {
	struct curl_slist *hs = curl_slist_append(0, "Content-Type: application/json");

	s->curl = curl_easy_init();
	if (!s->curl) {
		log_syserror("sb_curl_init: curl_easy_init");
		return 1;
	}

//	curl_easy_setopt(s->curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(s->curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(s->curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(s->curl, CURLOPT_HTTPHEADER, hs);
	curl_easy_setopt(s->curl, CURLOPT_WRITEFUNCTION, getdata);
	curl_easy_setopt(s->curl, CURLOPT_WRITEDATA, s);
	return 0;
}

static void *sb_new(void *driver, void *driver_handle) {
	sb_session_t *s;

	s = calloc(1,sizeof(*s));
	if (!s) {
		log_syserror("sb_new: calloc");
		return 0;
	}
	if (curl_init(s)) {
		free(s);
		return 0;
	}
	s->buffer = malloc(SB_INIT_BUFSIZE);
	if (!s->buffer) {
		log_syserror("sb_new: malloc(%d)",SB_INIT_BUFSIZE);
		curl_easy_cleanup(s->curl);
		free(s);
		return 0;
	}
	s->bufsize = SB_INIT_BUFSIZE;
	s->bufidx = 0;

	return s;
}

static int sb_destroy(void *handle) {
	sb_session_t *s = handle;

	free(s->buffer);
	curl_easy_cleanup(s->curl);
	free(s);
	return 0;
}

static int sb_open(void *handle) {
	sb_session_t *s = handle;
	json_value_t *v;
	list results;

	if (!s) return 1;

	dprintf(dlevel,"open: %d\n", check_state(s,SB_STATE_OPEN));
	if (check_state(s,SB_STATE_OPEN)) return 0;

	/* Create the login info */
	if (!s->login_fields) {
		json_object_t *o;

		o = json_create_object();
		json_object_set_string(o,"right",s->user);
		json_object_set_string(o,"pass",s->password);
		s->login_fields = json_dumps(json_object_value(o),0);
		if (!s->login_fields) {
			log_syserror("sb_open: create login fields: %s\n", strerror(errno));
			return 1;
		}
		json_destroy_object(o);
	}

	if (!s->curl) curl_init(s);
        if (!s->curl) {
                log_syserror("sb_open: curl_init");
                return 1;
        }

	*s->session_id = 0;
	v = sb_request(s,SB_LOGIN,s->login_fields);
	if (!v) return 1;
	results = sb_get_results(s,v);
	json_destroy_value(v);
	if (!results) return 1;
	if (!*s->session_id) {
		log_error("sb_open: error getting session_id .. bad password?\n");
		return 1;
	}

	dprintf(dlevel,"session_id: %s\n", s->session_id);
	set_state(s,SB_STATE_OPEN);
	s->connected = 1;
	return 0;
}

static int sb_close(void *handle) {
	sb_session_t *s = handle;
	json_value_t *v;

	if (!s) return 1;
	dprintf(dlevel,"open: %d\n", check_state(s,SB_STATE_OPEN));
	if (!check_state(s,SB_STATE_OPEN)) return 0;

	v = sb_request(s,SB_LOGOUT,"{}");
	if (v) json_destroy_value(v);
	s->connected = 0;
	clear_state(s,SB_STATE_OPEN);
	curl_easy_cleanup(s->curl);
	s->curl = 0;
	return 0;
}


static int get_inverter(sb_session_t *s, solard_pvinverter_t *inv, list results) {
	sb_result_t *res;
	struct _sb_objmap *map;
	sb_value_t *vp;
	int found;
	struct _sb_objmap {
		char *key;
		int type;
		void *dest;
		int size;
		char *name;
	} sb_objmap[] = {
		// [6380_40451F00] DC Side -> DC measurements -> Voltage
		{ "6380_40451F00", DATA_TYPE_DOUBLE, &inv->input_voltage, 0, "input_voltage", },
		// {6380_40452100} DC Side -> DC measurements -> Current 
		{ "6380_40452100", DATA_TYPE_DOUBLE, &inv->input_current, 0, "input_current", },
		// {6380_40251E00} DC Side -> DC measurements -> Power
		{ "6380_40251E00", DATA_TYPE_DOUBLE, &inv->input_power, 0, "input_power", },
		// {6100_00464800} AC Side -> Grid measurements -> Phase voltage -> Phase L1:
		{ "6100_00464800", DATA_TYPE_DOUBLE, &inv->output_voltage, 0, "output_voltage", },
		// {6100_00464800} AC Side -> Grid measurements -> Phase voltage -> Phase L2:
		{ "6100_00464900", DATA_TYPE_DOUBLE, &inv->output_voltage, 0, "output_voltage", },
		// {6100_00464800} AC Side -> Grid measurements -> Phase voltage -> Phase L3:
		{ "6100_00464A00", DATA_TYPE_DOUBLE, &inv->output_voltage, 0, "output_voltage", },
		// {6100_00465700} AC Side -> Grid measurements -> Grid frequency
		{ "6100_00465700", DATA_TYPE_DOUBLE, &inv->output_frequency, 0, "output_frequency", },
		// {6100_40465300} AC Side -> Grid measurements -> Phase currents -> Phase L1
		{ "6100_40465300", DATA_TYPE_DOUBLE, &inv->output_current, 0, "output_current", },
		// {6100_40465400} AC Side -> Grid measurements -> Phase currents -> Phase L2
		{ "6100_40465400", DATA_TYPE_DOUBLE, &inv->output_current, 0, "output_current", },
		// {6100_40465500} AC Side -> Grid measurements -> Phase currents -> Phase L3
		{ "6100_40465500", DATA_TYPE_DOUBLE, &inv->output_current, 0, "output_current", },
		// {6100_40263F00} AC Side -> Grid measurements -> Power
		{ "6100_40263F00", DATA_TYPE_DOUBLE, &inv->output_power, 0, "output_power", },
		// {6400_00260100} AC Side -> Measured values -> Total yield
		{ "6400_00260100", DATA_TYPE_DOUBLE, &inv->total_yield, 0, "total_yield", },
		// {6400_00262200} AC Side -> Measured values -> Daily yield
		{ "6400_00262200", DATA_TYPE_DOUBLE, &inv->daily_yield, 0, "daily_yield", },
		{ 0 },
	};

	memset(inv,0,sizeof(*inv));
	strncat(inv->name,s->ap->instance_name,sizeof(inv->name)-1);
	list_reset(results);
	while((res = list_get_next(results)) != 0) {
		found = 0;
		for(map = sb_objmap; map->key; map++) {
//			dprintf(dlevel,"res->key: %s, map->key: %s\n", res->obj->key, map->key);
			if (strcmp(res->obj->key, map->key) == 0) {
				found = 1;
				break;
			}
		}
		if (found) {
			double val,total;

			total = 0;
			list_reset(res->values);
			while((vp = list_get_next(res->values)) != 0) {
				conv_type(DATA_TYPE_DOUBLE,&val,sizeof(val),vp->type,vp->data,vp->len);
				total += val;
//				dprintf(dlevel+1,"key: %s(%s), val: %f, total: %f\n", map->key, map->name, val, total);
			}
			*((double *)map->dest) += total;
		}
	}
//	pvinverter_dump(inv,0);

	return 0;
}

static int sb_read(void *handle, uint32_t *control, void *buf, int buflen) {
	sb_session_t *s = handle;
	solard_pvinverter_t inv;
	json_value_t *v;
	list results;
	int r;

	/* Open if not already open */
        if (sb_driver.open(s)) return 1;

	/* Read the values */
	if (!s->read_fields) s->read_fields = sb_mkfields(0,0);
	v = sb_request(s,SB_ALLVAL,s->read_fields);
	dprintf(dlevel,"v: %p\n", v);
	if (!v) {
		sb_close(s);
		return 1;
	}

	results = sb_get_results(s,v);
	json_destroy_value(v);
	dprintf(dlevel,"results: %p\n", results);
	if (!results) {
		sb_close(s);
		return 1;
	}

	/* convert results to inverter data */
	r = get_inverter(s,&inv,results);
	sb_destroy_results(results);
	if (r) return 1;

#ifdef JS
	/* If read script exists, we'll use that */
	if (agent_script_exists(s->ap, s->ap->js.read_script)) return 0;
#endif
	v = pvinverter_to_json(&inv);
	if (!v) return 1;
#ifdef MQTT
	if (mqtt_connected(s->ap->m)) agent_pubdata(s->ap, v);
#endif
#ifdef INFLUX
	if (influx_connected(s->ap->i)) influx_write_json(s->ap->i, "pvinverter", v);
#endif
	json_destroy_value(v);

	if ((inv.output_power != s->last_power) && (s->log_power == true)) {
		log_info("%.1f\n",inv.output_power);
		s->last_power = inv.output_power;
	}
	return 0;
}

solard_driver_t sb_driver = {
	"sb",
	sb_new,				/* New */
	sb_destroy,			/* Destroy */
	sb_open,			/* Open */
	sb_close,			/* Close */
	sb_read,			/* Read */
	0,				/* Write */
	sb_config			/* Config */
};
