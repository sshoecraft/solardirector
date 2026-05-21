#!/opt/sd/bin/sdjs -U512K
/*
 * Tests for agents/ac/direct.js — direct-mode entry validation, valve
 * helpers (pin1/pin2 control + pump.direct flag flips), state-machine
 * entry transitions, query helpers (is_active / is_pending / is_error),
 * direct_off short-circuits.
 *
 * Full state-machine integration (the 16-state lifecycle) is intentionally
 * not exercised here — that's better suited for run-loop integration tests.
 */

include(dirname(script_name) + "/harness.js");
include(dirname(script_name) + "/assert.js");
harness.init();

include(dirname(script_name) + "/../utils.js");
include(dirname(script_name) + "/../common.js");
include(dirname(script_name) + "/../direct.js");
direct_init();

function add_dg(name, opts) {
	let dg = {
		enabled: true,
		state:   DIRECT_STATE_STOPPED,
		active:  false,
		pending_fan:  "",
		active_fan:   "",
		target_mode:  FAN_MODE_NONE,
		mode:         FAN_MODE_NONE,
		fan: "ah1", unit: "ac1",
		fan_pump: "fp1", ac_pump: "ap1",
		pin1: 2, pin2: 28,
		primer: "",
		temp_threshold: 2.0,
		unit_timeout: 60,
		water_temp_timeout: 600,
		flow_confirm_threshold: 2.0,
		flow_confirm_timeout:   60,
		initial_temp: INVALID_TEMP,
		saved_water_temp: INVALID_TEMP,
		primer_start_time: 0,
		unit_start_time:   0,
		water_wait_time:   0,
		confirm_flow_start: 0,
	};
	if (opts) for (let k in opts) dg[k] = opts[k];
	directs[name] = dg;
	return dg;
}

function setup() {
	ac.mode = AC_MODE_COOL;
	ac.water_temp = 50;
}

// =========================================================================
// 1. dg_valve1_on/off — pin control + pump.direct flag flip.
// =========================================================================
function test_valve1_on_drives_pin_high_and_marks_pump_direct() {
	let dg = add_dg("dg1", { pin1: 2 });
	let fp = harness.add_pump("fp1", { pin: 5 });
	harness.pins[2] = LOW;
	let r = dg_valve1_on("dg1");
	assert.eq(r, 0);
	assert.eq(digitalRead(2), HIGH, "valve1 pin1 must be HIGH");
	assert.eq(fp.direct, true, "valve1 must mark fan_pump as direct");
}

function test_valve1_off_drives_pin_low_and_clears_pump_direct() {
	let dg = add_dg("dg1", { pin1: 2 });
	let fp = harness.add_pump("fp1", { pin: 5 });
	fp.direct = true;
	harness.pins[2] = HIGH;
	let r = dg_valve1_off("dg1");
	assert.eq(r, 0);
	assert.eq(digitalRead(2), LOW);
	assert.eq(fp.direct, false, "valve1 off must clear pump.direct");
}

function test_valve1_on_unknown_group_errors() {
	let r = dg_valve1_on("nope");
	assert.eq(r, 1);
	assert.truthy(config.errmsg.indexOf("not found") >= 0);
}

function test_valve1_on_no_pin1_configured_errors() {
	let dg = add_dg("dg1", { pin1: -1 });
	harness.add_pump("fp1");
	let r = dg_valve1_on("dg1");
	assert.eq(r, 1);
	assert.truthy(config.errmsg.indexOf("pin1") >= 0);
}

// =========================================================================
// 2. dg_valve2_on with pin2 < 0 is a no-op success.
// =========================================================================
function test_valve2_on_no_pin2_is_skip_success() {
	let dg = add_dg("dg1", { pin2: -1 });
	let r = dg_valve2_on("dg1");
	assert.eq(r, 0, "no pin2 → return 0, no error");
}

function test_valve2_on_drives_pin2_high() {
	let dg = add_dg("dg1", { pin2: 28 });
	harness.pins[28] = LOW;
	let r = dg_valve2_on("dg1");
	assert.eq(r, 0);
	assert.eq(digitalRead(28), HIGH);
}

