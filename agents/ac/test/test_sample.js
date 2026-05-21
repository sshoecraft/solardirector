#!/opt/sd/bin/sdjs -U512K
/*
 * Tests for agents/ac/sample.js — periodic temperature sampling state
 * machine and pump-selection logic.
 */

include(dirname(script_name) + "/harness.js");
include(dirname(script_name) + "/assert.js");
harness.init();

include(dirname(script_name) + "/../utils.js");
include(dirname(script_name) + "/../sample.js");
sample_init();

function add_pump_with_unit(pname, opts) {
	let p = harness.add_pump(pname, opts);
	p.direct = false;
	p.enabled = true;
	p.error = false;
	p.settled = false;
	p.temp_in_sensor = "func,readbme,0";
	if (opts) for (let k in opts) p[k] = opts[k];
	let u = harness.add_unit("u_" + pname, { pump: pname });
	return p;
}

function setup() {
	ac.sample_interval = 300;
	ac.sample_duration = 180;
	ac.sample_state = SAMPLE_STATE_STOPPED;
	ac.sample_pump  = "";
	ac.sample_time  = 0;
}

// =========================================================================
// 1. sample_interval < 1 → no-op.
// =========================================================================
function test_zero_interval_no_op() {
	ac.sample_interval = 0;
	add_pump_with_unit("p1");
	sample_main();
	assert.not_called(harness.spies.pump_start);
}

// =========================================================================
// 2. STOPPED state, diff < interval → nothing happens.
// =========================================================================
function test_stopped_within_interval_does_nothing() {
	ac.sample_state = SAMPLE_STATE_STOPPED;
	ac.sample_time  = harness.now - 100;   // 100 < 300
	add_pump_with_unit("p1");
	sample_main();
	assert.not_called(harness.spies.pump_start);
	assert.eq(ac.sample_state, SAMPLE_STATE_STOPPED);
}

// =========================================================================
// 3. STOPPED state, diff >= interval, valid pump → pump_start fires.
// =========================================================================
function test_stopped_past_interval_starts_pump() {
	ac.sample_state = SAMPLE_STATE_STOPPED;
	ac.sample_time  = harness.now - 400;
	add_pump_with_unit("p1");
	sample_main();
	assert.called_with(harness.spies.pump_start, ["p1"], "must start the selected pump");
	assert.eq(ac.sample_pump, "p1");
	assert.eq(ac.sample_state, SAMPLE_STATE_WAIT_PUMP);
}

// =========================================================================
// 4. STOPPED state, no eligible pumps → no_op (sample_pump stays empty).
// =========================================================================
function test_stopped_no_eligible_pump_no_op() {
	ac.sample_state = SAMPLE_STATE_STOPPED;
	ac.sample_time  = harness.now - 400;
	add_pump_with_unit("p1", { direct: true });   // skipped: direct
	sample_main();
	assert.not_called(harness.spies.pump_start);
	assert.eq(ac.sample_pump, "");
}

// =========================================================================
// 5. WAIT_PUMP: when pump reaches RUNNING → transition to RUNNING.
// =========================================================================
function test_wait_pump_to_running_when_pump_runs() {
	ac.sample_state = SAMPLE_STATE_WAIT_PUMP;
	ac.sample_pump  = "p1";
	let p = add_pump_with_unit("p1");
	p.state = PUMP_STATE_RUNNING;
	let before = harness.now;
	sample_main();
	assert.eq(ac.sample_state, SAMPLE_STATE_RUNNING);
	assert.eq(ac.sample_start_time, before, "start time captured at transition");
}

// =========================================================================
// 6. RUNNING: pump.settled → pump_stop, transition to STOPPED, cycle_time
//    updated, sample_pump cleared.
// =========================================================================
function test_running_pump_settled_stops_and_updates_cycle_time() {
	ac.sample_state = SAMPLE_STATE_RUNNING;
	ac.sample_pump  = "p1";
	ac.sample_start_time = harness.now - 50;
	let p = add_pump_with_unit("p1");
	p.settled = true;
	sample_main();
	assert.called_with(harness.spies.pump_stop, ["p1"]);
	assert.eq(ac.sample_state, SAMPLE_STATE_STOPPED);
	assert.eq(ac.sample_pump, "");
	assert.eq(p.cycle_time, harness.now, "cycle_time bumped so cycle.js doesn't re-fire immediately");
}

// =========================================================================
// 7. RUNNING: duration exceeded → also stops.
// =========================================================================
function test_running_duration_exceeded_stops() {
	ac.sample_state = SAMPLE_STATE_RUNNING;
	ac.sample_pump  = "p1";
	ac.sample_duration = 180;
	ac.sample_start_time = harness.now - 200;
	let p = add_pump_with_unit("p1");
	p.settled = false;
	sample_main();
	assert.called_with(harness.spies.pump_stop, ["p1"]);
	assert.eq(ac.sample_state, SAMPLE_STATE_STOPPED);
}

// =========================================================================
// 8. RUNNING: neither settled nor duration exceeded → stays.
// =========================================================================
function test_running_neither_condition_stays() {
	ac.sample_state = SAMPLE_STATE_RUNNING;
	ac.sample_pump  = "p1";
	ac.sample_duration = 180;
	ac.sample_start_time = harness.now - 60;
	let p = add_pump_with_unit("p1");
	p.settled = false;
	sample_main();
	assert.not_called(harness.spies.pump_stop);
	assert.eq(ac.sample_state, SAMPLE_STATE_RUNNING);
}

// =========================================================================
// 9. sample_select_pump skip rules.
// =========================================================================
function test_select_skips_direct_pump() {
	add_pump_with_unit("p1", { direct: true });
	sample_select_pump();
	assert.eq(ac.sample_pump, "");
}

function test_select_skips_disabled_pump() {
	add_pump_with_unit("p1", { enabled: false });
	sample_select_pump();
	assert.eq(ac.sample_pump, "");
}

function test_select_skips_error_pump() {
	add_pump_with_unit("p1", { error: true });
	sample_select_pump();
	assert.eq(ac.sample_pump, "");
}

function test_select_skips_pump_without_temp_sensor() {
	add_pump_with_unit("p1", { temp_in_sensor: "" });
	sample_select_pump();
	assert.eq(ac.sample_pump, "");
}

function test_select_picks_first_eligible() {
	add_pump_with_unit("p1", { direct: true });           // skipped
	add_pump_with_unit("p2");                             // eligible
	sample_select_pump();
	assert.eq(ac.sample_pump, "p2");
}

harness.run();
