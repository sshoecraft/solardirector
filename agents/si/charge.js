/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

// spread = max - min
// vtg = min + (spread * pct)
// pct = (vgt - min) / spread

function si_check_config() {

	let dlevel = 1;

	// No recursion please
	if (si.in_check) return 0;

//	printf("*** IN CHECK ***\n");
	si.in_check = true;

	if (typeof(si_isvrange) == "undefined") include(dirname(script_name)+"/utils.js");
	if (typeof(SI_VOLTAGE_MIN) == "undefined") SI_VOLTAGE_MIN = 41;
	if (typeof(SI_VOLTAGE_MAX) == "undefined") SI_VOLTAGE_MAX = 63;

	// Charge min/max (not SI min/max)
	const DEFAULT_MIN_VOLTAGE = 42;
	const DEFAULT_MAX_VOLTAGE = 58;

	// min_voltage
	dprintf(dlevel,"min_voltage: %f, max_voltage: %f\n", si.min_voltage, si.max_voltage);
	let ov,nv;
	ov = nv = si.min_voltage;
	if (!double_equals(si.min_voltage,0.0)) {
		if (!si_isvrange(si.min_voltage)) {
			printf("warning: min_voltage (%f) out of range (%.1f to %.1f)\n",
				si.min_voltage, SI_VOLTAGE_MIN, SI_VOLTAGE_MAX);
			nv = 0.0;
		} else if (!double_equals(si.max_voltage,0.0) && si.min_voltage >= si.max_voltage) {
			printf("error: min_voltage >= max_voltage\n");
			nv = 0.0;
		}
	}
	if (double_equals(nv,0.0)) nv = DEFAULT_MIN_VOLTAGE;
	nv = pround(nv,1);
	if (nv != ov) setcleanval("min_voltage",nv,"%.1f");

	// max_voltage
	ov = nv = si.max_voltage;
	if (!double_equals(si.max_voltage,0.0)) {
		if (!si_isvrange(si.max_voltage)) {
			printf("error: max_voltage (%f) out of range (%.1f to %.1f)\n",
				si.max_voltage, SI_VOLTAGE_MIN, SI_VOLTAGE_MAX);
			nv = 0.0;
		} else if (si.max_voltage <= si.min_voltage) {
			printf("error: max_voltage <= min_voltage\n");
			nv = 0.0;
		}
	}
	if (double_equals(nv,0.0)) nv = DEFAULT_MAX_VOLTAGE;
	nv = pround(nv,1);
	if (nv != ov) setcleanval("max_voltage",nv,"%.1f");

	dprintf(dlevel,"min_voltage: %f, max_voltage: %f\n", si.min_voltage, si.max_voltage);
	let spread = si.max_voltage - si.min_voltage;
	dprintf(dlevel,"spread: %f\n", spread);

	while(1) {
		dprintf(dlevel,"charge_start_voltage: %f\n", si.charge_start_voltage);
		if (isNaN(si.charge_start_voltage)) si.charge_start_voltage = 0.0;
		if (!double_equals(si.charge_start_voltage,0.0)) {
			if (!si_isvrange(si.charge_start_voltage)) {
				dprintf(dlevel,"charge_start_voltage (%f) out of range(%.1f to %.1f)\n",
					si.charge_start_voltage, SI_VOLTAGE_MIN, SI_VOLTAGE_MAX);
				si.charge_start_voltage = 0.0;
			} else if (si.charge_start_voltage >= si.charge_end_voltage) {
				dprintf(dlevel,"charge_start_voltage >= charge_end_voltage\n");
				si.charge_start_voltage = si.charge_end_voltage - 1;
			} else if (si.charge_start_voltage < si.min_voltage) {
				dprintf(dlevel,"charge_start_voltage < min_voltage\n");
				si.charge_start_voltage = si.min_voltage + 1;
			} else if (si.charge_start_voltage > si.max_voltage) {
				dprintf(dlevel,"charge_start_voltage > max_voltage\n");
				si.charge_start_voltage = si.max_voltage - 1;
			}
		}
		if (double_equals(si.charge_start_voltage,0.0)) {
			dprintf(dlevel,"charge_start_level: %f\n", si.charge_start_level);
			var csl = si.charge_start_level ? si.charge_start_level : 25;
			dprintf(dlevel,"setting charge_start_voltage to %f\n", si.charge_start_voltage);
			setcleanval("charge_start_voltage",si.min_voltage + (spread * (csl / 100)));
		}
		setcleanval("charge_start_voltage",pround(si.charge_start_voltage,1));

		dprintf(dlevel,"charge_end_voltage: %f\n", si.charge_end_voltage);
		if (isNaN(si.charge_end_voltage)) si.charge_end_voltage = 0.0;
		if (!double_equals(si.charge_end_voltage,0.0)) {
			if (!si_isvrange(si.charge_end_voltage)) {
				dprintf(dlevel,"charge_end_voltage (%f) out of range(%.1f to %.1f)\n",
					si.charge_end_voltage, SI_VOLTAGE_MIN, SI_VOLTAGE_MAX);
				si.charge_end_voltage = 0.0;
			} else if (si.charge_end_voltage <= si.charge_start_voltage) {
				dprintf(dlevel,"charge_end_voltage <= charge_start_voltage\n");
				si.charge_end_voltage = 0.0;
			} else if (si.charge_end_voltage < si.min_voltage) {
				dprintf(dlevel,"charge_end_voltage < min_voltage\n");
				si.charge_end_voltage = 0.0;
			} else if (si.charge_end_voltage > si.max_voltage) {
				dprintf(dlevel,"charge_end_voltage > max_voltage\n");
				si.charge_end_voltage = 0.0;
			}
		}
		if (double_equals(si.charge_end_voltage,0.0)) {
			dprintf(dlevel,"charge_end_level: %f\n", si.charge_end_level);
			var cel = si.charge_end_level ? si.charge_end_level : 85;
			dprintf(dlevel,"setting charge_end_voltage to %f\n", si.charge_end_voltage);
			setcleanval("charge_end_voltage",si.min_voltage + (spread * (cel / 100)));
			continue;
		}
		setcleanval("charge_end_voltage",pround(si.charge_end_voltage,1));

		break;
	}

	dprintf(dlevel,"si.battery_capacity: %f\n", si.battery_capacity);
	if (si.battery_capacity) {
		dprintf(dlevel,"charge_start_level: %s\n", si.charge_start_level);
		var csl;
		if (si.charge_start_level) csl = si.charge_start_level;
		else csl = ((si.charge_start_voltage - si.min_voltage) / spread) * 100.0;
		dprintf(dlevel,"csl: %f\n", csl);
		si.charge_start_ah = si.battery_capacity * (csl / 100.0);
		dprintf(dlevel,"charge_end_level: %s\n", si.charge_end_level);
		var cel;
		if (si.charge_end_level) cel = si.charge_end_level;
		else cel = si.charge_end_level = ((si.charge_end_voltage - si.min_voltage) / spread) * 100.0;
		dprintf(dlevel,"cel: %f\n", cel);
		si.charge_end_ah = si.battery_capacity * (cel / 100.0);
		dprintf(dlevel,"charge_start_ah: %f, charge_end_ah: %f\n", si.charge_start_ah, si.charge_end_ah);
	}

//	printf("*** CHECK DONE ***\n");
	si.in_check = false;
	return 0;
}