// =========================================================================
// 3. dg_valves_off: drives both pins LOW.
// =========================================================================
function test_valves_off_drives_both_low() {
	let dg = add_dg("dg1", { pin1: 2, pin2: 28 });
	let fp = harness.add_pump("fp1");
	fp.direct = true;
	harness.pins[2]  = HIGH;
	harness.pins[28] = HIGH;
	let r = dg_valves_off("dg1");
	assert.eq(r, 0);
	assert.eq(digitalRead(2),  LOW);
	assert.eq(digitalRead(28), LOW);
	assert.eq(fp.direct, false);
}

// =========================================================================
// 4. direct_on — validation paths.
// =========================================================================
function test_direct_on_unknown_group_errors() {
	let r = direct_on("nope", "cool");
	assert.eq(r, 1);
}

function test_direct_on_disabled_group_errors() {
	let dg = add_dg("dg1", { enabled: false });
	let r = direct_on("dg1", "cool");
	assert.eq(r, 1);
}

function test_direct_on_invalid_mode_str_errors() {
	let dg = add_dg("dg1");
	harness.add_unit("ac1");
	harness.add_pump("fp1"); harness.add_pump("ap1");
	let r = direct_on("dg1", "warm");
	assert.eq(r, 1);
	assert.truthy(errors.list.length > 0);
}

// =========================================================================
// 5. direct_on with running fan pump → state machine enters STOP_FAN_PUMP.
// =========================================================================
function test_direct_on_running_fan_pump_enters_stop_fan_pump() {
	let dg = add_dg("dg1");
	let unit = harness.add_unit("ac1");
	harness.add_pump("fp1", { state: PUMP_STATE_RUNNING });
	harness.add_pump("ap1");
	let r = direct_on("dg1", "cool");
	assert.eq(r, 0);
	assert.eq(dg.state, DIRECT_STATE_STOP_FAN_PUMP);
	assert.eq(dg.pending_fan, "ah1");
	assert.eq(dg.target_mode, FAN_MODE_COOL);
	assert.eq(unit.in_direct_pending, true);
}

// =========================================================================
// 6. direct_on with already-stopped fan pump → state machine skips ahead
//    to PREPARE_UNIT (the entry point of the unit-start-first sequence).
//    Pin1 must NOT be opened on entry — it gets opened later in the new
//    ACTIVATE_VALVE state, only after the unit reaches RUNNING + flow is
//    confirmed.
// =========================================================================
function test_direct_on_stopped_fan_pump_skips_to_prepare_unit() {
	let dg = add_dg("dg1");
	harness.add_unit("ac1");
	harness.add_pump("fp1", { state: PUMP_STATE_STOPPED });
	harness.add_pump("ap1");
	harness.pins[2] = LOW;
	let r = direct_on("dg1", "heat");
	assert.eq(r, 0);
	assert.eq(dg.state, DIRECT_STATE_PREPARE_UNIT);
	assert.eq(dg.target_mode, FAN_MODE_HEAT);
	assert.eq(digitalRead(2), LOW, "pin1 must NOT be HIGH after direct_on entry");
}

// =========================================================================
// 7. direct_on with default mode_str picks based on ac.mode.
// =========================================================================
function test_direct_on_default_mode_uses_ac_mode_cool() {
	ac.mode = AC_MODE_COOL;
	let dg = add_dg("dg1");
	harness.add_unit("ac1");
	harness.add_pump("fp1", { state: PUMP_STATE_STOPPED });
	harness.add_pump("ap1");
	direct_on("dg1");
	assert.eq(dg.target_mode, FAN_MODE_COOL);
}

function test_direct_on_default_mode_uses_ac_mode_heat() {
	ac.mode = AC_MODE_HEAT;
	let dg = add_dg("dg1");
	harness.add_unit("ac1");
	harness.add_pump("fp1", { state: PUMP_STATE_STOPPED });
	harness.add_pump("ap1");
	direct_on("dg1");
	assert.eq(dg.target_mode, FAN_MODE_HEAT);
}

