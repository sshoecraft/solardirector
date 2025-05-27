#ifdef INFLUX

/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 4
#include "debug.h"

#include <string.h>
//#include "common.h"
#include "influx.h"
#include <curl/curl.h>
#include "json.h"
#ifdef JS
#include "jsnum.h" /* for JSDOUBLE_IS_NaN */
#include "jsclass.h"
#endif

#define INFLUX_ENDPOINT_SIZE 128
#define INFLUX_DATABASE_SIZE 64
#define INFLUX_USERNAME_SIZE 32
#define INFLUX_PASSWORD_SIZE 32

#define INFLUX_INIT_BUFSIZE 4096
#define SESSION_ID_SIZE 128
static list influx_sessions = 0;

struct influx_session {
	bool enabled;
	char endpoint[INFLUX_ENDPOINT_SIZE];
	char database[INFLUX_DATABASE_SIZE];
	char username[INFLUX_USERNAME_SIZE];
	char password[INFLUX_PASSWORD_SIZE];
	char epoch[16];
	bool connected;
	int errcode;
	char errmsg[128];
	CURL *curl;
	struct curl_slist *hs;
	char version[8];
	char session_id[SESSION_ID_SIZE];
	char *buffer;                   /* Read buffer */
	int bufsize;                    /* Buffer size */
	int bufidx;                     /* Current write pos */
	list responses;
	char *login_fields;
	char *read_fields;
	char *config_fields;
	bool verbose;
	bool convdt;
	bool timeout;
	bool ctor;
};
typedef struct influx_session influx_session_t;

int influx_parse_config(influx_session_t *s, char *influx_info) {
	dprintf(dlevel,"info: %s\n", influx_info);
	strncpy(s->endpoint,strele(0,",",influx_info),sizeof(s->endpoint)-1);
	strncpy(s->database,strele(1,",",influx_info),sizeof(s->database)-1);
	strncpy(s->username,strele(2,",",influx_info),sizeof(s->username)-1);
	strncpy(s->password,strele(3,",",influx_info),sizeof(s->password)-1);
	dprintf(dlevel,"endpoint: %s, database: %s, user: %s, pass: %s\n", s->endpoint, s->database, s->username, s->password);
	return 0;
}

#if 0
static size_t influx_getheader(void *ptr, size_t size, size_t nmemb, void *ctx) {
	influx_session_t *s = ctx;
	char *version_line = "X-Influxdb-Version";
	char line[1024];
	int bytes = size*nmemb;

	memcpy(line,ptr,bytes);
	line[bytes] = 0;
	trim(line);
//	dprintf(dlevel,"header line(%d): %s\n", strlen(line), line);

	if (strncmp(line,version_line,strlen(version_line)) == 0)
		strncpy(s->version,strele(1,": ",line),sizeof(s->version)-1);
	return bytes;
}
#endif

static size_t getdata(void *ptr, size_t size, size_t nmemb, void *ctx) {
	influx_session_t *s = ctx;
	int bytes,newidx;

	if (s->verbose) log_info("data: %s\n",(char *)ptr);

	dprintf(dlevel,"bufsize: %d\n", s->bufsize);

	bytes = size*nmemb;
	dprintf(dlevel,"bytes: %d, bufidx: %d, bufsize: %d\n", bytes, s->bufidx, s->bufsize);
	newidx = s->bufidx + bytes;
	dprintf(dlevel+1,"newidx: %d\n", newidx);
	if (newidx >= s->bufsize) {
		char *newbuf;
		int newsize;

		newsize = (newidx / 4096) + 1;
		dprintf(dlevel+1,"newsize: %d\n", newsize);
		newsize *= 4096;
		dprintf(dlevel+1,"newsize: %d\n", newsize);
//		printf("*** influx_getdata: expanding buffer to: %d\n", newsize);
		newbuf = realloc(s->buffer,newsize);
		dprintf(dlevel,"newbuf: %p\n", newbuf);
		if (!newbuf) {
			log_syserror("influx_getdata: realloc(buffer,%d)",newsize);
			return 0;
		}
		s->buffer = newbuf;
		s->bufsize = newsize;
	}
	strcpy(s->buffer + s->bufidx,ptr);
	s->bufidx = newidx;

	return bytes;
}

