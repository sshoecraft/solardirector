
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "sb.h"

extern char *sb_agent_version_string;

void fix_user(struct opt_process_table *pt, char *arg) {
	if (strcmp(arg,"user") == 0) strcpy(pt->dest,"usr");
	else if (strcmp(arg,"installer") == 0) strcpy(pt->dest,"istl");
	else strncpy(pt->dest,arg,pt->len);
}

int sb_agent_init(sb_session_t *s, int argc, char **argv) {
	opt_proctab_t opts[] = {
		/* Spec, dest, type len, reqd, default val, have */
		{ "-E::url|endpoint",&s->endpoint,DATA_TYPE_STRING,sizeof(s->endpoint)-1,0,"" },
		{ "-u::|user",s->user,DATA_TYPE_STRING,sizeof(s->user)-1,0,"usr",fix_user },
		{ "-p::|password",s->password,DATA_TYPE_STRING,sizeof(s->password)-1,0,"" },
		{ "-S::|specify strings file",&s->strings_filename,DATA_TYPE_STRING,sizeof(s->strings_filename)-1,0,"" },
		{ "-O::|specify objects file",&s->objects_filename,DATA_TYPE_STRING,sizeof(s->objects_filename)-1,0,"" },
		{ "-w#|write flag",&s->write_flag,DATA_TYPE_BOOL,0,0,"" },
		{ "-H#|write C include",&s->inc_flag,DATA_TYPE_BOOL,0,0,"N" },
		{ "-N|dont enter run loop",&s->norun_flag,DATA_TYPE_BOOL,0,0,"N" },
		OPTS_END
	};
	config_property_t sb_props[] = {
		{ "endpoint", DATA_TYPE_STRING, s->endpoint, sizeof(s->endpoint)-1, 0, 0 },
		{ "user", DATA_TYPE_STRING, s->user, sizeof(s->user)-1, 0, 0, },
		{ "password", DATA_TYPE_STRING, s->password, sizeof(s->password)-1, 0, 0 },
		{ "connected", DATA_TYPE_BOOLEAN, &s->connected, 0, 0, CONFIG_FLAG_PRIVATE },
		{ "lang", DATA_TYPE_STRING, &s->lang, sizeof(s->lang)-1, "en-US", 0 },
		{ "strings", DATA_TYPE_STRING, &s->strings_filename, sizeof(s->strings_filename)-1, 0, 0 },
		{ "use_internal_strings", DATA_TYPE_BOOLEAN, &s->use_internal_strings, 0, "Y", 0 },
		{ "objects", DATA_TYPE_STRING, &s->objects_filename, sizeof(s->objects_filename)-1, 0, 0 },
		{ "log_power", DATA_TYPE_BOOL, &s->log_power, 0, "no", 0, "select", "0, 1", "log output_power from each read", 0, 1, 0 },

		{0}
	};
	config_function_t sb_funcs[] = {
		{0}
	};

	s->ap = agent_init(argc,argv,sb_agent_version_string,opts,&sb_driver,s,0,sb_props,sb_funcs);
	dprintf(1,"ap: %p\n",s->ap);
	if (!s->ap) return 1;
	if (strcmp(s->user,"user") == 0) strcpy(s->user,"usr");
	else if (strcmp(s->user,"installer") == 0) strcpy(s->user,"istl");
	return 0;
}

int sb_get_config(sb_session_t *s) {
	json_value_t *v;

	if (!s->config_fields) {
		s->config_fields = sb_mkfields(0,0);
		if (!s->config_fields) return 1;
	}
	v = sb_request(s,"dyn/getAllParamValues.json",s->config_fields);
	if (!v) return 1;
	json_destroy_value(v);
	return 0;
}

int sb_config(void *h, int req, ...) {
	sb_session_t *s = h;
	va_list va;
	int r;

	r = 1;
	va_start(va,req);
	dprintf(1,"req: %d\n", req);
	switch(req) {
	case SOLARD_CONFIG_INIT:
		/* 1st arg is AP */
		s->ap = va_arg(va,solard_agent_t *);

		dprintf(1,"strings_filename: %s(%d)\n", s->strings_filename,strlen(s->strings_filename));
		if (*s->strings_filename) {
			if (s->write_flag || s->inc_flag) {
				if (sb_get_strings(s)) goto config_error;
				if (sb_write_strings(s,s->strings_filename)) goto config_error;
			} else {
				if (sb_read_strings(s,s->strings_filename)) goto config_error;
			}
		} else {
			if (sb_get_strings(s))
				return 1;
		}
		dprintf(1,"objects_filename: %s(%d)\n", s->objects_filename,strlen(s->objects_filename));
		if (*s->objects_filename) {
			if (s->write_flag || s->inc_flag) {
				if (sb_get_objects(s)) goto config_error;
				if (sb_write_objects(s,s->objects_filename)) goto config_error;
			} else {
				if (sb_read_objects(s,s->objects_filename)) goto config_error;
			}
		} else {
			if (sb_get_objects(s)) goto config_error;
		}

		/* Add si_data to config */
//		if (si_config_add_si_data(s)) goto config_error;

#ifdef JS
		/* Init JS */
		sb_jsinit(s);
#endif

		r = 0;
		break;
	case SOLARD_CONFIG_GET_INFO:
		{
			json_value_t **vp = va_arg(va,json_value_t **);
			dprintf(1,"vp: %p\n", vp);
			if (vp) {
				*vp = sb_get_info(s);
				r = 0;
			}
		}
		break;
	default:
		break;
	}
config_error:
	dprintf(1,"returning: %d\n", r);
	return r;
}
