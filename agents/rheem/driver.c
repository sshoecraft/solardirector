/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "rheem.h"
#include <ctype.h>

#define RHEEM_INIT_BUFSIZE 4092

#define dlevel 1

#define RHEEM_STATE_OPEN 0x0001

#define HOST "rheem.clearblade.com"
#define REST_URL "https://"HOST"/api/v/1"
#define MQTT_URI "ssl://"HOST":1884"
#define CLEAR_BLADE_SYSTEM_KEY "e2e699cb0bb0bbb88fc8858cb5a401"
#define CLEAR_BLADE_SYSTEM_SECRET "E2E699CB0BE6C6FADDB1B0BC9A20"

#define ECONET_LOGIN "user/auth"
#define ECONET_LOCATION "code/"CLEAR_BLADE_SYSTEM_KEY"/getLocation"
#define ECONET_DYNACT "code/e2e699cb0bb0bbb88fc8858cb5a401/dynamicAction"

void rheem_dump_device(rheem_device_t *d) {
	char temp[1024];

	printf("%-20.20s: %s\n", "Id", d->id);
	printf("%-20.20s: %s\n", "Serial", d->serial);
//	printf("%-20.20s: %s\n", "Name", d->name);
	*temp = 0;
	switch(d->type) {
	case RHEEM_DEVICE_TYPE_WH:
		sprintf(temp,"Water Heater");
		break;
	case RHEEM_DEVICE_TYPE_HVAC:
		sprintf(temp,"Thermostat");
		break;
	default:
		sprintf(temp,"Unknown");
		break;
	}
	printf("%-20.20s: %s\n", "Type", temp);
	printf("%-20.20s: %s\n", "Enabled", d->enabled ? "Yes" : "No");
	printf("%-20.20s: %s\n", "Running", d->running ? "Yes" : "No");
	printf("%-20.20s: %s\n", "Mode", d->modes[d->mode]);
	printf("%-20.20s: %d\n", "Setpoint", d->temp);
	printf("%-20.20s: %d\n", "Level", d->level);
//	printf("%-20.20s: %s\n", "Mac", d->mac);
}

static size_t getdata(void *ptr, size_t size, size_t nmemb, void *ctx) {
	rheem_session_t *s = ctx;
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
//		dprintf(dlevel+1,"newsize: %d\n", newsize);
		newsize *= 4096;
		dprintf(dlevel,"newsize: %d\n", newsize);
		newbuf = realloc(s->buffer,newsize);
		if (!newbuf) {
			log_syserror("rheem_getdata: realloc(buffer,%d)",newsize);
			return 0;
		}
		s->buffer = newbuf;
		s->bufsize = newsize;
	}
	strcpy(s->buffer + s->bufidx,ptr);
	s->bufidx = newidx;

	return bytes;
}

json_value_t *rheem_request(rheem_session_t *s, char *func, char *fields) {
#ifdef HAVE_CURL
	char url[128],*p;
	CURLcode res;

	dprintf(dlevel,"func: %s, fields: %s\n", func, fields);
	dprintf(dlevel,"endpoint: %s\n", s->endpoint);
	if (!*s->endpoint) return 0;

	s->bufidx = 0;

	p = url;
	if (strncmp(s->endpoint,"http",4) != 0) p += sprintf(p,"https://");
	p += sprintf(p,"%s/%s",s->endpoint,func);
	dprintf(dlevel,"url: %s\n", url);
	curl_easy_setopt(s->curl, CURLOPT_URL, url);
	if (fields) curl_easy_setopt(s->curl, CURLOPT_POSTFIELDS, fields);
	else curl_easy_setopt(s->curl, CURLOPT_POSTFIELDS, "{}");
	res = curl_easy_perform(s->curl);
	if (res != CURLE_OK) {
		log_error("rheem_request failed: %s\n", curl_easy_strerror(res));
		rheem_driver.close(s);
		return 0;
	}

	/* Parse the buffer */
	dprintf(dlevel,"bufidx: %d\n", s->bufidx);
	return json_parse(s->buffer);
#else
	return 0;
#endif
}