influx_session_t *influx_new(void) {
	influx_session_t *s;

	s = malloc(sizeof(*s));
	if (!s) return 0;
	memset(s,0,sizeof(*s));
	s->responses = list_create();

	s->curl = curl_easy_init();
	if (!s->curl) {
		log_syserror("influx_new: curl_init");
		free(s);
		return 0;
	}
 	s->hs = curl_slist_append(0, "Content-Type: application/x-www-form-urlencoded");

#if 0
	/* init data buffer */
	s->buffer = malloc(INFLUX_INIT_BUFSIZE);
	if (!s->buffer) {
		log_syserror("influx_new: malloc(%d)",INFLUX_INIT_BUFSIZE);
		curl_easy_cleanup(s->curl);
		free(s);
		return 0;
	}
	s->bufsize = INFLUX_INIT_BUFSIZE;
#endif

	curl_easy_setopt(s->curl, CURLOPT_HTTPHEADER, s->hs);
	curl_easy_setopt(s->curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(s->curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(s->curl, CURLOPT_WRITEFUNCTION, getdata);
	curl_easy_setopt(s->curl, CURLOPT_WRITEDATA, s);

	if (strlen(s->username)) {
		curl_easy_setopt(s->curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
		curl_easy_setopt(s->curl, CURLOPT_USERPWD, s->username);
		curl_easy_setopt(s->curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
		curl_easy_setopt(s->curl, CURLOPT_USERPWD, s->password);
	}

	if (!influx_sessions) influx_sessions = list_create();
	list_add(influx_sessions, s, 0);
	return s;
}

int influx_enable(influx_session_t *s, int enabled) {
	int old_enabled = s->enabled;
	s->enabled = enabled;
	return old_enabled;
}

int influx_set_db(influx_session_t *s, char *name) {
	strncpy(s->database,name,sizeof(s->database)-1);
	return 0;
}

#if 0
static void display_value(influx_value_t *vp) {
	char value[2048];

	printf("vp: %p\n", vp);
	printf("type: %s\n", typestr(vp->type));
	conv_type(DATA_TYPE_STRING,value,sizeof(value),vp->type,vp->data,vp->len);
	printf("value: %s\n", value);
}

static void display_series(influx_series_t *sp) {
	int i,j;

	printf("sp: %p\n", sp);
	printf("name: %s\n", sp->name);
	printf("column_count: %d\n", sp->column_count);
	for(i=0; i < sp->column_count; i++) printf("column[%d]: %s\n", i, sp->columns[i]);
	printf("values: %p\n", sp->values);
	printf("value_count: %d\n", sp->value_count);
	for(i=0; i < sp->value_count; i++) {
		for(j=0; j < sp->column_count; j++) {
			printf("value[%d][%d]: %p\n", i, j, sp->values[i][j]);
			display_value(sp->values[i][j]);
		}
	}
}

static void display_result(influx_result_t *rp) {
	influx_series_t *sp;
//	int i;

	printf("--------------------------\n");
	printf("rp: %p\n", rp);
	printf("statement_id: %d\n", rp->statement_id);
	printf("errmsg: %s\n", rp->errmsg);
//	printf("series count: %d\n", rp->series_count);
	printf("series count: %d\n", list_count(rp->series));
//	for(i=0; i < rp->series_count; i++) display_series(rp->series[i]);
	list_reset(rp->series);
	while(sp = list_get_next(r->series)) != 0) display_series(sp);
}

static void display_response(influx_response_t *r) {
	influx_result_t *rp;
//	int i;

	printf("r: %p\n", r);
	printf("error: %d\n", r->error);
//	printf("result_count: %d\n", r->result_count);
	printf("result_count: %d\n", list_count(r->results));
//	for(i=0; i < r->result_count; i++) display_result(r->results[i]);
	list_reset(r->results);
	while(rp = list_get_next(r->results)) != 0) display_result(rp);
}
#endif

int influx_destroy_series(influx_series_t *sp) {
	int i,j;
	influx_value_t *vp;

	dprintf(dlevel,"sp: %p\n", sp);
	if (!sp) return 0;

	dprintf(dlevel,"refs: %d\n", sp->refs);
	if (sp->refs) return 1;

	dprintf(dlevel,"sp->columns: %p\n", sp->columns);
	if (sp->columns) {
		dprintf(dlevel,"column_count: %d\n", sp->column_count);
		for(i=0; i < sp->column_count; i++) {
			dprintf(dlevel,"columns[%d]: %p\n", i, sp->columns[i]);
			if (!sp->columns[i]) break;
			free(sp->columns[i]);
		}
		free(sp->columns);
		sp->columns = 0;
	}
	dprintf(dlevel,"sp->values: %p\n", sp->values);
	if (sp->values) {
		dprintf(dlevel,"value_count: %d\n", sp->value_count);
		for(i=0; i < sp->value_count; i++) {
			dprintf(dlevel,"values[%d]: %p\n", i, sp->values[i]);
			if (!sp->values[i]) continue;
			for(j=0; j < sp->column_count; j++) {
				vp = sp->values[i][j];
				if (vp && vp->data) {
					dprintf(dlevel,"freeing value[%d][%d]\n",i,j);
					free(vp->data);
				}
			}
			free(sp->values[i]);
		}
		free(sp->values);
		sp->values = 0;
	}

	if (sp->parent) {
		dprintf(dlevel,"sp->parent: %p\n", sp->parent);
		list_delete(sp->parent->series,sp);
	}
	return 0;
}

int influx_destroy_result(influx_result_t *rp) {
	influx_series_t *sp;
	int c;

	int ldlevel = dlevel;

	dprintf(ldlevel,"rp: %p\n", rp);
	if (!rp) return 0;

	dprintf(ldlevel,"refs: %d\n", rp->refs);
	if (rp->refs) return 1;

	/* Destroy series */
	list_reset(rp->series);
	while((sp = list_get_next(rp->series)) != 0) influx_destroy_series(sp);
	c = list_count(rp->series);
	dprintf(ldlevel,"c: %d\n", c);
	if (!c) {
		list_destroy(rp->series);
		if (rp->parent) {
			dprintf(ldlevel,"rp->parent: %p\n", rp->parent);
			list_delete(rp->parent->results,rp);
		}
	}
	return c;
}

int influx_destroy_response(influx_response_t *r) {
	influx_result_t *rp;
	int c;

	dprintf(dlevel,"r: %p\n", r);
	if (!r) return 0;

	dprintf(dlevel,"refs: %d\n", r->refs);
	if (r->refs) return 1;

	list_reset(r->results);
	while((rp = list_get_next(r->results)) != 0) influx_destroy_result(rp);
	c = list_count(r->results);
	if (!c) {
		list_destroy(r->results);
		dprintf(dlevel,"r->parent: %p\n", r->parent);
		if (r->parent) {
			dprintf(dlevel,"r->parent->responses: %p\n", r->parent->responses);
			list_delete(r->parent->responses,r);
		}
	}
	return c;
}

static void influx_cleanup(influx_session_t *s) {
	influx_response_t *r;
	if (s->buffer) {
		free(s->buffer);
		s->buffer = 0;
		s->bufsize = 0;
	}
	list_reset(s->responses);
	while((r = list_get_next(s->responses)) != 0) influx_destroy_response(r);
}

void influx_destroy_session(influx_session_t *s) {

	dprintf(dlevel,"s: %s\n", s);
	if (!s) return;

	influx_cleanup(s);
	if (s->buffer) free(s->buffer);
	curl_slist_free_all(s->hs);
	curl_easy_cleanup(s->curl);

	if (influx_sessions) list_delete(influx_sessions,s);
	free(s);
}

void influx_shutdown(void) {
	influx_session_t *s;

	if (influx_sessions) {
		while(true) {
			list_reset(influx_sessions);
			s = list_get_next(influx_sessions);
			if (!s) break;
			influx_destroy_session(s);
		}
		list_destroy(influx_sessions);
		influx_sessions = 0;
	}
}

char *influx_mkurl(influx_session_t *s, char *action, char *query) {
	char *url,*eq,*p;
	int size,db;

	dprintf(dlevel,"s: %p\n", s);
	if (!s) return 0;

	if (action) {
		db = 1;
		if (strcmp(action,"health")==0) db = 0;
	} else {
		db = 0;
	}

	/* default endpoint */
	if (!strlen(s->endpoint)) strcpy(s->endpoint,"http://localhost:8086");

	size = 0;
	size += strlen(s->endpoint);
	if (action) {
		size += strlen(action) + 1; /* + "/" */
		size += strlen(s->database) + 4;	/* + "?db=" */
	}
	if (strlen(s->epoch)) size += strlen(s->epoch) + 7; /* + "&epoch=" */
	eq = (query ? curl_easy_escape(s->curl, query, strlen(query)) : 0);
	if (eq) size += strlen(eq) + 3; /* + "&q=" */
	dprintf(dlevel,"size: %d\n", size);
	url = malloc(size+1);
	dprintf(dlevel,"url: %p\n", url);
	if (!url) {
		strcpy(s->errmsg,"memory allocation error");
		return 0;
	}
	p = url;
	p += sprintf(p,"%s",s->endpoint);
	if (action) {
		p += sprintf(p,"/%s",action);
		if (db) p += sprintf(p,"?db=%s",s->database);
	}
	if (strlen(s->epoch)) p += sprintf(p,"&epoch=%s",s->epoch);
	if (eq) {
		p += sprintf(p,"&q=%s",eq);
		curl_free(eq);
	}
	dprintf(dlevel,"url: %s\n", url);
	return url;
}

static influx_value_t *get_value(json_value_t *rv) {
	influx_value_t *vp;
	int type;
	register char *p;

	vp = malloc(sizeof(*vp));
	dprintf(dlevel,"vp: %p\n", vp);
	if (!vp) return 0;
	memset(vp,0,sizeof(*vp));
	vp->type = DATA_TYPE_NULL;

	type = json_value_get_type(rv);
	dprintf(dlevel,"type: %d (%s)\n", type, json_typestr(type));
	switch(type) {
	case JSON_TYPE_NUMBER:
	    {
		vp->type = DATA_TYPE_DOUBLE;
		vp->data = malloc(sizeof(double));
		if (!vp->data) {
			free(vp);
			return 0;
		}
		*((double *)vp->data) = json_value_get_number(rv);
		dprintf(dlevel,"number: %f\n", *((double *)vp->data));
	    }
	    break;
	case JSON_TYPE_STRING:
		vp->type = DATA_TYPE_STRING;
		p = json_value_get_string(rv);
		vp->len = strlen(p);
		vp->data = strdup(p);
		if (!vp->data) {
			free(vp);
			return 0;
		}
		dprintf(dlevel,"string: %s\n", (char *)vp->data);
		break;
	}

	dprintf(dlevel,"returning: %p\n", vp);
	return vp;
}

static int _get_series(json_object_t *o, influx_series_t *sp) {
	json_array_t *columns_array,*values_array,*aa;
	int i,j,type;
	char *p;

	columns_array = values_array = 0;
	dprintf(dlevel,"o->count: %d\n", o->count);
	for(i=0; i < o->count; i++) {
		type = json_value_get_type(o->values[i]);
		dprintf(dlevel,"o->names[%d]: %s, type: %s\n", i, o->names[i], json_typestr(type));

		/* measurement name */
		if (strcmp(o->names[i],"name") == 0) {
			strncpy(sp->name,o->names[i],sizeof(sp->name));

		/* columns */
		} else if (strcmp(o->names[i],"columns") == 0 && type == JSON_TYPE_ARRAY) {
			columns_array = json_value_array(o->values[i]);

		/* values */
		} else if (strcmp(o->names[i],"values") == 0 && type == JSON_TYPE_ARRAY) {
			values_array = json_value_array(o->values[i]);
		}
	}
	/* need both or its invalid response */
	if (!columns_array || !values_array) goto get_series_error;

	/* Get the columns */
	sp->column_count = columns_array->count;
	sp->columns = calloc(sp->column_count*sizeof(char *),1);
	dprintf(dlevel,"sp->columns: %p\n", sp->columns);
	if (!sp->columns) goto get_series_error;
	dprintf(dlevel,"column_count: %d\n", sp->column_count);
	for(i=0; i < sp->column_count; i++) {
		p = json_value_get_string(columns_array->items[i]);
		dprintf(dlevel,"column[%d]: %s\n", i, p);
		if (!p) goto get_series_error;
		sp->columns[i] = strdup(p);
		if (!sp->columns[i]) goto get_series_error;
	}

	/* Get the values */
	/* Values are a multidemential array of [values][column_count] */
	sp->value_count = values_array->count;
	sp->values = calloc(sp->value_count * sizeof(influx_value_t **),1);
	dprintf(dlevel,"values: %p\n", sp->values);
	if (!sp->values) goto get_series_error;
	dprintf(dlevel,"value_count: %d\n", sp->value_count);
	for(i=0; i < sp->value_count; i++) {
		/* MUST be array of arrays */
		if (json_value_get_type(values_array->items[i]) != JSON_TYPE_ARRAY) goto get_series_error;
		aa = json_value_array(values_array->items[i]);

		/* number of values MUST equal column_count */
		dprintf(dlevel,"aa->count: %d, column_count: %d\n", aa->count, sp->column_count);
		if (aa->count != sp->column_count) goto get_series_error;

		/* get the values */
		sp->values[i] = calloc(sp->column_count * sizeof(influx_value_t *),1);
		if (!sp->values[i]) goto get_series_error;
		for(j=0; j < aa->count; j++) {
			type = json_value_get_type(aa->items[j]);
			dprintf(dlevel,"items[%d] type: %s (%d)\n", j, json_typestr(type), type);
			sp->values[i][j] = get_value(aa->items[j]);
		}
	}

	return 0;

get_series_error:
	influx_destroy_series(sp);
	return 1;
}

static int _get_result(json_object_t *o, influx_result_t *rp) {
	influx_series_t newseries,*sp;
	int i,type;

	dprintf(dlevel,"o->count: %d\n", o->count);
	for(i=0; i < o->count; i++) {
		type = json_value_get_type(o->values[i]);
		dprintf(dlevel,"o->names[%d]: %s, type: %s\n", i, o->names[i], json_typestr(type));

		/* Statement ID */
		if (strcmp(o->names[i],"statement_id") == 0 && type == JSON_TYPE_NUMBER) {
			rp->statement_id = json_value_get_number(o->values[i]);
			dprintf(dlevel,"statement_id: %d\n", rp->statement_id);
		}

		if (strcmp(o->names[i],"error") == 0 && type == JSON_TYPE_STRING) {
			strncpy(rp->errmsg,json_value_get_string(o->values[i]),sizeof(rp->errmsg)-1);
			dprintf(dlevel,"errmsg: %s\n", rp->errmsg);
		}

		/* Series */
		if (strcmp(o->names[i],"series") == 0 && type == JSON_TYPE_ARRAY) {
			json_array_t *a = json_value_array(o->values[i]);

			dprintf(dlevel,"a->count: %d\n", a->count);
			/* XXX */
			if (!rp->series) continue;
			for(i=0; i < a->count; i++) {
				type = json_value_get_type(a->items[i]);
				dprintf(dlevel,"type[%d]: %s\n", i, json_typestr(type));
				if (type != JSON_TYPE_OBJECT) continue;
				memset(&newseries,0,sizeof(newseries));
				/* XXX */
				if (_get_series(json_value_object(a->items[i]),&newseries)) continue;
				sp = list_add(rp->series,&newseries,sizeof(newseries));
				dprintf(dlevel,"sp: %p\n", sp);
			}

		}
	}
	return 0;
}

static int _get_response(influx_session_t *s, influx_response_t *r) {
	influx_result_t newresult,*rp;
	json_object_t *o;
	int type;
	json_value_t *v;
	int i;

	/* Parse the buffer */
//	dprintf(dlevel,"buffer: %s\n", s->buffer);
	dprintf(dlevel,"bufidx: %d\n", s->bufidx);
	v = json_parse(s->buffer);
	dprintf(dlevel,"v: %p\n", v);
	/* May not be valiid json data */
	if (!v) {
		r->error = true;
		sprintf(r->errmsg,"invalid json output");
		return 1;
	}

	/* top level must be object */
	type = json_value_get_type(v);
	dprintf(dlevel,"type: %s\n", json_typestr(type));
	if (type == JSON_TYPE_OBJECT) {
		o = json_value_object(v);
		dprintf(dlevel,"o->count: %d\n", o->count);
		for(i=0; i < o->count; i++) {
			type = json_value_get_type(o->values[0]);
			dprintf(dlevel,"o->names[%d]: %s, type: %s\n", i, o->names[i], json_typestr(type));
			if (strcmp(o->names[i],"results") == 0 && type == JSON_TYPE_ARRAY) {
				json_array_t *a = json_value_array(o->values[i]);
				if (!r->results) r->results = list_create();
				dprintf(dlevel,"r->results: %p\n", r->results);
				if (!r->results) {
					r->error = true;
					sprintf(r->errmsg,"memory allocation error");
					return 1;
				}
				dprintf(dlevel,"a->count: %d\n", a->count);
				for(i=0; i < a->count; i++) {
					type = json_value_get_type(a->items[i]);
					dprintf(dlevel,"type[%d]: %s\n", i, json_typestr(type));
					if (type != JSON_TYPE_OBJECT) continue;
					memset(&newresult,0,sizeof(newresult));
					newresult.series = list_create();
					dprintf(dlevel,"newresult.series: %p\n", newresult.series);
					if (!newresult.series) continue;
					if (_get_result(json_value_object(a->items[i]),&newresult)) continue;
					rp = list_add(r->results,&newresult,sizeof(newresult));
					dprintf(dlevel,"rp: %p\n", rp);
				}
			}
		}
	}
	json_destroy_value(v);
	return 0;
}

static influx_response_t *influx_request(influx_session_t *s, char *url, int post, char *data) {
	influx_response_t newresp,*r;
	influx_result_t *rp;
	influx_series_t *sp;
//	char cl[128];
	CURLcode res;
	int rc;
//,err;
	char *msg;

	memset(&newresp,0,sizeof(newresp));
	newresp.results = list_create();
	if (!newresp.results) {
		newresp.error = true;
		strcpy(newresp.errmsg,"list_create results");
		goto influx_request_done;
	}

	if (!s->enabled) {
		newresp.error = true;
		strcpy(newresp.errmsg,"not enabled");
		goto influx_request_done;
	}

	dprintf(dlevel,"url: %s, post: %d, data: %s\n", url, post, data);

	influx_cleanup(s);
	s->bufidx = 0;

	/* Make the request */
#if 0
	if (data) {
		sprintf(cl,"Content-length: %d\n", (int)strlen(data));
		curl_easy_setopt(s->curl, CURLOPT_HTTPHEADER, curl_slist_append(0,cl));
	}
#endif
	curl_easy_setopt(s->curl, CURLOPT_VERBOSE, s->verbose);
	curl_easy_setopt(s->curl, CURLOPT_URL, url);
	curl_easy_setopt(s->curl, CURLOPT_POST, post);
	curl_easy_setopt(s->curl, CURLOPT_TIMEOUT, s->timeout);
	if (post && data) curl_easy_setopt(s->curl, CURLOPT_POSTFIELDS, data);
	dprintf(dlevel,"calling perform...\n");
	res = curl_easy_perform(s->curl);
	dprintf(dlevel,"res: %d\n", res);
	if (res != CURLE_OK) {
		sprintf(s->errmsg,"influx_request failed: %s", curl_easy_strerror(res));
		return 0;
	}

	/* Get the response code */
	dprintf(dlevel,"getting rc...\n");
	curl_easy_getinfo(s->curl, CURLINFO_RESPONSE_CODE, &rc);
	dprintf(dlevel,"rc: %d\n", rc);
	msg = "";
	newresp.error = true;
	switch(rc) {
	case 200:
		msg = "Success";
		newresp.error = false;
		break;
	case 204:
		msg = "No content";
		newresp.error = false;
		break;
	case 400:
		msg = "Bad Request";
		break;
	case 401:
		msg = "Unauthorized";
		break;
	case 404:
		msg = "Not found";
		break;
	case 413:
		msg = "Request Entity Too Large";
		break;
	case 422:
		msg = "Unprocessible entity";
		break;
	case 429:
		msg = "Too many requests";
		break;
	case 500:
		msg = "Internal Server Error";
		break;
	case 503:
		msg = "Service unavailable";
		break;
	default:
		dprintf(dlevel,"influx_request: unhandled rc: %d\n", rc);
		msg = "unknown response code";
		break;
	}
	strncpy(newresp.errmsg,msg,sizeof(newresp.errmsg)-1);
	/* _get_response could/should? be part of this func */
	if (!newresp.error) _get_response(s,&newresp);
influx_request_done:
	r = list_add(s->responses,&newresp,sizeof(newresp));
	dprintf(dlevel,"r: %p\n", r);
	if (r) r->parent = s;

	/* XXX must be done here */
	dprintf(dlevel,"r->results: %p\n", r->results);
	list_reset(r->results);
	while ((rp = list_get_next(r->results)) != 0) {
		dprintf(dlevel,"rp->series: %p\n", rp->series);
		list_reset(rp->series);
		while ((sp = list_get_next(rp->series)) != 0) sp->parent = rp;
		rp->parent = r;
	}

	r->refs++;
	return r;
}

int influx_release_response(influx_response_t *r) {
	dprintf(dlevel,"r: %p\n", r);
	if (!r) return 1;
	dprintf(dlevel,"r->refs: %d\n", r->refs);
	if (!r->refs) return 1;
	r->refs--;
	influx_destroy_response(r);
	return 0;
}

int influx_health(influx_session_t *s) {
	influx_response_t *r;
	char *url;
	int error;

	if (!s->enabled) return 0;

	url = influx_mkurl(s,"health",0);
	dprintf(dlevel,"url: %p\n", url);
	if (!url) return 1;
	r = influx_request(s,url,0,0);
	error = r->error;
	influx_destroy_response(r);
	free(url);
	return error;

#if 0
	curl_easy_getinfo(s->curl, CURLINFO_RESPONSE_CODE, &rc);
	dprintf(dlevel,"rc: %d\n", rc);
	free(url);
//	if (rc != 200) return(1);
#if 0
	if (_process_results(s)) return 1;
#if 0
	dprintf(dlevel,"data: %p\n", s->data);
	dprintf(dlevel,"type: %s\n", json_typestr(json_value_get_type(s->data)));
	if (json_value_get_type(s->data) != JSON_TYPE_OBJECT) return 1;
	p = json_object_dotget_string(json_value_object(s->data),"status");
	if (!p) {
		sprintf(s->errmsg,"unable to get influxdb status", p);
		return 1;
	}
	if (strcmp(p,"pass") != 0) {
		p = json_object_dotget_string(json_value_object(s->data),"message");
		if (p) strncpy(s->errmsg,p,sizeof(s->errmsg);
		else sprintf(s->errmsg,"influxdb status is %s", p);
		return 1;
	}
#endif
	p = json_object_dotget_string(json_value_object(s->data),"version");
	dprintf(dlevel,"p: %s\n", p);
	if (!p) return 1;
	strncpy(s->version,p,sizeof(s->version)-1);
#endif
#endif
	return 0;
}

int influx_ping(influx_session_t *s) {
	influx_response_t *r;
	char *url;
	long rc;
	int error;

	if (!s->enabled) return 0;

	url = influx_mkurl(s,"ping",0);
	dprintf(dlevel,"url: %p\n", url);
	if (!url) return 1;
	r = influx_request(s,url,0,0);
	error = r->error;
	influx_destroy_response(r);
	free(url);
	if (error) return error;

	curl_easy_getinfo(s->curl, CURLINFO_RESPONSE_CODE, &rc);
	dprintf(dlevel,"rc: %d\n", rc);
	return (rc == 200 || rc == 204 ? 0 : 1);
}

influx_response_t *influx_query(influx_session_t *s, char *query) {
	influx_response_t *r;
	char *postcmds[] = { "ALTER", "CREATE", "DELETE", "DROP", "GRANT", "KILL", "REVOKE", "SELECT INTO", 0 };
	char *url,*lcq,**p;
	int post;

	dprintf(dlevel,"enabled: %d\n", s->enabled);
	if (!s->enabled) return 0;

	lcq = stredit(query,"COMPRESS,UPPERCASE");
	dprintf(dlevel,"lcq: %s\n", lcq);
	post = 0;
	for(p = postcmds; *p; p++) {
		dprintf(dlevel+2,"p: %s\n", *p);
		if (strstr(lcq,*p)) {
			post = 1;
			break;
		}
	}
	dprintf(dlevel,"post: %d\n", post);

	url = influx_mkurl(s,"query",query);
	dprintf(dlevel,"url: %p\n", url);
	if (!url) return 0;
	dprintf(dlevel,"calling request..\n");
	r = influx_request(s,url,post,0);
	dprintf(dlevel,"r: %p\n", r);
	dprintf(dlevel,"freeing url...\n");
	free(url);
	return r;
}

int influx_get_first_value(influx_response_t *r, double *val, char *text, int text_size) {
        influx_result_t *rp;
        influx_series_t *sp;
	influx_value_t *vp;

        dprintf(dlevel,"r: %p\n", r);
        if (!r) return 1;

	dprintf(0,"r->results: %p\n", r->results);
	dprintf(0,"results count: %d\n", list_count(r->results));
	if (!list_count(r->results)) return 1;
	rp = list_get_first(r->results);
	dprintf(0,"rp: %p\n", rp);
	dprintf(0,"rp->series: %p\n", rp->series);
	dprintf(0,"series count: %d\n", list_count(rp->series));
	if (!list_count(rp->series)) return 1;
	sp = list_get_first(rp->series);
	dprintf(0,"sp: %p\n", sp);
	dprintf(0,"value_count: %d\n", sp->value_count);
	if (!sp->value_count) return 1;

	vp = sp->values[0][1];
	if (text) conv_type(DATA_TYPE_STRING,text,text_size,vp->type,vp->data,vp->len);
	if (val) conv_type(DATA_TYPE_DOUBLE,val,sizeof(*val),vp->type,vp->data,vp->len);
#if 0
	dprintf(0,"result_count: %d\n", r->result_count);
        if (r->result_count > 0) {
                rp = r->results[0];
		dprintf(0,"series_count: %d\n", rp->series_count);
                if (rp->series_count > 0) {
                        sp = rp->series[0];
			dprintf(0,"value_count: %d\n", sp->value_count);
                        if (sp->value_count > 0) {
				*vp = *sp->values[0][1];
				return 0;
                        }
                }
        }
#endif
	return 1;
}

int influx_connect(influx_session_t *s) {
	influx_response_t *r;
	int error;

	dprintf(dlevel,"connected: %d\n", s->connected);
	if (s->connected) return 0;

	error = 1;
	dprintf(dlevel,"endpoint: %s\n", s->endpoint);
	if (!strlen(s->endpoint)) goto influx_connect_done;
	dprintf(dlevel,"database: %s\n", s->database);
	if (!strlen(s->database)) goto influx_connect_done;
	dprintf(dlevel,"calling show measurements...\n");
	r = influx_query(s,"show measurements");
	if (!r) goto influx_connect_done;
	error = r->error;
	influx_destroy_response(r);
influx_connect_done:
	dprintf(dlevel,"error: %d\n", error);
	s->connected = (error == 0);
	return error;
}

int influx_connected(influx_session_t *s) {
	if (!s) return 0;
	else return s->connected;
}

influx_response_t *influx_write(influx_session_t *s, char *mm, char *string) {
	influx_response_t *r;
	char *temp,*url;

	dprintf(dlevel,"enabled: %d\n", s->enabled);
	if (!s->enabled) return 0;

	dprintf(dlevel,"mm: %s, string: %s\n", mm, string);
#if 0
consistency=[any,one,quorum,all]	Optional, available with InfluxEnterprise clusters only.	Sets the write consistency for the point. InfluxDB assumes that the write consistency is one if you do not specify consistency. See the InfluxEnterprise documentation for detailed descriptions of each consistency option.
db=<database>	Required	Sets the target database for the write.
p=<password>	Optional if you haven.t enabled authentication. Required if you.ve enabled authentication.*	Sets the password for authentication if you.ve enabled authentication. Use with the query string parameter u.
precision=[ns,u,ms,s,m,h]	Optional	Sets the precision for the supplied Unix time values. InfluxDB assumes that timestamps are in nanoseconds if you do not specify precision.**
rp=<retention_policy_name>	Optional	Sets the target retention policy for the write. InfluxDB writes to the DEFAULT retention policy if you do not specify a retention policy.
u=<username>	Optional if you haven.t enabled authentication. Required if you.ve enabled authentication.*	Sets the username for authentication if you.ve enabled authentication. The user must have write access to the database. Use with the query string parameter p.
#endif
	url = influx_mkurl(s,"write",0);
	dprintf(dlevel,"url: %p\n", url);
	if (!url) return 0;
	dprintf(dlevel,"calling request..\n");
	/* mm + <space> + string + 0 */
	temp = malloc(strlen(mm) + 1 + strlen(string) + 1);
	if (!temp) {
		strcpy(s->errmsg,"memory allocation error");
		free(url);
		return 0;
	}
	strcpy(temp,mm);
	strcat(temp," ");
	strcat(temp,string);
	dprintf(dlevel,"temp: %s\n", temp);
	r = influx_request(s,url,1,temp);
	dprintf(dlevel,"r: %d\n", r);
	free(url);
	free(temp);
	return r;
}

#ifndef NO_CONFIG
#include "config.h"
int influx_write_props(influx_session_t *s, char *name, config_property_t *props) {
	influx_response_t *r;
	register config_property_t *pp;
	char value[2048], *string;
	int strsize,stridx,newidx,len,count;

	if (!s->enabled) return 0;

	dprintf(dlevel,"props: %p\n", props);
	if (!props) return 1;

	strsize = INFLUX_INIT_BUFSIZE;
	string = malloc(strsize);
//	stridx = sprintf(string,"%s ",name);
	stridx = 0;
	count = 0;
	for(pp = props; pp->name; pp++) {
		dprintf(dlevel,"name: %s, type: %x, dest: %p, len: %d\n", pp->name, pp->type, pp->dest, pp->len);
		len = conv_type(DATA_TYPE_STRING,value,sizeof(value)-1,pp->type,pp->dest,pp->len);
		dprintf(dlevel,"len: %d, stridx: %d, strsize: %d\n", len, stridx, strsize);
		/* name=value, if string add 2 for quotes */
		newidx = stridx + strlen(pp->name) + 1 + len;
		if (pp->type == DATA_TYPE_STRING) newidx += 2;
		/* comma */
		if (count) newidx++;
		dprintf(dlevel,"newidx: %d\n", newidx);
		if (newidx >= strsize) {
			char *newbuf;
			int newsize;

			newsize = (newidx / 4096) + 1;
			dprintf(dlevel+1,"newsize: %d\n", newsize);
			newsize *= 4096;
			dprintf(dlevel+1,"newsize: %d\n", newsize);
			newbuf = realloc(string,newsize);
			dprintf(dlevel,"newbuf: %p\n", newbuf);
			if (!newbuf) {
				log_syserror("influx_write_props: realloc(buffer,%d)",newsize);
				free(string);
				return 1;
			}
			string = newbuf;
			strsize = newsize;
		}
		if (pp->type == DATA_TYPE_STRING)
			sprintf(string + stridx, "%s%s=\"%s\"\n", (count ? "," : ""), pp->name, value);
		else
			sprintf(string + stridx, "%s%s=%s\n", (count ? "," : ""), pp->name, value);
		stridx = newidx;
		count++;
	}
	dprintf(dlevel,"string: %s\n", string);
	r = influx_write(s,name,string);
	free(string);
	dprintf(dlevel,"returning: %d\n", r->error);
	return r->error;
}

int enabled_set(void *ctx, config_property_t *p, void *old_value) {
	influx_session_t *s = ctx;

	dprintf(dlevel,"enabled: %d, connected: %d\n", s->enabled, s->connected);
	if (s->enabled && !s->connected) return influx_connect(s);
	else if (!s->enabled && s->connected) s->connected = false;
	return 0;
}

void influx_add_props(influx_session_t *s, config_t *cp, char *name) {
	config_property_t influx_private_props[] = {
		{ "influx_enabled", DATA_TYPE_BOOL, &s->enabled, 0, "yes", 0, 0, 0, 0, 0, 0, 1, enabled_set, s },
		{ "influx_timeout", DATA_TYPE_INT, &s->timeout, 0, "10", 0, 0, 0, 0, 0, 0, 1, 0, 0 },
		{ "influx_endpoint", DATA_TYPE_STRING, s->endpoint, sizeof(s->endpoint)-1, "http://localhost:8086", 0, },
//                        0, 0, 0, 1, (char *[]){ "InfluxDB endpoint" }, 0, 1, 0 },
		{ "influx_database", DATA_TYPE_STRING, s->database, sizeof(s->database)-1, "", 0, },
//                        0, 0, 0, 1, (char *[]){ "InfluxDB database" }, 0, 1, 0 },
		{ "influx_username", DATA_TYPE_STRING, s->username, sizeof(s->username)-1, "", 0, },
//                        0, 0, 0, 1, (char *[]){ "InfluxDB username" }, 0, 1, 0 },
		{ "influx_password", DATA_TYPE_STRING, s->password, sizeof(s->password)-1, "", 0, },
//                        0, 0, 0, 1, (char *[]){ "InfluxDB password" }, 0, 1, 0 },
		{ 0 }
	};

	dprintf(dlevel,"name: %s\n", name);
	config_add_props(cp, name, influx_private_props, 0);
}
#endif

int influx_write_json(influx_session_t *s, char *name, json_value_t *rv) {
	influx_response_t *r;
	json_object_t *o;
	char value[2048], *string;
	int strsize,stridx,newidx,len,count;
	int i,type,error;

	if (!s->enabled) return 0;
	if (!rv) return 1;

	if (json_value_get_type(rv) != JSON_TYPE_OBJECT) {
		sprintf(s->errmsg,"influx_write_json: invalid root value type");
		return 1;
	}
	strsize = INFLUX_INIT_BUFSIZE;
	string = malloc(strsize);
//	stridx = sprintf(string,"%s ",name);
	stridx = 0;
	count = 0;
	o = json_value_object(rv);
	for(i=0; i < o->count; i++) {
		type = json_value_get_type(o->values[i]);
		dprintf(dlevel,"name: %s, type: %x\n", o->names[i], json_typestr(type));
		len = json_to_type(DATA_TYPE_STRING,value,sizeof(value)-1,o->values[i]);
		dprintf(dlevel,"len: %d, stridx: %d, strsize: %d\n", len, stridx, strsize);
		/* name=value, if string add 2 for quotes */
		newidx = stridx + strlen(o->names[i]) + 1 + len;
		if (type == JSON_TYPE_STRING) newidx += 2;
		/* comma */
		if (count) newidx++;
		dprintf(dlevel,"newidx: %d\n", newidx);
		if (newidx >= strsize) {
			char *newbuf;
			int newsize;

			newsize = (newidx / 4096) + 1;
			dprintf(dlevel+1,"newsize: %d\n", newsize);
			newsize *= 4096;
			dprintf(dlevel+1,"newsize: %d\n", newsize);
			newbuf = realloc(string,newsize);
			dprintf(dlevel,"newbuf: %p\n", newbuf);
			if (!newbuf) {
				log_syserror("influx_write_json: realloc(buffer,%d)",newsize);
				free(string);
				return 1;
			}
			string = newbuf;
			strsize = newsize;
		}
		if (type == JSON_TYPE_STRING)
			sprintf(string + stridx, "%s%s=\"%s\"", (count ? "," : ""), o->names[i], value);
		else
			sprintf(string + stridx, "%s%s=%s", (count ? "," : ""), o->names[i], value);
		stridx = newidx;
		count++;
	}
	dprintf(dlevel,"string: %s\n", string);
	r = influx_write(s,name,string);
	error = r->error;
	free(string);
	influx_release_response(r);
	dprintf(dlevel,"returning: %d\n", error);
	return error;
}

/********************************************************************************/
/*
*** SERIES
*/

#ifdef JS
#include "jsengine.h"
#include "jsdate.h"
#include "jsstr.h"

extern int js_mem_used_pct(JSContext *cx);

JSObject *js_influx_values_new(JSContext *cx, JSObject *obj, influx_series_t *sp) {
	JSObject *js_values,*vo,*dateobj;
	jsval ie,ke;
	int i,j,time_col,doconv,count;
	influx_value_t *vp;
	JSString *str;
	jsdouble d;
//	char *p;

	int ldlevel = dlevel;

	/* values is an array of influx_value pointers */
	dprintf(ldlevel,"value_count: %d\n", sp->value_count);
	js_values = JS_NewArrayObject(cx, 0, NULL);
	dprintf(ldlevel,"js_values: %p\n", js_values);
	if (!js_values) return 0;

	/* Get the time column */
	time_col = -1;
	dprintf(ldlevel,"column_count: %d\n", sp->column_count);
	for(i=0; i < sp->column_count; i++) {
//		printf("columns[%d]: %p\n", i, sp->columns[i]);
		if (strcmp(sp->columns[i],"time") == 0) {
			time_col = i;
			break;
		}
	}
	dprintf(ldlevel,"value_count: %d\n", sp->value_count);
	count = 0;
	for(i=0; i < sp->value_count; i++) {
		/* XXX req'd as strange things happen otherwise */
		if (js_mem_used_pct(cx) == 100) break;
		vo = JS_NewArrayObject(cx, 0, NULL);
		dprintf(ldlevel,"vo: %p\n", vo);
		if (!vo) break;
		for(j=0; j < sp->column_count; j++) {
			vp = sp->values[i][j];
			doconv = 1;
			/* Automatically convert the time field into a date object */
			if (j == time_col && sp->convdt) {
				char value[128];

				/* In case time is a number */
				conv_type(DATA_TYPE_STRING,value,sizeof(value),vp->type,vp->data,vp->len);
				str = JS_NewString(cx, value, strlen(value));
				if (str && date_parseString(str,&d)) {
					dateobj = js_NewDateObjectMsec(cx,d);
					if (dateobj) {
						ke = OBJECT_TO_JSVAL(dateobj);
						doconv = 0;
					}
				}
			}
			if (doconv) ke = type_to_jsval(cx,vp->type,vp->data,vp->len);
			JS_SetElement(cx, vo, j, &ke);
		}
		ie = OBJECT_TO_JSVAL(vo);
		JS_SetElement(cx, js_values, i, &ie);
		count++;
	}
	/* fix up the array object with the real count */
	dprintf(ldlevel,"returning: %p\n", js_values);
	return js_values;
}

enum SERIES_PROPERTY_ID {
	SERIES_PROPERTY_ID_NAME=1,
	SERIES_PROPERTY_ID_COLUMNS,
	SERIES_PROPERTY_ID_VALUES,
};

static void js_influx_series_free(JSContext *cx, JSObject *obj) {
	influx_series_t *sp;

	sp = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"sp: %p\n", sp);
	if (sp) {
		dprintf(dlevel,"sp->refs: %d\n", sp->refs);
		if (sp->refs) sp->refs--;
		influx_destroy_series(sp);
	}
}

static JSBool js_influx_series_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	influx_series_t *sp;
	int prop_id;

	sp = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"sp: %p\n", sp);
	if (!sp) {
		JS_ReportError(cx, "private is null!");
		return JS_FALSE;
	}

	dprintf(dlevel,"is_int: %d\n", JSVAL_IS_INT(id));
	if (JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case SERIES_PROPERTY_ID_NAME:
//			if (!sp->name_val) sp->name_val = type_to_jsval(cx,DATA_TYPE_STRING,sp->name,strlen(sp->name));
//			*rval = sp->name_val;
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,sp->name,strlen(sp->name));
			break;
		case SERIES_PROPERTY_ID_COLUMNS:
//			if (!sp->columns_val) sp->columns_val = type_to_jsval(cx,DATA_TYPE_STRING_ARRAY,sp->columns,sp->column_count);
//			*rval = sp->columns_val;
			*rval = type_to_jsval(cx,DATA_TYPE_STRING_ARRAY,sp->columns,sp->column_count);
			break;
		case SERIES_PROPERTY_ID_VALUES:
			*rval = OBJECT_TO_JSVAL(js_influx_values_new(cx,obj,sp));
#if 0
			{
				int i;
				JSObject *a,*aa;
				jsval e;

				a = JS_NewArrayObject(cx, 0, NULL);
				for(i=0; i < sp->values_count; i++) {
					aa = JS_NewArrayObject(cx, 0, NULL);
					for(j=0; j < sp->column_count; j++) {
						e = OBJECT_TO_JSVAL(js_influx_values_new(cx,obj,sp->values[i][j]));
						JS_SetElement(cx, aa, i, &e);
				}
				*rval = OBJECT_TO_JSVAL(a);
			}
#endif
			break;
		}
	}
	return JS_TRUE;
}