// =========================================================================
// 8. direct_on already-active for same fan → no-op success.
// =========================================================================
function test_direct_on_already_active_returns_success() {
	let dg = add_dg("dg1");
	dg.active = true;
	dg.active_fan = "ah1";
	harness.add_unit("ac1");
	harness.add_pump("fp1");
	harness.add_pump("ap1");
	let prior_state = dg.state;
	let r = direct_on("dg1", "cool");
	assert.eq(r, 0, "already active → 0");
	assert.eq(dg.state, prior_state, "no state change");
}

// =========================================================================
// 9. direct_on saves water_temp at entry — for later restoration.
// =========================================================================
function test_direct_on_saves_water_temp() {
	let dg = add_dg("dg1");
	harness.add_unit("ac1");
	harness.add_pump("fp1");
	harness.add_pump("ap1");
	ac.water_temp = 47.5;
	direct_on("dg1", "cool");
	assert.eq(dg.saved_water_temp, 47.5, "saved_water_temp captured at entry");
}

// =========================================================================
// 10. Query helpers.
// =========================================================================
// direct.js's own direct_get_pump — we want to use the real one here, so we
// reassign over the harness stub that was overwritten by direct.js include.
// (Including direct.js already restored the real ones; nothing extra needed.)
function test_get_pump_returns_ac_pump() {
	let dg = add_dg("dg1", { ac_pump: "ap42" });
	assert.eq(direct_get_pump("dg1"), "ap42");
}

function test_get_pump_unknown_returns_null() {
	assert.eq(direct_get_pump("nope"), null);
}

function test_is_active_only_when_active_and_fan_matches() {
	let dg = add_dg("dg1");
	assert.eq(direct_is_active("dg1", "ah1"), false, "not active");
	dg.active = true;
	dg.active_fan = "ah1";
	assert.eq(direct_is_active("dg1", "ah1"), true);
	assert.eq(direct_is_active("dg1", "other"), false);
}

function test_is_pending_when_pending_fan_set_and_not_active() {
	let dg = add_dg("dg1");
	dg.pending_fan = "ah1";
	dg.active = false;
	assert.eq(direct_is_pending("dg1", "ah1"), true);
	dg.active = true;
	assert.eq(direct_is_pending("dg1", "ah1"), false, "active overrides pending");
}

function test_is_error_only_when_state_is_error() {
	let dg = add_dg("dg1");
	assert.eq(direct_is_error("dg1"), false);
	dg.state = DIRECT_STATE_ERROR;
	assert.eq(direct_is_error("dg1"), true);
}

// =========================================================================
// 11. direct_off when group not active and not pending → no-op success.
// =========================================================================
function test_direct_off_inactive_is_noop() {
	let dg = add_dg("dg1");
	// Neither active_fan nor pending_fan set
	let r = direct_off("dg1");
	assert.eq(r, 0);
	assert.not_called(harness.spies.pump_stop, "no pump activity on noop direct_off");
}

function test_direct_off_unknown_group_returns_zero() {
	let r = direct_off("nope");
	assert.eq(r, 0, "missing group → 0 (per direct.js dprintf-and-return path)");
}

// =========================================================================
// 12. State-machine lifecycle: unit-start-before-valve ordering.
//
// These tests exercise the priming-bug fix: pin1 must remain LOW until
// (a) the AC unit reaches UNIT_STATE_RUNNING and (b) ac_pump shows a
// thermal delta confirming water is moving through the heat exchanger.
// =========================================================================

