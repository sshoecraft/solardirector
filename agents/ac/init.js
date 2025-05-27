
// Dirs
script_dir = dirname(script_name);
//printf("script_dir: %s\n", script_dir);
lib_dir = (script_dir == "." ? "../../lib" : ac.libdir);
//printf("script_dir: %s, lib_dir: %s\n", script_dir, lib_dir);
sdlib_dir = lib_dir + "/sd";
//prin("sdlib_dir: %s\n", sdlib_dir);

// sdlib utils
include(sdlib_dir+"/init.js");
include(sdlib_dir+"/pa_client.js");

//dumpobj(ac);
//abort(1);

// My utils
include(script_dir+"/common.js");
include(script_dir+"/utils.js");

// Global vars
const INVALID_TEMP = -200.0;

// Notify all modules when location set
function ac_location_trigger(a,p,o) {

	let dlevel = 1;

	let n = p ? p.value : undefined;
	dprintf(dlevel,"new val: %s, old val: %s\n", n, o);

	if (ac.location) ac.signal("Location","Set");
	else ac.signal("Location","Clear");
}

function init_main() {

	let dlevel = 1;

	AC_UNIT_STANDARD_US = 0
	AC_UNIT_STANDARD_METRIC = 1;

	// Call init funcs
	let init_funcs = [
//		"snap",
		"mode",
		"errors",
		// XXX pumps must load before fans and units
		"pump",
		"fan",
		"unit",
		"cycle",
		"sample",
		"charge",
	];

	if (common_init(ac,dirname(script_name),init_funcs)) abort(1);
	if (pa_client_init(ac)) abort(1);

	let props = [
		[ "location", DATA_TYPE_STRING, "", 0, ac_location_trigger ],
		[ "standard", DATA_TYPE_INT, AC_UNIT_STANDARD_US, 0 ],
	];
	config.add_props(ac,props,ac.driver_name);

	// Have agent_run clean up any unread messages
	agent.purge = true;

	// Repub
	agent.repub();

	// Make our publish topic
	ac.topic = agent.mktopic(SOLARD_FUNC_DATA);

	dprintf(dlevel,"done!\n");
	return 0;
}
