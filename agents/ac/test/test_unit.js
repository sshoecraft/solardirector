#!/opt/sd/bin/sdjs -U512K
/*
 * Tests for agents/ac/unit.js — refcount, mode change rules, reversing
 * valve pin logic, force_stop chain, revoke semantics, error transitions.
 */

include(dirname(script_name) + "/harness.js");
include(dirname(script_name) + "/assert.js");
harness.init();

include(dirname(script_name) + "/../utils.js");
include(dirname(script_name) + "/../common.js");
include(dirname(script_name) + "/../unit.js");
unit_init();

function add_unit(name, opts) {
	let u = harness.add_unit(name, opts);
	u.refs = 0;
	u.state = UNIT_STATE_STOPPED;
	u.error = false;
	u.charging = false;
	u.charge_state = CHARGE_STATE_STOPPED;
	u.in_direct = false;
	u.in_direct_pending = false;
	u.in_direct_group = "";
	u.run_started = false;
	u.coolpin = -1;
	u.heatpin = -1;
	u.rvpin   = -1;
	u.rvcool  = true;
	u.rvevery = false;
	u.pump = "";
	u.reserve = 0;
	u.priority = 0;
	u.mode = AC_MODE_COOL;
	if (opts) for (let k in opts) u[k] = opts[k];
	return u;
}

// =========================================================================
// 1. unit_start on unknown unit returns 1.
// =========================================================================
function test_start_unknown_returns_error() {
	let r = unit_start("missing");
	assert.eq(r, 1, "unknown unit must return 1");
	assert.truthy(config.errmsg.indexOf("unknown unit") >= 0);
}

// =========================================================================
// 2. Refcount: two starts, two stops needed before unit_off fires.
// =========================================================================
function test_refcount_increment_decrement() {
	let u = add_unit("ac1", { pump: "p1" });
	unit_start("ac1");
	unit_start("ac1");
	assert.eq(u.refs, 2);
	unit_stop("ac1");
	assert.eq(u.refs, 1, "first stop drops refs to 1, no pump_stop yet");
	assert.not_called(harness.spies.pump_stop);
}

// =========================================================================
// 3. Final stop calls pump_stop on the unit's pump.
// =========================================================================
function test_final_stop_calls_pump_stop() {
	let u = add_unit("ac1", { pump: "p1" });
	u.state = UNIT_STATE_RUNNING;
	u.refs  = 1;
	unit_stop("ac1");
	assert.called_with(harness.spies.pump_stop, ["p1"], "unit_off must stop the pump");
	assert.eq(u.state, UNIT_STATE_STOPPED);
}

// =========================================================================
// 4. Force-stop bypasses refcount and force-stops the pump.
// =========================================================================
function test_force_stop_calls_pump_force_stop() {
	let u = add_unit("ac1", { pump: "p1" });
	u.state = UNIT_STATE_RUNNING;
	u.refs  = 7;
	unit_force_stop("ac1");
	assert.eq(u.refs, 0);
	assert.called_with(harness.spies.pump_force_stop, ["p1"]);
	assert.eq(u.state, UNIT_STATE_STOPPED);
}

// Force-stop on a unit currently in_direct must turn off direct first.
function test_force_stop_in_direct_calls_direct_off() {
	let u = add_unit("ac1", { pump: "p1" });
	u.state = UNIT_STATE_RUNNING;
	u.refs  = 1;
	u.in_direct = true;
	u.in_direct_group = "dg1";
	unit_force_stop("ac1");
	assert.called_with(harness.spies.direct_off, ["dg1"], "in_direct unit must turn off direct first");
}

// =========================================================================
// 5. Reserve unit stops to RELEASE.
// =========================================================================
function test_reserve_unit_stops_to_release() {
	let u = add_unit("ac1", { pump: "p1", reserve: 1 });
	u.state = UNIT_STATE_RUNNING;
	u.refs  = 1;
	unit_stop("ac1");
	assert.eq(u.state, UNIT_STATE_RELEASE);
}

// =========================================================================
// 6. unit_mode: rvevery short-circuits — sets unit.mode and returns 0
//    without touching rvpin (caller is responsible per-cycle).
// =========================================================================
function test_mode_rvevery_short_circuits() {
	let u = add_unit("ac1", { pump: "p1", rvevery: true, rvpin: 7 });
	u.state = UNIT_STATE_STOPPED;
	u.mode  = AC_MODE_COOL;
	let r = unit_mode("ac1", u, AC_MODE_HEAT);
	assert.eq(r, 0);
	assert.eq(u.mode, AC_MODE_HEAT, "rvevery just records the mode");
	assert.eq(harness.pins[7], undefined, "rvpin must NOT be touched in rvevery path");
}

// =========================================================================
// 7. unit_mode while RUNNING and changing mode must error (unit must stop
//    first — compressor protection).
// =========================================================================
function test_mode_running_blocks_mode_change() {
	let u = add_unit("ac1", { pump: "p1", rvpin: 7 });
	u.state = UNIT_STATE_RUNNING;
	u.mode  = AC_MODE_COOL;
	let r = unit_mode("ac1", u, AC_MODE_HEAT);
	assert.eq(r, 1, "must reject mode change while RUNNING");
}

