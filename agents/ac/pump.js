
function _pump_init(name,pump) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, pin: %d\n", name, pump.pin);
	if (pump.pin >= 0) pinMode(pump.pin,OUTPUT);
	pump.state = PUMP_STATE_STOPPED;
	if (pump.pin >= 0 && digitalRead(pump.pin)) {
		pump_set_state(name,PUMP_STATE_RUNNING);
		pump.refs = 1;
	} else {
		pump_set_state(name,PUMP_STATE_STOPPED);
		pump.refs = 0;
	}
	pump.error = false;
	pump.start_time = 0;
	pump.stop_time = 0;
	pump.cycle_time = 0;
	pump.direct = false;
	pump.last_temp_in = INVALID_TEMP;
	pump.settled = false;
	dprintf(dlevel,"pump[%s]: state: %s, refs: %d\n", name, pump_statestr(pump.state), pump.refs)
	return 0;
}

function pump_add(name,opts) {
	return jconfig_add(name,opts,"pump",pumps,pump_props,_pump_init);
}

function pump_del(name) {
	let fan;
	for(let key in fans) {
		fan = fans[key];
		if (fan.pump == name) {
			config.errmsg = sprintf("unable to delete pump %s: pump is in use by fan %s", name, key);
			return 1;
		}
	}
	let unit;
	for(let key in units) {
		unit = units[key];
		if (unit.pump == name) {
			config.errmsg = sprintf("unable to delete pump %s: pump is in use by unit %s", name, key);
			return 1;
		}
	}
	if (name == ac.sample_pump) {
		config.errmsg = sprintf("unable to delete pump %s: pump is currently being used as a sample pump", name);
		return 1;
	}
	if (pump_is_primer(name)) {
		config.errmsg = sprintf("unable to delete pump %s: pump is designated as a primer by other pumps", name);
		return 1;
	}
	pump_force_stop(name);
	return jconfig_del(name,"pump",pumps,pump_props);
}

function pump_get(name) {
	return jconfig_get(name,"pump",pumps,pump_props);
}

function pump_set(name,opts) {
	let r = jconfig_set(name,opts,"pump",pumps,pump_props);
	if (!r) if (pumps[name].pin >= 0) pinMode(pumps[name].pin,OUTPUT);
	return r;
}

function pump_get_config(name) {
	return jconfig_get_config(name,"pump",pumps,pump_props);
}

function pump_set_config(config) {
	return jconfig_set_config(config,"pump",pumps,pump_props,load_pumps);
}

function load_pumps() {
	jconfig_load("pumps",pump_props,_pump_init);
//	dumpobj(pumps);
}

/*********************************************************************************************/

function pump_is_primer(name) {

	let dlevel = 1;

	let r = false;
	for(let pump_name in pumps) {
		let pump = pumps[pump_name];
		if (pump.primer == name) {
			r = true;
			break;
		}
	}
	return r;
}

function pump_get_state(name) {
	return common_get_state(name,pumps);
}

function pump_on(name,pump) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, pin: %d\n", name, pump.pin);
	if (set_pin(pump.pin,HIGH)) {
		error_set("pump",name,sprintf("start_pump: unable to start pump %s!",name));
		return 1;
	}
	pump.start_time = time();
	return 0;
}

function pump_start(name) {
//	dprintf(-1,"==> caller: %s\n", getCallerFunctionName());
	return common_start(name,"pump",pumps);
}

function pump_off(name,pump) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, pin: %d, primer %s\n", name, pump.pin, pump.primer);
	if (pump.primer.length) pump_stop(pump.primer);
	if (pump.pin >= 0 && set_pin(pump.pin,LOW)) {
		error_set("pump",name,sprintf("stop_pump: %s: unable to stop pump!", name));
		return 1;
	}
	if (pump.reserve) pump_set_state(name,PUMP_STATE_RELEASE);
	else pump_set_state(name,PUMP_STATE_STOPPED);
	pump.stop_time = time();
	return 0;
}

function pump_cooldown(name,pump) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, cooldown: %d\n", name, pump.cooldown);
	if (!pump.cooldown) return pump_off(name,pump);
	pump.time = time();
	pump_set_state(name,PUMP_STATE_COOLDOWN);
	return 0;
}

function pump_stop(name) {
	return common_stop(name,"pump",pumps,pump_cooldown,false)
}

function pump_force_stop(name) {
	if (pumps[name].primer.length) pump_stop(pumps[name].primer);
	return common_stop(name,"pump",pumps,pump_off,true)
}

function pump_disable(name) {
        return common_disable(name,"pump",pumps,pump_props);
}

function pump_enable(name) {
        return common_enable(name,"pump",pumps,pump_props);
}

