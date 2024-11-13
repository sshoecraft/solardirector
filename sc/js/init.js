
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

// Dirs
script_dir = dirname(script_name);
lib_dir = (script_dir == "." ? "../lib" : sc.libdir);
//printf("script_dir: %s, lib_dir: %s\n", script_dir, lib_dir);

// libsd scripts
sd_dir = lib_dir + "/sd";
//printf("sd_dir: %s\n", sd_dir);
include(sd_dir+"/init.js");
include(sd_dir+"/utils.js");


// Notify all modules when location set
function sc_location_trigger() {

	let dlevel = 1;

	dprintf(dlevel,"location: %s\n", sc.location);
	if (!sc.location) return;

	agent.event("Location","Set");
	return 0;
}

function init_main() {

	let dlevel = -1;

	dprintf(dlevel,"starting...\n");

	// Call init funcs
	let init_funcs = [
		"agents",
		"monitor"
	];

	dprintf(1,"length: %d\n", init_funcs.length);
	let error = 0;
	for(let i=0; i < init_funcs.length; i++) {
		dprintf(dlevel,"calling: %s\n", init_funcs[i]+"_init");
		let r = run(script_dir+"/"+init_funcs[i]+".js",init_funcs[i]+"_init");
		dprintf(-1,"r: %d\n", r);
		if (r) {
			printf("error calling %s in %s\n", init_funcs[i]+"_init", script_dir+"/"+init_funcs[i]+".js");
			error = 1;
			break;
		}
	}
	if (!error) return;

	let props = [
//		[ "location", DATA_TYPE_STRING, null, 0, sc_location_trigger ],
	];

	config.add_props(sc,props);

	// re-publish info (in the case of scripts being disable then enabled after init)
	agent.pubinfo();
	agent.pubconfig();

	dprintf(dlevel,"done!\n");
	return 0;
}
