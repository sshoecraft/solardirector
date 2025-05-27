
function unit_mode_event_handler(name,module,action) {

        let dlevel = 1;

        dprintf(dlevel,"name: %s, my name: %s, module: %s, action: %s\n", name, ac.name, module, action);
        if (name != ac.name) return;

        if (module == "Mode" && action == "Set") {
                dprintf(-1,"new mode: %s\n", ac_modestr(ac.mode));
		if (ac.mode != AC_MODE_COOL && ac.mode != AC_MODE_HEAT) return;
		for(let name in units) unit_mode(name,units[name],ac.mode);
        }
}

function _unit_init(name,unit) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, coolpin: %d, heatpin: %d\n", name, unit.coolpin, unit.heatpin);
	if (unit.coolpin >= 0) pinMode(unit.coolpin,OUTPUT);
	if (unit.heatpin >= 0) pinMode(unit.heatpin,OUTPUT);
	if (unit.rvpin >= 0) pinMode(unit.rvpin,OUTPUT);

	dprintf(dlevel,"pump: %s\n", unit.pump);
	if (unit.pump.length) {
		let found = false;
		for(let key in pumps) {
			if (key == unit.pump) {
				found = true;
				break;
			}
		}
		dprintf(dlevel,"found: %s\n", found);
		if (!found) {
			config.errmsg = sprintf("unit %s specifies pump %s which doesnt exist\n", name, unit.pump);
			log_error("%s\n", config.errmsg);
			return 1;		
		}
	}

	unit.mode = ac.mode;
	unit.state = UNIT_STATE_STOPPED;
	if (unit.coolpin >= 0 && digitalRead(unit.coolpin)) {
		unit_set_state(name,UNIT_STATE_RUNNING);
		unit.mode = AC_MODE_COOL;
		unit.refs = 1;
	} else if (unit.heatpin >= 0 && digitalRead(unit.heatpin)) {
		unit_set_state(name,UNIT_STATE_RUNNING);
		unit.mode = AC_MODE_HEAT;
		unit.refs = 1;
	} else {
		if (unit.rvpin >= 0 && digitalRead(unit.rvpin)) {
			if (unit.rvcool) unit.mode = AC_MODE_COOL;
			else unit.mode = AC_MODE_HEAT;
		}
		unit_set_state(name,UNIT_STATE_STOPPED);
		unit.refs = 0;
	}
	dprintf(dlevel,"unit[%s]: state: %s, mode: %s, refs: %d\n", name, unit_statestr(unit.state), ac_modestr(unit.mode), unit.refs)
	unit.error = false;
	unit.charging = false;
	return 0;
}

function unit_add(name,opts) {
	return jconfig_add(name,opts,"unit",units,unit_props,_unit_init);
}

function unit_del(name) {
	unit_force_stop(name);
	return jconfig_del(name,"unit",units,unit_props);
}

function unit_get(name) {
        return jconfig_get(name,"unit",units,unit_props);
}

function unit_set(name,opts) {
	return jconfig_set(name,opts,"unit",units,unit_props);
}

function unit_get_config(name) {
	return jconfig_get_config(name,"unit",units,unit_props);
}

function unit_set_config(config) {
	return jconfig_set_config(config,"unit",units,unit_props,load_units);
}

function load_units() {
	jconfig_load("units",unit_props,_unit_init);
//	dumpobj(units);
}

/*****************************************************************************************************/

function unit_get_state(name) {
	return common_get_state(name,units);
}

function unit_error(name,msg) {
	if (checkargs(arguments.callee,{ name: "string", msg: "string" })) return 1;
	error_set("unit",name,msg);
	units[name].enabled = false;
	return 1;
};

function unit_on(name,unit) {

	let dlevel = 1;

	unit.mode = ac.mode;
	dprintf(dlevel,"unit[%s]: mode: %s\n", name, ac_modestr(unit.mode));
	if (unit.mode == AC_MODE_COOL) {
		dprintf(dlevel,"unit[%s]: coolpin: %d\n", name, unit.coolpin);
		if (unit.coolpin >= 0 && set_pin(unit.coolpin,HIGH))
			return unit_error(name,sprintf("unable to start unit %s!",name));
		dprintf(dlevel,"unit[%s]: rvevery: %s, rvcool: %s\n", name, unit.rvevery, unit.rvcool);
		if (unit.rvevery && unit.rvcool) {
			dprintf(dlevel,"unit[%s]: rvpin: %d\n", name, unit.rvpin);
			// if rvevery is set, then rvpin must be set ... ?
			if (unit.rvpin < 0)
				return unit_error(name,sprintf("unit %s: rvevery is set but rvpin is %d",name,unit.rvpin));
			if (set_pin(unit.rvpin,HIGH))
				return unit_error(name,sprintf("unit %s: unable to set reversing valve!", name));
		}
	} else if (unit.mode == AC_MODE_HEAT) {
		dprintf(dlevel,"unit[%s]: heatpin: %d\n", name, unit.heatpin);
		if (unit.heatpin >= 0 && set_pin(unit.heatpin,HIGH))
			return unit_error(name,sprintf("unable to start unit %s!",name));
		dprintf(dlevel,"unit[%s]: rvevery: %s, rvcool: %s\n", name, unit.rvevery, unit.rvcool);
		if (unit.rvevery && !unit.rvcool) {
			dprintf(dlevel,"unit[%s]: rvpin: %d\n", name, unit.rvpin);
			// if rvevery is set, then rvpin must be set ... ?
			if (unit.rvpin < 0)
				return unit_error(name,sprintf("unit %s: rvevery is set but rvpin is %d",name,unit.rvpin));
			if (set_pin(unit.rvpin,HIGH))
				return unit_error(name,sprintf("unit %s: unable to set reversing valve!", name));
		}
	}

	dprintf(dlevel,"done!\n");
	return 0;
}

