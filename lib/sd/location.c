

/*
Copyright (c) 2024, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"
#include "utils.h"

double *get_location(bool useinet) {
#if 0
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
#endif
	return 0;
}