static void _getstring(json_object_t *o, char *dest, int dsize, char *name) {
	char *p;

	dprintf(dlevel,"name: %s\n", name);
	p = json_object_dotget_string(o,name);
	dprintf(dlevel,"p: %p\n", p);
	if (!p) return;
	snprintf(dest,dsize,p);
	trim(dest);
	dprintf(dlevel,"value: %s\n", dest);
}

static int _getnumber(json_object_t *o, int *dest, char *name) {
	json_value_t *v;

	dprintf(dlevel,"name: %s\n", name);
	v = json_object_dotget_value(o,name);
	dprintf(dlevel,"v: %p\n", v);
	if (!v) return 1;
	*dest = json_value_get_number(v);
	dprintf(dlevel,"dest: %d\n", *dest);
	return 0;
}

static int _getbool(json_object_t *o, bool *dest, char *name) {
	json_value_t *v;

	dprintf(dlevel,"name: %s\n", name);
	v = json_object_dotget_value(o,name);
	dprintf(dlevel,"v: %p\n", v);
	if (!v) return 1;
	*dest = (json_value_get_number(v) ? true : false);
	dprintf(dlevel,"dest: %d\n", *dest);
	return 0;
}

static void _getmodes(json_array_t *a, rheem_device_t *d) {
	register char *p;
	int i;

	dprintf(dlevel,"a: %p\n", a);
	if (!a) return;
	dprintf(dlevel,"count: %d\n",a->count);
	for(i=0; i < a->count; i++) {
		snprintf(d->modes[i],sizeof(d->modes[i])-1,json_value_get_string(a->items[i]));
		trim(d->modes[i]);
		for(p = d->modes[i]; *p; p++) {
			*p = tolower(*p);
			if (*p == ' ') *p = '_';
		}
	}
	d->modecount = a->count;
}

static bool _getstats(rheem_session_t *s, json_object_t *o, rheem_device_t *d) {
	char *p;
	bool updated = false;

	if (_getbool(o,&d->enabled,"@ENABLED.value")) {
		if (!_getbool(o,&d->enabled,"@ENABLED"))
			updated = true;
	} else {
		updated = true;
	}
	p = json_object_dotget_string(o,"@RUNNING");
	dprintf(dlevel,"p: %p\n", p);
	if (p) {
		if (strlen(trim(p))) {
			d->running = true;
			strncpy(d->status,p,sizeof(d->status)-1);
		} else {
			d->running = false;
			strcpy(d->status,"Not Running");
		}
		updated = true;
	}
	if (_getnumber(o,&d->mode,"@MODE.value")) {
		if (!_getnumber(o,&d->mode,"@MODE"))
			updated = true;
	} else {
		updated = true;
	}
	if (_getnumber(o,&d->temp,"@SETPOINT.value")) {
		if (!_getnumber(o,&d->temp,"@SETPOINT"))
			updated = true;
	} else {
		updated = true;
	}

	p = json_object_dotget_string(o,"@HOTWATER");
	if (p) {
		if (strstr(p,"hund"))
			d->level = 100;
		else if (strstr(p,"fourty"))
			d->level = 40;
		else if (strstr(p,"ten"))
			d->level = 10;
		else if (strstr(p,"empty") || strstr(p,"zero"))
			d->level = 0;
		else
			log_warning("unknown level string: %s\n", p);
		updated = true;
	}
	dprintf(dlevel,"updated: %d\n", updated);
	return updated;
}

#ifdef MQTT
static void rheem_getmsg(void *ctx, char *topic, char *message, int msglen, char *replyto) {
	rheem_session_t *s = ctx;
	json_value_t *v;
	json_object_t *o;
	rheem_device_t *d;
	char *name;

	dprintf(dlevel,"message: %s\n", message);
	v = json_parse(message);
	dprintf(dlevel+1,"v: %p\n", v);
	if (!v) return;
        if (json_value_get_type(v) != JSON_TYPE_OBJECT) goto rheem_getmsg_done;
        o = json_value_object(v);
	dprintf(dlevel+1,"o: %p\n", o);

	name = json_object_get_string(o,"device_name");
	dprintf(dlevel+1,"name: %s\n", name);
	if (!name) goto rheem_getmsg_done;

	d = rheem_get_device(s,name,false);
	dprintf(dlevel+1,"d: %p\n", d);
	if (!d) goto rheem_getmsg_done;

	if (_getstats(s,o,d)) s->repub = true;
	if (d->mode >= 0 && d->mode < d->modecount) strncpy(d->modestr,d->modes[d->mode],sizeof(d->modestr)-1);
//	rheem_dump_device(d);

rheem_getmsg_done:
	json_destroy_value(v);
	return;
}
#endif /* MQTT */