function set_charge_start_level(c,p,o) {

	let dlevel = 1;

	dprintf(dlevel,"c: %s, p: %s, o: %s\n", c, p ? p.value : p, o);

	dprintf(dlevel,"si.charge_start_level: %f\n", si.charge_start_level);
	if (!si.charge_start_level) return;
	dprintf(dlevel,"min_voltage: %f, max_voltage: %f\n", si.min_voltage, si.max_voltage);
	if (double_equals(si.min_voltage,0.0) || double_equals(si.max_voltage,0.0)) {
		dprintf(dlevel,"min || max == 0, returning\n");
		return;
	}

	dprintf(dlevel,"config: %s\n", config);
	config.get_property("blah");
	dprintf(dlevel,"getting prop\n");
	var lp = config.get_property("charge_start_level");
	dprintf(dlevel,"lp.flags: %x\n", lp.flags);
	let dv = (lp.default.length ? pround(lp.default,1) : NaN);
	dprintf(dlevel,"dv: %s\n", dv);
	var p = config.get_property("charge_start_voltage");
	// if voltage set in config file or by sdconfig, value will be set.
	// If default value set for level, dirty will be false
	dprintf(dlevel,"charge_start_voltage flags: %x, charge_start_level dirty: %s\n", p.flags, lp.dirty);
	if (!double_equals(si.charge_start_level,dv) && check_bit(p.flags,CONFIG_FLAG_VALUE) && !lp.dirty) return;
	let ov = si.charge_start_voltage;
	let nv = pround(si.min_voltage + ((si.max_voltage - si.min_voltage) * (si.charge_start_level / 100)),1);
	dprintf(dlevel,"ov: %f, nv: %f\n", ov, nv);
	if (nv != ov) setcleanval("charge_start_voltage",nv,"%.1f");
	si_check_config();
}