static JSClass js_influx_series_class = {
	"InfluxSeries",		/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	js_influx_series_getprop,/* getProperty */
	JS_PropertyStub,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	js_influx_series_free,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *js_InitInfluxSeriesClass(JSContext *cx, JSObject *parent) {
	JSPropertySpec series_props[] = { 
		{ "name",		SERIES_PROPERTY_ID_NAME,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "columns",		SERIES_PROPERTY_ID_COLUMNS,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "values",		SERIES_PROPERTY_ID_VALUES,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{0}
	};
	JSObject *obj;

	dprintf(dlevel,"creating %s class...\n",js_influx_series_class.name);
	obj = JS_InitClass(cx, parent, 0, &js_influx_series_class, 0, 0, series_props, 0, 0, 0);
	dprintf(dlevel,"obj: %p\n", obj);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s class", js_influx_series_class.name);
		return 0;
	}
	dprintf(dlevel,"done!\n");
	return obj;
}

JSObject *js_influx_series_new(JSContext *cx, JSObject *parent, influx_series_t *sp) {
	JSObject *newobj;

	/* Create the new object */
	dprintf(dlevel,"Creating %s object...\n", js_influx_series_class.name);
	newobj = JS_NewObject(cx, &js_influx_series_class, 0, parent);
	dprintf(dlevel,"newobj: %p\n", newobj);
	if (!newobj) return 0;

	JS_SetPrivate(cx,newobj,sp);
	/* inc ref count */
	sp->refs++;
	dprintf(dlevel,"sp->refs: %d\n", sp->refs);

	dprintf(dlevel,"newobj: %p\n",newobj);
	return newobj;
}

/********************************************************************************/
/*
*** RESULT
*/
enum RESULT_PROPERTY_ID {
	RESULT_PROPERTY_ID_STMTID=1,
	RESULT_PROPERTY_ID_ERRMSG,
	RESULT_PROPERTY_ID_SERIES,
};

static void js_influx_result_free(JSContext *cx, JSObject *obj) {
	influx_result_t *rp;

	rp = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"rp: %p\n", rp);
	if (rp) {
		dprintf(dlevel,"rp->refs: %d\n", rp->refs);
		if (rp->refs) rp->refs--;
		influx_destroy_result(rp);
	}
}