// PREPARE_UNIT: unit stopped → START_UNIT, pin1 stays LOW.
function test_prepare_unit_stopped_unit_goes_to_start_unit() {
	let dg = add_dg("dg1", { state: DIRECT_STATE_PREPARE_UNIT, pending_fan: "ah1", target_mode: FAN_MODE_COOL });
	let unit = harness.add_unit("ac1", { state: UNIT_STATE_STOPPED, mode: AC_MODE_COOL });
	harness.add_fan("ah1", { priority: 100 });
	harness.add_pump("fp1");
	harness.add_pump("ap1");
	harness.pins[2] = LOW;
	_direct_process("dg1", dg);
	assert.eq(dg.state, DIRECT_STATE_START_UNIT, "stopped unit → START_UNIT");
	assert.eq(digitalRead(2), LOW, "pin1 must stay LOW in PREPARE_UNIT");
	assert.eq(unit.in_direct, true, "in_direct flag set");
	assert.eq(unit.in_direct_pending, false, "in_direct_pending cleared");
}

// PREPARE_UNIT: unit already running in correct mode → CONFIRM_FLOW (hijack
// from charge.js). Pin1 stays LOW; flow confirmation still required.
function test_prepare_unit_running_correct_mode_goes_to_confirm_flow() {
	let dg = add_dg("dg1", { state: DIRECT_STATE_PREPARE_UNIT, pending_fan: "ah1", target_mode: FAN_MODE_COOL });
	let unit = harness.add_unit("ac1", { state: UNIT_STATE_RUNNING, mode: AC_MODE_COOL });
	harness.add_fan("ah1", { priority: 100 });
	harness.add_pump("fp1");
	harness.add_pump("ap1");
	harness.pins[2] = LOW;
	harness.now = 1000;
	_direct_process("dg1", dg);
	assert.eq(dg.state, DIRECT_STATE_CONFIRM_FLOW, "running in correct mode → CONFIRM_FLOW");
	assert.eq(digitalRead(2), LOW, "pin1 stays LOW");
	assert.eq(dg.confirm_flow_start, 1000, "confirm_flow_start captured");
}

// PREPARE_UNIT: unit running in wrong mode → STOP_UNIT first.
function test_prepare_unit_running_wrong_mode_goes_to_stop_unit() {
	let dg = add_dg("dg1", { state: DIRECT_STATE_PREPARE_UNIT, pending_fan: "ah1", target_mode: FAN_MODE_COOL });
	let unit = harness.add_unit("ac1", { state: UNIT_STATE_RUNNING, mode: AC_MODE_HEAT });
	harness.add_fan("ah1", { priority: 100 });
	harness.add_pump("fp1");
	harness.add_pump("ap1");
	_direct_process("dg1", dg);
	assert.eq(dg.state, DIRECT_STATE_STOP_UNIT, "wrong mode → STOP_UNIT");
}

// PREPARE_UNIT: unit in pre-running state (Reserve/Start pump/etc) → WAIT_UNIT
// (hijack the in-progress startup).
function test_prepare_unit_unit_starting_goes_to_wait_unit() {
	let dg = add_dg("dg1", { state: DIRECT_STATE_PREPARE_UNIT, pending_fan: "ah1", target_mode: FAN_MODE_COOL });
	let unit = harness.add_unit("ac1", { state: UNIT_STATE_RESERVE, mode: AC_MODE_COOL, charging: true, charge_state: CHARGE_STATE_RUNNING });
	harness.add_fan("ah1", { priority: 100 });
	harness.add_pump("fp1");
	harness.add_pump("ap1");
	harness.now = 1000;
	_direct_process("dg1", dg);
	assert.eq(dg.state, DIRECT_STATE_WAIT_UNIT, "hijacked startup → WAIT_UNIT");
	assert.eq(unit.charging, false, "charge.js detached from unit");
	assert.eq(dg.unit_start_time, 1000, "unit_start_time captured for timeout");
}

