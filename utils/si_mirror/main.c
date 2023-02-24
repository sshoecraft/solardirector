#ifdef MQTT
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "client.h"

void si_mirror_event(void *ctx, char *name, char *module, char *action) {
	char cmd[512];
	char *opt;

	dprintf(0,"event: name: %s, module: %s, action: %s\n", name, module, action);

	opt = 0;
	if (strcmp(name,"si") != 0) return;
	if (strcmp(module,"Charge") == 0 && strcmp(action,"Start") == 0) opt = "charge on";
	else if (strcmp(module,"Charge") == 0 && strcmp(action,"StartCV") == 0) opt = "charge cv";
	else if (strcmp(module,"Charge") == 0 && strcmp(action,"Stop") == 0) opt = "charge off";
	else if (strcmp(module,"Feed") == 0 && strcmp(action,"Start") == 0) opt = "set feed_enabled true";
	else if (strcmp(module,"Feed") == 0 && strcmp(action,"Stop") == 0) opt = "set feed_enabled false";
	else if (strcmp(module,"Battery") == 0 && strcmp(action,"Empty") == 0) opt = "battery_empty";
	else if (strcmp(module,"Battery") == 0 && strcmp(action,"Full") == 0) opt = "battery_full";
	dprintf(1,"opt: %s\n", opt);
	if (!opt) return;

	snprintf(cmd,sizeof(cmd)-1,"%s/sdconfig si %s",SOLARD_BINDIR,opt);
	log_info("Running command: %s\n", cmd);
	system(cmd);
	return;
}

int main(int argc, char **argv) {
	opt_proctab_t opts[] = {
		/* Spec, dest, type len, reqd, default val, have */
		OPTS_END
	};
	config_property_t props[]  = {
		/* name, type, dest, dsize, def, flags, scope, values, labels, units, scale, precision, trigger, ctx */
		{0}
	};
	event_session_t *e;
#ifdef MQTT
	solard_client_t *c;

	c = client_init(argc,argv,"1.0",opts,"si_mirror",CLIENT_FLAG_NOJS,props,0,0,0);
	if (!c) return 1;
	e = c->e;
#else
	solard_driver_t driver;
	solard_agent_t *ap;

	memset(&driver,0,sizeof(driver));
	driver.name = "si_mirror";
//	driver.config = si_mirror_config;
	ap = agent_init(argc,argv,"1.0",opts,&driver,0,AGENT_FLAG_NOJS,props,0);
	if (!ap) {
		log_error("unable to initialize agent\n");
		return 1;
	}
	e = ap->e;
#endif

	event_handler(e, si_mirror_event, 0, "si", "*", "*");

	log_info("Mirroring from host: %s\n", c->m->uri);
	while(1) {
		dprintf(1,"connected: %d\n", c->m->connected);
		if (!c->m->connected) mqtt_reconnect(c->m);
		sleep(10);
	}
	return 0;
}
#else
#include <stdio.h>
int main(void) { printf("error: compiled without mqtt support\n"); }
#endif