static JSBool js_influx_result_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	influx_result_t *rp;
	int prop_id;

	rp = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"rp: %p\n", rp);
	if (!rp) {
		JS_ReportError(cx, "js_influx_results_getprop: private is null!");
		return JS_FALSE;
	}

	dprintf(dlevel,"is_int: %d\n", JSVAL_IS_INT(id));
	if (JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case RESULT_PROPERTY_ID_STMTID:
			*rval = type_to_jsval(cx,DATA_TYPE_INT,&rp->statement_id,0);
			break;
		case RESULT_PROPERTY_ID_SERIES:
			{
				JSObject *a;
				int i;
				influx_series_t *sp;
				jsval e;

				a = JS_NewArrayObject(cx, 0, NULL);
				list_reset(rp->series);
				i = 0;
				while((sp = list_get_next(rp->series)) != 0) {
					e = OBJECT_TO_JSVAL(js_influx_series_new(cx,obj,sp));
					JS_SetElement(cx, a, i++, &e);
				}
				*rval = OBJECT_TO_JSVAL(a);
			}
			break;
		}
	}
	return JS_TRUE;
}

static JSClass js_influx_result_class = {
	"InfluxResult",		/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	js_influx_result_getprop,/* getProperty */
	JS_PropertyStub,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	js_influx_result_free,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *js_InitInfluxResultClass(JSContext *cx, JSObject *parent) {
	JSPropertySpec result_props[] = { 
		{ "statement_id",	RESULT_PROPERTY_ID_STMTID,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "series",		RESULT_PROPERTY_ID_SERIES,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{0}
	};
	JSObject *obj;

	dprintf(dlevel,"creating %s class...\n",js_influx_result_class.name);
	obj = JS_InitClass(cx, parent, 0, &js_influx_result_class, 0, 0, result_props, 0, 0, 0);
	dprintf(dlevel,"obj: %p\n", obj);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s class", js_influx_result_class.name);
		return 0;
	}
	dprintf(dlevel,"done!\n");
	return obj;
}

static JSObject *js_influx_result_new(JSContext *cx, JSObject *parent, influx_result_t *rp) {
	JSObject *newobj;

	/* Create the new object */
	dprintf(dlevel,"Creating %s object...\n", js_influx_result_class.name);
	newobj = JS_NewObject(cx, &js_influx_result_class, 0, parent);
	dprintf(dlevel,"newobj: %p\n", newobj);
	if (!newobj) return 0;

	JS_SetPrivate(cx,newobj,rp);
	/* inc ref count */
	rp->refs++;
	dprintf(dlevel,"rp->refs: %d\n", rp->refs);

	dprintf(dlevel,"newobj: %p\n",newobj);
	return newobj;
}

/********************************************************************************/
/*
*** RESPONSE
*/
enum RESPONSE_PROPERTY_ID {
	RESPONSE_PROPERTY_ID_STATUS=1,
	RESPONSE_PROPERTY_ID_RESULTS,
};

static void js_influx_response_free(JSContext *cx, JSObject *obj) {
	influx_response_t *r;

	r = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"r: %p\n", r);
	if (r) influx_release_response(r);
}

