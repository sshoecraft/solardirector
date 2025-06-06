
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

// Dirs
script_dir = dirname(script_name);
lib_dir = (script_dir == "." ? "../../lib" : si.libdir);
//printf("script_dir: %s, lib_dir: %s\n", script_dir, lib_dir);
core_dir = lib_dir + "/sd";
//printf("core_dir: %s\n", core_dir);

// Core
include(core_dir+"/init.js");
include(core_dir+"/utils.js");
include(core_dir+"/kalman.js");

// Our utils
include(script_dir+"/utils.js");

/* global vars */
info = si.info;

// Notify all modules when location set
function si_location_trigger() {

	let dlevel = 1;

	dprintf(dlevel,"location: %s\n", si.location);
	if (!si.location) return;

	si.signal(si.name,"Location","Set");
}

function init_main() {

	let dlevel = 1;

	// Call init funcs
	let init_funcs = [
		"mirror",
		"sync",
		"charge",
		"feed",
		"grid",
		"gen",
		"soc",
		"current",
	];

	dprintf(1,"length: %d\n", init_funcs.length);
	for(let i=0; i < init_funcs.length; i++) {
		dprintf(dlevel,"calling: %s\n", init_funcs[i]+"_init");
		run(script_dir+"/"+init_funcs[i]+".js",init_funcs[i]+"_init");
	}

	let props = [
		[ "location", DATA_TYPE_STRING, null, 0, si_location_trigger ],
	];

	config.add_props(si,props,si.driver_name);

	// re-publish info (in the case of scripts being disable then enabled after init)
	agent.pubinfo();
	agent.pubconfig();

	si.topic = agent.mktopic(SOLARD_FUNC_DATA);

	dprintf(dlevel,"done!\n");
	return 0;
}
