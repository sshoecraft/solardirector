
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "sb.h"

extern char *sb_agent_version_string;

json_value_t *sb_get_info(sb_session_t *s) {
	json_object_t *o;
	char model[64];
	char *fields;

	dprintf(1,"creating info...\n");

	if (sb_driver.open(s)) return 0;

	*model = 0;
	fields = sb_mkfields((char *[]){ "6800_08822000" }, 1);
	if (fields) {
		json_value_t *v = sb_request(s,SB_GETVAL,fields);
		if (v) {
			list results = sb_get_results(s,v);
			if (results) {
				sb_value_t *vp = sb_get_result_value(results,"6800_08822000");
				conv_type(DATA_TYPE_STRING,model,sizeof(model),vp->type,vp->data,vp->len);
				sb_destroy_results(results);
			}
			json_destroy_value(v);
		}
		free(fields);
	}

	o = json_create_object();
	if (!o) return 0;
	json_object_set_string(o,"agent_name",sb_driver.name);
	json_object_set_string(o,"agent_role",SOLARD_ROLE_PVINVERTER);
	json_object_set_string(o,"agent_description","SMA Sunny Boy Agent");
	json_object_set_string(o,"agent_version",sb_agent_version_string);
	json_object_set_string(o,"agent_author","Stephen P. Shoecraft");
	json_object_set_string(o,"agent_transports","http,https");
	json_object_set_string(o,"device_manufacturer","SMA");
	if (*model) json_object_set_string(o,"device_model",model);
	config_add_info(s->ap->cp, o);

	return json_object_value(o);
}