static JSBool js_influx_response_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	influx_response_t *r;
	int prop_id;

	r = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"r: %p\n", r);
	if (!r) {
		JS_ReportError(cx, "js_influx_response_getprop: private is null!");
		return JS_FALSE;
	}

	dprintf(dlevel,"is_int: %d\n", JSVAL_IS_INT(id));
	if (JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case RESPONSE_PROPERTY_ID_STATUS:
			*rval = type_to_jsval(cx,DATA_TYPE_INT,&r->error,0);
			break;
		case RESPONSE_PROPERTY_ID_RESULTS:
			{
				JSObject *a;
				int i;
				influx_result_t *rp;
				jsval e;

				a = JS_NewArrayObject(cx, 0, NULL);
				list_reset(r->results);
				i = 0;
				while((rp = list_get_next(r->results)) != 0) {
					e = OBJECT_TO_JSVAL(js_influx_result_new(cx,obj,rp));
					JS_SetElement(cx, a, i++, &e);
				}
				*rval = OBJECT_TO_JSVAL(a);
			}
			break;
		}
	}
	return JS_TRUE;
}

static JSClass js_influx_response_class = {
	"InfluxResponse",	/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	js_influx_response_getprop,/* getProperty */
	JS_PropertyStub,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	js_influx_response_free,/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *js_InitInfluxResponseClass(JSContext *cx, JSObject *parent) {
	JSPropertySpec response_props[] = { 
		{ "status",		RESPONSE_PROPERTY_ID_STATUS,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "results",		RESPONSE_PROPERTY_ID_RESULTS,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{0}
	};
	JSObject *obj;

	dprintf(dlevel,"creating %s class...\n",js_influx_response_class.name);
	obj = JS_InitClass(cx, parent, 0, &js_influx_response_class, 0, 0, response_props, 0, 0, 0);
	dprintf(dlevel,"obj: %p\n", obj);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s class", js_influx_response_class.name);
		return 0;
	}
	dprintf(dlevel,"done!\n");
	return obj;
}

static JSObject *js_influx_response_new(JSContext *cx, JSObject *parent, influx_response_t *r) {
	JSObject *newobj;

	/* Create the new object */
	dprintf(dlevel,"Creating %s object...\n", js_influx_response_class.name);
	newobj = JS_NewObject(cx, &js_influx_response_class, 0, parent);
	dprintf(dlevel,"newobj: %p\n", newobj);
	if (!newobj) return 0;

	JS_SetPrivate(cx,newobj,r);
	/* XXX no need to inc responses refs - its done @ request */

	dprintf(dlevel,"newobj: %p\n",newobj);
	return newobj;
}
/********************************************************************************/
/*
*** INFLUX
*/

enum INFLUX_PROPERTY_ID {
	INFLUX_PROPERTY_ID_ENABLED=1,
	INFLUX_PROPERTY_ID_URL,
	INFLUX_PROPERTY_ID_DATABASE,
	INFLUX_PROPERTY_ID_USERNAME,
	INFLUX_PROPERTY_ID_PASSWORD,
	INFLUX_PROPERTY_ID_VERBOSE,
	INFLUX_PROPERTY_ID_ERRMSG,
	INFLUX_PROPERTY_ID_CONVDT,
	INFLUX_PROPERTY_ID_EPOCH,
	INFLUX_PROPERTY_ID_CONNECTED,
};

static JSBool influx_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	influx_session_t *s;
	int prop_id;

	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx, "private is null!");
		return JS_FALSE;
	}

	dprintf(dlevel,"is_int: %d\n", JSVAL_IS_INT(id));
	if (JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case INFLUX_PROPERTY_ID_ENABLED:
			*rval = type_to_jsval(cx,DATA_TYPE_BOOL,&s->enabled,0);
			break;
		case INFLUX_PROPERTY_ID_URL:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,s->endpoint,strlen(s->endpoint));
			break;
		case INFLUX_PROPERTY_ID_DATABASE:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,s->database,strlen(s->database));
			break;
		case INFLUX_PROPERTY_ID_USERNAME:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,s->username,strlen(s->username));
			break;
		case INFLUX_PROPERTY_ID_PASSWORD:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,s->password,strlen(s->password));
			break;
		case INFLUX_PROPERTY_ID_VERBOSE:
			dprintf(dlevel,"verbose: %d\n", s->verbose);
			*rval = type_to_jsval(cx,DATA_TYPE_BOOL,&s->verbose,0);
			break;
		case INFLUX_PROPERTY_ID_ERRMSG:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,s->errmsg,strlen(s->errmsg));
			break;
		case INFLUX_PROPERTY_ID_CONVDT:
			*rval = type_to_jsval(cx,DATA_TYPE_BOOL,&s->convdt,0);
			break;
		case INFLUX_PROPERTY_ID_EPOCH:
			*rval = type_to_jsval(cx,DATA_TYPE_STRING,s->epoch,strlen(s->epoch));
			break;
		case INFLUX_PROPERTY_ID_CONNECTED:
			*rval = type_to_jsval(cx,DATA_TYPE_BOOL,&s->connected,0);
			break;
		default:
			break;
		}
	}
	return JS_TRUE;
}

