
function _fan_init(name,fan) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, topic: %s\n", name, fan.topic);
	if (!fan.topic) {
		fan.topic = SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/" + name + "/" + SOLARD_FUNC_DATA;
		dprintf(dlevel,"NEW topic: %s\n", fan.topic);
	}
	fan.refs = 0;
	fan.state = FAN_STATE_STOPPED;
	fan.error = false;
	fan.wt_warned = false;
	fan.fan_state = "off";
	fan.heat_state = "off";
	fan.cool_state = "off";
	fan.mode = FAN_MODE_NONE;
	mqtt.sub(fan.topic);

	dprintf(dlevel,"pump: %s\n", fan.pump);
	if (fan.pump.length) {
		let found = false;
		for(let key in pumps) {
			if (key == fan.pump) {
				found = true;
				break;
			}
		}
		dprintf(dlevel,"found: %s\n", found);
		if (!found) {
			config.errmsg = sprintf("fan %s specifies pump %s which doesnt exist\n", name, fan.pump);
			log_error("%s\n", config.errmsg);
			return 1;		
		}
	}

	// We could go ask if the fan is running
	fan_set_state(name,FAN_STATE_STOPPED);
	return 0;
}

function fan_add(name,opts) {
	return jconfig_add(name,opts,"fan",fans,fan_props,_fan_init);
}

function fan_del(name) {
	fan_force_stop(name);
	return jconfig_del(name,"fan",fans,fan_props);
}

function fan_get(name) {
	return jconfig_get(name,"fan",fans,fan_props);
}

function fan_set(name,opts) {
	return jconfig_set(name,opts,"fan",fans,fan_props);
}

function fan_get_config(name) {
	return jconfig_get_config(name,"fan",fans,fan_props);
}

function fan_set_config(config) {
	return jconfig_set_config(config,"fan",fans,fan_props,load_fans);
}

function load_fans() {
	jconfig_load("fans",fan_props,_fan_init);
}

/*************************************************************************************************/

function fan_set_mode(name,mode) {

	let dlevel = 1;

	let fan = fans[name];
	dprintf(dlevel,"fan[%s]: current mode: %s, new mode: %s\n", name, fan_modestr(fan.mode), fan_modestr(mode));
	if (mode != fan.mode) {
		let pump = pumps[fan.pump];
		if (mode == FAN_MODE_NONE) {
			dprintf(dlevel,"*** STOPPING FAN ***\n");
			if (pump.direct_group.length) direct_disable(pump.direct_group);
			fan.refs = 1;
			fan_stop(name);
			fan.mode = FAN_MODE_NONE;
		} else if (fan.state == FAN_STATE_RUNNING) {
			dprintf(dlevel,"fan[%s]: (RUNNING) current mode: %s, new mode: %s\n", name, fan_modestr(fan.mode), fan_modestr(mode));
				fan.mode = mode;
if (0) {
			if (fan.mode == FAN_MODE_NONE) {
				fan.mode = mode;
			} else {
				// fan is running and mode change from COOL to HEAT or HEAT to COOL
				error_set("fan",name,sprintf("unable to set fan %s mode from %s to %s: fan is running", name, fan_modestr(fan.mode), fan_modestr(mode)));
				return 1;
			}
}
		} else {
			dprintf(dlevel,"fan[%s]: state: %s\n", name, fan_statestr(fan.state));
			dprintf(dlevel,"ac.mode: %s\n", ac_modestr(ac.mode));
			if (fan_modestr(mode) != ac_modestr(ac.mode)) {
				// If the mode doesnt match the storage mode, go direct
				if (pump.direct_group.length) {
					if (direct_enable(pump.direct_group))
						return 1;
				} else {
					error_set("fan",name,sprintf("fan %s requested mode (%s) does not match storage (%s) and direct group not set", name, fan_modestr(mode), ac_modestr(ac.mode)));
					return 1;
				}
			}
			dprintf(dlevel,"*** STARTING FAN ***\n");
			if (!fan_start(name)) fan.mode = mode;
		}
	}
	return 0;
}