function unit_start(name) {
	return common_start(name,"unit",units);
}

function unit_off(name,unit) {

	let dlevel = 1;

	dprintf(dlevel,"unit[%s]: pump: %s\n", name, unit.pump);
	if (unit.pump.length) pump_stop(unit.pump);

//	dprintf(dlevel,"unit[%s]: coolpin: %d(%s), heatpin: %d(%s)\n", name, unit.coolpin, digitalRead(unit.coolpin) ? "on" : "off", unit.heatpin, digitalRead(unit.heatpin) ? "on" : "off");

	dprintf(dlevel,"mode: %s\n", ac_modestr(unit.mode));
	if (unit.mode == AC_MODE_COOL) {
		dprintf(dlevel,"unit[%s]: coolpin: %d\n", name, unit.coolpin);
		if (unit.coolpin >= 0 && set_pin(unit.coolpin,LOW)) {
			error_set("unit",name,sprintf("unable to start unit %s!",name));
			return 1;
		}
		dprintf(dlevel,"unit[%s]: rvevery: %s, rvcool: %s\n", name, unit.rvevery, unit.rvcool);
		if (unit.rvevery && unit.rvcool) {
			dprintf(dlevel,"unit[%s]: rvpin: %d\n", name, unit.rvpin);
			if (set_pin(unit.rvpin,LOW)) {
				error_set("unit",name,sprintf("unit %s: unable to set reversing valve!", name));
				return 1;
			}
		}
	} else if (unit.mode == AC_MODE_HEAT) {
		dprintf(dlevel,"unit[%s]: heatpin: %d\n", name, unit.heatpin);
		if (unit.heatpin >= 0 && set_pin(unit.heatpin,LOW)) {
			error_set("unit",name,sprintf("unable to start unit %s!",name));
			return 1;
		}
		dprintf(dlevel,"unit[%s]: rvevery: %s, rvcool: %s\n", name, unit.rvevery, unit.rvcool);
		if (unit.rvevery && !unit.rvcool) {
			dprintf(dlevel,"unit[%s]: rvpin: %d\n", name, unit.rvpin);
			if (set_pin(unit.rvpin,LOW)) {
				error_set("unit",name,sprintf("unit %s: unable to set reversing valve!", name));
				return 1;
			}
		}
	}

	if (unit.reserve) unit_set_state(name,UNIT_STATE_RELEASE);
	else unit_set_state(name,UNIT_STATE_STOPPED);
	unit_mode(name,unit,ac.mode);
	return 0;
}

function unit_stop(name) {
	return common_stop(name,"unit",units,unit_off,false)
}

function unit_force_stop(name) {
	units[name].charge_state = CHARGE_STATE_STOPPED;
	if (units[name].pump.length) pump_force_stop(units[name].pump);
	return common_stop(name,"unit",units,unit_off,true)
}

function unit_mode(name,unit,mode) {

	let dlevel = 1;

	dprintf(dlevel,"unit[%s]: rvevery: %s\n", name, unit.rvevery);
	if (unit.rvevery) {
		unit.mode = mode;
		return 0
	}

	dprintf(dlevel,"unit[%s]: unit.mode: %d, mode: %d\n", name, unit.mode, mode);
	if (unit.mode != mode) {
		dprintf(-1,"unit[%s]: state: %s\n", name, unit_statestr(unit.state));
		if (unit.state == UNIT_STATE_RUNNING) {
			config.errmsg = sprintf("unit_mode: unit %s must be stopped before setting mode", name);
			log_error("%s\n",config.errmsg);
			return 1;
		}
		if (unit.direct) {
			config.errmsg = sprintf("unit_mode: unit %s mode cannot be changed while in direct mode", name);
			log_error("%s\n",config.errmsg);
			return 1;
		}
		if (mode == AC_MODE_COOL) {
			if (unit.rvpin >= 0 && set_pin(unit.rvpin,unit.rvcool ? HIGH : LOW)) {
				config.errmsg = sprintf("unable to set rvpin for unit %s!\n", name);
				error_set("unit",name,config.errmsg);
				return 1;
			}
			unit.mode = AC_MODE_COOL;
		} else if (mode == AC_MODE_HEAT) {
			if (unit.rvpin >= 0 && set_pin(unit.rvpin,unit.rvcool ? LOW : HIGH)) {
				config.errmsg = sprintf("unable to set rvpin for unit %s!\n", name);
				error_set("unit",name,config.errmsg);
				return 1;
			}
			unit.mode = AC_MODE_HEAT;
		}
	}
	return 0;
}