function set_charge_end_level(c,p,o) {

	let dlevel = 1;

	dprintf(dlevel,"c: %s, p: %s, o: %s\n", c, p ? p.value : p, o);

	dprintf(dlevel,"si.charge_end_level: %f\n", si.charge_end_level);
	if (!si.charge_end_level) return;
	dprintf(dlevel,"min_voltage: %f, max_voltage: %f\n", si.min_voltage, si.max_voltage);
	if (double_equals(si.min_voltage,0.0) || double_equals(si.max_voltage,0.0)) return;

	var lp = config.get_property("charge_end_level");
	dprintf(dlevel,"lp.flags: %x\n", lp.flags);
	let dv = (lp.default.length ? pround(lp.default,1) : NaN);
	dprintf(dlevel,"dv: %s\n", dv);
	var p = config.get_property("charge_end_voltage");
	// if voltage set in config file or by sdconfig, value will be set.
	// If default value set for level, dirty will be false
	dprintf(dlevel,"charge_end_voltage flags: %x, charge_end_level dirty: %s\n", p.flags, lp.dirty);
	if (!double_equals(si.charge_end_level,dv) && check_bit(p.flags,CONFIG_FLAG_VALUE) && !lp.dirty) return;
	let ov = si.charge_end_voltage;
	let nv = pround(si.min_voltage + ((si.max_voltage - si.min_voltage) * (si.charge_end_level / 100)),1);
	dprintf(dlevel,"ov: %f, nv: %f\n", ov, nv);
	if (nv != ov) setcleanval("charge_end_voltage",nv,"%.1f");
	si_check_config();
}

function set_min_voltage() {

	var dlevel = 1;

	dprintf(dlevel,"min_voltage: %f\n", si.min_voltage);
	if (!si.min_voltage) return;
	si.in_check = true;
	set_charge_start_level();
	set_charge_end_level();
	si.in_check = false;
	si_check_config();
}

function set_max_voltage() {

	var dlevel = 1;

	dprintf(dlevel,"max_voltage: %f\n", si.max_voltage);
	if (!si.max_voltage) return;
	si.in_check = true;
	set_charge_start_level();
	set_charge_end_level();
	si.in_check = false;
	si_check_config();
}

// ensure cv_cutoff is set if battery_capacity is set (or in case "clear")
function set_cv_cutoff() {

	var dlevel = 1;

	dprintf(dlevel,"battery_capacity: %s\n", si.battery_capacity);
	if (!si.battery_capacity) return;

	// If cv_cutoff not set, set to 3% of battery capacity
	var p = config.get_property("cv_cutoff");
	dprintf(dlevel,"p: %s, dirty: %s, cv_cutoff: %s\n", p, p.dirty, si.cv_cutoff);
	if (!p || !p.dirty) {
		setcleanval("cv_cutoff",si.battery_capacity * 0.03);
		dprintf(dlevel,"NEW cv_cutoff: %f\n", si.cv_cutoff);
	}
}

