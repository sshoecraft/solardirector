
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "rheem.h"

extern char *rheem_agent_version_string;

static int set_location(void *ctx, config_property_t *p, void *old_value) {
	rheem_session_t *s = ctx;

	/* punt to javascript */
	if (s->ap) agent_jsexec(s->ap, "set_location();");
	return 0;
}

static int set_readonly(void *ctx, config_property_t *p, void *old_value) {
	rheem_session_t *s = ctx;
	bool b;

	dprintf(1,"connected: %d\n", s->connected);
	if (!s->connected) return 0;
	if (p->dest) conv_type(DATA_TYPE_BOOLEAN,&b,sizeof(b),p->type,p->dest,p->len);
	else b = false;
#ifdef MQTT
	dprintf(1,"readonly: %d, mqtt_connected: %d\n", b, s->m->connected);
	if (!s->readonly && !s->m->connected) rheem_connect_mqtt(s);
#endif

	return 0;
}

static int rheem_refresh(void *ctx, list args, char *errmsg, json_object_t *results) {
	rheem_session_t *s = ctx;

	dprintf(dlevel,"refreshing...\n");
	if (!rheem_driver.read(ctx,0,0,0)) {
		agent_start_script(s->ap,s->ap->js.read_script);
		if (!rheem_driver.write(ctx,0,0,0)) agent_start_script(s->ap,s->ap->js.write_script);
	}
	return 0;
}

#ifdef MQTT
static int rheem_cb(void *ctx) {
	rheem_session_t *s = ctx;

	if (!s->m) return 0;
	if (s->m->connected != s->lastcon) {
		dprintf(dlevel,"connected: %d\n", s->m->connected);
		s->lastcon = s->m->connected;
	}
	dprintf(dlevel,"repub: %d\n", s->repub);
	if (s->repub) {
		agent_start_jsfunc(s->ap, "pub.js", "rheem_publish", 0, 0);
		s->repub = false;
	}
	return 0;
}
#endif

int rheem_agent_init(int argc, char **argv, rheem_session_t *s) {
	config_property_t rheem_props[] = {
		/* name, type, dest, dsize, def, flags, scope, values, labels, units, scale, precision, trigger, ctx */
		{ "email", DATA_TYPE_STRING, &s->email, sizeof(s->email)-1, 0, 0 },
		{ "password", DATA_TYPE_STRING, &s->password, sizeof(s->password)-1, 0, 0 },
		{ "location", DATA_TYPE_STRING, &s->location, sizeof(s->location)-1, 0, 0, 0, 0, 0, 0, 0, 0, set_location, s },
		{ "readonly", DATA_TYPE_BOOL, &s->readonly, 0, "no", 0, 0, 0, 0, 0, 0, 0, set_readonly, s },
		{ 0 }
	};
	config_function_t rheem_funcs[] = {
		{ "refresh", rheem_refresh, s, 0 },
		{0}
	};

	s->ap = agent_init(argc,argv,rheem_agent_version_string,0,&rheem_driver,s,0,rheem_props,rheem_funcs);
	if (!s->ap) return 1;
#ifdef MQTT
	agent_set_callback(s->ap, rheem_cb, s);
#endif
	return 0;
}

rheem_device_t *rheem_get_device(rheem_session_t *s, char *id, bool add) {
	int i;
	rheem_device_t *d;

	dprintf(dlevel,"id: %s, devcount: %d, max: %d\n", id, s->devcount, RHEEM_MAX_DEVICES);
	if (s->devcount >= RHEEM_MAX_DEVICES) return 0;
	for(i=0; i < s->devcount; i++) {
		if (strcmp(s->devices[i].id,id) == 0)
			return &s->devices[i];
	}
	if (!add) return 0;
	d = &s->devices[s->devcount++];
	d->type = RHEEM_DEVICE_TYPE_WH;
	strcpy(d->id,id);
	d->s = s;
	return d;
}

