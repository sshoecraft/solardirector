
// all errors have a predefined ID and go into an errors array

function errors_init() {

	// Create the errors array
	ac.errors = [];

	let ac_error_funcs = [
		[ "clear_error", clear_error, 1 ]
	];

	config.add_funcs(ac,ac_error_funcs);
}

function error_set(module,action,msg) {

	let dlevel = 1;

	dprintf(dlevel,"module: %s, action: %s, msg: %s\n", module, action, msg);

	dprintf(dlevel,"length: %d\n", ac.errors.length);
	for(let i = 0; i < ac.errors.length; i++) {
		let ent = ac.errors[i];
		if (ent.module == module && ent.action == action && ent.msg == msg) return;
	}

	let newent = {
		module: module,
		action: action,
		msg: msg
	};
//	dumpobj(newent);
	ac.errors.push(newent);
//	dumpobj(ac.errors);
	log_error("%s\n",msg);
}

function error_clear(module,action,msg) {

	let dlevel = -1;

	if (typeof(module) == "undefined") module = "all";
	if (typeof(action) == "undefined") action = "all";
	if (typeof(msg) == "undefined") msg = "all";
	dprintf(dlevel,"module: %s, action: %s, msg: %s\n", module, action, msg);

	dprintf(dlevel,"length: %d\n", ac.errors.length);
	for(let i = 0; i < ac.errors.length; i++) {
		let ent = ac.errors[i];
		if ((ent.module == module || module == "all") &&
				(ent.action == action || action == "all") &&
				(ent.msg == msg || msg == "all")) {
			dprintf(-1,"clearing error: %s\n", msg);
			ac.errors.splice(i,1);
		}
	}
}

function clear_error(msg) {

	let dlevel = 1;

	dprintf(dlevel,"msg: %s\n", msg);

	dprintf(dlevel,"length: %d\n", ac.errors.length);
	let found = 1;
	while(found) {
		found = 0;
		for(let i = 0; i < ac.errors.length; i++) {
			if (ac.errors[i].msg == msg || msg == 'all') {
				dprintf(dlevel,"clearing error!\n");
				ac.errors.splice(i,1);
				found = 1;
				break;
			}
		}
	}
	if (msg == "all") {
		for(let name in fans) { fans[name].error = false; }
		for(let name in pumps) { pumps[name].error = false; }
		for(let name in units) { units[name].error = false; }
	}
}

function get_errors(module,action) {

	let dlevel = 1;

	if (typeof(module) == "undefined") module = "all";
	if (typeof(action) == "undefined") action = "all";
	dprintf(dlevel,"module: %s, action: %s\n", module, action);

	let msgarray = [];

	dprintf(dlevel,"length: %d\n", ac.errors.length);
	for(let i = 0; i < ac.errors.length; i++) {
		let ent = ac.errors[i];
		if ((ent.module == module || module == "all") && (ent.action == action || action == "all")) 
			msgarray.push(ent.msg);
	}

	return msgarray;
}
