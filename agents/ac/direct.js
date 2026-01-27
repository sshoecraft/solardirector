
// Direct mode state machine
DIRECT_STATE_STOPPED = 0;
DIRECT_STATE_STOP_FAN_PUMP = 1;
DIRECT_STATE_WAIT_FAN_PUMP = 2;
DIRECT_STATE_STOP_UNIT = 3;
DIRECT_STATE_WAIT_UNIT_STOP = 4;
DIRECT_STATE_ACTIVATE_VALVE = 5;
DIRECT_STATE_START_UNIT = 6;
DIRECT_STATE_WAIT_UNIT = 7;
DIRECT_STATE_RUNNING = 8;
DIRECT_STATE_ERROR = 9;

function direct_statestr(state) {
	let strings = [
		"Stopped",
		"Stop fan pump",
		"Wait fan pump",
		"Stop unit",
		"Wait unit stop",
		"Activate valve",
		"Start unit",
		"Wait unit",
		"Running",
		"Error"
	];
	return (state >= 0 && state < strings.length) ? strings[state] : "Unknown";
}

function direct_set_state(name, newstate) {
	let dg = directs[name];
	if (!dg) return 1;
	if (newstate != dg.state) {
		log_info("directs[%s]: state: %s -> %s\n", name, direct_statestr(dg.state), direct_statestr(newstate));
		dg.state = newstate;
	}
	return 0;
}

function load_directs() {
	jconfig_load("directs", direct_props, _direct_init);
}

function _direct_init(name, dg) {

	let dlevel = 1;

	// Validate fan exists
	let fan = fans[dg.fan];
	if (typeof(fan) == "undefined") {
		log_error("direct group %s: fan %s does not exist\n", name, dg.fan);
		dg.enabled = false;
		return 1;
	}

	// Validate unit exists
	let unit = units[dg.unit];
	if (typeof(unit) == "undefined") {
		log_error("direct group %s: unit %s does not exist\n", name, dg.unit);
		dg.enabled = false;
		return 1;
	}

	// Set direct_group on fan and unit (runtime only, not saved)
	fan.direct_group = name;
	unit.direct_group = name;

	// Derive pumps
	dg.fan_pump = fan.pump;
	dg.ac_pump = unit.pump;

	// Validate pumps exist
	if (!dg.fan_pump || typeof(pumps[dg.fan_pump]) == "undefined") {
		log_error("direct group %s: fan %s has no valid pump\n", name, dg.fan);
		dg.enabled = false;
		return 1;
	}

	if (!dg.ac_pump || typeof(pumps[dg.ac_pump]) == "undefined") {
		log_error("direct group %s: unit %s has no valid pump\n", name, dg.unit);
		dg.enabled = false;
		return 1;
	}

	// Initialize pin
	if (dg.pin >= 0) pinMode(dg.pin, OUTPUT);

	// Initialize state
	dg.enabled = true;
	dg.state = DIRECT_STATE_STOPPED;
	dg.active = false;
	dg.pending_fan = "";
	dg.active_fan = "";
	dg.target_mode = FAN_MODE_NONE;
	dg.mode = FAN_MODE_NONE;

	dprintf(dlevel, "direct group %s: pin=%d fan=%s unit=%s fan_pump=%s ac_pump=%s\n",
		name, dg.pin, dg.fan, dg.unit, dg.fan_pump, dg.ac_pump);

	return 0;
}

function direct_init() {

	let dlevel = 1;

	direct_props = [
		[ "enabled", DATA_TYPE_BOOL, "true", 0 ],
		[ "pin", DATA_TYPE_INT, -1, 0 ],
		[ "fan", DATA_TYPE_STRING, "", 0 ],
		[ "unit", DATA_TYPE_STRING, "", 0 ],
	];

	let funcs = [
		[ "direct_on", direct_on, 1 ],
		[ "direct_off", direct_off, 1 ],
		[ "direct_enable", direct_enable, 1 ],
		[ "direct_disable", direct_disable, 1 ],
		[ "direct_get_pump", direct_get_pump, 1 ],
		[ "direct_is_pending", direct_is_pending, 2 ],
		[ "direct_is_active", direct_is_active, 2 ],
		[ "direct_is_error", direct_is_error, 1 ],
		[ "dg_valve_on", dg_valve_on, 1 ],
		[ "dg_valve_off", dg_valve_off, 1 ],
	];

	config.add_funcs(ac, funcs, ac.driver_name);
	load_directs();
}

