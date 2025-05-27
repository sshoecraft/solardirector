
// Load common modules
include(dirname(script_name)+"/utils.js");
include(dirname(script_name)+"/jconfig.js");
include(dirname(script_name)+"/sdconfig.js");
include(dirname(script_name)+"/suncalc.js");

// args are agent instance (object of type Agent) and modules array (array of strings)
// module strings can be just module name or module name:init func format
function common_init(instance,script_dir,modules) {

	let dlevel = 1;

	if (checkargs(arguments,{ instance: "object", script_dir: "string", modules: "array" })) return 1;

	dprintf(1,"length: %d\n", modules.length);
	for(let i=0; i < modules.length; i++) {

		let module_name, func_name;
		let idx = modules[i].indexOf(":")
		if (idx >=0) {
			let arr = modules[i].split(":");
			module_name = arr[0];
			func_name = arr[1];
		} else {
			module_name = modules[i];
			func_name = module_name + "_init";
		}
		dprintf(dlevel,"module_name: %s, func_name: %s\n", module_name, func_name);

		include(script_dir+"/"+module_name+".js");

		// Call the init func
		let func = global[func_name];
		dprintf(dlevel,"func type: %s\n", typeof(func));
		if (typeof(func) != "function") {
			log_error("init: %s is not a function!\n",func_name);
			return 1;
		}
		if (func(instance)) {
			log_error("error calling %s\n", func_name);
			return 1;
		}
	}

	return 0;
}
