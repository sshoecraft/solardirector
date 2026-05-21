#!/opt/sd/bin/sdjs -U512K
/*
 * Tests for agents/ac/cycle.js — pump anti-freeze cycling logic.
 *
 * Coverage focus (from the 2026-04-25 incident):
 *   - The 8-hour stuck-running bug: when ambient temp recovers above
 *     cycle_start, any pump still in CYCLE_STATE_RUNNING must be stopped.
 *   - Continuous-mode behavior when temp is well below cycle_start
 *     (off-time would be < 2x on-time → don't churn the pumps).
 *   - Normal cycling at mild cold.
 *   - Sentinel/skip cases (invalid temp, disabled pump, direct pump, error pump).
 */

include(dirname(script_name) + "/harness.js");
include(dirname(script_name) + "/assert.js");
harness.init();

// Real utility helpers (is_valid_temp, get_sensor, set_pin)
include(dirname(script_name) + "/../utils.js");

// Module under test
include(dirname(script_name) + "/../cycle.js");

// Initialize cycle.js (defines CYCLE_STATE_*, registers ac config props)
cycle_init();

// ----- shared setup for every test -----
function setup() {
	// Drive cycle_main() through the BME-backed temp sensor by default.
	// Tests can either change harness.bme.temp (sensor-driven path) or set
	// ac.temp directly with ac.temp_sensor = "" (skip-sensor path).
	ac.cycle_start    = 30;
	ac.cycle_interval = 1800;
	ac.cycle_duration = 180;
	ac.temp_sensor    = "";    // default: don't override ac.temp via sensor
	ac.temp           = 70;    // safe default — well above threshold
}

// ===========================================================================
// 1. Invalid temp → no action
// ===========================================================================
function test_invalid_temp_does_nothing() {
	ac.temp = INVALID_TEMP;
	let p = harness.add_pump("ac1");
	cycle_main();
	assert.not_called(harness.spies.pump_start, "must not start on invalid temp");
	assert.not_called(harness.spies.pump_stop,  "must not stop on invalid temp");
}

// ===========================================================================
// 2. THE REGRESSION: temp recovery must stop pumps stuck in cycle's RUNNING.
//    Last night's bug: cycle_main() returned at the threshold check without
//    stopping pumps that were still in CYCLE_STATE_RUNNING. Fix: explicitly
//    stop them on recovery.
// ===========================================================================
function test_temp_recovery_stops_running_pump() {
	ac.temp = 35;   // recovered above cycle_start (30)
	let p = harness.add_pump("ac1", { state: PUMP_STATE_RUNNING });
	p.cycle_state       = CYCLE_STATE_RUNNING;
	p.cycle_start_time  = harness.now - 200;
	cycle_main();
	assert.called_with(harness.spies.pump_stop, ["ac1"], "must stop cycling pump on recovery");
	assert.eq(p.cycle_state, CYCLE_STATE_STOPPED, "cycle_state must reset after recovery-stop");
}

// Recovery must also clean up pumps mid-startup (WAIT_PUMP).
function test_temp_recovery_stops_wait_pump() {
	ac.temp = 35;
	let p = harness.add_pump("ac1", { state: PUMP_STATE_WAIT_PUMP });
	p.cycle_state = CYCLE_STATE_WAIT_PUMP;
	cycle_main();
	assert.called_with(harness.spies.pump_stop, ["ac1"], "must stop pump in WAIT cycle state on recovery");
	assert.eq(p.cycle_state, CYCLE_STATE_STOPPED);
}

// ===========================================================================
// 3. Recovery must NOT touch a pump that's already cleanly STOPPED
//    in cycle_state — leaves user-controlled pumps alone.
// ===========================================================================
function test_temp_above_threshold_leaves_stopped_pump_alone() {
	ac.temp = 35;
	let p = harness.add_pump("ac1");
	p.cycle_state = CYCLE_STATE_STOPPED;
	cycle_main();
	assert.not_called(harness.spies.pump_stop, "STOPPED pump must not be touched");
	assert.not_called(harness.spies.pump_start);
}

// ===========================================================================
// 4. Normal cycling at mild cold (20°F): start, then stop after cycle_duration.
// ===========================================================================
function test_normal_cycle_at_20F_starts_pump() {
	ac.temp = 20;          // m=0.667, scaled_interval=1200 (>= 2*180), continuous=false
	let p = harness.add_pump("ac1");
	p.cycle_state = CYCLE_STATE_STOPPED;
	p.cycle_time  = 0;     // last cycle was long ago — eligible to start
	cycle_main();
	assert.called_with(harness.spies.pump_start, ["ac1"], "must start pump at 20F when off-time elapsed");
	assert.eq(p.cycle_state, CYCLE_STATE_WAIT_PUMP);
}