function unit_init() {

	let dlevel = 1;

	let s = 0;
	UNIT_STATE_STOPPED = s++;
	UNIT_STATE_START_PUMP = s++;
	UNIT_STATE_WAIT_PUMP = s++;
	UNIT_STATE_RESERVE = s++;
	UNIT_STATE_START = s++;
	UNIT_STATE_RUNNING = s++;
	UNIT_STATE_RELEASE = s++;
	UNIT_STATE_ERROR = s++;

	// Declare the per-unit props here as a global
	unit_props = [
		[ "enabled", DATA_TYPE_BOOL, "true", 0 ],
		[ "pump", DATA_TYPE_STRING, null, 0 ],
		[ "pump_timeout", DATA_TYPE_INT, 60, 0 ],
		[ "coolpin", DATA_TYPE_INT, -1, 0 ],
		[ "heatpin", DATA_TYPE_INT, -1, 0 ],
		[ "rvpin", DATA_TYPE_INT, -1, 0 ],
		[ "rvcool", DATA_TYPE_BOOL, "true", 0 ],
		[ "rvevery", DATA_TYPE_BOOL, "no", 0 ],
		[ "liquid_temp_sensor", DATA_TYPE_STRING, "", 0 ],
		[ "vapor_temp_sensor", DATA_TYPE_STRING, "", 0 ],
		[ "direct", DATA_TYPE_BOOL, "false", 0 ],
		[ "reserve", DATA_TYPE_INT, 0, 0 ],
		[ "priority", DATA_TYPE_INT, 0, 0 ],
		[ "error", DATA_TYPE_BOOL, "false", CONFIG_FLAG_PRIVATE ],
		[ "charge_state", DATA_TYPE_INT, 0, CONFIG_FLAG_PRIVATE ],
		[ "charge_priority", DATA_TYPE_INT, 0, CONFIG_FLAG_PRIVATE ],
	];

	let ac_unit_props = [
		[ "units", DATA_TYPE_STRING, null, 0 ],
	];
	let ac_unit_funcs = [
		[ "add_unit", unit_add, 2 ],
		[ "del_unit", unit_del, 1 ],
		[ "set_unit", unit_set, 2 ],
		[ "get_unit", unit_get, 1 ],
		[ "get_unit_config", unit_get_config, 1 ],
		[ "set_unit_config", unit_set_config, 1 ],
		[ "start_unit", unit_start, 1 ],
		[ "stop_unit", unit_stop, 1 ],
		[ "force_stop_unit", unit_force_stop, 1 ],
	];

	config.add_props(ac,ac_unit_props);
	config.add_funcs(ac,ac_unit_funcs);
	event.handler(unit_mode_event_handler,ac.name,"Mode","Set");
	load_units();
}

function unit_set_state(name,state) {
	return common_set_state(name,"unit",units,state);
}

function unit_statestr(state) {
	let dlevel = 4;

	dprintf(dlevel,"state: %d\n", state);
	switch(state) {
	case UNIT_STATE_STOPPED:
		str = "Stopped";
		break;
	case UNIT_STATE_RESERVE:
		str = "Reserve";
		break;
	case UNIT_STATE_START_PUMP:
		str = "Start pump";
		break;
	case UNIT_STATE_WAIT_PUMP:
		str = "Wait pump";
		break;
	case UNIT_STATE_START:
		str = "Start";
		break;
	case UNIT_STATE_RUNNING:
		str = "Running";
		break;
	case UNIT_STATE_RELEASE:
		str = "Release";
		break;
	case UNIT_STATE_ERROR:
		str = "Error";
		break;
	default:
		str = sprintf("Unknown(%d)",state);
		break;
	}

	dprintf(dlevel,"returning: %s\n", str);
	return str;
}

function unit_revoke(name) {

	let dlevel = -1;

	dprintf(dlevel,"name: %s\n", name);

	let unit = units[name];
	dprintf(dlevel,"unit: %s\n", unit);
	if (typeof(unit) == "undefined") {
		config.errmsg = "invalid unit";
		return 1;
	}
	dprintf(dlevel,"unit[%s]: state: %s\n", name, unit_statestr(unit.state));
	return unit_force_stop(name);
}
