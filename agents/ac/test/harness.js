/*
 * AC agent JS test harness.
 *
 * Provides stubs for the agent runtime, controllable time, hardware fakes,
 * and spies for cross-module function calls so production AC modules can be
 * loaded under sdjs and exercised deterministically.
 *
 * Usage:
 *   include(dirname(script_name)+"/harness.js");
 *   include(dirname(script_name)+"/assert.js");
 *   harness.init();
 *   include(dirname(script_name)+"/../utils.js");
 *   include(dirname(script_name)+"/../<module>.js");
 *   <module>_init();
 *   function test_xxx() { ... }
 *   harness.run();
 */

harness = {};

// Constants the AC *_init() functions reference. DATA_TYPE_* are already
// defined by the sdjs runtime; we set them here only if missing.
if (typeof(INVALID_TEMP) == "undefined") INVALID_TEMP = -200.0;
if (typeof(CONFIG_FLAG_PRIVATE) == "undefined") CONFIG_FLAG_PRIVATE = 0x100;
if (typeof(CONFIG_FLAG_NOPUB) == "undefined") CONFIG_FLAG_NOPUB = 0x200;
if (typeof(CONFIG_FLAG_NOSAVE) == "undefined") CONFIG_FLAG_NOSAVE = 0x400;
if (typeof(CONFIG_FLAG_VALUE) == "undefined") CONFIG_FLAG_VALUE = 0x800;
if (typeof(HIGH) == "undefined") HIGH = 1;
if (typeof(LOW) == "undefined") LOW = 0;
if (typeof(INPUT) == "undefined") INPUT = 0;
if (typeof(OUTPUT) == "undefined") OUTPUT = 1;
if (typeof(debug) == "undefined") debug = 0;

// State and mode enum values. Hardcoded here (matching the corresponding
// *_init() ordering in pump.js, fan.js, unit.js, mode.js, charge.js,
// sample.js, direct.js) so tests don't have to pull every module's init
// chain. If a test includes the source module, its *_init() reassigns
// these to identical values.
PUMP_STATE_STOPPED      = 0;
PUMP_STATE_RESERVE      = 1;
PUMP_STATE_START_PRIMER = 2;
PUMP_STATE_WAIT_PRIMER  = 3;
PUMP_STATE_START        = 4;
PUMP_STATE_WAIT_PUMP    = 5;
PUMP_STATE_FLOW         = 6;
PUMP_STATE_WARMUP       = 7;
PUMP_STATE_RUNNING      = 8;
PUMP_STATE_COOLDOWN     = 9;
PUMP_STATE_RELEASE      = 10;
PUMP_STATE_ERROR        = 11;

FAN_STATE_STOPPED       = 0;
FAN_STATE_RESERVE       = 1;
FAN_STATE_START_PUMP    = 2;
FAN_STATE_WAIT_PUMP     = 3;
FAN_STATE_START         = 4;
FAN_STATE_WAIT_START    = 5;
FAN_STATE_RUNNING       = 6;
FAN_STATE_RELEASE       = 7;
FAN_STATE_ERROR         = 8;

FAN_MODE_NONE           = 0;
FAN_MODE_COOL           = 1;
FAN_MODE_HEAT           = 2;

UNIT_STATE_STOPPED      = 0;
UNIT_STATE_START_PUMP   = 1;
UNIT_STATE_WAIT_PUMP    = 2;
UNIT_STATE_RESERVE      = 3;
UNIT_STATE_START        = 4;
UNIT_STATE_RUNNING      = 5;
UNIT_STATE_RELEASE      = 6;
UNIT_STATE_ERROR        = 7;

AC_MODE_NONE            = 0;
AC_MODE_COOL            = 1;
AC_MODE_HEAT            = 2;

CHARGE_STATE_STOPPED    = 0;
CHARGE_STATE_START      = 1;
CHARGE_STATE_WAIT_START = 2;
CHARGE_STATE_RUNNING    = 3;
CHARGE_STATE_STOP       = 4;

