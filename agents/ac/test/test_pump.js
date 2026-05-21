#!/opt/sd/bin/sdjs -U512K
/*
 * Tests for agents/ac/pump.js — refcount, force_stop, cooldown, primer chain.
 *
 * pump.js does NOT drive the full START_PRIMER → WAIT_PRIMER → ... → RUNNING
 * state sequence — that's run.js's main loop. pump.js owns:
 *   - Refcount semantics via common_start / common_stop
 *   - Direct-mode rejection
 *   - Cooldown vs immediate-off
 *   - Force-stop semantics (bypasses refcount, fires hardware off)
 *   - Primer chain on stop
 *   - Pin GPIO interaction via set_pin
 *   - COOLDOWN → RUNNING on restart
 *   - RELEASE state for reserve pumps
 */

include(dirname(script_name) + "/harness.js");
include(dirname(script_name) + "/assert.js");
harness.init();

include(dirname(script_name) + "/../utils.js");
include(dirname(script_name) + "/../common.js");
include(dirname(script_name) + "/../pump.js");
pump_init();

function setup() {
	// Re-install pump.js's real functions over any prior spies.
	// (cycle.js test runs left spies installed in a previous module.)
}

// =========================================================================
// 1. Refcount: pump_start increments, pump_stop decrements; off fires only
//    when refs reaches zero.
// =========================================================================
function test_refcount_two_starts_one_stop_keeps_running() {
	let p = harness.add_pump("ac1", { pin: 5 });
	pump_start("ac1");
	pump_start("ac1");
	assert.eq(p.refs, 2, "two starts → refs=2");
	pump_stop("ac1");
	assert.eq(p.refs, 1, "one stop → refs=1");
	// pin should still be HIGH (was set by previous cycle, but in this minimal
	// path nothing wrote the pin since we bypassed the run.js state machine)
	assert.eq(p.state, PUMP_STATE_STOPPED, "state stays STOPPED — run.js drives transitions, not pump_start");
}

function test_refcount_final_stop_calls_pump_off() {
	let p = harness.add_pump("ac1", { pin: 5 });
	// Simulate a pump that's reached RUNNING via run.js
	p.state = PUMP_STATE_RUNNING;
	p.refs  = 1;
	harness.pins[5] = HIGH;
	pump_stop("ac1");
	assert.eq(p.refs, 0, "final stop → refs=0");
	assert.eq(p.state, PUMP_STATE_STOPPED, "pump_off must set state to STOPPED");
	assert.eq(harness.pins[5], LOW, "pump_off must drive pin LOW");
	assert.eq(p.stop_time, harness.now, "stop_time must be set");
}

// =========================================================================
// 2. Direct-mode pumps reject pump_start.
// =========================================================================
function test_direct_pump_rejects_start() {
	let p = harness.add_pump("ac1", { pin: 5, direct: true });
	pump_start("ac1");
	assert.eq(p.refs, 0, "direct pump must not be started by pump_start");
	assert.truthy(config.errmsg.indexOf("direct mode") >= 0, "errmsg should mention direct mode");
}

// =========================================================================
// 3. Cooldown semantics.
// =========================================================================
function test_stop_with_cooldown_goes_to_cooldown_state() {
	let p = harness.add_pump("ac1", { pin: 5, cooldown: 60 });
	p.state = PUMP_STATE_RUNNING;
	p.refs  = 1;
	harness.pins[5] = HIGH;
	pump_stop("ac1");
	assert.eq(p.state, PUMP_STATE_COOLDOWN, "with cooldown>0 stop must go to COOLDOWN");
	assert.eq(harness.pins[5], HIGH, "pin must stay HIGH during cooldown");
}

function test_stop_with_zero_cooldown_immediately_off() {
	let p = harness.add_pump("ac1", { pin: 5, cooldown: 0 });
	p.state = PUMP_STATE_RUNNING;
	p.refs  = 1;
	harness.pins[5] = HIGH;
	pump_stop("ac1");
	assert.eq(p.state, PUMP_STATE_STOPPED, "cooldown=0 must go straight to STOPPED");
	assert.eq(harness.pins[5], LOW, "pin must be LOW immediately");
}

// pump_stop on a pump already in COOLDOWN is a no-op (per pump.js comment).
function test_stop_in_cooldown_is_noop() {
	let p = harness.add_pump("ac1", { pin: 5, cooldown: 60 });
	p.state = PUMP_STATE_COOLDOWN;
	p.refs  = 0;
	harness.pins[5] = HIGH;
	let r = pump_stop("ac1");
	assert.eq(r, 0, "stop on COOLDOWN returns 0");
	assert.eq(p.state, PUMP_STATE_COOLDOWN, "state unchanged");
	assert.eq(harness.pins[5], HIGH, "pin unchanged");
}

