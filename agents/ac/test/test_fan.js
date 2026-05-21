#!/opt/sd/bin/sdjs -U512K
/*
 * Tests for agents/ac/fan.js — refcount, force_stop, mode transitions,
 * pump-stop chain, direct-mode handoff, water-depleted fallback.
 */

include(dirname(script_name) + "/harness.js");
include(dirname(script_name) + "/assert.js");
harness.init();

include(dirname(script_name) + "/../utils.js");
include(dirname(script_name) + "/../common.js");
include(dirname(script_name) + "/../fan.js");
fan_init();

function add_fan(name, opts) {
	let f = harness.add_fan(name, opts);
	f.refs = 0;
	f.state = FAN_STATE_STOPPED;
	f.error = false;
	f.mode = FAN_MODE_NONE;
	f.fan_state = "off";
	f.heat_state = "off";
	f.cool_state = "off";
	f.direct_group = "";
	f.stop_wt = true;
	f.wt_thresh = 3;
	f.reserve = 0;
	f.cooldown = 30;
	f.pump = "";
	if (opts) for (let k in opts) f[k] = opts[k];
	return f;
}

// =========================================================================
// 1. Refcount: two starts → refs=2; one stop → refs=1; second stop → refs=0
//    triggers fan_off.
// =========================================================================
function test_refcount_two_starts_one_stop() {
	let f = add_fan("ah1", { pump: "ac1" });
	fan_start("ah1");
	fan_start("ah1");
	assert.eq(f.refs, 2, "two starts → refs=2");
	fan_stop("ah1");
	assert.eq(f.refs, 1, "one stop → refs=1, off not yet called");
	assert.not_called(harness.spies.pump_stop, "pump_stop must not fire while refs>0");
}

function test_refcount_final_stop_calls_pump_stop() {
	let f = add_fan("ah1", { pump: "ac1" });
	f.state = FAN_STATE_RUNNING;
	f.refs  = 1;
	fan_stop("ah1");
	assert.eq(f.refs, 0, "final stop → refs=0");
	assert.called_with(harness.spies.pump_stop, ["ac1"], "fan_off must stop the fan's pump");
}

// =========================================================================
// 2. Force-stop bypasses refcount and force-stops the pump too.
// =========================================================================
function test_force_stop_calls_pump_force_stop() {
	let f = add_fan("ah1", { pump: "ac1" });
	f.state = FAN_STATE_RUNNING;
	f.refs  = 5;
	fan_force_stop("ah1");
	assert.eq(f.refs, 0, "force_stop zeros refs");
	assert.called_with(harness.spies.pump_force_stop, ["ac1"], "fan_force_stop must force_stop the pump");
}

function test_force_stop_no_pump_skips_pump_stop() {
	let f = add_fan("ah1", { pump: "" });
	f.state = FAN_STATE_RUNNING;
	f.refs  = 1;
	fan_force_stop("ah1");
	assert.not_called(harness.spies.pump_force_stop, "no pump → no pump_force_stop call");
}

// =========================================================================
// 3. Reserve fan stops to RELEASE; non-reserve stops to STOPPED.
// =========================================================================
function test_reserve_fan_stops_to_release() {
	let f = add_fan("ah1", { pump: "ac1", reserve: 1 });
	f.state = FAN_STATE_RUNNING;
	f.refs  = 1;
	fan_stop("ah1");
	assert.eq(f.state, FAN_STATE_RELEASE, "reserve fan goes to RELEASE on stop");
}

function test_nonreserve_fan_stops_to_stopped() {
	let f = add_fan("ah1", { pump: "ac1", reserve: 0 });
	f.state = FAN_STATE_RUNNING;
	f.refs  = 1;
	fan_stop("ah1");
	assert.eq(f.state, FAN_STATE_STOPPED, "non-reserve fan goes to STOPPED");
}

// =========================================================================
// 4. fan_set_mode(NONE) stops the fan.
// =========================================================================
function test_set_mode_none_stops_fan() {
	let f = add_fan("ah1", { pump: "ac1" });
	f.state = FAN_STATE_RUNNING;
	f.mode  = FAN_MODE_COOL;
	f.refs  = 1;
	harness.add_pump("ac1");
	fan_set_mode("ah1", FAN_MODE_NONE);
	assert.called_with(harness.spies.pump_stop, ["ac1"], "set_mode NONE must stop pump");
	assert.eq(f.mode, FAN_MODE_NONE);
}

// =========================================================================
// 5. fan_set_mode while fan already RUNNING just updates the mode field
//    (no fan restart, no direct handoff — see fan.js:88-99).
// =========================================================================
function test_set_mode_while_running_updates_mode_only() {
	let f = add_fan("ah1", { pump: "ac1" });
	f.state = FAN_STATE_RUNNING;
	f.mode  = FAN_MODE_COOL;
	f.refs  = 1;
	fan_set_mode("ah1", FAN_MODE_HEAT);
	assert.eq(f.mode, FAN_MODE_HEAT, "mode field updated");
	assert.not_called(harness.spies.pump_stop);
	assert.not_called(harness.spies.direct_on);
}

