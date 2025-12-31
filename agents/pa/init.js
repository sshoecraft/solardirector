
// Dirs
script_dir = dirname(script_name);
lib_dir = (script_dir == "." ? "../../lib" : pa.libdir);
sdlib_dir = lib_dir + "/sd";

// sdlib init
include(sdlib_dir+"/init.js");
// Needed until new sd code running on solardirector
include(sdlib_dir+"/sdconfig.js");

// My utils/funcs
include(script_dir+"/utils.js");
include(script_dir+"/funcs.js");

// Global constants
var PA_ID_DELIMITER = "/";
var DEFAULT_DAY_START = "06:00";
var DEFAULT_DAY_LSTART = "sunrise+30";
var DEFAULT_NIGHT_START = "20:00";
var DEFAULT_NIGHT_LSTART = "sunset+120";

// Property trigger functions
function pa_budget_trigger(a,p,o) {
	let dlevel = 1;

	if (!a) return;
	dprintf(dlevel,"new budget: %.1f\n", p.value);
	if (typeof(a.reserved) != 'undefined') {
		a.avail = a.budget - a.reserved;
		dprintf(dlevel,"NEW avail: %.1f\n", a.avail);
	}
}

function pa_server_config_trigger(a,p,o) {
	let dlevel = 1;

	dprintf(dlevel,"new val: %s, old val: %s\n", p.value, o);

	dprintf(dlevel,"p.name: %s\n", p.name);
	let vname = p.name.substr(0,p.name.indexOf("_server_config")) + "_mqtt";
	dprintf(dlevel,"vname: %s\n", vname);
	let m = a[vname];
	let isdef = (typeof(m) != 'undefined');
	dprintf(dlevel,"isdef: %s\n", isdef);
	let subs = [];
	if (isdef) {
		dprintf(dlevel,"connected: %s\n", m.connected);
		if (m.connected) m.disconnect();
		dprintf(dlevel,"m.subs: %s\n", m.subs);
		for(let i = 0; i < m.subs.length; i++) {
			let sub = m.subs[i];
			dprintf(dlevel,"unsubbing: %s\n", sub);
			subs.push(sub);
			m.unsub(sub);
		}
		delobj(a[vname]);
	}

	dprintf(dlevel,"config: %s(%d)\n", p.value, p.value.length);
	if (p.value.length) {
		a[vname] = new MQTT(p.value);
		if (!a[vname]) {
			log_error("error: unable to create %s\n", vname);
			return 1;
		}
		m = a[vname];
		m.enabled = true;
		for(let i = 0; i < subs.length; i++) {
			let sub = subs[i];
			dprintf(dlevel,"subbing: %s\n", sub);
			m.sub(sub);
		}
		dprintf(dlevel,"connecting...\n");
		m.connect();
        dprintf(dlevel,"connected: %s\n", m.connected);
	}
	return 0;
}

function pa_topic_trigger(a,p,o) {
	let dlevel = 1;

	dprintf(dlevel,"new val: %s, old val: %s\n", p.value, o);

	dprintf(dlevel,"p.name: %s\n", p.name);
	let vname = p.name.substr(0,p.name.indexOf("_topic")) + "_mqtt";
	dprintf(dlevel,"vname: %s\n", vname);
	let isdef = (typeof(a[vname]) != 'undefined');
	dprintf(dlevel,"isdef: %s\n", isdef);

	let m = a[vname];
	dprintf(dlevel,"m: %s\n", m);
	if (isdef) {
		if (m.connected) m.disconnect();
		if (typeof(o) != "undefined") m.unsub(o);
		m.sub(p.value);
		m.connect();
	}
	return 0;
}

function pa_property_trigger(a,p,o) {
	let dlevel = 2;

	dprintf(dlevel,"new val: %s, old val: %s\n", p.value, o);

	dprintf(dlevel,"p.name: %s\n", p.name);
	let name = p.name.substr(0,p.name.indexOf("_property"));
	dprintf(dlevel,"name: %s\n", name);
	a["have_" + name] = false;
	a[name] = 0.0;
}

function pa_set_fspc(a,p,o) {
	let dlevel = 2;

	dprintf(dlevel,"pa_set_fspc called: a=%s, p=%s, o=%s\n", a, p, o);
	if (!a) {
		dprintf(dlevel,"agent parameter is undefined, returning\n");
		return 0;
	}
	dprintf(dlevel,"fspc: %s\n", a.fspc);
	if (typeof(a.fspc) == "undefined") return;
	if (!a.fspc) return;
	dprintf(dlevel,"fspc_start: %s\n", a.fspc_start);
	if (typeof(a.fspc_start) == "undefined") return;
	dprintf(dlevel,"fspc_end: %s\n", a.fspc_end);
	if (typeof(a.fspc_end) == "undefined") return;
	if (a.fspc_start > a.fspc_end) {
		config.errmsg = "fspc_start must be < fspc_end";
		return 1;
	}
	dprintf(dlevel,"nominal_frequency: %s\n", a.nominal_frequency);
	if (typeof(a.nominal_frequency) == "undefined") return;

	a.fspc_start_frq = a.nominal_frequency + a.fspc_start;
	a.fspc_end_frq = a.nominal_frequency + a.fspc_end;
	dprintf(dlevel,"fspc_start_frq: %.02f, fspc_end_frq: %.02f\n", a.fspc_start_frq, a.fspc_end_frq);
	return 0;
}

