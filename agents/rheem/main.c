
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "rheem.h"

#define TESTING 0
#define TESTLVL 3

char *rheem_agent_version_string = "1.0";

#define dlevel 1

int main(int argc, char **argv) {
	rheem_session_t *s;
//	char mqtt_info[1024];
#if TESTING
        char *args[] = { "rheem", "-d", STRINGIFY(TESTLVL), "-c", "rheemtest.json" };
        argc = (sizeof(args)/sizeof(char *));
        argv = args;
#endif

	s = rheem_driver.new(0,0);
	if (!s) return 1;
	if (rheem_agent_init(argc,argv,s)) return 1;

#if 0
	s->c = client_init(0,0,rheem_agent_version_string,0,"rheem",CLIENT_FLAG_NOJS,0,0);
	if (!s->c) return 1;
printf("client init'd!\n");
	mqtt_disconnect(s->c->m,1);
	mqtt_get_config(mqtt_info,sizeof(mqtt_info),s->ap->m,0);
	mqtt_parse_config(s->c->m,mqtt_info);
	mqtt_newclient(s->c->m);
	mqtt_connect(s->c->m,10);
	mqtt_resub(s->c->m);
#endif

	if (!strlen(s->location)) {
		char epsave[256];
		json_value_t *v;

		strcpy(epsave,s->endpoint);
		strcpy(s->endpoint,"https://geolocation-db.com");
		v = rheem_request(s,"json/",0);
		dprintf(dlevel,"v: %p\n", v);
		if (v) {
			json_object_t *o = json_value_object(v);
			json_value_t *lat,*lon;

			lat = json_object_dotget_value(o,"latitude");
			lon = json_object_dotget_value(o,"longitude");
			dprintf(dlevel,"lat: %p, lon: %p\n", lat, lon);
			if (lat && lon) {
				float flat = json_value_get_number(lat);
				float flon = json_value_get_number(lon);
				char location[32];
				config_property_t *p;

				sprintf(location,"%f,%f", flat, flon);
				p = config_get_property(s->ap->cp,s->ap->instance_name,"location");
				dprintf(dlevel,"p: %p\n", p);
				if (p) {
					config_property_set_value(p,DATA_TYPE_STRING,location,strlen(location),true,true);
					config_write(s->ap->cp);
				}
			}
		}
		strcpy(s->endpoint,epsave);
	}

	agent_run(s->ap);
	rheem_driver.destroy(s);
	agent_shutdown();
	return 0;
}