// =========================================================================
// 8. unit_mode while in_direct must error.
// =========================================================================
function test_mode_in_direct_blocks_mode_change() {
	let u = add_unit("ac1", { pump: "p1", rvpin: 7 });
	u.state = UNIT_STATE_STOPPED;
	u.mode  = AC_MODE_COOL;
	u.in_direct = true;
	let r = unit_mode("ac1", u, AC_MODE_HEAT);
	assert.eq(r, 1, "must reject mode change while in direct");
}

// =========================================================================
// 9. unit_mode COOL with rvcool=true → rvpin HIGH; HEAT with rvcool=true → LOW.
//    Also asserts the inverse (rvcool=false) flips the pin meaning.
// =========================================================================
function test_mode_cool_rvcool_true_drives_rvpin_high() {
	let u = add_unit("ac1", { pump: "p1", rvpin: 7, rvcool: true });
	u.state = UNIT_STATE_STOPPED;
	u.mode  = AC_MODE_HEAT;     // change to COOL
	harness.pins[7] = LOW;      // start at known LOW so set_pin actually writes
	let r = unit_mode("ac1", u, AC_MODE_COOL);
	assert.eq(r, 0);
	assert.eq(digitalRead(7), HIGH, "rvcool=true + COOL → rvpin HIGH");
	assert.eq(u.mode, AC_MODE_COOL);
}

function test_mode_heat_rvcool_true_drives_rvpin_low() {
	let u = add_unit("ac1", { pump: "p1", rvpin: 7, rvcool: true });
	u.state = UNIT_STATE_STOPPED;
	u.mode  = AC_MODE_COOL;     // change to HEAT
	harness.pins[7] = HIGH;     // start HIGH so set_pin will write LOW
	let r = unit_mode("ac1", u, AC_MODE_HEAT);
	assert.eq(r, 0);
	assert.eq(digitalRead(7), LOW, "rvcool=true + HEAT → rvpin LOW");
	assert.eq(u.mode, AC_MODE_HEAT);
}

function test_mode_cool_rvcool_false_drives_rvpin_low() {
	let u = add_unit("ac1", { pump: "p1", rvpin: 7, rvcool: false });
	u.state = UNIT_STATE_STOPPED;
	u.mode  = AC_MODE_HEAT;
	harness.pins[7] = HIGH;     // start HIGH so set_pin will write LOW
	let r = unit_mode("ac1", u, AC_MODE_COOL);
	assert.eq(r, 0);
	assert.eq(digitalRead(7), LOW, "rvcool=false + COOL → rvpin LOW (inverted)");
}

// =========================================================================
// 10. unit_off drives coolpin LOW when mode is COOL, heatpin LOW when HEAT.
// =========================================================================
function test_off_drives_coolpin_low_when_mode_cool() {
	let u = add_unit("ac1", { pump: "p1", coolpin: 10, heatpin: 11, rvpin: 12 });
	u.state = UNIT_STATE_RUNNING;
	u.refs  = 1;
	u.mode  = AC_MODE_COOL;
	harness.pins[10] = HIGH;
	unit_stop("ac1");
	assert.eq(harness.pins[10], LOW, "COOL stop must drive coolpin LOW");
}

function test_off_drives_heatpin_low_when_mode_heat() {
	let u = add_unit("ac1", { pump: "p1", coolpin: 10, heatpin: 11, rvpin: 12 });
	u.state = UNIT_STATE_RUNNING;
	u.refs  = 1;
	u.mode  = AC_MODE_HEAT;
	harness.pins[11] = HIGH;
	unit_stop("ac1");
	assert.eq(harness.pins[11], LOW, "HEAT stop must drive heatpin LOW");
}

// =========================================================================
// 11. unit_revoke(immediate=true) → unit_force_stop path.
// =========================================================================
function test_revoke_immediate_force_stops() {
	let u = add_unit("ac1", { pump: "p1" });
	u.state = UNIT_STATE_RUNNING;
	u.refs  = 4;
	let r = unit_revoke("ac1", 6500, "true");
	assert.eq(u.refs, 0);
	assert.called_with(harness.spies.pump_force_stop, ["p1"]);
}

function test_revoke_immediate_in_direct_calls_direct_off() {
	let u = add_unit("ac1", { pump: "p1" });
	u.state = UNIT_STATE_RUNNING;
	u.refs  = 1;
	u.in_direct = true;
	u.in_direct_group = "dg1";
	unit_revoke("ac1", 6500, "true");
	assert.called_with(harness.spies.direct_off, ["dg1"]);
}

// =========================================================================
// 12. unit_enable resets unit_state + charge_state.
// =========================================================================
function test_enable_resets_state() {
	let u = add_unit("ac1", { pump: "p1" });
	u.unit_state  = 99;
	u.charge_state = CHARGE_STATE_RUNNING;
	u.enabled = false;
	unit_enable("ac1");
	assert.eq(u.unit_state,  UNIT_STATE_STOPPED);
	assert.eq(u.charge_state, CHARGE_STATE_STOPPED);
}

harness.run();