static JSBool influx_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	influx_session_t *s;
	int prop_id;

	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);
	if (!s) {
		JS_ReportError(cx, "private is null!");
		return JS_FALSE;
	}

	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	if (JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"influx_setprop: prop_id: %d\n", prop_id);
		switch(prop_id) {
		case INFLUX_PROPERTY_ID_ENABLED:
			jsval_to_type(DATA_TYPE_BOOL,&s->enabled,0,cx,*vp);
			break;
		case INFLUX_PROPERTY_ID_URL:
			jsval_to_type(DATA_TYPE_STRING,s->endpoint,sizeof(s->endpoint),cx,*vp);
			break;
		case INFLUX_PROPERTY_ID_DATABASE:
			jsval_to_type(DATA_TYPE_STRING,s->database,sizeof(s->database),cx,*vp);
			break;
		case INFLUX_PROPERTY_ID_USERNAME:
			jsval_to_type(DATA_TYPE_STRING,s->username,sizeof(s->username),cx,*vp);
			break;
		case INFLUX_PROPERTY_ID_PASSWORD:
			jsval_to_type(DATA_TYPE_STRING,s->password,sizeof(s->password),cx,*vp);
			break;
		case INFLUX_PROPERTY_ID_VERBOSE:
			jsval_to_type(DATA_TYPE_BOOL,&s->verbose,0,cx,*vp);
			break;
		case INFLUX_PROPERTY_ID_CONVDT:
			jsval_to_type(DATA_TYPE_BOOL,&s->convdt,0,cx,*vp);
			break;
		case INFLUX_PROPERTY_ID_EPOCH:
			jsval_to_type(DATA_TYPE_STRING,s->epoch,sizeof(s->epoch),cx,*vp);
			break;
		default:
			break;
		}
	}
	return JS_TRUE;
}