function charge_event_handler(name,module,action) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, module: %s, action: %s\n", name, module, action);
	if (name != agent.name) return;

	if (module == "Location" && action == "Set") {
		if (si.charge_start_time && si.charge_start_time.length) set_charge_start_date();
		if (si.charge_stop_time && si.charge_stop_time.length) set_charge_stop_date();
	} else if (module == "Battery" && action == "Set") {
		set_cv_cutoff();
		si_check_config();
	}
}

function set_charge_start_date() {

	var dlevel = 1;

	dprintf(dlevel,"charge_start_date: %s\n", si.charge_start_date);
	if (!si.charge_start_time.length) si.charge_start_date = undefined;
	else si.charge_start_date = get_date(si.location,si.charge_start_time,"charge_start_time",true);
	if (si.charge_start_date) dprintf(dlevel,"charge_start_date: %s\n",si.charge_start_date);
}

function set_charge_stop_date() {

	var dlevel = 1;

	dprintf(dlevel,"charge_stop_date: %s\n", si.charge_stop_date);
	if (!si.charge_stop_time.length) si.charge_stop_date = undefined;
	else si.charge_stop_date = get_date(si.location,si.charge_stop_time,"charge_stop_time",true);
	if (si.charge_stop_date) dprintf(dlevel,"charge_stop_date: %s\n",si.charge_stop_date);
}

/* Init the charging modules */
function charge_init() {

	var dlevel = 1;

	if (typeof(si) == "undefined") {
		include("./utils.js");
		core_dir="../../lib/sd/";
		include(core_dir+"utils.js");
		include(core_dir+"suncalc.js");
		si = {};
		si.script_dir = ".";
		si.interval = 10;
		si.notify = function() { };
		si.min_voltage = 41;
		si.max_voltage = 58.4;
		data = {};
		data.battery_voltage = 45;
		data.battery_current = 0;
		data.GnRn = false;
		agent = {};
		agent.event = function() { };
		agent.driver_name = "si";
		config.read("./sidev.json",CONFIG_FILE_FORMAT_JSON);
//		smanet = new SMANET("rdev","192.168.1.164","serial0");
		smanet = new SMANET();
		if (!smanet) abort(0);
//		smanet.load_channels(SOLARD_LIBDIR + "/agents/si/" + smanet.type + "_channels.json");
		smanet.readonly = true;
	}

//	printf("*** IN INIT ***\n");

	CHARGE_METHOD_FAST = 0;
	CHARGE_METHOD_CCCV = 1;

	CHARGE_MODE_NONE = 0;
	CHARGE_MODE_CC = 1;
	CHARGE_MODE_CV = 2;

	CV_METHOD_DYNAMIC = 0;
	CV_METHOD_TIME = 1;
	CV_METHOD_AMPS = 2;
	CV_METHOD_AH = 4;

	/* Make sure all our variables are defined */
	var flags = CONFIG_FLAG_NOPUB | CONFIG_FLAG_NOSAVE;
	var charge_props = [
		[ "charge_start_time", DATA_TYPE_STRING, null, 0, set_charge_start_date ],
		[ "charge_stop_time", DATA_TYPE_STRING, null, 0, set_charge_stop_date ],
		[ "charge_source", DATA_TYPE_STRING, null, 0 ],
		[ "ignore_gnrn", DATA_TYPE_BOOL, "false", 0 ],
		[ "charge_creep", DATA_TYPE_BOOL, "false", 0 ],
		[ "charge_amps_temp_modifier", DATA_TYPE_INT, "1", flags ],
		[ "charge_amps_soc_modifier", DATA_TYPE_INT, "1", flags ],
		[ "charge_method", DATA_TYPE_INT, CHARGE_METHOD_FAST.toString(), 0 ],
		// XXX should be called: dont_stop_feeding_while_charging
		[ "charge_feed", DATA_TYPE_BOOL, "false", 0 ],
		// Start feeding when starting grid
		[ "grid_feed", DATA_TYPE_BOOL, "false", 0 ],
		[ "empty_command", DATA_TYPE_STRING, "", 0 ],
		[ "full_command", DATA_TYPE_STRING, "", 0 ],
		[ "cc_feed", DATA_TYPE_BOOL, "false", 0 ],
		[ "cc_command", DATA_TYPE_STRING, "", 0 ],
		[ "cv_method", DATA_TYPE_INT, CV_METHOD_DYNAMIC.toString(), 0 ],
		[ "cv_time", DATA_TYPE_INT, "60", 0 ],
		[ "cv_samples", DATA_TYPE_INT, "6", 0 ],
		[ "cv_cutoff", DATA_TYPE_DOUBLE, null, 0, set_cv_cutoff ],
		[ "cv_feed", DATA_TYPE_BOOL, "false", 0 ],
		[ "cv_command", DATA_TYPE_STRING, "", 0 ],
		[ "ce_feed", DATA_TYPE_BOOL, "false", 0 ],
		[ "ce_command", DATA_TYPE_STRING, "", 0 ],
		[ "charge_start_voltage", DATA_TYPE_DOUBLE, null, 0, si_check_config ],
		[ "charge_start_level", DATA_TYPE_DOUBLE, "25.0", 0, set_charge_start_level ],
		[ "charge_end_voltage", DATA_TYPE_DOUBLE, null, 0, si_check_config ],
		[ "charge_end_level", DATA_TYPE_DOUBLE, "85.0", 0, set_charge_end_level ],
		[ "charge_at_max", DATA_TYPE_BOOL, "false", 0 ],
	];

	// Dont allow check to run while initializing
	si.in_check = true;
	config.add_props(si, charge_props);

	// Set triggers for min/max to auto set levels
	set_trigger("min_voltage",set_min_voltage);
	set_trigger("max_voltage",set_max_voltage);

	agent.event_handler(charge_event_handler,agent.name);

	si.ba = [];
	si.baidx = 0;
	si.bafull = 0;
	si.grid_started = false;
	si.gen_started = false;

	charging_initialized = true;
//	printf("*** INIT DONE ***\n");

	// Now run check
	si.in_check = false;
	return si_check_config();
}