function pa_samples_trigger(a,p,o) {
    let dlevel = 1;

	dprintf(dlevel,"new value: %d\n", a.samples);
	for(let i=0; i < a.samples; i++) a.values[i] = 0;
	a.idx = 0;
}

function pa_sample_period_trigger(a,p,o) {
    let dlevel = 1;

	dprintf(dlevel,"new value: %d\n", a.sample_period);
    dprintf(dlevel,"interval: %d\n", a.interval);
	if (a.sample_period < a.interval) {
		printf("warning: sample_period(%d) < interval(%d), setting samples to 1\n", a.sample_period, a.interval);
		a.samples = 1;
	} else {
		a.samples = a.sample_period / a.interval;
	}
    return 0;
}

function pa_reserved_trigger(a,p,o) {
	let dlevel = -1;

	if (!a) return;
	dprintf(dlevel,"budget: %d\n", a.budget);
	if (a.budget) {
		dprintf(dlevel,"new reserved: %.1f\n", p.value);
		a.avail = a.budget - a.reserved;
		dprintf(dlevel,"NEW avail: %.1f\n", a.avail);
	}
}

function pa_night_budget_trigger(a,p,o) {
	let dlevel = 1;

	if (!a) return;
	dprintf(dlevel,"new night_budget: %.1f, old: %.1f\n", p.value, o);

	// Check if it's currently nighttime - recalculate avail using the new night_budget
	if (pa_is_night_period()) {
		dprintf(dlevel,"it's nighttime, recalculating avail with new night_budget\n");
		// Don't modify a.budget - just recalculate avail based on night_budget
		if (typeof(a.reserved) != 'undefined') {
			a.avail = p.value - a.reserved;
			dprintf(dlevel,"NEW avail: %.1f (night_budget: %.1f, reserved: %.1f)\n", a.avail, p.value, a.reserved);
		}
		log_info("Night budget updated during nighttime: %.1f (avail: %.1f)\n", p.value, a.avail);
	} else {
		dprintf(dlevel,"not nighttime, night_budget will be used when night period starts\n");
	}
	return 0;
}