static void js_influx_finalize(JSContext *cx, JSObject *obj) {
	influx_session_t *s;

	int ldlevel = dlevel;

	dprintf(ldlevel,"obj: %p\n", obj);
	s = JS_GetPrivate(cx, obj);
	dprintf(ldlevel,"s: %p\n", s);
	/* XXX return silently */
	if (!s) return;
	/* Only destroy sessions we created */
	dprintf(ldlevel,"ctor: %d\n", s->ctor);
	if (s->ctor) influx_destroy_session(s);
	return;
}

static JSClass js_influx_class = {
	"Influx",		/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	influx_getprop,		/* getProperty */
	influx_setprop,		/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	js_influx_finalize,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static char *js_string(JSContext *cx, jsval val) {
	JSString *str;

	str = JS_ValueToString(cx, val);
	if (!str) return JS_FALSE;
	return JS_EncodeString(cx, str);
}

static JSBool js_influx_query(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	influx_session_t *s;
	influx_response_t *r;
	jsval *argv = vp + 2;
	char *type,*query;

	obj = JS_THIS_OBJECT(cx, vp);
	dprintf(dlevel,"obj: %p\n", obj);
	if (!obj) return JS_FALSE;
	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);

	if (argc != 1) goto js_influx_query_usage;

	/* If the 2nd arg is a string, treat it as JSON */
	type = jstypestr(cx,argv[0]);
	dprintf(dlevel,"arg type: %s\n", type);
	if (strcmp(type,"string") != 0) goto js_influx_query_usage;

	query = (char *)JS_EncodeString(cx,JSVAL_TO_STRING(argv[0]));
	dprintf(dlevel,"query: %s\n", query);
	r = influx_query(s,query);
	JS_free(cx,query);
	if (!r) *vp = JSVAL_VOID;
	else *vp = OBJECT_TO_JSVAL(js_influx_response_new(cx, obj, r));
	return JS_TRUE;

js_influx_query_usage:
	JS_ReportError(cx,"influx.query requires 1 argument (query:string)");
	return JS_FALSE;
}