static json_value_t *rheem_get_info(rheem_session_t *s) {
	json_object_t *o;

	dprintf(dlevel,"creating info...\n");

	o = json_create_object();
	if (!o) return 0;
	json_object_set_string(o,"agent_name",rheem_driver.name);
	json_object_set_string(o,"agent_role",SOLARD_ROLE_UTILITY);
	json_object_set_string(o,"agent_description","Rheem Water Heater Agent");
	json_object_set_string(o,"agent_version",rheem_agent_version_string);
	json_object_set_string(o,"agent_author","Stephen P. Shoecraft");
	json_object_set_string(o,"device_manufacturer","Rheem");
	config_add_info(s->ap->cp, o);

	return json_object_value(o);
}

static int rheem_set_enabled(void *ctx, config_property_t *p, void *old_value) {
#ifdef MQTT
        rheem_device_t *d = ctx;
	rheem_session_t *s = d->s;
	json_object_t *o;
	char ts[32], temp[64], *str;

	dprintf(dlevel,"s: %p\n", s);
	if (!s) return 1;

	dprintf(dlevel,"old_value: %p\n", old_value);
	if (old_value && *(bool *)old_value == d->enabled) return 0;

	log_info("Setting %s.enabled to: %s\n", d->id, d->enabled ? "true" : "false");
	if (s->readonly) return 0;

	o = json_create_object();
	if (!o) return 1;
	get_timestamp(ts,sizeof(ts),true);
	str = strchr(ts,' ');
	if (str) *str = 'T';
	sprintf(temp,"ANDROID_%s",ts);
	dprintf(dlevel,"temp: %s\n", temp);
	json_object_set_string(o,"transactionId",temp);
	json_object_set_string(o,"device_name",d->id);
	json_object_set_string(o,"serial_number",d->serial);
	json_object_set_number(o,"@ENABLED",d->enabled);
	str = json_dumps(json_object_value(o),0);
	dprintf(dlevel,"str: %s\n", str);
	if (!s->readonly && mqtt_pub(s->m, s->desired, str, 0, 0)) log_error("set_enabled: mqtt_pub failed!\n");
	free(str);
#endif
	return 0;
}

static int rheem_set_mode(void *ctx, config_property_t *p, void *old_value) {
#ifdef MQTT
        rheem_device_t *d = ctx;
	rheem_session_t *s = d->s;
	json_object_t *o;
	char ts[32], temp[64], *str;
	int i,mode;

	dprintf(dlevel,"s: %p\n", s);
	if (!s) return 1;

	dprintf(dlevel,"old_value: %p\n", old_value);
	if (old_value && strcmp((char *)old_value,d->modestr) == 0) return 0;

	mode = 1;
	for(i=0; i < d->modecount; i++) {
		dprintf(dlevel,"mode[%d]: %s\n", i, d->modes[i]);
		if (strcmp(d->modestr, d->modes[i]) == 0) {
			mode = i;
			break;
		}
	}

	log_info("Setting %s.mode to: %s (%d)\n", d->id, d->modes[mode], mode);
	if (s->readonly) return 0;

	o = json_create_object();
	if (!o) return 1;
	get_timestamp(ts,sizeof(ts),true);
	str = strchr(ts,' ');
	if (str) *str = 'T';
	sprintf(temp,"ANDROID_%s",ts);
	dprintf(dlevel,"temp: %s\n", temp);
	json_object_set_string(o,"transactionId",temp);
	json_object_set_string(o,"device_name",d->id);
	json_object_set_string(o,"serial_number",d->serial);
	json_object_set_number(o,"@MODE",mode);
	str = json_dumps(json_object_value(o),0);
	dprintf(dlevel,"str: %s\n", str);
	if (mqtt_pub(s->m, s->desired, str, 0, 0)) log_error("set_mode: mqtt_pub failed!\n");
	free(str);
#endif /* MQTT */
	return 0;
}