function direct_get_pump(group_name) {
	let dg = directs[group_name];
	if (!dg) return null;
	return dg.ac_pump;
}

// Check if direct mode is pending for this fan
function direct_is_pending(group_name, fan_name) {
	let dg = directs[group_name];
	if (!dg) return false;
	return (dg.pending_fan == fan_name && !dg.active);
}

// Check if direct mode is active for this fan
function direct_is_active(group_name, fan_name) {
	let dg = directs[group_name];
	if (!dg) return false;
	return (dg.active && dg.active_fan == fan_name);
}

// Check if direct mode is in error state
function direct_is_error(group_name) {
	let dg = directs[group_name];
	if (!dg) return false;
	return (dg.state == DIRECT_STATE_ERROR);
}

// Enable direct group (allows it to be turned on)
function direct_enable(group_name) {
	return common_enable(group_name, "direct", directs, direct_props);
}

// Disable direct group (prevents it from being turned on)
function direct_disable(group_name) {
	let dg = directs[group_name];
	// If active, turn it off first
	if (dg && (dg.active || dg.pending_fan.length)) {
		direct_off(group_name);
	}
	return common_disable(group_name, "direct", directs, direct_props);
}

// Valve control - centralizes valve logic and pump.direct flag
function dg_valve_on(group_name) {
	let dg = directs[group_name];
	if (!dg) {
		config.errmsg = sprintf("direct group %s not found", group_name);
		return 1;
	}
	if (dg.pin < 0) {
		config.errmsg = sprintf("direct group %s has no valve pin configured", group_name);
		return 1;
	}
	log_info("valve ON: group=%s pin=%d\n", group_name, dg.pin);
	if (set_pin(dg.pin, HIGH)) {
		config.errmsg = sprintf("unable to set valve pin %d HIGH", dg.pin);
		return 1;
	}
	// Mark fan pump as in direct mode - L-valve blocks its output when activated
	let fan_pump = pumps[dg.fan_pump];
	if (fan_pump) fan_pump.direct = true;
	return 0;
}

function dg_valve_off(group_name) {
	let dg = directs[group_name];
	if (!dg) {
		config.errmsg = sprintf("direct group %s not found", group_name);
		return 1;
	}
	if (dg.pin < 0) {
		config.errmsg = sprintf("direct group %s has no valve pin configured", group_name);
		return 1;
	}
	log_info("valve OFF: group=%s pin=%d\n", group_name, dg.pin);
	if (set_pin(dg.pin, LOW)) {
		config.errmsg = sprintf("unable to set valve pin %d LOW", dg.pin);
		return 1;
	}
	// Clear fan pump direct mode - L-valve no longer blocking output
	let fan_pump = pumps[dg.fan_pump];
	if (fan_pump) fan_pump.direct = false;
	return 0;
}

