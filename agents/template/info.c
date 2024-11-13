
/*
Copyright (c) 2024, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "template.h"

extern char *template_version_string;

json_value_t *template_get_info(template_session_t *s) {
	json_object_t *o;

	dprintf(dlevel,"creating info...\n");

	o = json_create_object();
	if (!o) return 0;
	json_object_set_string(o,"agent_name",template_driver.name);
	json_object_set_string(o,"agent_role",SOLARD_ROLE_UTILITY);
        json_object_set_string(o,"agent_description","+DESC+");
	json_object_set_string(o,"agent_version",template_version_string);
	json_object_set_string(o,"agent_author","Stephen P. Shoecraft");

	config_add_info(s->ap->cp, o);

	dprintf(dlevel,"returning: %p\n", json_object_value(o));
	return json_object_value(o);
}