function charge_stop(force) {

	var dlevel = 1;

	let lforce = force;
	if (typeof(lforce) == "undefined") lforce = false;
	dprintf(dlevel,"charge_mode: %d, force: %s\n", si.charge_mode, lforce);
	if (!si.charge_mode && !lforce) return;

	si.charge_mode = 0;
	agent.event("Charge","Stop");
	config.save();

	// If we started grid/gen, stop them now
	if (si.gen_started) si_stop_gen(false);
	if (si.grid_started && !si.feed_enabled) si_stop_grid(false);

	/* Need this AFTER ending charge */
	if (si_check_config()) {
		log_error("%s\n",si.errmsg);
		return;
	}

	dprintf(dlevel,"charge_end_voltage: %f, min_charge_amps: %f\n", si.charge_end_voltage, si.min_charge_amps);
	si.battery_empty = false;
        remain_index = 0;
        remain_full = false;
}

function charge_start(force) {

	var dlevel = 1;

	dprintf(dlevel,"data.Run: %d\n", data.Run);
	if (!data.Run) return;

        let lforce = force;
        if (typeof(lforce) == "undefined") lforce = false;
	dprintf(dlevel,"charge_mode: %d, force: %s\n", si.charge_mode, lforce);
	if (si.charge_mode && !lforce) return;

	if (si.have_battery_temp && data.battery_temp <= 0.0) {
		log_warning("battery_temp <= 0.0, not starting charge\n");
		return;
	}

	dprintf(dlevel,"charge_at_max: %d\n", si.charge_at_max);
	si.charge_voltage = (si.charge_at_max ? si.max_voltage : si.charge_end_voltage);

	si.charge_amps = si.max_charge_amps;
	dprintf(dlevel,"charge_voltage: %f, charge_amps: %f\n", si.charge_voltage, si.charge_amps);

	si.charge_mode = 1;
	config.save();
	agent.event("Charge","Start");
	si.charge_amps_soc_modifier = 1.0;
	si.charge_amps_temp_modifier = 1.0;
	if (si.have_battery_temp) si.start_temp = data.battery_temp;
	dprintf(0,"charge_feed: %s, cc_feed: %s\n", si.charge_feed, si.cc_feed);
	if (si.charge_feed || si.cc_feed) feed_start(false);
	if (si.cc_command.length > 0) system(si.cc_command);
        remain_index = 0;
        remain_full = false;
}

