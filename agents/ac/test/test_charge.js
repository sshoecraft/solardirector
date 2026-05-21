#!/opt/sd/bin/sdjs -U512K
/*
 * Tests for agents/ac/charge.js — water-temperature-driven charge state
 * machine, do_start/do_stop boundary logic, charge priority math,
 * unit-skip conditions (in_direct / disabled / error).
 */

include(dirname(script_name) + "/harness.js");
include(dirname(script_name) + "/assert.js");
harness.init();

include(dirname(script_name) + "/../utils.js");
include(dirname(script_name) + "/../charge.js");
charge_init();

function add_unit(name, opts) {
	let u = harness.add_unit(name, opts);
	u.state = UNIT_STATE_STOPPED;
	u.enabled = true;
	u.error = false;
	u.in_direct = false;
	u.in_direct_pending = false;
	u.charging = false;
	u.charge_state = CHARGE_STATE_STOPPED;
	u.charge_priority = 0;
	u.priority = 100;
	u.reserve = 0;
	u.mode = AC_MODE_COOL;
	u.pump = "";
	u.last_pri_temp = INVALID_TEMP;
	if (opts) for (let k in opts) u[k] = opts[k];
	return u;
}

function setup() {
	ac.mode = AC_MODE_COOL;
	ac.cool_low_temp   = 35;
	ac.cool_high_temp  = 60;
	ac.heat_low_temp   = 100;
	ac.heat_high_temp  = 125;
	ac.charge_threshold = 3;
	ac.repri_interval  = 1.0;
	ac.water_temp      = 50;
	ac.interval        = 10;
}

// =========================================================================
// 1. Invalid water_temp → no-op.
// =========================================================================
function test_invalid_water_temp_does_nothing() {
	ac.water_temp = INVALID_TEMP;
	let u = add_unit("ac1", { pump: "p1" });
	charge_main();
	assert.eq(u.charge_state, CHARGE_STATE_STOPPED);
	assert.not_called(harness.spies.unit_start);
}

// =========================================================================
// 2. do_start path (COOL mode, water_temp >= cool_low + threshold) →
//    transitions STOPPED → START → WAIT_START via fall-through, calls
//    unit_start.
// =========================================================================
function test_cool_above_low_plus_threshold_starts_unit() {
	ac.mode = AC_MODE_COOL;
	ac.cool_low_temp = 35;
	ac.charge_threshold = 3;
	ac.water_temp = 40;        // 40 >= 35+3
	let u = add_unit("ac1", { pump: "p1" });
	charge_main();
	assert.called_with(harness.spies.unit_start, ["ac1"]);
	assert.eq(u.charge_state, CHARGE_STATE_WAIT_START);
	assert.eq(u.charging, true);
	assert.eq(u.last_pri_temp, ac.water_temp, "last_pri_temp captured at start");
}

// =========================================================================
// 3. HEAT mode equivalent: water_temp <= heat_high - threshold → start.
// =========================================================================
function test_heat_below_high_minus_threshold_starts_unit() {
	ac.mode = AC_MODE_HEAT;
	ac.heat_high_temp = 125;
	ac.charge_threshold = 3;
	ac.water_temp = 120;       // 120 <= 125-3
	let u = add_unit("ac1", { pump: "p1" });
	u.mode = AC_MODE_HEAT;
	charge_main();
	assert.called_with(harness.spies.unit_start, ["ac1"]);
	assert.eq(u.charge_state, CHARGE_STATE_WAIT_START);
}

// =========================================================================
// 4. Below do_start threshold but in band → no start.
//    (cool_low < water < cool_low+threshold)
// =========================================================================
function test_cool_in_band_no_start() {
	ac.mode = AC_MODE_COOL;
	ac.cool_low_temp = 35;
	ac.charge_threshold = 3;
	ac.water_temp = 36;        // in band (35 < 36 < 38), neither start nor stop
	let u = add_unit("ac1", { pump: "p1" });
	charge_main();
	assert.not_called(harness.spies.unit_start);
	assert.eq(u.charge_state, CHARGE_STATE_STOPPED);
}

// =========================================================================
// 5. do_stop path: water_temp <= cool_low (COOL) stops a running charge cycle.
// =========================================================================
function test_cool_at_low_stops_charging_unit() {
	ac.mode = AC_MODE_COOL;
	ac.cool_low_temp = 35;
	ac.water_temp = 30;        // 30 <= cool_low
	let u = add_unit("ac1", { pump: "p1" });
	u.charge_state = CHARGE_STATE_RUNNING;
	u.charging = true;
	u.state = UNIT_STATE_RUNNING;
	charge_main();
	assert.called_with(harness.spies.unit_stop, ["ac1"]);
	assert.eq(u.charge_state, CHARGE_STATE_STOPPED);
	assert.eq(u.charging, false);
}