SAMPLE_STATE_STOPPED    = 0;
SAMPLE_STATE_WAIT_PUMP  = 1;
SAMPLE_STATE_RUNNING    = 2;

// ---- Time control ----
harness.now = 1700000000;     // arbitrary fixed start
harness.advance = function(seconds) { harness.now += seconds; };
time = function() { return harness.now; };

// ---- Log capture ----
harness.log = [];
harness.verbose = false;
function _harness_record(level, args) {
	let s;
	try { s = sprintf.apply(null, args); }
	catch (e) { s = args.join(" "); }
	harness.log.push({level: level, msg: s});
	if (harness.verbose) printf("[%s] %s", level, s);
}
log_info     = function() { _harness_record("info",    arguments); };
log_warning  = function() { _harness_record("warning", arguments); };
log_error    = function() { _harness_record("error",   arguments); };
log_verbose  = function() { _harness_record("verbose", arguments); };
log_write    = function() { _harness_record("write",   arguments); };
dprintf      = function() { /* level-gated; no-op in tests unless verbose */
	if (!harness.verbose) return;
	let args = []; for (let i = 1; i < arguments.length; i++) args.push(arguments[i]);
	_harness_record("dprintf", args);
};
dumpobj = function(o) { if (harness.verbose) printf("dumpobj: %s\n", JSON.stringify(o)); };

// ---- Agent runtime stubs ----
agent  = { name: "ac", driver_name: "ac", purge: false, messages: [],
           mktopic: function(f) { return "solar/ac/" + f; },
           repub:   function() {} };
event  = { handler: function(/*cb, name, mod, action*/) {} };
config = {
	errmsg: "",
	add_props: function(target, props, label) {
		// Install each [name, type, default, flags, trigger] row's default onto target.
		for (let i = 0; i < props.length; i++) {
			let row  = props[i];
			let name = row[0];
			let dflt = row[2];
			if (typeof(target[name]) != "undefined") continue;  // don't overwrite test setup
			if (dflt === null) { target[name] = undefined; continue; }
			// Coerce string defaults to numbers if the type slot indicates a number.
			let t = row[1];
			if (typeof(dflt) == "string" && t !== DATA_TYPE_STRING) {
				let n = parseFloat(dflt);
				target[name] = isNaN(n) ? dflt : n;
			} else {
				target[name] = dflt;
			}
		}
	},
	add_funcs:    function() {},
	save:         function() {},
	read:         function() {},
	get_property: function(/*name*/) { return { value: undefined, dirty: false, flags: 0, default: "" }; },
};
mqtt   = { enabled: false, connected: false, sub: function() {}, pub: function() {} };
influx = { enabled: false, connected: false, connect: function() {}, write: function() {}, query: function() { return {}; } };

// PA client stubs (always succeed).
pa_client_init    = function() { return 0; };
pa_client_reserve = function() { return 0; };
pa_client_release = function() { return 0; };
pa_client_repri   = function() { return 0; };

// Errors recorder.
errors = { list: [], add: function(mod, action, msg) { errors.list.push({module: mod, action: action, msg: msg}); } };
error_set   = function(mod, action, msg) { errors.list.push({module: mod, action: action, msg: msg}); };
error_clear = function(mod, name) {
	let kept = [];
	for (let i = 0; i < errors.list.length; i++) {
		let e = errors.list[i];
		if (e.module == mod && e.action == name) continue;
		kept.push(e);
	}
	errors.list = kept;
};
get_errors  = function() { return errors.list; };

// Misc runtime helpers used by AC modules.
sleep    = function(/*s*/) {};
sdconfig = function(/*agent, name, action*/) { return { status: 0, message: "" }; };
set_trigger    = function(/*name, fn*/) {};
double_equals  = function(a, b) { return Math.abs(a - b) < 1e-9; };
getBoolean     = function(v) {
	if (typeof(v) == "boolean") return v;
	if (typeof(v) == "number")  return v != 0;
	if (typeof(v) == "string")  { let s = v.toLowerCase(); return s == "true" || s == "yes" || s == "1"; }
	return false;
};
checkargs      = function(/*args, schema*/) { return 0; };

