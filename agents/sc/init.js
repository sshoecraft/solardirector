
// Dirs
script_dir = dirname(script_name);
//printf("script_dir: %s\n", script_dir);
lib_dir = (script_dir == "." ? "../../lib" : sc.libdir);
//printf("script_dir: %s, lib_dir: %s\n", script_dir, lib_dir);
sdlib_dir = lib_dir + "/sd";
//prin("sdlib_dir: %s\n", sdlib_dir);

// sdlib utils
include(sdlib_dir+"/init.js");
include(sdlib_dir+"/pa_client.js");

function init_main() {

	let dlevel = 1;

	// Call init funcs
	let init_funcs = [
	];

	if (common_init(sc,dirname(script_name),init_funcs)) abort(1);
	if (pa_client_init(sc)) abort(1);

	let props = [
//		[ "location", DATA_TYPE_STRING, "", 0, sc_location_trigger ],
	];
	config.add_props(sc,props,sc.driver_name);

	// Have agent_run clean up any unread messages
	agent.purge = true;

	// Repub
	agent.repub();

	// Make our publish topic
	sc.topic = agent.mktopic(SOLARD_FUNC_DATA);

	dprintf(dlevel,"done!\n");
	return 0;
}