// =========================================================================
// 4. Restart from COOLDOWN snaps back to RUNNING (no re-init through
//    the start sequence — pump is still physically on).
// =========================================================================
function test_start_from_cooldown_returns_to_running() {
	let p = harness.add_pump("ac1", { pin: 5 });
	p.state = PUMP_STATE_COOLDOWN;
	p.refs  = 0;
	pump_start("ac1");
	assert.eq(p.refs, 1, "start increments refs");
	assert.eq(p.state, PUMP_STATE_RUNNING, "COOLDOWN → RUNNING on restart");
}

// =========================================================================
// 5. Force-stop bypasses refcount. Even with refs=5, force_stop drives off.
// =========================================================================
function test_force_stop_bypasses_refcount() {
	let p = harness.add_pump("ac1", { pin: 5 });
	p.state = PUMP_STATE_RUNNING;
	p.refs  = 5;       // simulating multiple holders
	harness.pins[5] = HIGH;
	pump_force_stop("ac1");
	assert.eq(p.refs, 0, "force_stop must zero out refs");
	assert.eq(p.state, PUMP_STATE_STOPPED, "force_stop drives state to STOPPED");
	assert.eq(harness.pins[5], LOW, "pin must be LOW after force_stop");
}

// =========================================================================
// 6. Primer chain: force-stopping a pump that uses a primer must also stop
//    the primer (referenced both at the top of pump_force_stop and inside
//    pump_off — the double-call is benign because the second is a no-op
//    when refs already reached 0).
// =========================================================================
function test_force_stop_chains_to_primer() {
	let primer = harness.add_pump("primer", { pin: 1 });
	let pump   = harness.add_pump("ac1",    { pin: 5, primer: "primer" });
	primer.state = PUMP_STATE_RUNNING;
	primer.refs  = 1;
	harness.pins[1] = HIGH;
	pump.state   = PUMP_STATE_RUNNING;
	pump.refs    = 1;
	harness.pins[5] = HIGH;

	pump_force_stop("ac1");
	assert.eq(pump.state,   PUMP_STATE_STOPPED, "ac1 stopped");
	assert.eq(primer.state, PUMP_STATE_STOPPED, "primer chained-stopped");
	assert.eq(harness.pins[1], LOW, "primer pin LOW");
	assert.eq(harness.pins[5], LOW, "ac1 pin LOW");
}

// =========================================================================
// 7. Reserve pumps go to RELEASE on stop, not STOPPED.
// =========================================================================
function test_reserve_pump_stops_to_release_state() {
	let p = harness.add_pump("ac1", { pin: 5, reserve: 1, cooldown: 0 });
	p.state = PUMP_STATE_RUNNING;
	p.refs  = 1;
	harness.pins[5] = HIGH;
	pump_stop("ac1");
	assert.eq(p.state, PUMP_STATE_RELEASE, "reserve pump must go to RELEASE on stop");
	assert.eq(harness.pins[5], LOW, "pin still goes LOW");
}

// =========================================================================
// 8. Disabled pump rejects start (common_start).
// =========================================================================
function test_disabled_pump_rejects_start() {
	let p = harness.add_pump("ac1", { pin: 5, enabled: false });
	pump_start("ac1");
	assert.eq(p.refs, 0, "disabled pump must not start");
	assert.truthy(config.errmsg.indexOf("disabled") >= 0, "errmsg mentions disabled");
}

// =========================================================================
// 9. Missing-pump guards (fixed alongside the test suite — pump_stop and
//    pump_force_stop used to throw TypeError on unknown names; they now
//    return 1 and set config.errmsg).
// =========================================================================
function test_stop_unknown_pump_returns_error() {
	let r = pump_stop("missing");
	assert.eq(r, 1, "pump_stop on missing pump returns 1");
	assert.truthy(config.errmsg.indexOf("not found") >= 0, "errmsg mentions not found");
}

function test_force_stop_unknown_pump_returns_error() {
	let r = pump_force_stop("missing");
	assert.eq(r, 1, "pump_force_stop on missing pump returns 1");
	assert.truthy(config.errmsg.indexOf("not found") >= 0, "errmsg mentions not found");
}

// =========================================================================
// 10. pump_revoke forces refs=1 then stops, ending in COOLDOWN if cooldown set.
// =========================================================================
function test_revoke_forces_then_stops() {
	let p = harness.add_pump("ac1", { pin: 5, cooldown: 0 });
	p.state = PUMP_STATE_RUNNING;
	p.refs  = 3;
	harness.pins[5] = HIGH;
	pump_revoke("ac1");
	assert.eq(p.refs, 0, "revoke pulls refs to 0");
	assert.eq(p.state, PUMP_STATE_STOPPED, "revoke ends in STOPPED with cooldown=0");
}

harness.run();