// influx2arr/influx2obj — return canned data so charge/mode tests can stub responses.
harness.influx_responses = [];
influx2arr = function(/*r*/) {
	if (harness.influx_responses.length) return harness.influx_responses.shift();
	return null;
};
influx2obj = function(/*r*/) { return harness.influx_responses.length ? harness.influx_responses.shift() : null; };

// suncalc / date helpers used by mode.js — return a deterministic Date.
harness.date_responses = {};   // keyed by label (e.g. "day_start")
get_date = function(loc, spec, label/*, next, date*/) {
	if (harness.date_responses[label]) return harness.date_responses[label];
	// default: noon today shifted by spec sentinels
	let d = new Date(harness.now * 1000);
	return d;
};
SOLARD_TZNAME    = SOLARD_TZNAME    || "";
SOLARD_TOPIC_ROOT   = SOLARD_TOPIC_ROOT   || "solar";
SOLARD_TOPIC_AGENTS = SOLARD_TOPIC_AGENTS || "agents";
SOLARD_FUNC_DATA    = SOLARD_FUNC_DATA    || "data";

// State/mode-name helpers some modules reference cross-file. Provide minimal
// versions so tests don't all have to include the defining module.
ac_modestr = function(m) {
	if (m === AC_MODE_COOL) return "Cool";
	if (m === AC_MODE_HEAT) return "Heat";
	return "None";
};
unit_statestr = function(s)   { return "unit-state(" + s + ")"; };
fan_statestr  = function(s)   { return "fan-state("  + s + ")"; };
fan_modestr   = function(m)   {
	if (m === FAN_MODE_COOL) return "Cool";
	if (m === FAN_MODE_HEAT) return "Heat";
	return "None";
};
charge_statestr = function(s) { return "charge-state(" + s + ")"; };

// Cross-module helpers: getters and a few non-state-mutating functions that
// some modules call directly. Tests can override these or use the defaults.
pump_get_state = function(name) { return pumps[name] ? pumps[name].state : PUMP_STATE_STOPPED; };
fan_get_state  = function(name) { return fans[name]  ? fans[name].state  : FAN_STATE_STOPPED; };
unit_get_state = function(name) { return units[name] ? units[name].state : UNIT_STATE_STOPPED; };
fan_set_state  = function(name, state) { if (fans[name])  fans[name].state  = state; };
unit_set_state = function(name, state) { if (units[name]) units[name].state = state; };
pump_set_state = function(name, state) { if (pumps[name]) pumps[name].state = state; };
unit_mode      = function(/*name, unit, mode*/) { return 0; };
charge_get_pri = function() { return 50; };
direct_get_pump      = function(/*g*/) { return null; };
direct_is_active     = function(/*g, f*/) { return false; };
direct_is_pending    = function(/*g, f*/) { return false; };
direct_is_error      = function(/*g*/) { return false; };
direct_main          = function() {};

// jconfig stubs — no-op for module *_init() paths that call jconfig_load(),
// since tests populate object maps directly via harness.add_*().
jconfig_add        = function(name, opts, label, objs/*, props, init*/) { objs[name] = opts || {}; return 0; };
jconfig_del        = function(name, label, objs/*, props*/) { delete objs[name]; return 0; };
jconfig_get        = function(name, label, objs/*, props*/) { return objs[name]; };
jconfig_set        = function(name, opts, label, objs/*, props*/) {
	let obj = objs[name];
	if (!obj) return 1;
	// opts can be a string "key=val" or an object
	if (typeof(opts) == "string") {
		let eq = opts.indexOf("=");
		if (eq > 0) {
			let k = opts.substring(0, eq);
			let v = opts.substring(eq + 1);
			if (v == "true")  obj[k] = true;
			else if (v == "false") obj[k] = false;
			else { let n = parseFloat(v); obj[k] = isNaN(n) ? v : n; }
		}
	} else if (opts) {
		for (let k in opts) obj[k] = opts[k];
	}
	return 0;
};
jconfig_load       = function(/*collection, props, init*/) { /* tests populate via harness.add_* */ };
jconfig_get_config = function(name, label, objs/*, props*/) { return objs[name] || {}; };
jconfig_set_config = function(/*config, label, objs, props, reload*/) { return 0; };

