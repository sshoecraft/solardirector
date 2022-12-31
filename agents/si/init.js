
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

// Dirs
script_dir = dirname(script_name);
core_dir = script_dir + "/../../core";

// Core
include(core_dir+"/init.js");
include(core_dir+"/utils.js");
include(core_dir+"/kalman.js");
include(core_dir+"/inverter.js");
include(core_dir+"/charger.js");

// Our utils
include(script_dir+"/utils.js");

/* global vars */
info = si.info;
fields = inverter_fields.concat(charger_fields).unique();
fields.push("name");
fields.push("remain_text");
fields.sort();
data_topic = SOLARD_TOPIC_ROOT+"/Agents/"+si.agent.name+"/"+SOLARD_FUNC_DATA;

function init_main() {

	var dlevel = 1;

	// Call init funcs
	var init_funcs = [
		"soc",				// SoC must be before charge due to possible battery_kwh reset
		"charge",
//		"sim",
		"feed",
		"grid",
		"gen",
	];

	dprintf(1,"length: %d\n", init_funcs.length);
	for(i=0; i < init_funcs.length; i++) {
		dprintf(dlevel,"calling: %s\n", init_funcs[i]+"_init");
		run(script_dir+"/"+init_funcs[i]+".js",init_funcs[i]+"_init");
	}
//	abort(0);

	// re-publish info (in the case of scripts being disable then enabled after init)
	agent.pubinfo();

	dprintf(dlevel,"done!\n");
	return 0;
}