function pump_init() {

	let dlevel = 1;

	let s = 0;
	PUMP_STATE_STOPPED = s++;
	PUMP_STATE_RESERVE = s++;
	PUMP_STATE_START_PRIMER = s++;
	PUMP_STATE_WAIT_PRIMER = s++;
	PUMP_STATE_START = s++;
	PUMP_STATE_WAIT_PUMP = s++;
	PUMP_STATE_FLOW = s++;
	PUMP_STATE_WARMUP = s++;
	PUMP_STATE_RUNNING = s++;
	PUMP_STATE_COOLDOWN = s++
	PUMP_STATE_RELEASE = s++;
	PUMP_STATE_ERROR = s++;

	// Declare the per-pump props here as a global
	pump_props = [
		[ "enabled", DATA_TYPE_BOOL, "true", 0 ],
		[ "pin", DATA_TYPE_INT, "-1", 0 ],
		[ "primer", DATA_TYPE_STRING, "", 0 ],
		[ "primer_timeout", DATA_TYPE_INT, 30, 0 ],
		[ "flow_sensor", DATA_TYPE_STRING, "", 0 ],
		[ "min_flow", DATA_TYPE_DOUBLE, 0, 0 ],
		[ "wait_time", DATA_TYPE_INT, 10, 0 ],
		[ "flow_wait_time", DATA_TYPE_INT, 5, 0 ],
		[ "flow_timeout", DATA_TYPE_INT, 20, 0 ],
		[ "temp_in_sensor", DATA_TYPE_STRING, "", 0 ],
		[ "temp_out_sensor", DATA_TYPE_STRING, "", 0 ],
		[ "warmup", DATA_TYPE_INT, "30", 0 ],
		[ "warmup_threshold", DATA_TYPE_DOUBLE, "10.0", 0 ],
		[ "cooldown", DATA_TYPE_INT, "0", 0 ],
		[ "cooldown_threshold", DATA_TYPE_DOUBLE, "10.0", 0 ],
		[ "direct_group", DATA_TYPE_STRING, "", 0 ],
		[ "reserve", DATA_TYPE_INT, 0, 0 ],
		[ "priority", DATA_TYPE_INT, 100, 0 ],
		[ "error", DATA_TYPE_BOOL, "false", 0, 0 ],
	];

	let funcs = [
		[ "add_pump", pump_add, 2 ],
		[ "del_pump", pump_del, 1 ],
		[ "get_pump", pump_get, 1 ],
		[ "set_pump", pump_set, 2 ],
		[ "get_pump_config", pump_get_config, 1 ],
		[ "set_pump_config", pump_set_config, 1 ],
		[ "start_pump", pump_start, 1 ],
		[ "stop_pump", pump_stop, 1 ],
		[ "force_stop_pump", pump_force_stop, 1 ],
		[ "disable_pump", pump_disable, 1 ],
		[ "enable_pump", pump_enable, 1 ],
	];

	config.add_funcs(ac,funcs,ac.driver_name);
	load_pumps();
}

function pump_set_state(name,state) {
	return common_set_state(name,"pump",pumps,state);
}

function pump_statestr(state) {
	let dlevel = 4;

	dprintf(dlevel,"state: %d\n", state);
	switch(state) {
	case PUMP_STATE_STOPPED:
		str = "Stopped";
		break;
	case PUMP_STATE_RESERVE:
		str = "Reserve";
		break;
	case PUMP_STATE_START_PRIMER:
		str = "Start primer";
		break;
	case PUMP_STATE_WAIT_PRIMER:
		str = "Wait primer";
		break;
	case PUMP_STATE_START:
		str = "Start";
		break;
	case PUMP_STATE_WAIT_PUMP:
		str = "Wait pump";
		break;
	case PUMP_STATE_FLOW:
		str = "Wait flow";
		break;
	case PUMP_STATE_WARMUP:
		str = "Warmup";
		break;
	case PUMP_STATE_RUNNING:
		str = "Running";
		break;
	case PUMP_STATE_COOLDOWN:
		str = "Cooldown";
		break;
	case PUMP_STATE_RELEASE:
		str = "Release";
		break;
	case PUMP_STATE_ERROR:
		str = "Error";
		break;
	default:
		str = sprintf("Unknown(%d)",state);
		break;
	}

	dprintf(dlevel,"returning: %s\n", str);
	return str;
}

function pump_revoke(name) {

	let dlevel = -1;

	dprintf(dlevel,"name: %s\n", name);

	let pump = pumps[name];
	dprintf(dlevel,"pump: %s\n", pump);
	if (typeof(pump) == "undefined") {
		config.errmsg = "invalid pump";
		return 1;
	}
	dprintf(dlevel,"pump[%s]: state: %s\n", name, pump_statestr(pump.state));
	pump_stop(name);
}
