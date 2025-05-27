
// Called from PA asking us to release power
function pa_client_revoke(module,item,amount_str) {

	let dlevel = 1;

	let amount = parseFloat(amount_str);
	log_verbose("reovking: module: %s, item: %s, amount: %s\n", module, item, amount);

	let func_name = module + "_revoke";
	dprintf(dlevel,"func_name: %s\n", func_name);
	let func = global[func_name];
	dprintf(dlevel,"func type: %s\n", typeof(func));
	if (typeof(func) != "function") {
		config.errmsg = sprintf("revoke: %s is not a function!", func_name);
		log_error("%s\n",config.errmsg);
		return 1;
	}
	return func(item,amount);
}

function pa_client_init(instance) {
	let props = [
		[ "pa_name", DATA_TYPE_STRING, "pa", 0 ],
		[ "pa_delay", DATA_TYPE_INT, 10, 0 ],
	];
	config.add_props(instance,props,instance.driver_name);
	let funcs = [
		[ "revoke", pa_client_revoke, 3 ],
	];
	config.add_funcs(instance,funcs,instance.driver_name);
}

function pa_client_reserve(instance,module,item,amount,priority) {

	let dlevel = 1;

	dprintf(dlevel,"module: %s, item: %s, amount: %.1f, priority: %d\n",module,item,amount,priority);

	if (arguments.length < 5) {
		log_error("pa_client_reserve requires 5 arguments: instance,module,item,amount,priority\n");
		return 1;
	}

	if (typeof(instance[module]) == "undefined") instance[module] = [];
	if (typeof(instance[module][item]) == "undefined") instance[module][item] = time() - instance.pa_delay;
	dprintf(dlevel,"last reserve time: %s\n", instance[module][item]);
	let diff = time() - instance[module][item];
	dprintf(dlevel,"diff: %d, pa_delay: %d\n", diff, instance.pa_delay);
	if (diff < instance.pa_delay) return 1;
	let r = sdconfig(instance,instance.pa_name,"reserve",instance.name,module,item,amount,priority);
//	if (debug >= dlevel) dumpobj(r,"sdconfig result");
	dprintf(dlevel,"r.status: %d\n", r.status);
	if (r.status) dprintf(dlevel,"failed: %s\n", r.message);
	instance[module][item] = time();
	if (!r.status) log_info("reserved: module: %s, item: %s, amount: %.1f, priority: %d\n",module,item,amount,priority);
	return r.status;
}

function pa_client_release(instance,module,item,amount) {

	let dlevel = 1;

	log_info("release: module: %s, item: %s, amount: %.1f\n",module,item,amount);

	if (arguments.length < 4) {
		log_error("pa_client_release requires 4 arguments: instance,module,item,amount\n");
		return 1;
	}

	let r = sdconfig(instance,instance.pa_name,"release",instance.name,module,item,amount);
//	if (debug >= dlevel) dumpobj(r,"sdconfig result");
	dprintf(dlevel,"r.status: %d\n", r.status);
	if (r.status) dprintf(dlevel,"failed: %s\n", r.message);
	return r.status;
}

function pa_client_repri(instance,module,item,amount,priority) {

	let dlevel = 1;

	log_info("repri: module: %s, item: %s, amount: %.1f, priority: %d\n",module,item,amount,priority);

	if (arguments.length < 5) {
		log_error("pa_client_repri requires 5 arguments: instance,module,item,amount,priority\n");
		return 1;
	}

	let r = sdconfig(instance,instance.pa_name,"repri",instance.name,module,item,amount,priority);
	if (debug >= dlevel) dumpobj(r,"sdconfig result");
	dprintf(dlevel,"r.status: %d\n", r.status);
	if (r.status) dprintf(dlevel,"failed: %s\n", r.message);
	return r.status;
}