function fan_get_state(name) {
	return common_get_state(name,fans);
}

function fan_on(name,fan) {

	let dlevel = 1;

	// air handlers use mqtt
	dprintf(dlevel,"name: %s, state: %d\n", name, fan.state);
	let retries = 3;
	let r;
	while(retries--) {
		r = sdconfig(ac,name,"fan_on");
//		dprintf(dlevel,"r: %s\n", r);
//		if (debug >= dlevel) dumpobj(r,"sdconfig result");
		if (r.status == 0) break;
		sleep(1);
	}
	if (r.status != 0) {
		error_set("fan",name,r.message);
		fan.enabled = false;
	}
	return r.status;
}

function fan_start(name) {
	return common_start(name,"fan",fans);
}

function fan_off(name,fan) {

	let dlevel = 1;

	// Stop the pump first
	dprintf(dlevel,"fan[%s]: pump: %s\n", name, fan.pump);
	if (fan.pump.length) pump_stop(fan.pump);

	dprintf(dlevel,"name: %s, state: %d\n", name, fan.state);
	let retries = 3;
	let r;
	while(retries--) {
		r = sdconfig(ac,name,"fan_off");
//		dprintf(dlevel,"r: %s\n", r);
//		if (debug >= dlevel) dumpobj(r,"sdconfig result");
		if (r.status == 0) break;
		sleep(1);
	}
	if (r.status != 0) {
		error_set("fan",name,r.message);
		fan.enabled = false;
	}
	if (fan.reserve) fan_set_state(name,FAN_STATE_RELEASE);
	else fan_set_state(name,FAN_STATE_STOPPED);
	return r.status;
}

function fan_cooldown(name,fan) {

    let dlevel = 1;

        dprintf(dlevel,"name: %s, min_runtime: %d, cooldown: %d\n", name, fan.min_runtime, fan.cooldown);
        let runtime_needed = fan.min_runtime > fan.cooldown ? fan.min_runtime : fan.cooldown;
        if (!runtime_needed) return fan_off(name,fan);
        // Don't reset fan.time - it was set when fan reached RUNNING state
        // Keep pump running during MIN_RUNTIME_WAIT - prevents pump cycling
        fan_set_state(name,FAN_STATE_MIN_RUNTIME_WAIT);
        return 0;
}

function fan_stop(name) {
		// XXX if its in min runtime wait already dont reset time
		if (fans[name].state == FAN_STATE_MIN_RUNTIME_WAIT) return 0;
        return common_stop(name,"fan",fans,fan_cooldown,false)
}

function fan_force_stop(name) {
	if (fans[name].pump.length) pump_force_stop(fans[name].pump);
        return common_stop(name,"fan",fans,fan_off,true)
}

function fan_disable(name) {
	return common_disable(name,"fan",fans,fan_props);
}

function fan_enable(name) {
	let fan = fans[name];
	if (fan) {
		fan.fan_state = fan.heat_state = fan.cool_state = "off";
		fan.mode = FAN_MODE_NONE;
	}
	return common_enable(name,"fan",fans,fan_props);
}