static int rheem_destroy(void *handle);
static void *rheem_new(void *driver, void *driver_handle) {
	rheem_session_t *s;

	s = calloc(1,sizeof(*s));
	if (!s) {
		log_syserror("rheem_new: calloc");
		return 0;
	}

#ifdef HAVE_CURL
	s->curl = curl_easy_init();
	if (!s->curl) {
		log_syserror("rheem_new: curl_easy_init");
		free(s);
		return 0;
	}
//	curl_easy_setopt(s->curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(s->curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(s->curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(s->curl, CURLOPT_WRITEFUNCTION, getdata);
	curl_easy_setopt(s->curl, CURLOPT_WRITEDATA, s);
	sprintf(s->endpoint,REST_URL);
#endif

	s->buffer = malloc(RHEEM_INIT_BUFSIZE);
	if (!s->buffer) {
		log_syserror("rheem_new: malloc(%d)",RHEEM_INIT_BUFSIZE);
#ifdef HAVE_CURL
		curl_easy_cleanup(s->curl);
#endif
		free(s);
		return 0;
	}
	s->bufsize = RHEEM_INIT_BUFSIZE;
	s->bufidx = 0;

#ifdef MQTT
	s->m = mqtt_new(true, rheem_getmsg, s);
	if (!s->m) {
		log_syserror("rheem_new: mqtt_new");
		rheem_destroy(s);
		return 0;
	}
#endif

	return s;
}

static int rheem_destroy(void *handle) {
	rheem_session_t *s = handle;
	rheem_device_t *d;
	int i;

#ifdef JS
	if (s->props) free(s->props);
	if (s->funcs) free(s->funcs);
#if 0
	if (s->ap && s->ap->js.e) {
		JSContext *cx = JS_EngineGetCX(s->ap->js.e);
		if (cx) {
			void *p = JS_GetPrivate(cx,s->an_obj);
			if (p) JS_free(cx,p);
		}
	}
#endif
#endif

	for(i=0; i < s->devcount; i++) {
		d = &s->devices[i];
		if (d->class_name) free(d->class_name);
	}
        if (s->ap) agent_destroy_agent(s->ap);
#ifdef MQTT
	if (s->m) mqtt_destroy_session(s->m);
#endif
	if (s->buffer) free(s->buffer);
#ifdef HAVE_CURL
	if (s->hs) curl_slist_free_all(s->hs);
	if (s->curl) curl_easy_cleanup(s->curl);
#endif
	free(s);
	return 0;
}

static int rheem_get_token(rheem_session_t *s, json_value_t *v) {
	json_object_t *o;
	char *p;

	dprintf(dlevel,"value type: %s\n", json_typestr(json_value_get_type(v)));
        if (json_value_get_type(v) != JSON_TYPE_OBJECT) return 1;
        o = json_value_object(v);
	p = json_object_dotget_string(o,"user_token");
	dprintf(dlevel,"p: %s\n", p);
	if (!p) {
		log_error("rheem_open: error getting user_token .. bad email/password?\n");
		return 1;
	}
	strncpy(s->user_token,p,sizeof(s->user_token)-1);
	p = json_object_dotget_string(o,"options.account_id");
	dprintf(dlevel,"p: %s\n", p);
	if (!p) {
		log_error("rheem_open: internal error: unable to get account_id\n");
		return 1;
	}
	strncpy(s->account_id,p,sizeof(s->account_id)-1);
	return 0;
}

