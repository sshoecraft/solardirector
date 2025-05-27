
function common_get_state(name,objs,def) {

	let dlevel = 2;

	if (typeof(def) == "undefined") def = 0;
	dprintf(dlevel,"name: %s\n", name);
	let obj = objs[name];
	dprintf(dlevel,"obj: %s\n", obj);
	if (!obj) return def;

	dprintf(dlevel,"returning: %d\n", obj.state);
	return obj.state;
}

function common_set_state(name,label,objs,newstate) {

	let dlevel = 2;

	dprintf(dlevel,"name: %s\n", name);
	let obj = objs[name];
	dprintf(dlevel,"obj: %s\n", obj);
	if (!obj) return 1;

	ssfunc = label + "_statestr";
	dprintf(dlevel,"ssfunc: %s\n", ssfunc);
	if (newstate != obj.state) {
		log_verbose("%ss[%s]: state: %s -> %s\n", label, name,
			window[ssfunc](obj.state),window[ssfunc](newstate));
		obj.state = newstate;
	}
}

function common_start(name,label,objs,callback) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s\n", name);
	let obj = objs[name];
	if (!obj) {
		config.errmsg = sprintf("start_%s: %s %s not found",label,label,name);
		log_error("%s\n",config.errmsg);
		return 1;
	}

	dprintf(dlevel,"%ss[%s]: enabled: %s\n", label, name, obj.enabled);
	if (!obj.enabled) {
		config.errmsg = sprintf("start_%s: %s %s is disabled",label,label,name);
		log_error("%s\n",config.errmsg);
		return 1;
	}

	dprintf(dlevel,"%ss[%s]: refs: %s\n", label, name, obj.refs);
	if (!obj.refs) {
		log_info("Starting %s %s\n", label, name);
		ac.signal(label + " " + name,"Start");
	}
	obj.refs++;
//	dprintf(dlevel,"%ss[%s]: NEW refs: %s\n", label, name, obj.refs);
	return (callback ? callback(name,obj) : 0);
}

function common_stop(name,label,objs,callback,force) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, force: %s\n", name, force);
	let obj = objs[name];
	if (!obj) {
		config.errmsg = sprintf("stop_%s: %s %s not found",label,label,name);
		log_error("%s\n",config.errmsg);
		return 1;
	}

	if (force) obj.refs = 1;
	dprintf(dlevel,"name: %s, refs: %d\n", name, obj.refs);
	if (obj.refs) {
		obj.refs--;
		dprintf(dlevel,"name: %s, refs: %d\n", name, obj.refs);
		if (!obj.refs) {
			log_info("%sStopping %s %s\n", force ? "[force] " : "", label, name);
			ac.signal(label + " " + name,"Stop");
			return callback(name,obj);
		}
	}
	if (force) error_clear(label,name);
	return 0;
}