function fan_init() {

	let dlevel = 1;

	let s = 0;
	FAN_STATE_STOPPED = s++;
	FAN_STATE_RESERVE = s++;
	FAN_STATE_START_PUMP = s++;
	FAN_STATE_WAIT_PUMP = s++;
	FAN_STATE_START = s++;
	FAN_STATE_WAIT_START = s++;
	FAN_STATE_RUNNING = s++;
	FAN_STATE_MIN_RUNTIME_WAIT = s++;
	FAN_STATE_RELEASE = s++;
	FAN_STATE_ERROR = s++;

	// Fans can have a "none" mode (fan on)
	s = 0;
	FAN_MODE_NONE = s++;
	FAN_MODE_COOL = s++;
	FAN_MODE_HEAT = s++;

	// Declare the per-fan props here as a global
	fan_props = [
		[ "enabled", DATA_TYPE_BOOL, "true", 0 ],
		[ "topic", DATA_TYPE_STRING, null, 0 ],
		[ "pump", DATA_TYPE_STRING, null, 0 ],
		[ "start_timeout", DATA_TYPE_INT, 60, 0 ],
		[ "pump_timeout", DATA_TYPE_INT, 40, 0 ],
		[ "min_runtime", DATA_TYPE_INT, 600, 0 ],
		[ "cooldown", DATA_TYPE_INT, 30, 0 ],
		[ "cooldown_threshold", DATA_TYPE_DOUBLE, 25.0, 0 ],
		[ "reserve", DATA_TYPE_INT, 0, 0 ],
		[ "priority", DATA_TYPE_INT, 100, 0 ],
		[ "error", DATA_TYPE_BOOL, "false", 0, CONFIG_FLAG_PRIVATE ],
		[ "stop_wt", DATA_TYPE_BOOL, "true", 0 ],
		[ "wt_thresh", DATA_TYPE_INT, 3, 0 ],
	];

	let funcs = [
		[ "add_fan", fan_add, 2 ],
		[ "del_fan", fan_del, 1 ],
		[ "set_fan", fan_set, 2 ],
		[ "get_fan", fan_get, 1 ],
		[ "get_fan_config", fan_get_config, 1 ],
		[ "set_fan_config", fan_set_config, 1 ],
		[ "start_fan", fan_start, 1 ],
		[ "stop_fan", fan_stop, 1 ],
		[ "force_stop_fan", fan_force_stop, 1 ],
		[ "disable_fan", fan_disable, 1 ],
		[ "enable_fan", fan_enable, 1 ],
	];

	config.add_funcs(ac,funcs,ac.driver_name);
	load_fans();
}

function fan_set_state(name,state) {
	return common_set_state(name,"fan",fans,state);
}

function fan_statestr(state) {
	let dlevel = 5;

	dprintf(dlevel,"state: %d\n", state);
	switch(state) {
	case FAN_STATE_STOPPED:
		str = "Stopped";
		break;
	case FAN_STATE_RESERVE:
		str = "Reserve";
		break;
	case FAN_STATE_START_PUMP:
		str = "Start pump";
		break;
	case FAN_STATE_WAIT_PUMP:
		str = "Wait pump";
		break;
	case FAN_STATE_START:
		str = "Start";
		break;
	case FAN_STATE_WAIT_START:
		str = "Wait start";
		break;
	case FAN_STATE_RUNNING:
		str = "Running";
		break;
	case FAN_STATE_MIN_RUNTIME_WAIT:
		str = "Min Runtime Wait";
		break;
	case FAN_STATE_RELEASE:
		str = "Release";
		break;
	case FAN_STATE_ERROR:
		str = "Error";
		break;
	default:
		str = sprintf("Unknown(%d)",state);
		break;
	}

	dprintf(dlevel,"returning: %s\n", str);
	return str;
}

function fan_modestr(mode) {

	let dlevel = 5;

	dprintf(dlevel,"mode: %d\n", mode);
	switch(mode) {
	case FAN_MODE_NONE:
		str = "None";
		break;
	case FAN_MODE_COOL:
		str = "Cool";
		break;
	case FAN_MODE_HEAT:
		str = "Heat";
		break;
	default:
		str = sprintf("Unknown(%d)",mode);
		break;
	}

	dprintf(dlevel,"returning: %s\n", str);
	return str;
}

function fan_revoke(name,amount,immediate) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, amount: %s, immediate: %s\n", name, amount, immediate);

	let fan = fans[name];
	dprintf(dlevel,"fan: %s\n", fan);
	if (typeof(fan) == "undefined") {
		config.errmsg = "invalid fan";
		return 1;
	}
	dprintf(dlevel,"fan[%s]: state: %s\n", name, fan_statestr(fan.state));

	log_info("revoke request received for fan %s (immediate: %s)\n", name, immediate ? "yes" : "no");
	fan.refs = 1;

	// immediate flag bypasses min_runtime
	if (immediate) {
		fan_force_stop(name);
	} else {
		fan_stop(name);
	}
}