// WAIT_UNIT: unit reaches RUNNING → CONFIRM_FLOW (NOT RUNNING_OPEN), pin1
// stays LOW. This is the core ordering fix.
function test_wait_unit_reaches_running_goes_to_confirm_flow_not_running_open() {
	let dg = add_dg("dg1", {
		state: DIRECT_STATE_WAIT_UNIT,
		pending_fan: "ah1",
		target_mode: FAN_MODE_COOL,
		unit_start_time: 100,
	});
	let unit = harness.add_unit("ac1", { state: UNIT_STATE_RUNNING, mode: AC_MODE_COOL });
	harness.add_fan("ah1", { priority: 100 });
	harness.add_pump("fp1");
	harness.add_pump("ap1");
	harness.pins[2] = LOW;
	harness.now = 130;
	_direct_process("dg1", dg);
	assert.eq(dg.state, DIRECT_STATE_CONFIRM_FLOW, "WAIT_UNIT success → CONFIRM_FLOW");
	assert.eq(digitalRead(2), LOW, "pin1 must remain LOW until ACTIVATE_VALVE");
	assert.eq(dg.active, false, "active stays false until valve opens");
	assert.eq(dg.confirm_flow_start, 130, "confirm_flow_start captured");
}

// CONFIRM_FLOW: ac_pump shows thermal delta ≥ threshold → ACTIVATE_VALVE.
// Pin1 is still closed at this point — that's the next state's job.
function test_confirm_flow_delta_above_threshold_advances_to_activate_valve() {
	let dg = add_dg("dg1", {
		state: DIRECT_STATE_CONFIRM_FLOW,
		pending_fan: "ah1",
		target_mode: FAN_MODE_COOL,
		confirm_flow_start: 100,
		flow_confirm_threshold: 2.0,
		flow_confirm_timeout: 60,
	});
	harness.add_unit("ac1", { state: UNIT_STATE_RUNNING });
	harness.add_fan("ah1");
	harness.add_pump("fp1");
	harness.add_pump("ap1", { temp_in_sensor: "test_in", temp_out_sensor: "test_out" });
	let saved_get_sensor = get_sensor;
	get_sensor = function(spec) {
		if (spec == "test_in")  return 55.0;
		if (spec == "test_out") return 49.0;
		return INVALID_TEMP;
	};
	harness.pins[2] = LOW;
	harness.now = 110;
	_direct_process("dg1", dg);
	get_sensor = saved_get_sensor;
	assert.eq(dg.state, DIRECT_STATE_ACTIVATE_VALVE, "delta 6.0°F → ACTIVATE_VALVE");
	assert.eq(digitalRead(2), LOW, "pin1 still LOW in CONFIRM_FLOW (gets opened in next state)");
}

// CONFIRM_FLOW: delta below threshold and timeout not yet hit → stay in
// CONFIRM_FLOW. No state change, no valve activation.
function test_confirm_flow_delta_below_threshold_waits() {
	let dg = add_dg("dg1", {
		state: DIRECT_STATE_CONFIRM_FLOW,
		pending_fan: "ah1",
		target_mode: FAN_MODE_COOL,
		confirm_flow_start: 100,
		flow_confirm_threshold: 2.0,
		flow_confirm_timeout: 60,
	});
	harness.add_unit("ac1", { state: UNIT_STATE_RUNNING });
	harness.add_fan("ah1");
	harness.add_pump("fp1");
	harness.add_pump("ap1", { temp_in_sensor: "test_in", temp_out_sensor: "test_out" });
	let saved_get_sensor = get_sensor;
	get_sensor = function(spec) {
		if (spec == "test_in")  return 55.0;
		if (spec == "test_out") return 54.5;
		return INVALID_TEMP;
	};
	harness.now = 120;
	_direct_process("dg1", dg);
	get_sensor = saved_get_sensor;
	assert.eq(dg.state, DIRECT_STATE_CONFIRM_FLOW, "delta 0.5°F < 2.0 → stay");
}