function cvremain(start, end) {
	var diff,hours,mins;

	var dlevel = 1;

	dprintf(dlevel,"start: %d, end: %d\n", start, end);
	diff = end - start;
	dprintf(dlevel,"diff: %f\n", diff);
	if (diff > 0) {
		hours = parseInt(diff / 3600);
		dprintf(dlevel,"hours: %d\n", hours);
		if (hours > 0) {
			diff -= (hours * 3600.0);
			dprintf(dlevel,"new diff: %d\n", diff);
		}
		mins = parseInt(diff / 60);
		dprintf(dlevel,"mins: %d\n", mins);
		if (mins) {
			diff -= parseInt(mins * 60);
			dprintf(dlevel,"new diff: %d\n", diff);
		}
	} else {
		hours = mins = diff = 0;
	}
	printf("CV Time remaining: %02d:%02d:%02d\n",hours,mins,diff);
}

function charge_start_cv(force) {

	var dlevel = 1;

	if (!data.Run) return;

	let lforce = force;
	if (typeof(lforce) == "undefined") lforce = false;
	dprintf(dlevel,"charge_mode: %d, force: %s\n", si.charge_mode, lforce);
	if (si.charge_mode == 2 && !lforce) return;

	agent.event("Charge","StartCV");
	si.charge_mode = 2;
	config.save();
	si.cv_start_time = time();
	si.start_temp = data.battery_temp;
	si.baidx = si.bafull = 0;
	dprintf(0,"charge_feed: %s, cv_feed: %s\n", si.charge_feed, si.cv_feed);
	if (si.charge_feed || si.cv_feed) feed_start(false);
	if (si.cv_command.length > 0) system(si.cv_command);
}

function charge_end() {

	var dlevel = 1;

	if (si.mirror) return;

	agent.event("Battery","Full");

	charge_stop();

	if (si.ce_feed) feed_start(false);
	if (si.ce_command.length > 0) system(si.ce_command);
}

function cv_check_time() {

	var dlevel = 1;

	if (typeof(si.cv_start_time) == "undefined") si.cv_start_time = time();
	dprintf(dlevel,"cv_start_time: %s\n", si.cv_start_time);

	var current_time = time();
	var end_time = si.cv_start_time + MINUTES(si.cv_time);
	dprintf(dlevel,"current_time: %s, end_time: %s\n", current_time, end_time);
	cvremain(current_time,end_time);
	return (current_time >= end_time ? true : false);
}

function cv_check_amps() {

	var dlevel = 1;

	if (!si.cv_cutoff) return false;

	var amps = data.battery_current * (-1);
	dprintf(dlevel,"battery_amps: %f\n", amps);

	/* Amps < 0 (battery drain) will clear the hist */
	if (amps < 0) {
		si.baidx = si.bafull = 0;
		return false;
	}

	/* If dynamic charging, dont add to the history if charge_amps < cutoff */
	dprintf(dlevel,"force_charge_amps: %d, si.charge_amps: %d\n", si.force_charge_amps, si.charge_amps);
	if (si.force_charge_amps && (si.charge_amps < si.cv_cutoff)) return false;

// XXX experimental
	if (typeof(kf_cva) == "undefined") kf_cva = new KalmanFilter();
	let num = kf_cva.filter(amps,1);
	dprintf(0,"amps: %f, num: %f\n", amps, num);

	/* Average the last X amp samples */
	dprintf(dlevel,"baidx: %d\n", si.baidx);
	si.ba[si.baidx++] = amps;
	dprintf(dlevel,"cv_samples: %d\n", si.cv_samples);
	if (si.baidx >= si.cv_samples) {
		si.baidx = 0;
		si.bafull = 1;
	}
	dprintf(dlevel,"baidx: %d, bafull: %d\n", si.baidx, si.bafull);
	if (si.bafull) {
		amps = 0;
		for(let i=0; i < si.cv_samples; i++) {
			dprintf(dlevel,"ba[%d]: %f\n", i, si.ba[i]);
			amps += si.ba[i];
		}
		dprintf(dlevel,"amps: %f, samples: %d\n", amps, si.cv_samples);
		var avg = amps / si.cv_samples;
		dprintf(0,"CV: avg: %.1f, cutoff: %.1f\n", avg, si.cv_cutoff);
		if (avg < si.cv_cutoff) return true;
	}
	return false;
}