static int rheem_set_temp(void *ctx, config_property_t *p, void *old_value) {
#ifdef MQTT
        rheem_device_t *d = ctx;
	rheem_session_t *s = d->s;
	json_object_t *o;
	char ts[32], temp[64], *str;

	dprintf(dlevel,"s: %p\n", s);
	if (!s) return 1;

	dprintf(dlevel,"old_value: %p\n", old_value);
	if (old_value && *(int *)old_value == d->temp) return 0;

	log_info("Setting %s.temp to: %d\n", d->id, d->temp);
	if (s->readonly) return 0;

	o = json_create_object();
	if (!o) return 1;
	get_timestamp(ts,sizeof(ts),true);
	str = strchr(ts,' ');
	if (str) *str = 'T';
	sprintf(temp,"ANDROID_%s",ts);
	dprintf(dlevel,"temp: %s\n", temp);
	json_object_set_string(o,"transactionId",temp);
	json_object_set_string(o,"device_name",d->id);
	json_object_set_string(o,"serial_number",d->serial);
	json_object_set_number(o,"@SETPOINT",d->temp);
	str = json_dumps(json_object_value(o),0);
	dprintf(dlevel,"str: %s\n", str);
	if (mqtt_pub(s->m, s->desired, str, 0, 0)) log_error("set_emp: mqtt_pub failed!\n");
	free(str);
#endif
	return 0;
}

void rheem_add_whprops(rheem_session_t *s) {
	int i,j,n,r,added;
	rheem_device_t *d;
	char temp[256],*p;

	added = false;
	dprintf(dlevel,"devcount: %d\n", s->devcount);
	for(i=0; i < s->devcount; i++) {
		d = &s->devices[i];
		if (!d->haveprops) {
			p = temp;
			r = sizeof(temp);
			dprintf(dlevel,"modecount: %d\n", d->modecount);
			for(j=0; j < d->modecount; j++) {
				dprintf(dlevel+1,"j: %d\n", j);
				if (j) {
					n = snprintf(p,r,",");
					p += n;
					r -= n;
				}
				n = snprintf(p,r,d->modes[j]);
				p += n;
				r -= n;
			}
			dprintf(dlevel+1,"temp: %s\n",temp);
			p = strdup(temp);
			int flags = CONFIG_FLAG_NOSAVE;
			config_property_t rheem_wh_props[] = {
				{ "enabled", DATA_TYPE_BOOL, &d->enabled, 0, 0, flags, 0, 0, 0, 0, 0, 1, rheem_set_enabled, d },
				{ "running", DATA_TYPE_BOOL, &d->running, 0, 0, flags | CONFIG_FLAG_READONLY },
				{ "mode", DATA_TYPE_STRING, d->modestr, sizeof(d->modestr)-1, 0, flags, "select", p, 0, 0, 0, 1, rheem_set_mode, d },
				{ "temp", DATA_TYPE_INT, &d->temp, 0, 0, flags, "range", "0,140,1", 0, 0, 0, 1, rheem_set_temp, d },
				{ "level", DATA_TYPE_INT, &d->level, 0, 0, flags | CONFIG_FLAG_READONLY },
				{0},
			};
			dprintf(dlevel,"adding props to section: %s\n", d->id);
			config_add_props(s->ap->cp, d->id, rheem_wh_props, 0);
//			free(p);
			d->haveprops = true;
			added = true;
		}
	}
	if (added) {
		/* Re-create info and publish it */
		if (s->ap->info) json_destroy_value(s->ap->info);
		s->ap->info = rheem_get_info(s);
#ifdef MQTT
		agent_pubinfo(s->ap,0);
#endif
	}
}

int rheem_config(void *h, int req, ...) {
	rheem_session_t *s = h;
	va_list va;
	int r;

	va_start(va,req);
	dprintf(dlevel,"req: %d\n", req);
	r = 1;
	switch(req) {
	case SOLARD_CONFIG_INIT:
		s->ap = va_arg(va,solard_agent_t *);
		r = rheem_jsinit(s);
		break;
	case SOLARD_CONFIG_GET_INFO:
		{
			json_value_t **vp = va_arg(va,json_value_t **);
			dprintf(dlevel,"vp: %p\n", vp);
			if (vp) {
				*vp = rheem_get_info(s);
				r = 0;
			}
		}
		break;
	}
	return r;
}