// Turn on direct mode - starts the state machine
function direct_on(group_name, mode_str) {

	let dlevel = 1;

	let dg = directs[group_name];
	if (!dg || !dg.enabled) {
		error_set("direct", group_name, sprintf("direct group %s not found or disabled", group_name));
		return 1;
	}

	// Fan name comes from direct group config
	let fan_name = dg.fan;

	// Parse mode string, default to ac.mode
	let fan_mode;
	if (typeof(mode_str) == "undefined" || !mode_str || mode_str == "") {
		fan_mode = (ac.mode == AC_MODE_COOL) ? FAN_MODE_COOL : FAN_MODE_HEAT;
	} else if (mode_str.toLowerCase() == "heat") {
		fan_mode = FAN_MODE_HEAT;
	} else if (mode_str.toLowerCase() == "cool") {
		fan_mode = FAN_MODE_COOL;
	} else {
		error_set("direct", group_name, sprintf("invalid mode: %s (use 'heat' or 'cool')", mode_str));
		return 1;
	}

	dprintf(dlevel, "direct_enable: group=%s fan_mode=%s fan=%s\n", group_name, fan_modestr(fan_mode), fan_name);

	// Check if already active for this fan
	if (dg.active && dg.active_fan == fan_name) {
		dprintf(dlevel, "direct group %s already active for fan %s\n", group_name, fan_name);
		return 0;
	}

	// Check if already pending for this fan
	if (dg.pending_fan == fan_name && dg.state != DIRECT_STATE_STOPPED && dg.state != DIRECT_STATE_ERROR) {
		dprintf(dlevel, "direct group %s already pending for fan %s, state=%s\n", group_name, fan_name, direct_statestr(dg.state));
		return 0;
	}

	// Check if in use by another fan
	if (dg.active || (dg.pending_fan.length && dg.pending_fan != fan_name)) {
		error_set("direct", fan_name, sprintf("direct group %s already in use by fan %s", group_name, dg.active_fan || dg.pending_fan));
		return 1;
	}

	// Mark unit as in direct mode (prevents charge.js control)
	let unit = units[dg.unit];
	unit.direct = true;

	// Store request
	dg.pending_fan = fan_name;
	dg.target_mode = fan_mode;

	// Check if fan pump is already stopped - if so, skip to ACTIVATE_VALVE
	let fan_pump_state = pump_get_state(dg.fan_pump);
	if (fan_pump_state == PUMP_STATE_STOPPED) {
		dprintf(dlevel, "fan pump %s already stopped, skipping to ACTIVATE_VALVE\n", dg.fan_pump);
		direct_set_state(group_name, DIRECT_STATE_ACTIVATE_VALVE);
	} else {
		dprintf(dlevel, "fan pump %s state=%s, starting STOP_FAN_PUMP\n", dg.fan_pump, pump_statestr(fan_pump_state));
		direct_set_state(group_name, DIRECT_STATE_STOP_FAN_PUMP);
	}

	log_info("direct mode requested: group=%s fan=%s mode=%s\n", group_name, fan_name, fan_modestr(fan_mode));

	return 0;
}