// CONFIRM_FLOW: timeout reached without flow → ERROR, unit stopped.
// Opening the valve into an under-flowing HX is exactly what trips the
// vapor-temp freeze protection — we error out instead.
function test_confirm_flow_timeout_errors_and_stops_unit() {
	let dg = add_dg("dg1", {
		state: DIRECT_STATE_CONFIRM_FLOW,
		pending_fan: "ah1",
		target_mode: FAN_MODE_COOL,
		confirm_flow_start: 100,
		flow_confirm_threshold: 2.0,
		flow_confirm_timeout: 60,
	});
	let unit = harness.add_unit("ac1", { state: UNIT_STATE_RUNNING });
	harness.add_fan("ah1");
	harness.add_pump("fp1");
	harness.add_pump("ap1", { temp_in_sensor: "test_in", temp_out_sensor: "test_out" });
	let saved_get_sensor = get_sensor;
	get_sensor = function(spec) {
		if (spec == "test_in")  return 55.0;
		if (spec == "test_out") return 55.0;
		return INVALID_TEMP;
	};
	harness.now = 165;
	_direct_process("dg1", dg);
	get_sensor = saved_get_sensor;
	assert.eq(dg.state, DIRECT_STATE_ERROR, "timeout with no delta → ERROR");
	assert.called(harness.spies.unit_stop, "unit_stop called on flow-confirm timeout");
	assert.eq(unit.in_direct, false, "in_direct cleared");
}

// CONFIRM_FLOW: unit drops out of RUNNING during the wait → ERROR.
function test_confirm_flow_unit_leaves_running_errors() {
	let dg = add_dg("dg1", {
		state: DIRECT_STATE_CONFIRM_FLOW,
		pending_fan: "ah1",
		target_mode: FAN_MODE_COOL,
		confirm_flow_start: 100,
	});
	let unit = harness.add_unit("ac1", { state: UNIT_STATE_ERROR });
	harness.add_fan("ah1");
	harness.add_pump("fp1");
	harness.add_pump("ap1");
	harness.now = 105;
	_direct_process("dg1", dg);
	assert.eq(dg.state, DIRECT_STATE_ERROR, "unit not RUNNING during CONFIRM_FLOW → ERROR");
	assert.eq(unit.in_direct, false, "in_direct cleared");
}

// ACTIVATE_VALVE: opens pin1 and (with pin2 configured) enters RUNNING_OPEN.
// This is the only place pin1 should ever go HIGH during normal startup.
function test_activate_valve_opens_pin1_and_enters_running_open() {
	let dg = add_dg("dg1", {
		state: DIRECT_STATE_ACTIVATE_VALVE,
		pending_fan: "ah1",
		target_mode: FAN_MODE_COOL,
		pin1: 2,
		pin2: 28,
	});
	harness.add_unit("ac1", { state: UNIT_STATE_RUNNING });
	harness.add_fan("ah1");
	let fp = harness.add_pump("fp1", { temp_out_sensor: "fp_out" });
	harness.add_pump("ap1");
	let saved_get_sensor = get_sensor;
	get_sensor = function(spec) { if (spec == "fp_out") return 72.5; return INVALID_TEMP; };
	harness.pins[2] = LOW;
	_direct_process("dg1", dg);
	get_sensor = saved_get_sensor;
	assert.eq(digitalRead(2), HIGH, "pin1 HIGH after ACTIVATE_VALVE");
	assert.eq(fp.direct, true, "fan_pump marked as direct");
	assert.eq(dg.state, DIRECT_STATE_RUNNING_OPEN, "with pin2 → RUNNING_OPEN");
	assert.eq(dg.active, true, "active set after pin1 opens");
	assert.eq(dg.active_fan, "ah1");
	assert.eq(dg.mode, FAN_MODE_COOL);
	assert.eq(dg.initial_temp, 72.5, "initial_temp captured for stage 1 monitoring");
}

// ACTIVATE_VALVE without pin2 configured: opens pin1, starts fan, goes to
// RUNNING (open-loop direct, no closed loop, can't wait for water temp).
function test_activate_valve_without_pin2_starts_fan_and_enters_running() {
	let dg = add_dg("dg1", {
		state: DIRECT_STATE_ACTIVATE_VALVE,
		pending_fan: "ah1",
		target_mode: FAN_MODE_COOL,
		pin1: 2,
		pin2: -1,
	});
	harness.add_unit("ac1", { state: UNIT_STATE_RUNNING });
	harness.add_fan("ah1");
	harness.add_pump("fp1");
	harness.add_pump("ap1");
	harness.pins[2] = LOW;
	_direct_process("dg1", dg);
	assert.eq(digitalRead(2), HIGH, "pin1 HIGH");
	assert.eq(dg.state, DIRECT_STATE_RUNNING, "no pin2 → RUNNING directly");
	assert.called(harness.spies.fan_start, "fan_start called");
}

