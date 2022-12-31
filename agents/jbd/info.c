
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "jbd.h"

extern char *jbd_version_string;

static int _can_get_hwinfo(jbd_session_t *s) {
	return 1;
}

static int _std_get_hwinfo(jbd_session_t *s) {
	uint8_t data[128];
	unsigned short mfgdate;
	int day,mon,year,len;
	double version;

//	return 0;
	if (jbd_rw(s, JBD_CMD_READ, JBD_CMD_HWINFO, data, sizeof(data)) < 0) return 1;
	mfgdate = jbd_getshort(&data[10]);
	day = mfgdate & 0x1f;
	mon = (mfgdate >> 5) & 0x0f;
	year = 2000 + (mfgdate >> 9);
	dprintf(3,"year: %d, mon: %d, day: %d\n", year, mon, day);
	sprintf(s->hwinfo.mfgdate,"%04d%02d%02d",year,mon,day);
	dprintf(3,"mfgdate: %s\n", s->hwinfo.mfgdate);
	version = data[18] / 10.0;
	sprintf(s->hwinfo.version,"%.1f", version);
	if (jbd_eeprom_open(s) < 0) return 1;
	if (jbd_rw(s, JBD_CMD_READ, JBD_REG_MFGNAME, data, sizeof(data)) < 0) return 1;
	/* prefixed with a 1 byte length */
	len = data[0];
	if (len > (sizeof(s->hwinfo.manufacturer)-1)) len = sizeof(s->hwinfo.manufacturer)-1;
	strncat(s->hwinfo.manufacturer,(char *)&data[1],data[0]);
//	bindump("manuf",s->hwinfo.manufacturer,strlen(s->hwinfo.manufacturer));
	trim(s->hwinfo.manufacturer);
	dprintf(1,"mfgname: %s\n", s->hwinfo.manufacturer);
	if (jbd_rw(s, JBD_CMD_READ, JBD_REG_MODEL, data, sizeof(data)) < 0) return 1;
	/* prefixed with a 1 byte length */
	len = data[0];
	if (len > (sizeof(s->hwinfo.model)-1)) len = sizeof(s->hwinfo.model)-1;
	strncat(s->hwinfo.model,(char *)&data[1],len);
//	bindump("model",s->hwinfo.model,strlen(s->hwinfo.model));
	trim(s->hwinfo.model);
	dprintf(1,"model: %s\n", s->hwinfo.model);
	jbd_eeprom_close(s);
	return 0;
}

int jbd_get_hwinfo(jbd_session_t *s) {
	int r;

	if (!s->tp) {
		dprintf(1,"no tp!\n");
		return 1;
	}
	dprintf(5,"transport: %s\n", s->tp->name);
	if (strncmp(s->tp->name,"can",3)==0) 
		r = _can_get_hwinfo(s);
	else
		r = _std_get_hwinfo(s);
	return r;
}

json_value_t *jbd_get_info(jbd_session_t *s) {
	json_object_t *o;
	uint8_t data[8];
	uint16_t val;
	char tpstr[128];
	int i,have_info,bytes;

	if (!s) return 0;

	/* Get the info */
	if (jbd_open(s) < 0) return 0;
	have_info = (jbd_get_hwinfo(s) ? 0 : 1);

	/* Get balance info */
	if (jbd_eeprom_open(s) < 0) return 0;
	bytes = jbd_rw(s, JBD_CMD_READ, JBD_REG_FUNCMASK, data, sizeof(data));
	if (bytes < 0) return 0;
	val = jbd_getshort(data);
	dprintf(1,"val: %d\n", val);
	if (val & JBD_FUNC_CHG_BALANCE)
		s->balancing = 2;
	else if (val & JBD_FUNC_BALANCE_EN)
		s->balancing = 1;
	else
		s->balancing = 0;
	jbd_eeprom_close(s);

	o = json_create_object();
	if (!o) return 0;
	json_object_set_string(o,"agent_name",jbd_driver.name);
	json_object_set_string(o,"agent_role",SOLARD_ROLE_BATTERY);
	json_object_set_string(o,"agent_description","JBD BMS Agent");
	json_object_set_string(o,"agent_version",jbd_version_string);
	json_object_set_string(o,"agent_author","Stephen P. Shoecraft");
	for(*tpstr=i=0; jbd_transports[i]; i++) {
		if (i) strcat(tpstr,",");
		strcat(tpstr,jbd_transports[i]->name);
        }
	json_object_set_string(o,"agent_transports",tpstr);
	if (have_info) {
		json_object_set_string(o,"device_manufacturer",s->hwinfo.manufacturer);
		json_object_set_string(o,"device_model",s->hwinfo.model);
		json_object_set_string(o,"device_mfgdate",s->hwinfo.mfgdate);
		json_object_set_string(o,"device_version",s->hwinfo.version);
	}
	config_add_info(s->ap->cp,o);

	return json_object_value(o);
}
