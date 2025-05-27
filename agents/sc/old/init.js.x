
include("../../lib/sd/init.js");
dumpobj(sc);
abort(1);
exit(1);

/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

// Dirs
script_dir = dirname(script_name);
if (new File("../lib").exists) {
	lib_dir = (script_dir == "." ? "../lib" : sc.libdir);
} else {
	lib_dir = (script_dir == "." ? "../../lib" : sc.libdir);
}
//printf("script_dir: %s, lib_dir: %s\n", script_dir, lib_dir);

// libsd scripts
sd_dir = lib_dir + "/sd";
//printf("sd_dir: %s\n", sd_dir);
include(sd_dir+"/init.js");
include(sd_dir+"/utils.js");
include(sd_dir+"/jconfig.js");

// My data topic
var data_topic = SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/" + agent.name + "/" + SOLARD_FUNC_DATA;

// Notify all modules when location set
function sc_location_trigger() {

	let dlevel = 1;

	dprintf(dlevel,"location: %s\n", sc.location);
	if (!sc.location) return;

	agent.event("Location","Set");
	return 0;
}

function sc_event_handler(name,module,action) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, module: %s, action: %s\n", name, module, action);
//	if (name != agent.name) return;

	// handle repub events
	if (module == "Agent" && action == "repub") {
		init_main();
	}
}

function init_main() {

	let dlevel = 1;

	dprintf(dlevel,"starting...\n");

	// XXX for testing
	if (typeof(sc) == "undefined") sc = new Driver("sc");

	// global props
	let props = [
//		[ "location", DATA_TYPE_STRING, null, 0, sc_location_trigger ],
		[ "notify", DATA_TYPE_BOOL, "true", 0 ],
		[ "notify_path", DATA_TYPE_STRING, SOLARD_BINDIR + "/notify", 0 ],
		[ "warning_time", DATA_TYPE_INT, 120, 0 ],
		[ "error_time", DATA_TYPE_INT, 300, 0 ],
		[ "notify_time", DATA_TYPE_INT, 600, 0 ],

	];

	// define props before calling init funcs
	config.add_props(sc,props);

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
		dprintf(dlevel,"r: %d\n", r);
		if (r) {
			printf("error calling %s in %s\n", init_funcs[i]+"_init", script_dir+"/"+init_funcs[i]+".js");
			error = 1;
			break;
		}
	}
	if (error) return 1;

	// Load agent config
	load_agents();

        // Dont purge unprocessed messages
        agent.purge = false;

	// re-publish info (in the case of scripts being disable then enabled after init)
	agent.pubinfo();
	agent.pubconfig();

	agent.interval = 5;
	dprintf(dlevel,"done!\n");
	return 0;
}