// Process state machine for a single direct group
function _direct_process(name, dg) {

	let dlevel = 1;

	if (dg.state == DIRECT_STATE_STOPPED || dg.state == DIRECT_STATE_RUNNING || dg.state == DIRECT_STATE_ERROR) {
		return;
	}

	dprintf(dlevel, "direct[%s]: state=%s pending_fan=%s\n", name, direct_statestr(dg.state), dg.pending_fan);

	let unit = units[dg.unit];
	let ac_pump = pumps[dg.ac_pump];
	let fan_pump = pumps[dg.fan_pump];

	switch(dg.state) {
	case DIRECT_STATE_STOP_FAN_PUMP:
		dprintf(dlevel, "stopping fan pump %s\n", dg.fan_pump);
		pump_stop(dg.fan_pump);
		direct_set_state(name, DIRECT_STATE_WAIT_FAN_PUMP);
		break;

	case DIRECT_STATE_WAIT_FAN_PUMP:
		let fan_pump_state = pump_get_state(dg.fan_pump);
		dprintf(dlevel, "waiting for fan pump %s, state=%s\n", dg.fan_pump, pump_statestr(fan_pump_state));
		if (fan_pump_state == PUMP_STATE_STOPPED) {
			// Check if unit needs to be stopped (running in wrong mode)
			let target_ac_mode = (dg.target_mode == FAN_MODE_COOL) ? AC_MODE_COOL : AC_MODE_HEAT;
			if (unit.state == UNIT_STATE_RUNNING && unit.mode != target_ac_mode) {
				dprintf(dlevel, "unit %s running in %s mode, need %s - stopping\n",
					dg.unit, ac_modestr(unit.mode), ac_modestr(target_ac_mode));
				direct_set_state(name, DIRECT_STATE_STOP_UNIT);
			} else {
				direct_set_state(name, DIRECT_STATE_ACTIVATE_VALVE);
			}
		}
		// else stay in WAIT_FAN_PUMP
		break;

	case DIRECT_STATE_STOP_UNIT:
		dprintf(dlevel, "stopping unit %s for mode change\n", dg.unit);
		unit_stop(dg.unit);
		direct_set_state(name, DIRECT_STATE_WAIT_UNIT_STOP);
		break;

	case DIRECT_STATE_WAIT_UNIT_STOP:
		dprintf(dlevel, "waiting for unit %s to stop, state=%s\n", dg.unit, unit_statestr(unit.state));
		if (unit.state == UNIT_STATE_STOPPED) {
			direct_set_state(name, DIRECT_STATE_ACTIVATE_VALVE);
		} else if (unit.state == UNIT_STATE_ERROR) {
			error_set("direct", dg.pending_fan, sprintf("unit %s entered error state while stopping", dg.unit));
			direct_set_state(name, DIRECT_STATE_ERROR);
		}
		// else stay in WAIT_UNIT_STOP
		break;

	case DIRECT_STATE_ACTIVATE_VALVE:
		dprintf(dlevel, "activating 3-way valve\n");
		if (dg_valve_on(name)) {
			error_set("direct", dg.pending_fan, sprintf("unable to activate 3-way valve: %s", config.errmsg));
			direct_set_state(name, DIRECT_STATE_ERROR);
			break;
		}

		// Set unit mode to match fan's requested mode
		let ac_mode = (dg.target_mode == FAN_MODE_COOL) ? AC_MODE_COOL : AC_MODE_HEAT;
		if (unit_mode(dg.unit, unit, ac_mode)) {
			error_set("direct", dg.pending_fan, sprintf("unable to set unit %s to mode %s", dg.unit, ac_modestr(ac_mode)));
			dg_valve_off(name);
			direct_set_state(name, DIRECT_STATE_ERROR);
			break;
		}

		// Set unit direct_priority to match fan's priority (unit runs on behalf of fan)
		let fan = fans[dg.fan];
		unit.direct_priority = fan.priority;
		dprintf(dlevel, "unit %s direct_priority set to %d (fan %s priority)\n", dg.unit, unit.direct_priority, dg.fan);

		// Check if unit is already running
		if (unit.state == UNIT_STATE_RUNNING) {
			dprintf(dlevel, "unit %s already running, skipping to RUNNING\n", dg.unit);
			dg.active = true;
			dg.active_fan = dg.pending_fan;
			dg.mode = dg.target_mode;
			direct_set_state(name, DIRECT_STATE_RUNNING);
			// Start the fan
			fan_start(dg.pending_fan);
			log_info("direct mode enabled: group=%s fan=%s unit=%s mode=%s\n",
				name, dg.active_fan, dg.unit, fan_modestr(dg.mode));
		} else {
			direct_set_state(name, DIRECT_STATE_START_UNIT);
		}
		break;

	case DIRECT_STATE_START_UNIT:
		dprintf(dlevel, "starting unit %s\n", dg.unit);
		if (unit_start(dg.unit)) {
			error_set("direct", dg.pending_fan, sprintf("unable to start unit %s", dg.unit));
			dg_valve_off(name);
			direct_set_state(name, DIRECT_STATE_ERROR);
			break;
		}
		direct_set_state(name, DIRECT_STATE_WAIT_UNIT);
		break;

	case DIRECT_STATE_WAIT_UNIT:
		dprintf(dlevel, "waiting for unit %s, state=%s\n", dg.unit, unit_statestr(unit.state));
		if (unit.state == UNIT_STATE_RUNNING) {
			// Direct mode fully active
			dg.active = true;
			dg.active_fan = dg.pending_fan;
			dg.mode = dg.target_mode;
			direct_set_state(name, DIRECT_STATE_RUNNING);
			// Start the fan
			fan_start(dg.pending_fan);
			log_info("direct mode enabled: group=%s fan=%s unit=%s mode=%s\n",
				name, dg.active_fan, dg.unit, fan_modestr(dg.mode));
		} else if (unit.state == UNIT_STATE_ERROR) {
			error_set("direct", dg.pending_fan, sprintf("unit %s entered error state", dg.unit));
			dg_valve_off(name);
			direct_set_state(name, DIRECT_STATE_ERROR);
		}
		// else stay in WAIT_UNIT
		break;
	}
}

// Main loop function - process all direct groups
function direct_main() {

	let dlevel = 4;

	for(let name in directs) {
		let dg = directs[name];
		if (!dg.enabled) continue;
		_direct_process(name, dg);
	}
}

