
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "jk.h"

extern char *jk_version_string;
//#define _getint(p) (int)(*(p) | (*((p)+1) << 8) | (*((p)+2) << 16) | (*((p)+3) << 24))

json_value_t *jk_get_info(void *handle) {
	jk_session_t *s = handle;
	json_object_t *j;

	dprintf(1,"s: %p\n", s);
	if (!handle) return 0;

	/* Get the info */
	if (jk_open(s) < 0) return 0;
	if (jk_get_hwinfo(s)) return 0;
//	jk_close(s);

	j = json_create_object();
	if (!j) return 0;
	json_object_set_string(j,"agent_name","jk");
	json_object_set_string(j,"agent_role",SOLARD_ROLE_BATTERY);
	json_object_set_string(j,"agent_description","JIKONG BMS Agent");
	json_object_set_string(j,"agent_version",jk_version_string);
	json_object_set_string(j,"agent_author","Stephen P. Shoecraft");
	json_object_set_string(j,"device_manufacturer","JIKONG");
	json_object_set_string(j,"device_model",s->hwinfo.model);
	json_object_set_string(j,"device_version",s->hwinfo.hwvers);

	if (s->ap && s->ap->cp) config_add_info(s->ap->cp,j);

	return json_object_value(j);
}
