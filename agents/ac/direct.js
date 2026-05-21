
// Direct mode state machine
//
// Cold-start ordering (the priming bug fix):
//   STOPPED -> [STOP_FAN_PUMP -> WAIT_FAN_PUMP ->] PREPARE_UNIT ->
//   START_UNIT -> WAIT_UNIT -> CONFIRM_FLOW -> ACTIVATE_VALVE ->
//   RUNNING_OPEN -> ACTIVATE_PIN2 -> START_PRIMER -> WAIT_PRIMER ->
//   WAIT_WATER_TEMP -> RUNNING
//
// The stage-2 (pin2) sticky-valve reset is NOT done here — it lives in the
// unit start sequence (run.js: UNIT_STATE_RESET_VALVE) so it runs on every
// AC start, direct mode or charge.js storage run alike. valve_reset_time
// below configures the pin2 hold duration; dg_valve2_on/off are reused by
// the unit state machine.
//
// The unit (compressor + ac_pump) reaches RUNNING with normal storage-loop
// flow BEFORE pin1 is activated. CONFIRM_FLOW then verifies thermal delta
// on the ac_pump (proves water is moving through the heat exchanger) before
// the valve redirects flow into the longer AC->AH loop. This prevents the
// "valve-first" failure where the AC pump couldn't prime the long loop,
// over-cooled the HX, and tripped vapor-temp freeze protection.

DIRECT_STATE_STOPPED = 0;
DIRECT_STATE_STOP_FAN_PUMP = 1;
DIRECT_STATE_WAIT_FAN_PUMP = 2;
DIRECT_STATE_STOP_UNIT = 3;
DIRECT_STATE_WAIT_UNIT_STOP = 4;
DIRECT_STATE_PREPARE_UNIT = 5;		// Set unit mode + in_direct flags, dispatch to START_UNIT/WAIT_UNIT/CONFIRM_FLOW
DIRECT_STATE_START_UNIT = 6;
DIRECT_STATE_WAIT_UNIT = 7;
DIRECT_STATE_WAIT_WATER_TEMP = 8;	// Wait for water to reach temp before starting fan
DIRECT_STATE_RUNNING_OPEN = 9;		// Stage 1: pin1 on, monitoring temp on fan_pump
DIRECT_STATE_WAIT_TEMP = 10;		// Waiting for temp change on fan pump output
DIRECT_STATE_ACTIVATE_PIN2 = 11;	// Activate pin2 to close the loop
DIRECT_STATE_START_PRIMER = 12;		// Start primer pump
DIRECT_STATE_WAIT_PRIMER = 13;		// Wait for primer to run
DIRECT_STATE_RUNNING = 14;		// Fully closed loop
DIRECT_STATE_ERROR = 15;
DIRECT_STATE_CONFIRM_FLOW = 16;		// Verify ac_pump temp_in/out delta before activating valve
DIRECT_STATE_ACTIVATE_VALVE = 17;	// Open pin1 (only reached after unit RUNNING + flow confirmed)