// Turn off direct mode - stops the state machine
function direct_off(group_name) {

	let dlevel = 1;

	let dg = directs[group_name];
	if (!dg) {
		dprintf(dlevel, "direct group %s not found\n", group_name);
		return 0;
	}

	// Fan name comes from direct group config
	let fan_name = dg.fan;

	dprintf(dlevel, "direct_off: group=%s fan=%s\n", group_name, fan_name);

	// Check if direct group is active or pending
	if (dg.active_fan != fan_name && dg.pending_fan != fan_name) {
		// Not active or pending - nothing to do
		dprintf(dlevel, "direct group %s not active or pending\n", group_name);
		return 0;
	}

	let unit = units[dg.unit];
	let fan_pump = pumps[dg.fan_pump];

	// Deactivate 3-way valve (routes flow back to storage, clears pump.direct)
	if (dg_valve_off(group_name)) {
		error_set("direct", fan_name, sprintf("unable to deactivate 3-way valve: %s", config.errmsg));
		// Continue anyway - best effort cleanup
	}

	// Stop the fan BEFORE re-enabling fan pump
	// Otherwise fan will check fan pump state while still RUNNING and error
	fan_stop(fan_name);
	// Force fan state change immediately - fan_stop just sets mode,
	// state transition happens in run loop which may be too late
	let fan = fans[fan_name];
	if (fan && fan.state != FAN_STATE_STOPPED && fan.state != FAN_STATE_RELEASE) {
		// Go to RELEASE if power reserved, otherwise STOPPED
		if (fan.reserve) fan_set_state(fan_name, FAN_STATE_RELEASE);
		else fan_set_state(fan_name, FAN_STATE_STOPPED);
	}

	// dg_valve_off already re-enabled fan pump (pump.direct = false, pump.enabled = true)

	// Clear direct_priority
	unit.direct_priority = 0;

	// Only hand off to charge.js if we were fully active (unit was started by us)
	if (dg.active) {
		// Check if unit mode matches storage mode
		let storage_mode = ac.mode;
		let direct_mode = (dg.mode == FAN_MODE_COOL) ? AC_MODE_COOL : AC_MODE_HEAT;

		if (direct_mode != storage_mode) {
			// Mode mismatch - stop unit and set back to storage mode
			dprintf(dlevel, "mode mismatch: direct=%s storage=%s - stopping unit\n",
				ac_modestr(direct_mode), ac_modestr(storage_mode));
			unit_stop(dg.unit);

			// Release power reservation
			if (unit.reserve) {
				pa_client_release(ac, "unit", dg.unit, unit.reserve);
				unit.reserve = 0;
			}

			// Set mode back to storage mode (unit must be stopped first)
			unit_mode(dg.unit, unit, storage_mode);
			dprintf(dlevel, "unit %s mode set to %s\n", dg.unit, ac_modestr(storage_mode));

			// Hand off to charge.js in stopped state - it will restart
			unit.charging = true;
			unit.charge_state = CHARGE_STATE_STOPPED;
			unit.charge_priority = charge_get_pri();
			dprintf(dlevel, "handing off unit %s to charge.js (stopped): priority=%d\n", dg.unit, unit.charge_priority);
		} else {
			// Mode matches - hand off running unit to charge.js
			unit.charging = true;
			unit.charge_state = CHARGE_STATE_RUNNING;
			unit.charge_priority = charge_get_pri();
			dprintf(dlevel, "handing off unit %s to charge.js (running): priority=%d\n", dg.unit, unit.charge_priority);

			// Repri to new charge priority
			if (unit.reserve) {
				if (pa_client_repri(ac, "unit", dg.unit, unit.reserve, unit.charge_priority)) {
					// Repri failed - stop unit to be safe
					log_warning("direct_disable: repri failed for unit %s, stopping unit\n", dg.unit);
					unit_stop(dg.unit);
					unit.charging = false;
					unit.charge_state = CHARGE_STATE_STOPPED;
				}
			}
		}
	}

	// Clear unit direct mode flag (allows charge.js to resume control)
	unit.direct = false;

	// Clear state
	direct_set_state(group_name, DIRECT_STATE_STOPPED);
	dg.active = false;
	dg.pending_fan = "";
	dg.active_fan = "";
	dg.target_mode = FAN_MODE_NONE;
	dg.mode = FAN_MODE_NONE;

	log_info("direct mode disabled: group=%s fan=%s\n", group_name, fan_name);

	return 0;
}