function cv_check_ah() {

	var dlevel = 1;

	dprintf(dlevel,"battery_ah: %f, battery_end_ah: %f\n", si.battery_ah, si.battery_end_ah);
	if (si.battery_ah >= si.battery_end_ah) return true;

	return false;
}

// Return true if battery is considered empty
function battery_is_empty() {

	let dlevel = 1;

	if (si.mirror) return false;

	dprintf(dlevel,"battery_voltage: %f, charge_start_voltage: %f\n",
		pround(data.battery_voltage,1), si.charge_start_voltage);
//	dprintf(dlevel,"battery_ah: %f, charge_start_ah: %f\n", si.battery_ah, si.charge_start_ah);
//	let r = (pround(data.battery_voltage,1) <= si.charge_start_voltage || si.battery_ah <= si.charge_start_ah);
	let r = (pround(data.battery_voltage,1) <= si.charge_start_voltage);
	dprintf(dlevel,"returning: %s\n", r);
	return r;
}

function charge_start_grid() {
	if (si.grid_feed) feed_start(false);
	if (!si.grid_started) si_start_grid();
}

function charge_main()  {

	var dlevel = 1;

	if (typeof(charging_initialized) == "undefined") charge_init();

	// In case of data errors
	if (!si_isvrange(data.battery_voltage)) {
		log_error("battery voltage of range, aborting charge check\n");
		return 0;
	}

	// If we started the gen and the grid came back on, stop it and start the grid
	if (si.gen_started) dprintf(0,"gen_started: %s, ignore_gnrn: %s, GnRn: %s\n", si.gen_started, si.ignore_gnrn, data.GnRn);
	if (si.gen_started && (!si.ignore_gnrn && !data.GnRn)) {
		printf("Stopping gen and starting grid...\n");
		si_stop_gen(true);
		charge_start_grid();
	}

	// If we started the grid and lost it, turn gen on
	if (si.grid_started && !data.ExtVfOk) dprintf(0,"grid_started: %s, ExtVfOk: %s, ignore_gnrn: %s, GnRn: %s\n", si.grid_started, data.ExtVfOk, si.ignore_gnrn, data.GnRn);
	if (si.grid_started && !data.ExtVfOk && (si.ignore_gnrn || data.GnRn)) {
		printf("Stopping grid and starting gen...\n");
		si_stop_grid(true);
		si_start_gen();
	}

	// Charge auto start/stop
	let cur = new Date();
	dprintf(dlevel,"cur date: %s\n", cur);
	dprintf(dlevel,"charge_start_date: %s\n", si.charge_start_date);
	if (si.charge_start_date) dprintf(dlevel,"cur: %s, start: %s, diff: %s\n",
		cur.getTime(), si.charge_start_date.getTime(), si.charge_start_date.getTime() - cur.getTime());
	if (si.charge_start_date && cur.getTime() >= si.charge_start_date.getTime()) {
		set_charge_start_date();
		let doit = true;
		if (si.charge_stop_date && cur.getTime() >= si.charge_stop_date.getTime()) doit = false;
		if (si.charge_mode) doit = false;
		if (doit) charge_start();
	}
	dprintf(dlevel,"charge_stop_date: %s\n", si.charge_stop_date);
	if (si.charge_stop_date) dprintf(dlevel,"cur: %s, stop: %s, diff: %s\n",
		cur.getTime(), si.charge_stop_date.getTime(), si.charge_stop_date.getTime() - cur.getTime());
	if (si.charge_stop_date && cur.getTime() >= si.charge_stop_date.getTime()) {
		set_charge_stop_date();
		if (si.charge_mode) charge_stop();
	}

	// Charge mode
	dprintf(dlevel,"charge_mode: %d, battery_voltage: %f, charge_start_voltage: %f\n",
		si.charge_mode, data.battery_voltage, si.charge_start_voltage);
	if (si.charge_mode) {
		// Just in case
		if (battery_is_empty() && !(si.grid_started || si.gen_started)) {
			dprintf(dlevel,"ExtVfOk: %s, ignore_gnrn: %s, GnRn: %s\n", data.ExtVfOk, si.ignore_gnrn, data.GnRn);
			if (si.ignore_gnrn || data.GnRn) si_start_gen();
			else if (data.ExtVfOk) charge_start_grid();
		}
		// Check battery temp
		dprintf(dlevel,"have_battery_temp: %s\n", si.have_battery_temp);
		if (si.have_battery_temp) {
			/* If battery temp is <= 0, stop charging immediately */
			dprintf(dlevel,"battery_temp: %2.1f\n", data.battery_temp);
			if (data.battery_temp <= 0.0) {
				charge_stop();
				return;
			}

			/* Set charge_amps_temp_modifier */
			if (data.battery_temp <= 5.0) si.charge_amps_temp_modifier = 1.0 - ((5.0 - data.battery_temp) * 0.2);
			else si.charge_amps_temp_modifier = 1.0;

			/* Watch for rise in battery temp, anything above 5 deg C is an error */
			/* We could lower charge amps until temp goes down and then set that as max amps */
			if (pct(data.battery_temp,si.start_temp) > 5) {
				log_error("current_temp: %f, start_temp: %f\n", data.battery_temp, si.start_temp);
				charge_stop();
				return;
			}
		}

		/* CC */
		if (si.charge_mode == CHARGE_MODE_CC) {
			dprintf(dlevel,"charge_at_max: %s\n", si.charge_at_max);
			si.charge_voltage = (si.charge_at_max ? si.max_voltage : si.charge_end_voltage);
			dprintf(dlevel,"battery_voltage: %f, charge_end_voltage: %f\n", data.battery_voltage, si.charge_end_voltage);
			if ((data.battery_voltage+0.0001) >= si.charge_end_voltage) {
				dprintf(dlevel,"charge_method: %s\n",si.charge_method.toString());
				if (si.charge_method == CHARGE_METHOD_CCCV) {
					charge_start_cv();
				} else {
					charge_end();
				}
			}
		/* CV */
		} else if (si.charge_mode == CHARGE_MODE_CV) {
			si.charge_voltage = si.charge_end_voltage;
			let end = false;
			dprintf(dlevel,"cv_time: %s\n", si.cv_time);
			// we always check time first if set
			if (si.cv_time && cv_check_time()) end = true;
			if (!end) end = cv_check_amps();
			dprintf(dlevel,"end: %s\n", end);
			if (end) charge_end();
		}

	// Battery is "empty", start charging
	} else if (battery_is_empty()) {
		agent.event("Battery","Empty");

		charge_start();

		// Start a charging source
		dprintf(dlevel,"charge_source: %s\n", si.charge_source);
		if (!si.charge_source) {
			dprintf(dlevel,"getting charge_source\n");
			si.charge_source = si_smanet_get_value("ExtSrc",2);
			if (!si.charge_source) {
				if (smanet.connected) log_error("unable to get SMANET.ExtSrc: %s\n", smanet.errmsg);
				si.charge_source = "";
			}
		}
		dprintf(dlevel,"ignore_gnrn: %s, GnRn: %s\n", si.ignore_gnrn, data.GnRn);
		if (si.charge_source.indexOf("Gen") !== -1 && (si.ignore_gnrn || data.GnRn)) si_start_gen();
		else if (si.charge_source.indexOf("Grid") !== -1 || data.ExtVfOk) charge_start_grid();
	} else {
		si.charge_voltage = si.charge_end_voltage;
	}
}