function test_normal_cycle_at_20F_stops_after_duration() {
	ac.temp = 20;
	let p = harness.add_pump("ac1", { state: PUMP_STATE_RUNNING });
	p.cycle_state       = CYCLE_STATE_RUNNING;
	p.cycle_start_time  = harness.now - 181;   // just past cycle_duration
	cycle_main();
	assert.called_with(harness.spies.pump_stop, ["ac1"], "must stop pump after cycle_duration at mild cold");
	assert.eq(p.cycle_state, CYCLE_STATE_STOPPED);
}

// Pump in RUNNING but cycle_duration not yet reached → no stop.
function test_normal_cycle_at_20F_keeps_running_during_duration() {
	ac.temp = 20;
	let p = harness.add_pump("ac1", { state: PUMP_STATE_RUNNING });
	p.cycle_state       = CYCLE_STATE_RUNNING;
	p.cycle_start_time  = harness.now - 60;    // only 60s in
	cycle_main();
	assert.not_called(harness.spies.pump_stop, "must not stop before cycle_duration elapsed");
}

// ===========================================================================
// 5. Continuous mode when very cold (below ~6F crossover):
//    cycle_interval scaled below cycle_duration*2 → no stop after duration.
// ===========================================================================
function test_continuous_below_6F_does_not_stop() {
	ac.temp = 0;          // m=0, scaled_interval=0, continuous=true
	let p = harness.add_pump("ac1", { state: PUMP_STATE_RUNNING });
	p.cycle_state       = CYCLE_STATE_RUNNING;
	p.cycle_start_time  = harness.now - 1000;  // far past duration
	cycle_main();
	assert.not_called(harness.spies.pump_stop, "0F must run continuously, no churn");
}

function test_continuous_at_negative_temp_does_not_stop() {
	ac.temp = -20;
	let p = harness.add_pump("ac1", { state: PUMP_STATE_RUNNING });
	p.cycle_state       = CYCLE_STATE_RUNNING;
	p.cycle_start_time  = harness.now - 5000;
	cycle_main();
	assert.not_called(harness.spies.pump_stop, "negative temp must run continuously");
}

// At ~6F the off-time crosses cycle_duration*2 (=360). Just above the
// crossover (e.g. 7F: m=0.233, off=420 > 360) it's still cycling.
function test_normal_cycle_at_7F_still_cycles() {
	ac.temp = 7;
	let p = harness.add_pump("ac1", { state: PUMP_STATE_RUNNING });
	p.cycle_state       = CYCLE_STATE_RUNNING;
	p.cycle_start_time  = harness.now - 200;
	cycle_main();
	assert.called_with(harness.spies.pump_stop, ["ac1"], "at 7F still in cycling band — must stop after duration");
}

// Just below crossover (e.g. 5F: m=0.167, off=300 < 360) → continuous.
function test_continuous_at_5F() {
	ac.temp = 5;
	let p = harness.add_pump("ac1", { state: PUMP_STATE_RUNNING });
	p.cycle_state       = CYCLE_STATE_RUNNING;
	p.cycle_start_time  = harness.now - 1000;
	cycle_main();
	assert.not_called(harness.spies.pump_stop, "5F should be in continuous band");
}

// ===========================================================================
// 6. Skip cases: pumps that should never be touched by cycle_main.
// ===========================================================================
function test_disabled_pump_skipped() {
	ac.temp = 10;
	let p = harness.add_pump("ac1", { enabled: false });
	p.cycle_state = CYCLE_STATE_STOPPED;
	p.cycle_time  = 0;
	cycle_main();
	assert.not_called(harness.spies.pump_start, "disabled pump must not be started");
}

function test_direct_pump_skipped() {
	ac.temp = 10;
	let p = harness.add_pump("ac1", { direct: true });
	p.cycle_state = CYCLE_STATE_STOPPED;
	p.cycle_time  = 0;
	cycle_main();
	assert.not_called(harness.spies.pump_start, "direct pump must not be cycled");
}

function test_pump_in_error_skipped() {
	ac.temp = 10;
	let p = harness.add_pump("ac1", { error: true });
	p.cycle_state = CYCLE_STATE_STOPPED;
	p.cycle_time  = 0;
	cycle_main();
	assert.not_called(harness.spies.pump_start, "error-flagged pump must not be cycled");
}

// ===========================================================================
// 7. Multi-pump recovery: all five pumps stopped on temp recovery.
// ===========================================================================
function test_recovery_with_multiple_pumps() {
	ac.temp = 35;
	let names = ["primer", "ac1", "ac2", "ah1", "ah2"];
	for (let i = 0; i < names.length; i++) {
		let p = harness.add_pump(names[i], { state: PUMP_STATE_RUNNING });
		p.cycle_state      = CYCLE_STATE_RUNNING;
		p.cycle_start_time = harness.now - 300;
	}
	cycle_main();
	assert.call_count(harness.spies.pump_stop, 5, "all 5 pumps should be stopped on recovery");
	for (let i = 0; i < names.length; i++) {
		assert.eq(pumps[names[i]].cycle_state, CYCLE_STATE_STOPPED);
	}
}

harness.run();