// ---- Hardware fakes ----
harness.pins = {};
pinMode      = function(pin, mode) { /* no-op */ };
digitalRead  = function(pin)       { return harness.pins[pin] || 0; };
digitalWrite = function(pin, v)    { harness.pins[pin] = v; };
analogRead   = function(pin)       { return harness.pins[pin] || 0; };
analogWrite  = function(pin, v)    { harness.pins[pin] = v; };

// BME sensor — tests set harness.bme to control reading.
harness.bme = { status: 0, temp: 70.0, humidity: 50.0, pressure: 1013.0 };
readbme = function(/*calib*/) {
	let calib = arguments.length ? parseFloat(arguments[0]) : 0;
	let out = { status: harness.bme.status, temp: harness.bme.temp, humidity: harness.bme.humidity, pressure: harness.bme.pressure };
	if (out.status == 0) out.temp += calib;
	return out;
};

// CAN fakes.
harness.can = {};
can_get_sensor = function(target, offset/*, wait*/) {
	let key = target + ":" + offset;
	return harness.can[key];
};

// ---- Spies ----
// A spy records every call as an args array. spy.calls is the history.
function _make_spy(name, impl) {
	let spy = function() {
		let a = []; for (let i = 0; i < arguments.length; i++) a.push(arguments[i]);
		spy.calls.push(a);
		if (typeof(impl) == "function") return impl.apply(null, a);
	};
	spy.calls = [];
	spy.name  = name;
	return spy;
}
harness.spies = {};

// Default impls for spies — keep object state coherent so chained tests work.
function _impl_pump_start(name) {
	let p = pumps && pumps[name];
	if (p) p.state = PUMP_STATE_RUNNING;   // simplified for tests; pump.js drives the real sequence
}
function _impl_pump_stop(name) {
	let p = pumps && pumps[name];
	if (p) p.state = PUMP_STATE_STOPPED;
}

harness._install_spies = function() {
	harness.spies.pump_start      = _make_spy("pump_start",      _impl_pump_start);
	harness.spies.pump_stop       = _make_spy("pump_stop",       _impl_pump_stop);
	harness.spies.pump_force_stop = _make_spy("pump_force_stop", _impl_pump_stop);
	harness.spies.pump_statestr   = _make_spy("pump_statestr",   function(s) { return "state(" + s + ")"; });
	harness.spies.fan_start       = _make_spy("fan_start");
	harness.spies.fan_stop        = _make_spy("fan_stop");
	harness.spies.fan_force_stop  = _make_spy("fan_force_stop");
	harness.spies.unit_start      = _make_spy("unit_start");
	harness.spies.unit_stop       = _make_spy("unit_stop");
	harness.spies.unit_force_stop = _make_spy("unit_force_stop");
	harness.spies.direct_on       = _make_spy("direct_on");
	harness.spies.direct_off      = _make_spy("direct_off");

	pump_start      = harness.spies.pump_start;
	pump_stop       = harness.spies.pump_stop;
	pump_force_stop = harness.spies.pump_force_stop;
	pump_statestr   = harness.spies.pump_statestr;
	fan_start       = harness.spies.fan_start;
	fan_stop        = harness.spies.fan_stop;
	fan_force_stop  = harness.spies.fan_force_stop;
	unit_start      = harness.spies.unit_start;
	unit_stop       = harness.spies.unit_stop;
	unit_force_stop = harness.spies.unit_force_stop;
	direct_on       = harness.spies.direct_on;
	direct_off      = harness.spies.direct_off;
};

