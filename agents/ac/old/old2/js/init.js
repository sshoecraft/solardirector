
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

// My utils
include(script_dir+"/common.js");
include(script_dir+"/utils.js");

// Global vars
var INVALID_TEMP = -200.0;
var data_topic = SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/" + ac.name + "/" + SOLARD_FUNC_DATA;

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
		"temp",
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

	// make the std objects global
	config = ac.config;
	mqtt = ac.mqtt;
	influx = ac.influx;
	event = ac.event;

	if (common_init(ac,dirname(script_name),init_funcs)) abort(1);
	if (pa_client_init(ac)) abort(1);

if (0) {
	let loc = getLocation(true);
	let location;
	if (loc) location = loc.lat + "," + loc.lon;
	else location = "";
}

	let props = [
		[ "location", DATA_TYPE_STRING, "", 0, ac_location_trigger ],
		[ "standard", DATA_TYPE_INT, AC_UNIT_STANDARD_US, 0 ],
		[ "interval", DATA_TYPE_INT, 10, CONFIG_FLAG_NOWARN ],
	];

	config.add_props(ac,props);

//	agent.purge = true;

	// Repub
//	ac.repub();

	dprintf(dlevel,"done!\n");
	return 0;
}