function direct_statestr(state) {
	let strings = [
		"Stopped",
		"Stop fan pump",
		"Wait fan pump",
		"Stop unit",
		"Wait unit stop",
		"Prepare unit",
		"Start unit",
		"Wait unit",
		"Wait water temp",
		"Running open",
		"Wait temp",
		"Activate pin2",
		"Start primer",
		"Wait primer",
		"Running",
		"Error",
		"Confirm flow",
		"Activate valve"
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
	unit.in_direct_group = name;

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

	// Initialize pins
	if (dg.pin1 >= 0) pinMode(dg.pin1, OUTPUT);
	if (dg.pin2 >= 0) pinMode(dg.pin2, OUTPUT);

	// Derive primer from AC pump config
	let ac_pump = pumps[dg.ac_pump];
	dg.primer = ac_pump.primer || "";

	// Initialize state
	dg.enabled = true;
	dg.state = DIRECT_STATE_STOPPED;
	dg.active = false;
	dg.pending_fan = "";
	dg.active_fan = "";
	dg.target_mode = FAN_MODE_NONE;
	dg.mode = FAN_MODE_NONE;
	dg.initial_temp = INVALID_TEMP;
	dg.saved_water_temp = INVALID_TEMP;
	dg.primer_start_time = 0;
	dg.unit_start_time = 0;
	dg.water_wait_time = 0;
	dg.confirm_flow_start = 0;

	dprintf(dlevel, "direct group %s: pin1=%d pin2=%d fan=%s unit=%s fan_pump=%s ac_pump=%s primer=%s\n",
		name, dg.pin1, dg.pin2, dg.fan, dg.unit, dg.fan_pump, dg.ac_pump, dg.primer);

	return 0;
}

function direct_set(name, opts) {
	let r = jconfig_set(name, opts, "direct", directs, direct_props);
	if (!r) {
		let dg = directs[name];
		if (dg.pin1 >= 0) pinMode(dg.pin1, OUTPUT);
		if (dg.pin2 >= 0) pinMode(dg.pin2, OUTPUT);
	}
	return r;
}

function direct_get_config(name) {
	return jconfig_get_config(name, "direct", directs, direct_props);
}

function direct_set_config(config) {
	return jconfig_set_config(config, "direct", directs, direct_props, load_directs);
}

function direct_init() {

	let dlevel = 1;

	direct_props = [
		[ "enabled", DATA_TYPE_BOOL, "true", 0 ],
		[ "pin1", DATA_TYPE_INT, -1, 0 ],
		[ "pin2", DATA_TYPE_INT, -1, 0 ],
		[ "fan", DATA_TYPE_STRING, "", 0 ],
		[ "unit", DATA_TYPE_STRING, "", 0 ],
		[ "primer_time", DATA_TYPE_INT, 30, 0 ],
		[ "temp_threshold", DATA_TYPE_DOUBLE, 2.0, 0 ],
		[ "unit_timeout", DATA_TYPE_INT, 60, 0 ],
		[ "water_temp_timeout", DATA_TYPE_INT, 600, 0 ],
		[ "flow_confirm_threshold", DATA_TYPE_DOUBLE, 2.0, 0 ],
		[ "flow_confirm_timeout", DATA_TYPE_INT, 60, 0 ],
		// pin2 hold time for the unit-start sticky-valve reset (run.js)
		[ "valve_reset_time", DATA_TYPE_INT, 10, 0 ],
	];

	let funcs = [
		[ "direct_set", direct_set, 2 ],
		[ "direct_get_config", direct_get_config, 1 ],
		[ "direct_set_config", direct_set_config, 1 ],
		[ "direct_on", direct_on, 1 ],
		[ "direct_off", direct_off, 1 ],
		[ "direct_enable", direct_enable, 1 ],
		[ "direct_disable", direct_disable, 1 ],
		[ "direct_get_pump", direct_get_pump, 1 ],
		[ "direct_is_pending", direct_is_pending, 2 ],
		[ "direct_is_active", direct_is_active, 2 ],
		[ "direct_is_error", direct_is_error, 1 ],
		[ "dg_valve1_on", dg_valve1_on, 1 ],
		[ "dg_valve1_off", dg_valve1_off, 1 ],
		[ "dg_valve2_on", dg_valve2_on, 1 ],
		[ "dg_valve2_off", dg_valve2_off, 1 ],
		[ "dg_valves_off", dg_valves_off, 1 ],
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
// pin1 = stage 1 valves (AC output → AH input)
function dg_valve1_on(group_name) {
	let dg = directs[group_name];
	if (!dg) {
		config.errmsg = sprintf("direct group %s not found", group_name);
		return 1;
	}
	if (dg.pin1 < 0) {
		config.errmsg = sprintf("direct group %s has no valve pin1 configured", group_name);
		return 1;
	}
	log_info("valve1 ON: group=%s pin1=%d\n", group_name, dg.pin1);
	if (set_pin(dg.pin1, HIGH)) {
		config.errmsg = sprintf("unable to set valve pin1 %d HIGH", dg.pin1);
		return 1;
	}
	// Mark fan pump as in direct mode - L-valve blocks its output when activated
	let fan_pump = pumps[dg.fan_pump];
	if (fan_pump) fan_pump.direct = true;
	return 0;
}

function dg_valve1_off(group_name) {
	let dg = directs[group_name];
	if (!dg) {
		config.errmsg = sprintf("direct group %s not found", group_name);
		return 1;
	}
	if (dg.pin1 >= 0) {
		log_info("valve1 OFF: group=%s pin1=%d\n", group_name, dg.pin1);
		if (set_pin(dg.pin1, LOW)) {
			config.errmsg = sprintf("unable to set valve pin1 %d LOW", dg.pin1);
			return 1;
		}
	}
	// Clear fan pump direct mode - L-valve no longer blocking output
	let fan_pump = pumps[dg.fan_pump];
	if (fan_pump) fan_pump.direct = false;
	return 0;
}

// pin2 = stage 2 valves (AH output → AC input, closes the loop)
function dg_valve2_on(group_name) {
	let dg = directs[group_name];
	if (!dg) {
		config.errmsg = sprintf("direct group %s not found", group_name);
		return 1;
	}
	if (dg.pin2 < 0) {
		// No pin2 configured - skip (stage 2 not available)
		dprintf(1, "direct group %s has no pin2 configured, skipping stage 2\n", group_name);
		return 0;
	}
	log_info("valve2 ON: group=%s pin2=%d\n", group_name, dg.pin2);
	if (set_pin(dg.pin2, HIGH)) {
		config.errmsg = sprintf("unable to set valve pin2 %d HIGH", dg.pin2);
		return 1;
	}
	return 0;
}

function dg_valve2_off(group_name) {
	let dg = directs[group_name];
	if (!dg) {
		config.errmsg = sprintf("direct group %s not found", group_name);
		return 1;
	}
	if (dg.pin2 >= 0) {
		log_info("valve2 OFF: group=%s pin2=%d\n", group_name, dg.pin2);
		if (set_pin(dg.pin2, LOW)) {
			config.errmsg = sprintf("unable to set valve pin2 %d LOW", dg.pin2);
			return 1;
		}
	}
	return 0;
}

// Turn off all valves (both pin1 and pin2)
function dg_valves_off(group_name) {
	if (dg_valve1_off(group_name)) return 1;
	if (dg_valve2_off(group_name)) return 1;
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

	// Block charge.js during transition (cleared on success or error)
	let unit = units[dg.unit];

	// Clear unit error state if needed - previous direct mode cycle may have
	// left the unit disabled (e.g. flow check false positive). Direct mode
	// needs the unit available to start.
	if (unit.error) {
		log_info("direct[%s]: clearing unit %s error state for retry\n", group_name, dg.unit);
		unit.error = false;
		unit.enabled = true;
	}

	unit.in_direct_pending = true;

	// Save water temp NOW before anything changes - once valves activate,
	// pump readings get contaminated with direct loop water
	dg.saved_water_temp = ac.water_temp;
	log_info("direct[%s]: saved water_temp: %.1f\n", group_name, dg.saved_water_temp);

	// Store request
	dg.pending_fan = fan_name;
	dg.target_mode = fan_mode;

	// Check if fan pump is already stopped - if so, skip to PREPARE_UNIT
	let fan_pump_state = pump_get_state(dg.fan_pump);
	if (fan_pump_state == PUMP_STATE_STOPPED) {
		dprintf(dlevel, "fan pump %s already stopped, skipping to PREPARE_UNIT\n", dg.fan_pump);
		direct_set_state(group_name, DIRECT_STATE_PREPARE_UNIT);
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

	if (dg.state == DIRECT_STATE_STOPPED || dg.state == DIRECT_STATE_ERROR) {
		return;
	}

	// Monitor unit/pump while in RUNNING or RUNNING_OPEN states
	if (dg.state == DIRECT_STATE_RUNNING || dg.state == DIRECT_STATE_RUNNING_OPEN) {
		let unit = units[dg.unit];
		let ac_pump = pumps[dg.ac_pump];
		// If unit or pump stopped, turn off direct mode
		if (unit.state != UNIT_STATE_RUNNING || pump_get_state(dg.ac_pump) != PUMP_STATE_RUNNING) {
			dprintf(dlevel, "direct[%s]: unit/pump stopped (unit=%s, pump=%s), turning off\n",
				name, unit_statestr(unit.state), pump_statestr(pump_get_state(dg.ac_pump)));
			direct_off(name);
			return;
		}
		// If in RUNNING_OPEN, monitor for temp change to activate stage 2
		if (dg.state == DIRECT_STATE_RUNNING_OPEN) {
			let fan_pump = pumps[dg.fan_pump];
			let fan_pump_temp = get_sensor(fan_pump.temp_out_sensor, false);
			dprintf(dlevel, "direct[%s]: RUNNING_OPEN - fan_pump temp_out: %.1f, initial: %.1f, threshold: %.1f\n",
				name, fan_pump_temp, dg.initial_temp, dg.temp_threshold);
			if (is_valid_temp(fan_pump_temp) && is_valid_temp(dg.initial_temp)) {
				let temp_diff = Math.abs(fan_pump_temp - dg.initial_temp);
				if (temp_diff >= dg.temp_threshold) {
					log_info("direct[%s]: temp change detected (%.1f -> %.1f, diff=%.1f), activating stage 2\n",
						name, dg.initial_temp, fan_pump_temp, temp_diff);
					direct_set_state(name, DIRECT_STATE_ACTIVATE_PIN2);
				}
			}
		}
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
				direct_set_state(name, DIRECT_STATE_PREPARE_UNIT);
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
			direct_set_state(name, DIRECT_STATE_PREPARE_UNIT);
		} else if (unit.state == UNIT_STATE_ERROR) {
			error_set("direct", dg.pending_fan, sprintf("unit %s entered error state while stopping", dg.unit));
			unit.in_direct_pending = false;
			direct_set_state(name, DIRECT_STATE_ERROR);
		}
		// else stay in WAIT_UNIT_STOP
		break;

	case DIRECT_STATE_PREPARE_UNIT:
		// Set unit mode + in_direct flags, then dispatch based on current unit state.
		// Pin1 (valve) is NOT activated here - it gets opened later in ACTIVATE_VALVE,
		// only after the unit reaches RUNNING and flow is confirmed. This prevents
		// the AC pump from trying to prime into the long AC->AH loop with no
		// established flow column, which under-flowed the HX and tripped vapor-temp
		// freeze protection.
		let target_ac_mode_check = (dg.target_mode == FAN_MODE_COOL) ? AC_MODE_COOL : AC_MODE_HEAT;
		if (unit.state != UNIT_STATE_STOPPED) {
			if (unit.state == UNIT_STATE_RUNNING && unit.mode == target_ac_mode_check) {
				// Unit already running in correct mode - proceed
			} else if (unit.state == UNIT_STATE_RUNNING) {
				// Running in wrong mode - must properly stop (compressor on, thermal safety)
				dprintf(dlevel, "unit %s running in %s mode, need %s - stopping first\n",
					dg.unit, ac_modestr(unit.mode), ac_modestr(target_ac_mode_check));
				direct_set_state(name, DIRECT_STATE_STOP_UNIT);
				break;
			} else {
				// Pre-running state (Reserve, Start pump, etc) - hijack the in-progress
				// startup. Do NOT release PA reservation - PA has a 180s settle time
				// between grants and re-requesting would always exceed unit_timeout.
				dprintf(dlevel, "unit %s in state %s - hijacking startup (keeping PA reservation)\n", dg.unit, unit_statestr(unit.state));
				unit.charging = false;
				unit.charge_state = CHARGE_STATE_STOPPED;
			}
		}

		// Set unit mode (RV pin) BEFORE starting the unit so it comes up in correct mode
		let ac_mode = (dg.target_mode == FAN_MODE_COOL) ? AC_MODE_COOL : AC_MODE_HEAT;
		if (unit_mode(dg.unit, unit, ac_mode)) {
			error_set("direct", dg.pending_fan, sprintf("unable to set unit %s to mode %s", dg.unit, ac_modestr(ac_mode)));
			unit.in_direct_pending = false;
			direct_set_state(name, DIRECT_STATE_ERROR);
			break;
		}

		// Mark unit as in direct mode (prevents charge.js control)
		unit.in_direct = true;
		unit.in_direct_pending = false;

		// Set unit direct_priority to match fan's priority (unit runs on behalf of fan)
		let fan = fans[dg.pending_fan];
		unit.in_direct_priority = fan.priority;
		dprintf(dlevel, "unit %s direct_priority set to %d (fan %s priority)\n", dg.unit, unit.in_direct_priority, dg.pending_fan);

		// Dispatch based on unit state
		if (unit.state == UNIT_STATE_RUNNING) {
			// Already running in correct mode (charge.js had it running, or we hijacked).
			// Skip START_UNIT/WAIT_UNIT and go straight to flow confirmation.
			dprintf(dlevel, "unit %s already running, skipping to CONFIRM_FLOW\n", dg.unit);
			dg.confirm_flow_start = time();
			direct_set_state(name, DIRECT_STATE_CONFIRM_FLOW);
		} else if (unit.state == UNIT_STATE_STOPPED) {
			direct_set_state(name, DIRECT_STATE_START_UNIT);
		} else {
			// Unit already starting (hijacked from charge.js) - skip to WAIT_UNIT
			dprintf(dlevel, "unit %s already starting (state %s), waiting for RUNNING\n", dg.unit, unit_statestr(unit.state));
			dg.unit_start_time = time();
			direct_set_state(name, DIRECT_STATE_WAIT_UNIT);
		}
		break;

	case DIRECT_STATE_START_UNIT:
		dprintf(dlevel, "starting unit %s\n", dg.unit);
		if (unit_start(dg.unit)) {
			error_set("direct", dg.pending_fan, sprintf("unable to start unit %s", dg.unit));
			dg_valves_off(name);
			unit.in_direct_pending = false;
			unit.in_direct = false;
			direct_set_state(name, DIRECT_STATE_ERROR);
			break;
		}
		dg.unit_start_time = time();
		direct_set_state(name, DIRECT_STATE_WAIT_UNIT);
		break;

	case DIRECT_STATE_WAIT_UNIT:
		dprintf(dlevel, "waiting for unit %s, state=%s\n", dg.unit, unit_statestr(unit.state));
		// Timeout only fires while unit is stuck in UNIT_STATE_RESERVE — that's
		// the PA-denial case we need to break out of (low battery, no power).
		// Once unit moves past Reserve (Start pump / Wait pump / Warmup / Start),
		// the unit's own state machine owns failure detection; killing it here
		// mid-startup would just retry-loop right before success, which is
		// exactly the bug observed on 2026-05-19 when 120s timer fired at the
		// end of Warmup (~30s away from RUNNING).
		if (dg.unit_start_time > 0 && unit.state == UNIT_STATE_RESERVE) {
			let unit_elapsed = time() - dg.unit_start_time;
			dprintf(dlevel, "unit wait elapsed=%d in Reserve, unit_timeout=%d\n", unit_elapsed, dg.unit_timeout);
			if (unit_elapsed >= dg.unit_timeout) {
				log_warning("direct[%s]: timeout waiting for PA reserve on unit %s (%d seconds in Reserve), retrying\n", name, dg.unit, unit_elapsed);
				// Clear direct flags BEFORE unit_stop so unit_off -> unit_mode doesn't reject
				unit.in_direct_pending = false;
				unit.in_direct = false;
				unit.in_direct_priority = 0;
				unit_stop(dg.unit);
				// Go back to STOPPED instead of ERROR - reset the fan's heat/cool
				// state so the MQTT handler re-triggers fan_set_mode on the next
				// thermostat message, which calls direct_on again. This handles
				// PA deny due to low battery (< 35%) where we just need to wait
				// and retry when power becomes available.
				let retry_fan = fans[dg.pending_fan];
				if (retry_fan) {
					let was_mode = dg.target_mode;
					if (was_mode == FAN_MODE_HEAT) retry_fan.heat_state = "off";
					else if (was_mode == FAN_MODE_COOL) retry_fan.cool_state = "off";
					retry_fan.mode = FAN_MODE_NONE;
				}
				direct_set_state(name, DIRECT_STATE_STOPPED);
				dg.active = false;
				dg.pending_fan = "";
				dg.active_fan = "";
				dg.target_mode = FAN_MODE_NONE;
				dg.mode = FAN_MODE_NONE;
				dg.initial_temp = INVALID_TEMP;
				dg.saved_water_temp = INVALID_TEMP;
				dg.primer_start_time = 0;
				dg.unit_start_time = 0;
				dg.water_wait_time = 0;
				dg.confirm_flow_start = 0;
				break;
			}
		}
		if (unit.state == UNIT_STATE_RUNNING) {
			// Unit is running with normal storage-loop flow (pin1 still closed).
			// Verify thermal delta on ac_pump (proves water is moving through HX
			// and compressor is doing work) before opening pin1.
			dprintf(dlevel, "unit %s reached RUNNING, transitioning to CONFIRM_FLOW\n", dg.unit);
			dg.confirm_flow_start = time();
			direct_set_state(name, DIRECT_STATE_CONFIRM_FLOW);
		} else if (unit.state == UNIT_STATE_ERROR) {
			error_set("direct", dg.pending_fan, sprintf("unit %s entered error state", dg.unit));
			unit.in_direct_pending = false;
			unit.in_direct = false;
			direct_set_state(name, DIRECT_STATE_ERROR);
		}
		// else stay in WAIT_UNIT
		break;

	case DIRECT_STATE_CONFIRM_FLOW:
		// Unit is RUNNING (compressor on, pump post-warmup). Pin1 is still closed,
		// so water is circulating through the normal storage loop. Confirm flow
		// through the AC heat exchanger by checking the temp_in/out delta on the
		// ac_pump: a thermal differential proves both (a) compressor is doing
		// work and (b) water is moving through the HX. Without flow, in == out
		// even with the compressor running. Error out on timeout - opening the
		// valve into an under-flowing HX is what trips vapor-temp freeze
		// protection on the unit.
		if (unit.state != UNIT_STATE_RUNNING) {
			error_set("direct", dg.pending_fan, sprintf("unit %s left RUNNING during flow confirmation (state=%s)", dg.unit, unit_statestr(unit.state)));
			unit.in_direct = false;
			unit.in_direct_pending = false;
			direct_set_state(name, DIRECT_STATE_ERROR);
			break;
		}
		if (ac_pump.temp_in_sensor && ac_pump.temp_out_sensor) {
			let t_in = get_sensor(ac_pump.temp_in_sensor, false);
			let t_out = get_sensor(ac_pump.temp_out_sensor, false);
			if (is_valid_temp(t_in) && is_valid_temp(t_out)) {
				let delta = Math.abs(t_in - t_out);
				dprintf(dlevel, "direct[%s]: flow check: ac_pump temp_in=%.1f, temp_out=%.1f, delta=%.1f, threshold=%.1f\n",
					name, t_in, t_out, delta, dg.flow_confirm_threshold);
				if (delta >= dg.flow_confirm_threshold) {
					log_info("direct[%s]: flow confirmed (ac_pump delta=%.1f >= %.1f), activating valve\n",
						name, delta, dg.flow_confirm_threshold);
					direct_set_state(name, DIRECT_STATE_ACTIVATE_VALVE);
					break;
				}
			}
		}
		// Timeout - flow never developed. Opening the valve now would risk the
		// exact priming/freeze failure this state was added to prevent. Error
		// out and let the next thermostat call retry.
		if (dg.confirm_flow_start > 0) {
			let flow_elapsed = time() - dg.confirm_flow_start;
			if (flow_elapsed >= dg.flow_confirm_timeout) {
				error_set("direct", dg.pending_fan, sprintf("unit %s: no flow detected on ac_pump after %d seconds (delta < %.1f)", dg.unit, flow_elapsed, dg.flow_confirm_threshold));
				unit_stop(dg.unit);
				unit.in_direct = false;
				unit.in_direct_pending = false;
				direct_set_state(name, DIRECT_STATE_ERROR);
			}
		}
		break;

	case DIRECT_STATE_ACTIVATE_VALVE:
		// Unit is RUNNING with confirmed flow through the HX. Safe to redirect
		// the established flow into the AC->AH loop by opening pin1.
		if (unit.state != UNIT_STATE_RUNNING) {
			error_set("direct", dg.pending_fan, sprintf("unit %s left RUNNING before valve activation (state=%s)", dg.unit, unit_statestr(unit.state)));
			unit.in_direct = false;
			unit.in_direct_pending = false;
			direct_set_state(name, DIRECT_STATE_ERROR);
			break;
		}
		dprintf(dlevel, "activating 3-way valve (pin1) with unit RUNNING and flow confirmed\n");
		if (dg_valve1_on(name)) {
			error_set("direct", dg.pending_fan, sprintf("unable to activate 3-way valve: %s", config.errmsg));
			unit.in_direct = false;
			unit.in_direct_pending = false;
			direct_set_state(name, DIRECT_STATE_ERROR);
			break;
		}
		dg.active = true;
		dg.active_fan = dg.pending_fan;
		dg.mode = dg.target_mode;
		if (dg.pin2 >= 0) {
			// Stage 2 available - go to RUNNING_OPEN, wait for temp change on
			// fan_pump.temp_out (proves stage 1 flow reached the AH side).
			dg.initial_temp = get_sensor(fan_pump.temp_out_sensor, false);
			dprintf(dlevel, "captured initial fan_pump temp_out: %.1f\n", dg.initial_temp);
			direct_set_state(name, DIRECT_STATE_RUNNING_OPEN);
			log_info("direct mode stage 1: group=%s fan=%s unit=%s mode=%s (waiting for temp change)\n",
				name, dg.active_fan, dg.unit, fan_modestr(dg.mode));
		} else {
			// No stage 2 - start fan immediately (open loop can't reach water temp)
			fan_start(dg.active_fan);
			direct_set_state(name, DIRECT_STATE_RUNNING);
			log_info("direct mode enabled: group=%s fan=%s unit=%s mode=%s\n",
				name, dg.active_fan, dg.unit, fan_modestr(dg.mode));
		}
		break;

	case DIRECT_STATE_WAIT_WATER_TEMP:
		// Wait for water to reach temperature before starting fan
		// In cool mode: wait until water <= cool_high_temp
		// In heat mode: wait until water >= heat_low_temp
		if (unit.state != UNIT_STATE_RUNNING) {
			error_set("direct", dg.active_fan, sprintf("unit %s stopped while waiting for water temp", dg.unit));
			dg_valves_off(name);
			unit.in_direct = false;
			direct_set_state(name, DIRECT_STATE_ERROR);
			break;
		}
		let wt_sensor = ac_pump.temp_out_sensor;
		if (wt_sensor) {
			let water_temp = get_sensor(wt_sensor, false);
			if (is_valid_temp(water_temp)) {
				let temp_ready = false;
				let threshold;
				if (dg.mode == FAN_MODE_COOL) {
					threshold = ac.cool_high_temp;
					temp_ready = (water_temp <= threshold);
				} else if (dg.mode == FAN_MODE_HEAT) {
					threshold = ac.heat_low_temp;
					temp_ready = (water_temp >= threshold);
				}
				// Log water temp every 60 seconds
				let wt_log_elapsed = time() - dg.water_wait_time;
				if (wt_log_elapsed % 60 < ac.interval) {
					log_info("direct[%s]: water temp: %.1f, target: %s %.1f\n",
						name, water_temp, (dg.mode == FAN_MODE_COOL) ? "<=" : ">=", threshold);
				}
				if (temp_ready) {
					log_info("direct[%s]: water temp ready (%.1f), starting fan %s\n", name, water_temp, dg.active_fan);
					fan_start(dg.active_fan);
					direct_set_state(name, DIRECT_STATE_RUNNING);
					log_info("direct mode fully active: group=%s fan=%s unit=%s mode=%s\n",
						name, dg.active_fan, dg.unit, fan_modestr(dg.mode));
					break;
				}
			}
		}
		// Timeout - start fan anyway after water_temp_timeout
		if (dg.water_wait_time > 0) {
			let wt_elapsed = time() - dg.water_wait_time;
			if (wt_elapsed >= dg.water_temp_timeout) {
				log_warning("direct[%s]: water temp timeout after %d seconds, starting fan anyway\n", name, wt_elapsed);
				fan_start(dg.active_fan);
				direct_set_state(name, DIRECT_STATE_RUNNING);
			}
		}
		break;

	case DIRECT_STATE_ACTIVATE_PIN2:
		// Monitor unit - if it stops, abort
		if (unit.state != UNIT_STATE_RUNNING) {
			error_set("direct", dg.active_fan, sprintf("unit %s stopped before activating pin2", dg.unit));
			dg_valves_off(name);
			unit.in_direct_pending = false;
			unit.in_direct = false;
			direct_set_state(name, DIRECT_STATE_ERROR);
			break;
		}
		dprintf(dlevel, "activating pin2 to close the loop\n");
		if (dg_valve2_on(name)) {
			error_set("direct", dg.active_fan, sprintf("unable to activate valve2: %s", config.errmsg));
			dg_valves_off(name);
			unit.in_direct_pending = false;
			unit.in_direct = false;
			direct_set_state(name, DIRECT_STATE_ERROR);
			break;
		}
		direct_set_state(name, DIRECT_STATE_START_PRIMER);
		break;

	case DIRECT_STATE_START_PRIMER:
		// Monitor unit - if it stops, abort
		if (unit.state != UNIT_STATE_RUNNING) {
			error_set("direct", dg.active_fan, sprintf("unit %s stopped before starting primer", dg.unit));
			dg_valves_off(name);
			unit.in_direct_pending = false;
			unit.in_direct = false;
			direct_set_state(name, DIRECT_STATE_ERROR);
			break;
		}
		dprintf(dlevel, "starting primer %s\n", dg.primer);
		if (dg.primer && dg.primer.length) {
			if (pump_start(dg.primer)) {
				log_warning("direct[%s]: unable to start primer %s, continuing anyway\n", name, dg.primer);
			}
		}
		dg.primer_start_time = time();
		direct_set_state(name, DIRECT_STATE_WAIT_PRIMER);
		break;

	case DIRECT_STATE_WAIT_PRIMER:
		// Monitor unit - if it stops during priming, abort
		if (unit.state != UNIT_STATE_RUNNING) {
			error_set("direct", dg.active_fan, sprintf("unit %s stopped during priming", dg.unit));
			if (dg.primer && dg.primer.length) pump_stop(dg.primer);
			dg_valves_off(name);
			unit.in_direct_pending = false;
			unit.in_direct = false;
			direct_set_state(name, DIRECT_STATE_ERROR);
			break;
		}
		let primer_elapsed = time() - dg.primer_start_time;
		dprintf(dlevel, "waiting for primer, elapsed=%d, primer_time=%d\n", primer_elapsed, dg.primer_time);
		if (primer_elapsed >= dg.primer_time) {
			// Stop primer
			if (dg.primer && dg.primer.length) {
				pump_stop(dg.primer);
			}
			// Loop closed - wait for water temp before starting fan
			log_info("direct mode stage 2 complete: group=%s fan=%s unit=%s mode=%s (loop closed)\n",
				name, dg.active_fan, dg.unit, fan_modestr(dg.mode));
			// Reset flow check timer - water is still warming up in the closed loop
			unit.run_started = false;
			dg.water_wait_time = time();
			direct_set_state(name, DIRECT_STATE_WAIT_WATER_TEMP);
			let wt_target = (dg.target_mode == FAN_MODE_COOL) ? ac.cool_high_temp : ac.heat_low_temp;
			log_info("direct[%s]: loop closed, waiting for water temp %s %.1f before starting fan\n",
				name, (dg.target_mode == FAN_MODE_COOL) ? "<=" : ">=", wt_target);
		}
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
// stop_fan: true (default) = stop the fan, false = keep fan running (transition to storage)
function direct_off(group_name, stop_fan) {

	let dlevel = 1;

	if (typeof(stop_fan) == "undefined") stop_fan = true;

	let dg = directs[group_name];
	if (!dg) {
		dprintf(dlevel, "direct group %s not found\n", group_name);
		return 0;
	}

	// Fan name comes from direct group config
	let fan_name = dg.fan;

	dprintf(dlevel, "direct_off: group=%s fan=%s stop_fan=%s\n", group_name, fan_name, stop_fan);

	// Check if direct group is active or pending
	if (dg.active_fan != fan_name && dg.pending_fan != fan_name) {
		// Not active or pending - nothing to do
		dprintf(dlevel, "direct group %s not active or pending\n", group_name);
		return 0;
	}

	let unit = units[dg.unit];
	let fan_pump = pumps[dg.fan_pump];

	// Always stop primer if configured (may have been left running)
	if (dg.primer && dg.primer.length) {
		dprintf(dlevel, "stopping primer %s\n", dg.primer);
		pump_stop(dg.primer);
	}

	// Deactivate all valves (routes flow back to storage, clears pump.direct)
	if (dg_valves_off(group_name)) {
		error_set("direct", fan_name, sprintf("unable to deactivate valves: %s", config.errmsg));
		// Continue anyway - best effort cleanup
	}

	if (stop_fan) {
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
	} else {
		// Transition to storage mode - start fan pump so fan can use storage
		dprintf(dlevel, "transitioning fan %s to storage mode\n", fan_name);
		pump_start(dg.fan_pump);
	}

	// dg_valves_off already re-enabled fan pump (pump.direct = false, pump.enabled = true)

	// Clear direct mode flags FIRST so unit_mode/unit_stop can change mode
	unit.in_direct = false;
	unit.in_direct_pending = false;
	unit.in_direct_priority = 0;
	// Reset flow check so it gets a fresh 300s window after transition
	unit.run_started = false;

	// Hand off to charge.js if we were fully active (unit was started by us)
	// If unit errored (e.g. flow check), do NOT restore water_temp or hand off -
	// just stop everything and stay stopped to prevent restart loops
	if (dg.active && unit.state == UNIT_STATE_ERROR) {
		log_warning("direct[%s]: unit in error state, stopping everything (no handoff)\n", group_name);
		unit_stop(dg.unit);
		if (unit.reserve) {
			pa_client_release(ac, "unit", dg.unit, unit.reserve);
		}
		unit.charging = false;
		unit.charge_state = CHARGE_STATE_STOPPED;
	} else if (dg.active) {
		// Restore water_temp from before direct mode - the AC pump has residual
		// cold/hot water from the direct loop that would give charge.js a false reading.
		// This must happen BEFORE any charge_get_pri() calls or handoff decisions.
		if (is_valid_temp(dg.saved_water_temp)) {
			log_info("direct[%s]: restoring water_temp: %.1f (was %.1f)\n", group_name, dg.saved_water_temp, ac.water_temp);
			ac.water_temp = dg.saved_water_temp;
		}

		// Reset pump settled flag so charge.js waits for real tank readings
		let ac_pump = pumps[dg.ac_pump];
		if (ac_pump) ac_pump.settled = false;

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
			}

			// Set mode back to storage mode (unit must be stopped first)
			unit_mode(dg.unit, unit, storage_mode);
			dprintf(dlevel, "unit %s mode set to %s\n", dg.unit, ac_modestr(storage_mode));

			// Hand off to charge.js in stopped state
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
	} else {
		// Not fully active, but unit mode may have been changed during
		// ACTIVATE_VALVE (rvpin set to direct mode). Restore to storage mode.
		let target_ac_mode = (dg.target_mode == FAN_MODE_COOL) ? AC_MODE_COOL : AC_MODE_HEAT;
		if (dg.target_mode != FAN_MODE_NONE && unit.mode != ac.mode) {
			log_info("direct[%s]: aborted before active, restoring unit %s mode from %s to %s\n",
				group_name, dg.unit, ac_modestr(unit.mode), ac_modestr(ac.mode));
			// Stop unit if it was started
			if (unit.state != UNIT_STATE_STOPPED) {
				unit_stop(dg.unit);
				if (unit.reserve) {
					pa_client_release(ac, "unit", dg.unit, unit.reserve);
				}
			}
			unit_mode(dg.unit, unit, ac.mode);
		}
	}

	// Clear state
	direct_set_state(group_name, DIRECT_STATE_STOPPED);
	dg.active = false;
	dg.pending_fan = "";
	dg.active_fan = "";
	dg.target_mode = FAN_MODE_NONE;
	dg.mode = FAN_MODE_NONE;
	dg.initial_temp = INVALID_TEMP;
	dg.saved_water_temp = INVALID_TEMP;
	dg.primer_start_time = 0;
	dg.unit_start_time = 0;
	dg.water_wait_time = 0;
	dg.confirm_flow_start = 0;

	log_info("direct mode disabled: group=%s fan=%s\n", group_name, fan_name);

	return 0;
}