// =========================================================================
// 6. fan_set_mode while STOPPED, no mode mismatch, no water depletion →
//    starts the fan normally (no direct).
// =========================================================================
function test_set_mode_normal_start_no_direct() {
	let f = add_fan("ah1", { pump: "ac1" });
	ac.mode = AC_MODE_COOL;
	ac.water_temp = 50;       // valid + within bounds
	ac.cool_high_temp = 60;
	ac.heat_low_temp  = 100;
	f.state = FAN_STATE_STOPPED;
	fan_set_mode("ah1", FAN_MODE_COOL);
	assert.eq(f.mode, FAN_MODE_COOL, "mode set");
	assert.eq(f.refs, 1, "fan_start increments refs");
	assert.not_called(harness.spies.direct_on, "no direct handoff when mode matches and water sufficient");
}

// =========================================================================
// 7. Mode mismatch (asking for COOL when ac.mode=HEAT) without direct_group →
//    error.
// =========================================================================
function test_set_mode_mismatch_no_direct_group_errors() {
	let f = add_fan("ah1", { pump: "ac1" });
	ac.mode = AC_MODE_HEAT;
	ac.water_temp = 100;
	f.state = FAN_STATE_STOPPED;
	f.direct_group = "";
	let r = fan_set_mode("ah1", FAN_MODE_COOL);
	assert.eq(r, 1, "must return error");
	assert.truthy(errors.list.length > 0, "must record an error");
}

// =========================================================================
// 8. Mode mismatch with a configured direct_group → calls direct_on.
// =========================================================================
function test_set_mode_mismatch_with_direct_group_calls_direct_on() {
	let f = add_fan("ah1", { pump: "ac1", direct_group: "dg1" });
	directs["dg1"] = { enabled: true, fan: "ah1" };
	ac.mode = AC_MODE_HEAT;
	ac.water_temp = 100;
	f.state = FAN_STATE_STOPPED;
	let r = fan_set_mode("ah1", FAN_MODE_COOL);
	assert.eq(r, 0);
	assert.called_with(harness.spies.direct_on, ["dg1", "cool"], "direct_on called with group + mode string");
	assert.eq(f.mode, FAN_MODE_COOL, "fan mode is recorded");
}

// =========================================================================
// 9. Water depleted (ac.water_temp >= cool_high + wt_thresh + 0.5) when
//    cooling triggers direct mode.
// =========================================================================
function test_set_mode_water_depleted_triggers_direct() {
	let f = add_fan("ah1", { pump: "ac1", direct_group: "dg1", stop_wt: true, wt_thresh: 3 });
	directs["dg1"] = { enabled: true, fan: "ah1" };
	ac.mode = AC_MODE_COOL;
	ac.cool_high_temp = 60;
	ac.water_temp = 64;       // 64 >= 60 + 3 + 0.5 → depleted
	f.state = FAN_STATE_STOPPED;
	let r = fan_set_mode("ah1", FAN_MODE_COOL);
	assert.eq(r, 0);
	assert.called_with(harness.spies.direct_on, ["dg1", "cool"]);
}

// =========================================================================
// 10. Water depleted but direct_group disabled → falls back to storage mode
//     (normal start, no direct call).
// =========================================================================
function test_water_depleted_with_disabled_direct_falls_back() {
	let f = add_fan("ah1", { pump: "ac1", direct_group: "dg1", stop_wt: true, wt_thresh: 3 });
	directs["dg1"] = { enabled: false, fan: "ah1" };
	ac.mode = AC_MODE_COOL;
	ac.cool_high_temp = 60;
	ac.water_temp = 64;       // depleted
	f.state = FAN_STATE_STOPPED;
	let r = fan_set_mode("ah1", FAN_MODE_COOL);
	assert.eq(r, 0);
	assert.not_called(harness.spies.direct_on, "direct disabled → no direct_on");
	assert.eq(f.refs, 1, "fan starts normally");
}

// =========================================================================
// 11. fan_revoke(immediate=true) → fan_force_stop path.
// =========================================================================
function test_revoke_immediate_force_stops() {
	let f = add_fan("ah1", { pump: "ac1" });
	f.state = FAN_STATE_RUNNING;
	f.refs  = 3;
	fan_revoke("ah1", 1300, "true");
	assert.eq(f.refs, 0);
	assert.called_with(harness.spies.pump_force_stop, ["ac1"], "immediate revoke must force_stop the pump");
}

function test_revoke_non_immediate_uses_stop() {
	let f = add_fan("ah1", { pump: "ac1" });
	f.state = FAN_STATE_RUNNING;
	f.refs  = 3;
	fan_revoke("ah1", 1300, "false");
	assert.eq(f.refs, 0, "revoke pulls refs to 1 then stops → 0");
	assert.called_with(harness.spies.pump_stop, ["ac1"], "non-immediate revoke uses pump_stop");
	assert.not_called(harness.spies.pump_force_stop);
}

// =========================================================================
// 12. fan_enable resets fan/heat/cool state strings to "off" and mode to NONE.
// =========================================================================
function test_enable_resets_state_strings() {
	let f = add_fan("ah1", { pump: "ac1" });
	f.fan_state = f.heat_state = f.cool_state = "on";
	f.mode = FAN_MODE_HEAT;
	f.enabled = false;
	fan_enable("ah1");
	assert.eq(f.fan_state,  "off");
	assert.eq(f.heat_state, "off");
	assert.eq(f.cool_state, "off");
	assert.eq(f.mode, FAN_MODE_NONE);
}

harness.run();