// =========================================================================
// 6. Unit in direct mode is skipped (charging cleared, no unit_start).
// =========================================================================
function test_unit_in_direct_skipped() {
	ac.mode = AC_MODE_COOL;
	ac.water_temp = 50;        // would normally start
	let u = add_unit("ac1", { pump: "p1" });
	u.in_direct = true;
	u.charging = true;
	u.charge_state = CHARGE_STATE_RUNNING;
	charge_main();
	assert.not_called(harness.spies.unit_start);
	assert.eq(u.charge_state, CHARGE_STATE_STOPPED, "direct unit reset to STOPPED");
	assert.eq(u.charging, false);
}

function test_unit_in_direct_pending_skipped() {
	ac.water_temp = 50;
	let u = add_unit("ac1", { pump: "p1" });
	u.in_direct_pending = true;
	charge_main();
	assert.not_called(harness.spies.unit_start);
}

function test_disabled_unit_skipped() {
	ac.water_temp = 50;
	let u = add_unit("ac1", { pump: "p1", enabled: false });
	charge_main();
	assert.not_called(harness.spies.unit_start);
}

function test_error_unit_skipped() {
	ac.water_temp = 50;
	let u = add_unit("ac1", { pump: "p1", error: true });
	charge_main();
	assert.not_called(harness.spies.unit_start);
}

// =========================================================================
// 7. WAIT_START: unit reaches RUNNING → transitions to RUNNING and updates
//    last_pri_temp.
// =========================================================================
function test_wait_start_promotes_to_running_when_unit_runs() {
	ac.water_temp = 50;
	let u = add_unit("ac1", { pump: "p1" });
	u.charge_state = CHARGE_STATE_WAIT_START;
	u.state = UNIT_STATE_RUNNING;
	u.last_pri_temp = INVALID_TEMP;
	charge_main();
	assert.eq(u.charge_state, CHARGE_STATE_RUNNING);
	assert.eq(u.last_pri_temp, ac.water_temp, "last_pri_temp pinned on entry to RUNNING");
}

// =========================================================================
// 8. WAIT_START: unit not yet running → stays in WAIT_START.
// =========================================================================
function test_wait_start_stays_until_unit_running() {
	ac.water_temp = 50;
	let u = add_unit("ac1", { pump: "p1" });
	u.charge_state = CHARGE_STATE_WAIT_START;
	u.state = UNIT_STATE_RESERVE;
	charge_main();
	assert.eq(u.charge_state, CHARGE_STATE_WAIT_START);
}

// =========================================================================
// 9. charge_get_pri: COOL math.
//    val = (cool_high - water_temp) / (cool_high - cool_low)
//    pri = ceil(val * 100 + 1), clamped to [1, 100].
// =========================================================================
function test_charge_pri_cool_at_high() {
	ac.mode = AC_MODE_COOL;
	ac.cool_high_temp = 60;
	ac.cool_low_temp  = 35;
	ac.water_temp     = 60;        // val = 0 → pri = ceil(0+1) = 1
	assert.eq(charge_get_pri(), 1);
}

function test_charge_pri_cool_at_low() {
	ac.mode = AC_MODE_COOL;
	ac.cool_high_temp = 60;
	ac.cool_low_temp  = 35;
	ac.water_temp     = 35;        // val = 1 → pri = ceil(101) = 101 → clamped 100
	assert.eq(charge_get_pri(), 100);
}

function test_charge_pri_cool_midpoint() {
	ac.mode = AC_MODE_COOL;
	ac.cool_high_temp = 60;
	ac.cool_low_temp  = 35;
	ac.water_temp     = 47.5;      // val = 0.5 → pri = ceil(51) = 51
	assert.eq(charge_get_pri(), 51);
}

// HEAT math: val = (water_temp - heat_low) / (heat_high - heat_low)
function test_charge_pri_heat_at_low() {
	ac.mode = AC_MODE_HEAT;
	ac.heat_high_temp = 125;
	ac.heat_low_temp  = 100;
	ac.water_temp     = 100;
	assert.eq(charge_get_pri(), 1);
}

function test_charge_pri_heat_at_high() {
	ac.mode = AC_MODE_HEAT;
	ac.heat_high_temp = 125;
	ac.heat_low_temp  = 100;
	ac.water_temp     = 125;
	assert.eq(charge_get_pri(), 100);
}

harness.run();