// ---- Object factories ----
harness.add_pump = function(name, opts) {
	if (typeof(pumps) == "undefined") pumps = {};
	let p = {
		name: name,
		direct: false,
		enabled: true,
		error: false,
		state: PUMP_STATE_STOPPED,
		refs: 0,
		pin: -1,
		primer: "",
		wait_time: 10,
		flow_wait_time: 5,
		warmup: 30,
		cooldown: 0,
		reserve: 0,
		priority: 100,
		settled: false,
		temp_in: INVALID_TEMP,
		temp_out: INVALID_TEMP,
		last_temp_in: INVALID_TEMP,
		start_time: 0,
		stop_time: 0,
		cycle_time: 0,
	};
	if (opts) for (let k in opts) p[k] = opts[k];
	pumps[name] = p;
	return p;
};

harness.add_fan = function(name, opts) {
	if (typeof(fans) == "undefined") fans = {};
	let f = { name: name, enabled: true, error: false, state: 0, refs: 0, pump: "", mode: 0 };
	if (opts) for (let k in opts) f[k] = opts[k];
	fans[name] = f;
	return f;
};

harness.add_unit = function(name, opts) {
	if (typeof(units) == "undefined") units = {};
	let u = { name: name, enabled: true, error: false, state: 0, refs: 0, pump: "", mode: 0 };
	if (opts) for (let k in opts) u[k] = opts[k];
	units[name] = u;
	return u;
};

// ---- Lifecycle ----
harness.signal_calls = [];
function _make_ac() {
	let a = { name: "ac", driver_name: "ac", script_dir: ".", interval: 10,
	          signal: function(label, action) { harness.signal_calls.push([label, action]); },
	          notify: function() {} };
	return a;
}

harness.init = function() {
	ac    = _make_ac();
	pumps = {};
	fans  = {};
	units = {};
	dgs   = {};
	directs = dgs;   // some modules use either name
	data  = {};
	errors.list = [];
	harness.signal_calls = [];
	harness._install_spies();
};

harness.reset = function() {
	ac    = _make_ac();
	pumps = {};
	fans  = {};
	units = {};
	dgs   = {};
	directs = dgs;
	data  = {};
	errors.list = [];
	harness.signal_calls = [];
	harness.log = [];
	harness.now = 1700000000;
	harness.bme = { status: 0, temp: 70.0, humidity: 50.0, pressure: 1013.0 };
	harness.pins = {};
	harness.can  = {};
	// Reset spy histories without rebuilding (preserves identity for assertions)
	for (let k in harness.spies) harness.spies[k].calls = [];
};

// ---- Test runner ----
// Discovers all globals matching ^test_, runs each in a try/catch, prints
// PASS/FAIL <name>, and exits non-zero if any fail.
harness.run = function() {
	let names = [];
	for (let k in window) {
		if (k.indexOf("test_") != 0) continue;
		if (typeof(window[k]) != "function") continue;
		names.push(k);
	}
	names.sort();
	let pass = 0, fail = 0;
	let failures = [];
	let setup_fn   = (typeof(window["setup"])    == "function") ? window["setup"]    : null;
	let teardown_fn = (typeof(window["teardown"]) == "function") ? window["teardown"] : null;
	for (let i = 0; i < names.length; i++) {
		let n = names[i];
		harness.reset();
		if (setup_fn) setup_fn();
		try {
			window[n]();
			pass++;
			printf("  PASS %s\n", n);
		} catch (e) {
			fail++;
			failures.push({name: n, err: e});
			printf("  FAIL %s: %s\n", n, e);
		}
		if (teardown_fn) try { teardown_fn(); } catch (e) {}
	}
	printf("\n%d passed, %d failed\n", pass, fail);
	if (fail > 0) {
		printf("\nFailures:\n");
		for (let i = 0; i < failures.length; i++) {
			printf("  %s: %s\n", failures[i].name, failures[i].err);
		}
	}
	// sdjs's exit() always returns 0 to the shell; abort(N) actually propagates.
	abort(fail > 0 ? 1 : 0);
};