#ifdef MQTT
int rheem_connect_mqtt(rheem_session_t *s) {
	char temp[1024];
	char client_id[128];

	/* Connect to the MQTT server */
	s->m->enabled = true;
	sprintf(client_id,"%s%ld_android",s->email,time(0));
	snprintf(temp,sizeof(temp)-1,"%s,%s,%s,%s",MQTT_URI,client_id,s->user_token,CLEAR_BLADE_SYSTEM_KEY);
	dprintf(dlevel,"temp: %s\n", temp);
	mqtt_parse_config(s->m,temp);
	if (mqtt_init(s->m)) {
		log_error("rheem_mqtt: mqtt_init failed: %s\n",s->m->errmsg);
		return 1;
	}
	/* Sub to the channels */
	snprintf(s->reported,sizeof(s->reported)-1,"user/%s/device/reported",s->account_id);
	dprintf(dlevel,"reported: %s\n", s->reported);
	mqtt_sub(s->m,s->reported);
	snprintf(s->desired,sizeof(s->desired)-1,"user/%s/device/desired",s->account_id);
	dprintf(dlevel,"desired: %s\n", s->desired);
//	mqtt_sub(s->m,s->desired);
	return 0;
}
#endif

static int rheem_close(void *handle);
static int rheem_open(void *handle) {
#ifdef HAVE_CURL
	rheem_session_t *s = handle;
	json_value_t *v;
	json_object_t *o;
	char temp[1024];

	if (!s) return 1;

	dprintf(dlevel,"open: %d\n", check_state(s,RHEEM_STATE_OPEN));
	if (check_state(s,RHEEM_STATE_OPEN)) return 0;

	/* Set initial header */
	if (s->hs) curl_slist_free_all(s->hs);
 	s->hs = curl_slist_append(0, "Content-Type: application/json; charset=UTF-8");
	s->hs = curl_slist_append(s->hs, "ClearBlade-SystemKey: " CLEAR_BLADE_SYSTEM_KEY);
	s->hs = curl_slist_append(s->hs, "ClearBlade-SystemSecret: "CLEAR_BLADE_SYSTEM_SECRET);
	curl_easy_setopt(s->curl, CURLOPT_HTTPHEADER, s->hs);

	/* Create the login info */
	o = json_create_object();
	json_object_set_string(o,"email",s->email);
	json_object_set_string(o,"password",s->password);
	json_dumps_r(json_object_value(o),temp,sizeof(temp)-1);
	dprintf(dlevel,"temp fields: %s\n", temp);
	json_destroy_object(o);

	/* Login and get user_token */
	*s->account_id = 0;
	*s->user_token = 0;
	v = rheem_request(s,ECONET_LOGIN,temp);
	dprintf(dlevel,"v: %p\n", v);
	if (!v) {
		json_destroy_value(v);
		goto rheem_open_error;
	}
	if (rheem_get_token(s,v)) {
		json_destroy_value(v);
		goto rheem_open_error;
	}
	json_destroy_value(v);
	snprintf(temp,sizeof(temp)-1,"ClearBlade-UserToken: %s", s->user_token);
	dprintf(dlevel,"temp: %s\n", temp);
	s->hs = curl_slist_append(s->hs, temp);
	curl_easy_setopt(s->curl, CURLOPT_HTTPHEADER, s->hs);

#ifdef MQTT
	/* Connect to the MQTT server for updates */
	if (!s->readonly && rheem_connect_mqtt(s)) goto rheem_open_error;
#endif

	set_state(s,RHEEM_STATE_OPEN);
	s->connected = 1;
	dprintf(dlevel,"done!\n");
	return 0;

rheem_open_error:
	rheem_close(s);
#endif
	return 1;
}

static int rheem_close(void *handle) {
	rheem_session_t *s = handle;

	if (!s) return 1;

#ifdef HAVE_CURL
	dprintf(dlevel,"hs: %p\n", s->hs);
	if (s->hs) {
		curl_slist_free_all(s->hs);
		s->hs = 0;
	}
	dprintf(dlevel,"curl: %p\n", s->curl);
#endif

#if 0
	if (s->curl) {
		curl_easy_cleanup(s->curl);
		s->curl = 0;
	}
	if (s->m) {
		mqtt_unsub(s->m,s->reported);
//		mqtt_unsub(s->m,s->desired);
		mqtt_disconnect(s->m,0);
		s->subed = false;
	}
#endif

	clear_state(s,RHEEM_STATE_OPEN);
	return 0;
}