function init_main() {
	let dlevel = 1;
	dprintf(dlevel, "*** PA INIT ***\n");

	// Initialize PA-specific data structures
	pa.reservations = [];
	pa.revokes = [];
	pa.values = [];
	pa.neg_power_time = 0;
	// XXX before trigger def
	pa.samples = 0;

	// Initialize data staleness timestamps
	pa.grid_power_time = 0;
	pa.battery_power_time = 0;
	pa.pv_power_time = 0;
	pa.charge_mode_time = 0;
	pa.battery_level_time = 0;
	pa.frequency_time = 0;

	// Initialize location from global location if not set
	if (!pa.location || pa.location.length == 0) {
		dprintf(dlevel, "location not set, trying to get global location\n");
		let loc = getLocation(false);
		if (loc && typeof(loc.lat) != "undefined" && typeof(loc.lon) != "undefined") {
			pa.location = loc.lat + "," + loc.lon;
			dprintf(dlevel, "initialized location to: %s\n", pa.location);
		} else {
			dprintf(dlevel, "unable to get global location, leaving empty\n");
		}
	}

	// Determine test mode string for topic construction
//	let tstr = pa.name == 'patest' ? "test" : "";
    let tstr = "";

	// Configure properties
	let props = [
		// Inverter power
		[ "inverter_server_config", DATA_TYPE_STRING, "localhost", 0, pa_server_config_trigger, pa ],
		[ "inverter_topic", DATA_TYPE_STRING, SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/si"+tstr+"/"+SOLARD_FUNC_DATA, 0, pa_topic_trigger, pa ],
		[ "grid_power_property", DATA_TYPE_STRING, "input_power", 0, pa_property_trigger, pa ],
		[ "invert_grid_power", DATA_TYPE_BOOL, "no", 0 ],
		[ "load_power_property", DATA_TYPE_STRING, "load_power", 0, pa_property_trigger, pa ],
		[ "invert_load_power", DATA_TYPE_BOOL, "no", 0 ],
		[ "frequency_property", DATA_TYPE_STRING, "output_frequency", 0 ],
		[ "charge_mode_property", DATA_TYPE_STRING, "charge_mode", 0, pa_property_trigger, pa ],

		// Solar power
		[ "pv_server_config", DATA_TYPE_STRING, "localhost", 0, pa_server_config_trigger, pa ],
		[ "pv_topic", DATA_TYPE_STRING, SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/pvc"+tstr+"/"+SOLARD_FUNC_DATA, 0, pa_topic_trigger, pa ],
		[ "pv_power_property", DATA_TYPE_STRING, "output_power", 0, pa_property_trigger, pa ],
		[ "invert_pv_power", DATA_TYPE_BOOL, "no", 0 ],
		[ "fspc", DATA_TYPE_BOOL, "false", 0, pa_set_fspc ],
		[ "fspc_start", DATA_TYPE_DOUBLE, 1, 0, pa_set_fspc ],
		[ "fspc_end", DATA_TYPE_DOUBLE, 2, 0, pa_set_fspc ],
		[ "nominal_frequency", DATA_TYPE_DOUBLE, 60, 0, pa_set_fspc ],

		// Battery level
		[ "battery_server_config", DATA_TYPE_STRING, "localhost", 0, pa_server_config_trigger, pa ],
		[ "battery_topic", DATA_TYPE_STRING, SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/si"+tstr+"/"+SOLARD_FUNC_DATA, 0, pa_topic_trigger, pa ],
		[ "battery_power_property", DATA_TYPE_STRING, "battery_power", 0, pa_property_trigger, pa ],
		[ "invert_battery_power", DATA_TYPE_BOOL, "no", 0 ],
		[ "battery_level_property", DATA_TYPE_STRING, "battery_level", 0, pa_property_trigger, pa ],
		[ "battery_level_min", DATA_TYPE_FLOAT, "0.0", 0 ],

		[ "buffer", DATA_TYPE_INT, 0, 0 ],
		[ "samples", DATA_TYPE_INT, 0, CONFIG_FLAG_PRIVATE, pa_samples_trigger, pa ],
		[ "sample_period", DATA_TYPE_INT, 90, 0, pa_sample_period_trigger, pa ],
		[ "reserve_delay", DATA_TYPE_INT, 180, 0 ],
		[ "protect_charge", DATA_TYPE_BOOL, "no", 0, 0 ],
		[ "approve_p1", DATA_TYPE_BOOL, "no", 0 ],
		[ "deficit_timeout", DATA_TYPE_INT, 300, 0 ],
		[ "limit", DATA_TYPE_FLOAT, "0.0", 0 ],
		[ "pub_topic", DATA_TYPE_STRING, SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/" + pa.name + "/"+SOLARD_FUNC_DATA, 0 ],
		[ "reserved", DATA_TYPE_INT, 0, CONFIG_FLAG_PRIVATE, pa_reserved_trigger, pa ],
		[ "budget", DATA_TYPE_FLOAT, "0.0", 0, pa_budget_trigger, pa ],
		[ "battery_soft_limit", DATA_TYPE_FLOAT, "0.0", 0 ],
		[ "battery_hard_limit", DATA_TYPE_FLOAT, "0.0", 0 ],
		[ "battery_scale", DATA_TYPE_BOOL, "yes", 0 ],
		[ "night_budget", DATA_TYPE_FLOAT, "0.0", 0, pa_night_budget_trigger, pa ],
		[ "night_approve_p1", DATA_TYPE_BOOL, "false", 0 ],
        [ "night_start", DATA_TYPE_STRING, DEFAULT_NIGHT_START, 0 ],
        [ "day_start", DATA_TYPE_STRING, DEFAULT_DAY_START, 0 ],
        [ "location", DATA_TYPE_STRING, "", 0 ],
		[ "data_stale_interval", DATA_TYPE_INT, 3, 0 ],
		[ "interval", DATA_TYPE_INT, 15, CONFIG_FLAG_NOWARN, pa_sample_period_trigger, pa ]
	];
	config.add_props(pa, props, pa.driver_name);

	// Configure MQTT functions
	let funcs = [
		[ "reserve", pa_reserve, 5 ],
		[ "release", pa_release, 4 ],
		[ "repri", pa_repri, 5 ],
		[ "revoke_all", pa_revoke_all, 1 ],
	];
	config.add_funcs(pa, funcs, pa.driver_name);

	// Init non-config vars (after properties are configured)
	pa.last_reserve_time = time() - pa.reserve_delay;

	// Initialize mode based on current time period
	pa.mode = pa_is_night_period() ? "night" : "day";

	// Have agent_run clean up any unread messages
	pa.purge = true;

	// Repub
	pa.repub();

	// Make our publish topic
	pa.topic = agent.mktopic(SOLARD_FUNC_DATA);

	dprintf(dlevel, "PA agent initialization complete\n");
	return 0;
}
