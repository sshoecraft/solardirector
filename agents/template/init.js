
// Dirs
script_dir = dirname(script_name);
lib_dir = (script_dir == "." ? "../../lib" : template.libdir);
//printf("script_dir: %s, lib_dir: %s\n", script_dir, lib_dir);
sdlib_dir = lib_dir + "/sd";
//printf("sdlib_dir: %s\n", sdlib_dir);

// sdlib utils
include(sdlib_dir+"/init.js");
include(sdlib_dir+"/utils.js");

// Notify all modules when location set
function template_location_trigger() {

	let dlevel = 1;

	dprintf(dlevel,"location: %s\n", template.location);
	if (!template.location) return;

	agent.event("Location","Set");
}

function init_main() {
	let props = [
		[ "location", DATA_TYPE_STRING, null, 0, template_location_trigger ],
	];

	let dlevel = 1;

	// Call init funcs
	let init_funcs = [
	];

	dprintf(1,"length: %d\n", init_funcs.length);
	for(let i=0; i < init_funcs.length; i++) {
		dprintf(dlevel,"calling: %s\n", init_funcs[i]+"_init");
		run(script_dir+"/"+init_funcs[i]+".js",init_funcs[i]+"_init");
	}

	config.add_props(template,props);

	dprintf(dlevel,"done!\n");
	return 0;
}