static int _getdev(rheem_session_t *s, json_value_t *rv) {
	json_object_t *o;
	rheem_device_t *d;
	char *p;

        if (json_value_get_type(rv) != JSON_TYPE_OBJECT) return 1;
	o = json_value_object(rv);

	/* Make sure it's a WH */
	p = json_object_dotget_string(o,"device_type");
	dprintf(dlevel,"p: %s\n", p);
	if (!p) return 0;
	if (strcmp(p,"WH") != 0) return 0;

	/* Get the devptr */
	p = json_object_dotget_string(o,"device_name");
	if (!p) return 1;
	d = rheem_get_device(s,p,true);
	if (!d) return 1;

//	_getstring(o,d->name,sizeof(d->name)-1,"@NAME.value");

	_getmodes(json_object_dotget_array(o,"@MODE.constraints.enumText"),d);
	_getstats(s,o,d);
	_getstring(o,d->serial,sizeof(d->serial)-1,"serial_number");
	_getstring(o,d->mac,sizeof(d->mac)-1,"mac_address");
//	rheem_dump_device(d);
	return 0;
}

static int rheem_get_devices(rheem_session_t *s) {
	json_value_t *rv,*v;
	json_array_t *a,*aa;
	int i,type;

	rv = rheem_request(s,ECONET_LOCATION,0);
	if (!rv) return 1;

        if (json_value_get_type(rv) != JSON_TYPE_OBJECT) {
		json_destroy_value(rv);
		return 1;
	}
	v = json_object_dotget_value(json_value_object(rv),"results.locations");
	dprintf(dlevel,"v: %p\n", v);
	if (!v) {
		json_destroy_value(rv);
		return 1;
	}
	a = json_value_array(v);
	dprintf(dlevel,"count: %d\n", a->count);
	for(i=0; i < a->count; i++) {
		type = json_value_get_type(a->items[i]);
		dprintf(dlevel,"item[%d]: type: %s\n", i, json_typestr(type));
		if (json_value_get_type(a->items[i]) != JSON_TYPE_OBJECT) continue;
		aa = json_object_dotget_array(json_value_object(a->items[i]),"equiptments");
		dprintf(dlevel,"aa: %p\n", aa);
		if (aa) {
			dprintf(dlevel,"count: %d\n", aa->count);
			if (aa->count) _getdev(s, aa->items[0]);
		}
	}

	json_destroy_value(rv);
	return 0;
}

static int rheem_read(void *handle, uint32_t *control, void *buf, int buflen) {
	rheem_session_t *s = handle;
	long start;

	if (!strlen(s->email) || !strlen(s->password)) {
		log_error("email and password must be set\n");
		return 1;
	}

	start = mem_used();

#if 1
	dprintf(dlevel, "open: %d\n", check_state(s,RHEEM_STATE_OPEN));
	if (!check_state(s,RHEEM_STATE_OPEN)) {
		log_info("Logging in...\n");
		if (rheem_open(s)) {
			log_info("Login failed!\n");
			return 1;
		}
	}

	/* Get devices */
	dprintf(dlevel,"getting devices...\n");
	if (rheem_get_devices(s)) {
		log_error("get devices failed, closing\n");
		rheem_close(s);
		log_info("Logging in...\n");
		if (!rheem_open(s)) {
			log_info("Login failed!\n");
			return 1;
		}
		if (rheem_get_devices(s)) {
			log_error("get devices failed!\n");
			return 1;
		}
	}
	rheem_add_whprops(s);
#endif

	dprintf(dlevel,"used: %ld\n", mem_used() - start);
	return 0;
}

solard_driver_t rheem_driver = {
	"rheem",
	rheem_new,			/* New */
	rheem_destroy,			/* Destroy */
	rheem_open,			/* Open */
	rheem_close,			/* Close */
	rheem_read,			/* Read */
	0,				/* Write */
	rheem_config			/* Config */
};