static JSBool js_influx_write(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj,*rec;
	JSIdArray *ida;
	jsval *ids;
	char *name;
	influx_session_t *s;
	influx_response_t *r;
	jsval *argv = vp + 2;
	int i;
	char *type,*key;
	jsval val;

	obj = JS_THIS_OBJECT(cx, vp);
	dprintf(dlevel,"obj: %p\n", obj);
	if (!obj) return JS_FALSE;
	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);

	if (argc != 2) goto js_influx_write_usage;

	/* Get the measurement name */
	if (strcmp(jstypestr(cx,argv[0]),"string") != 0) goto js_influx_write_usage;
	name = (char *)JS_EncodeString(cx,JSVAL_TO_STRING(argv[0]));
	dprintf(dlevel,"name: %s\n", name);

	/* If the 2nd arg is a string, send it direct */
	type = jstypestr(cx,argv[1]);
	dprintf(dlevel,"arg 2 type: %s\n", type);
	if (strcmp(type,"string") == 0) {
		char *value = (char *)JS_EncodeString(cx,JSVAL_TO_STRING(argv[1]));
		dprintf(dlevel,"value: %s\n", value);
		r = influx_write(s,name,value);
		JS_free(cx,value);

	/* Otherwise it's an object - gets the keys and values */
	} else if (strcmp(type,"object") == 0) {
		char *string,temp[2048],value[1536];
		int strsize,stridx,newidx,len;

		rec = JSVAL_TO_OBJECT(argv[1]);
		ida = JS_Enumerate(cx, rec);
		dprintf(dlevel,"ida: %p, count: %d\n", ida, ida->length);
		ids = &ida->vector[0];
		strsize = INFLUX_INIT_BUFSIZE;
		string = malloc(strsize);
//		stridx = sprintf(string,"%s",name);
		stridx = 0;
		for(i=0; i< ida->length; i++) {
	//		dprintf(dlevel+1,"ids[%d]: %s\n", i, jstypestr(cx,ids[i]));
			key = js_string(cx,ids[i]);
			if (!key) continue;
	//		dprintf(dlevel+1,"key: %s\n", key);
			val = JSVAL_VOID;
			if (!JS_GetProperty(cx,rec,key,&val)) {
				JS_free(cx,key);
				continue;
			}
			jsval_to_type(DATA_TYPE_STRING,&value,sizeof(value)-1,cx,val);
			dprintf(dlevel,"key: %s, type: %s, value: %s\n", key, jstypestr(cx,val), value);
			if (JSVAL_IS_NUMBER(val)) {
				jsdouble d;
				if (JSVAL_IS_INT(val)) {
					d = (jsdouble)JSVAL_TO_INT(val);
					if (JSDOUBLE_IS_NaN(d)) strcpy(value,"0");
				} else {
					d = *JSVAL_TO_DOUBLE(val);
					if (JSDOUBLE_IS_NaN(d)) strcpy(value,"0.0");
				}
				len = snprintf(temp,sizeof(temp)-1,"%s=%s",key,value);
			} else if (JSVAL_IS_STRING(val)) {
				len = snprintf(temp,sizeof(temp)-1,"%s=\"%s\"",key,value);
			} else {
				dprintf(dlevel,"ignoring field %s: unhandled type\n", key);
				continue;
			}
			dprintf(dlevel,"temp: %s\n", temp);
			/* XXX include comma */
			newidx = stridx + len + 1;
			dprintf(dlevel,"newidx: %d, size: %d\n", newidx, strsize);
			if (newidx > strsize) {
				char *newstr;

				dprintf(dlevel,"string: %p\n", string);
				newstr = realloc(string,newidx);
				dprintf(dlevel,"newstr: %p\n", newstr);
				if (!newstr) {
					JS_ReportError(cx,"Influx.write: memory allocation error");
					JS_free(cx,key);
					JS_free(cx,value);
					free(string);
					return JS_FALSE;
				}
				string = newstr;
				strsize = newidx;
			}
			sprintf(string + stridx,"%s%s",(i ? "," : " "),temp);
			stridx = newidx;
			JS_free(cx,key);
//			JS_free(cx,value);
		}
//		JS_free(cx,ida);
		JS_DestroyIdArray(cx, ida);
		string[stridx] = 0;
		dprintf(dlevel,"string: %s\n", string);
		r = influx_write(s,name,string);
		free(string);
	} else {
		JS_free(cx,name);
		goto js_influx_write_usage;
	}
	JS_free(cx,name);
	if (!r) *vp = JSVAL_VOID;
	else *vp = OBJECT_TO_JSVAL(js_influx_response_new(cx, obj, r));
    	return JS_TRUE;
js_influx_write_usage:
	JS_ReportError(cx,"Influx.write requires 2 arguments (measurement:string, rec:object OR rec:string)");
	return JS_FALSE;
}

static JSBool js_influx_connect(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	influx_session_t *s;

	obj = JS_THIS_OBJECT(cx, vp);
	dprintf(dlevel,"obj: %p\n", obj);
	if (!obj) return JS_FALSE;
	s = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"s: %p\n", s);

	influx_connect(s);
	return JS_TRUE;
}

static JSBool js_influx_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	influx_session_t *s;
	JSObject *newobj;
	char *url, *db, *user, *pass;

	dprintf(dlevel,"argc: %d\n", argc);
	if (argc && argc < 2) {
		JS_ReportError(cx, "INFLUX requires 2 arguments (endpoint:string, database:string) and 2 optional arguments (username:string, password:string)");
		return JS_FALSE;
	}

	s = influx_new();
	if (!s) {
		JS_ReportError(cx, "influx_ctor: unable to create new session");
		return JS_FALSE;
	}
	s->enabled = true;
	s->ctor = true;

	if (argc) {
		url = db = user = pass = 0;
		if (!JS_ConvertArguments(cx, argc, argv, "s s / s s", &url, &db, &user, &pass))
			return JS_FALSE;
		dprintf(dlevel,"url: %s, db: %s, user: %s, pass: %s\n", url, db, user, pass);

		strncpy(s->endpoint,url,sizeof(s->endpoint)-1);
		if (!strrchr(s->endpoint,':')) strcat(s->endpoint,":8086");
		strncpy(s->database,db,sizeof(s->database)-1);
		if (user) strncpy(s->username,user,sizeof(s->username)-1);
		if (pass) strncpy(s->password,pass,sizeof(s->password)-1);
		dprintf(dlevel,"endpoint: %s, database: %s, user: %s, pass: %s\n", s->endpoint, s->database, s->username, s->password);
	}

	newobj = js_influx_new(cx,JS_GetGlobalObject(cx),s);
	dprintf(dlevel,"newobj: %p\n", newobj);
	*rval = OBJECT_TO_JSVAL(newobj);

	return JS_TRUE;
}

static JSObject *js_InitInfluxClass(JSContext *cx, JSObject *global_object) {
	JSPropertySpec influx_props[] = { 
		{ "enabled",		INFLUX_PROPERTY_ID_ENABLED,	JSPROP_ENUMERATE },
		{ "url",		INFLUX_PROPERTY_ID_URL,		JSPROP_ENUMERATE },
		{ "endpoint",		INFLUX_PROPERTY_ID_URL,		JSPROP_ENUMERATE },
		{ "database",		INFLUX_PROPERTY_ID_DATABASE,	JSPROP_ENUMERATE },
		{ "username",		INFLUX_PROPERTY_ID_USERNAME,	JSPROP_ENUMERATE },
		{ "password",		INFLUX_PROPERTY_ID_PASSWORD,	JSPROP_ENUMERATE },
		{ "connected",		INFLUX_PROPERTY_ID_CONNECTED,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "verbose",		INFLUX_PROPERTY_ID_VERBOSE,	JSPROP_ENUMERATE },
		{ "epoch",		INFLUX_PROPERTY_ID_EPOCH,	JSPROP_ENUMERATE },
		{ "convert_time",	INFLUX_PROPERTY_ID_CONVDT,	JSPROP_ENUMERATE },
		{ "errmsg",		INFLUX_PROPERTY_ID_ERRMSG,	JSPROP_ENUMERATE | JSPROP_READONLY },
		{0}
	};
	JSFunctionSpec influx_funcs[] = {
		JS_FN("query",js_influx_query,1,1,0),
		JS_FN("write",js_influx_write,2,2,0),
		JS_FN("connect",js_influx_connect,0,0,0),
		{0}
	};
	JSObject *obj;

	dprintf(dlevel,"Creating %s class...\n", js_influx_class.name);
	obj = JS_InitClass(cx, global_object, 0, &js_influx_class, js_influx_ctor, 2, influx_props, influx_funcs, 0, 0);
	dprintf(dlevel,"obj: %p\n", obj);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize influx class");
		return 0;
	}
//	dprintf(dlevel,"done!\n");
	return obj;
}

JSObject *js_influx_new(JSContext *cx, JSObject *parent, influx_session_t *s) {
	JSObject *newobj;

	/* Create the new object */
	dprintf(dlevel,"Creating %s object...\n", js_influx_class.name);
	newobj = JS_NewObject(cx, &js_influx_class, 0, parent);
	dprintf(dlevel,"==> newobj: %p\n", newobj);
	if (!newobj) return 0;

	JS_SetPrivate(cx,newobj,s);

	dprintf(dlevel,"newobj: %p\n",newobj);
	return newobj;
}

int influx_jsinit(JSEngine *e) {
	JS_EngineAddInitClass(e, "js_InitInfluxClass", js_InitInfluxClass);
	JS_EngineAddInitClass(e, "js_InitInfluxResponseClass", js_InitInfluxResponseClass);
	JS_EngineAddInitClass(e, "js_InitInfluxResultClass", js_InitInfluxResultClass);
	JS_EngineAddInitClass(e, "js_InitInfluxSeriesClass", js_InitInfluxSeriesClass);
	return 0;
}
#endif /* JS */
#endif /* INFLUX */