// WAIT_UNIT timeout fires when unit is stuck in Reserve past unit_timeout.
// This is the PA-denial recovery path: drop direct flags, stop unit, return
// to Stopped, reset fan heat/cool_state so the next thermostat tick retries.
function test_wait_unit_timeout_in_reserve_resets_to_stopped() {
	let dg = add_dg("dg1", {
		state: DIRECT_STATE_WAIT_UNIT,
		pending_fan: "ah1",
		target_mode: FAN_MODE_COOL,
		unit_start_time: 100,
		unit_timeout: 120,
	});
	let unit = harness.add_unit("ac1", { state: UNIT_STATE_RESERVE });
	harness.add_fan("ah1", { cool_state: "on", mode: FAN_MODE_COOL });
	harness.add_pump("fp1");
	harness.add_pump("ap1");
	harness.now = 230;
	_direct_process("dg1", dg);
	assert.eq(dg.state, DIRECT_STATE_STOPPED, "timeout in Reserve → Stopped (for retry)");
	assert.eq(dg.pending_fan, "", "pending_fan cleared");
	assert.eq(fans["ah1"].cool_state, "off", "fan cool_state reset for re-trigger");
	assert.eq(unit.in_direct, false);
	assert.called(harness.spies.unit_stop, "unit_stop called on PA timeout");
}

// WAIT_UNIT does NOT timeout when unit has progressed past Reserve, even
// if dg.unit_timeout has elapsed. This prevents the 2026-05-19 bug where
// the timer fired at the end of Warmup, killing a healthy startup 30s
// away from RUNNING. Once unit is past Reserve, the unit's own state
// machine owns failure detection.
function test_wait_unit_no_timeout_when_past_reserve() {
	let dg = add_dg("dg1", {
		state: DIRECT_STATE_WAIT_UNIT,
		pending_fan: "ah1",
		target_mode: FAN_MODE_COOL,
		unit_start_time: 100,
		unit_timeout: 120,
	});
	let unit = harness.add_unit("ac1", { state: UNIT_STATE_WAIT_PUMP });
	harness.add_fan("ah1");
	harness.add_pump("fp1");
	harness.add_pump("ap1");
	harness.now = 300;
	_direct_process("dg1", dg);
	assert.eq(dg.state, DIRECT_STATE_WAIT_UNIT, "past Reserve → stay in WAIT_UNIT despite timeout");
	assert.eq(dg.pending_fan, "ah1", "request preserved");
	assert.not_called(harness.spies.unit_stop, "unit_stop NOT called when past Reserve");
}

// ACTIVATE_VALVE: unit dropped out of RUNNING between CONFIRM_FLOW and here →
// ERROR before opening pin1.
function test_activate_valve_unit_left_running_errors_without_opening_pin1() {
	let dg = add_dg("dg1", {
		state: DIRECT_STATE_ACTIVATE_VALVE,
		pending_fan: "ah1",
		target_mode: FAN_MODE_COOL,
	});
	let unit = harness.add_unit("ac1", { state: UNIT_STATE_ERROR });
	harness.add_fan("ah1");
	harness.add_pump("fp1");
	harness.add_pump("ap1");
	harness.pins[2] = LOW;
	_direct_process("dg1", dg);
	assert.eq(dg.state, DIRECT_STATE_ERROR, "unit not RUNNING → ERROR before pin1");
	assert.eq(digitalRead(2), LOW, "pin1 NOT opened on error path");
	assert.eq(dg.active, false, "active stays false");
}

harness.run();
