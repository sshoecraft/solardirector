#!/opt/sd/bin/sdjs -U512K
/*
 * Tests for agents/ac/mode.js — manual mode set, auto-mode threshold logic,
 * mode-change trigger firing, weighted-average decision boundary.
 *
 * get_wt() reaches into influxdb across 3 days; we override it post-include
 * to return harness.fake_wt so we test mode_auto's decision logic in isolation.
 */

include(dirname(script_name) + "/harness.js");
include(dirname(script_name) + "/assert.js");
harness.init();

include(dirname(script_name) + "/../utils.js");
include(dirname(script_name) + "/../mode.js");
mode_init();

// Override get_wt so we can drive mode_auto deterministically.
harness.fake_wt = INVALID_TEMP;
get_wt = function() { return harness.fake_wt; };

function setup() {
	ac.mode_threshold = 60;
	ac.mode = AC_MODE_NONE;
	harness.fake_wt = INVALID_TEMP;
}

// =========================================================================
// 1. mode_set("cool") / "heat" — direct mode set.
// =========================================================================
function test_mode_set_cool() {
	let r = mode_set("cool");
	assert.eq(r, 0);
	assert.eq(ac.mode, AC_MODE_COOL);
}

function test_mode_set_heat() {
	let r = mode_set("heat");
	assert.eq(r, 0);
	assert.eq(ac.mode, AC_MODE_HEAT);
}

function test_mode_set_case_insensitive() {
	let r = mode_set("HEAT");
	assert.eq(r, 0);
	assert.eq(ac.mode, AC_MODE_HEAT);
}

// =========================================================================
// 2. mode_set("invalid") returns 1 with an errmsg.
// =========================================================================
function test_mode_set_unknown_returns_error() {
	let r = mode_set("warm");
	assert.eq(r, 1);
	assert.truthy(config.errmsg.indexOf("unknown mode") >= 0);
}

// =========================================================================
// 3. mode_set("auto") consults get_wt and threshold.
// =========================================================================
function test_mode_set_auto_below_threshold_picks_heat() {
	ac.mode_threshold = 60;
	harness.fake_wt   = 50;   // below threshold → HEAT
	mode_set("auto");
	assert.eq(ac.mode, AC_MODE_HEAT);
}

function test_mode_set_auto_above_threshold_picks_cool() {
	ac.mode_threshold = 60;
	harness.fake_wt   = 75;
	mode_set("auto");
	assert.eq(ac.mode, AC_MODE_COOL);
}

// Boundary: wt == threshold → HEAT (per `wt <= threshold` in mode_auto).
function test_mode_set_auto_at_threshold_picks_heat() {
	ac.mode_threshold = 60;
	harness.fake_wt   = 60;
	mode_set("auto");
	assert.eq(ac.mode, AC_MODE_HEAT);
}

// =========================================================================
// 4. mode_auto with INVALID_TEMP (sensor unavailable) defaults to COOL.
// =========================================================================
function test_mode_auto_invalid_wt_defaults_cool() {
	ac.mode = AC_MODE_HEAT;
	harness.fake_wt = INVALID_TEMP;
	mode_auto();
	assert.eq(ac.mode, AC_MODE_COOL, "invalid wt → safe default of COOL");
}

// =========================================================================
// 5. mode_set_trigger fires ac.signal("Mode","Set") only when value changes.
// =========================================================================
function test_mode_trigger_fires_signal_on_change() {
	harness.signal_calls = [];
	let prop = { value: AC_MODE_HEAT };
	mode_set_trigger("ac", prop, AC_MODE_COOL);
	let found = false;
	for (let i = 0; i < harness.signal_calls.length; i++) {
		let c = harness.signal_calls[i];
		if (c[0] === "Mode" && c[1] === "Set") { found = true; break; }
	}
	assert.truthy(found, "trigger must fire ac.signal('Mode','Set') on value change");
}

function test_mode_trigger_no_signal_on_first_set() {
	harness.signal_calls = [];
	let prop = { value: AC_MODE_HEAT };
	mode_set_trigger("ac", prop, undefined);
	assert.eq(harness.signal_calls.length, 0, "no signal when old_val is undefined (first set)");
}

function test_mode_trigger_no_signal_on_unchanged_value() {
	harness.signal_calls = [];
	let prop = { value: AC_MODE_COOL };
	mode_set_trigger("ac", prop, AC_MODE_COOL);
	assert.eq(harness.signal_calls.length, 0, "no signal when value unchanged");
}

// =========================================================================
// 6. ac_modestr returns expected names.
// =========================================================================
function test_ac_modestr_names() {
	assert.eq(ac_modestr(AC_MODE_NONE), "None");
	assert.eq(ac_modestr(AC_MODE_COOL), "Cool");
	assert.eq(ac_modestr(AC_MODE_HEAT), "Heat");
	assert.truthy(ac_modestr(99).indexOf("Unknown") >= 0);
}

// =========================================================================
// 7. mode_init sets the AC_MODE_* enum values to 0/1/2 in order.
// =========================================================================
function test_mode_init_enum_values() {
	assert.eq(AC_MODE_NONE, 0);
	assert.eq(AC_MODE_COOL, 1);
	assert.eq(AC_MODE_HEAT, 2);
}

harness.run();
