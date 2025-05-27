
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

function soc_battery_capacity_trigger() {

	var dlevel = 1;

	dprintf(dlevel,"si.battery_capacity: %d\n", si.battery_capacity);
	if (!si.battery_capacity) return;

	si.signal(si.name,"Battery","Set");

	if (typeof(soc_kf_secs) != "undefined") soc_kf_secs.reset();
}

// open checkpoint file
function soc_open_battcp() {

	var dlevel = 1;

	// Battery checkpoint file
	dprintf(dlevel,"battcp: %s\n", si.script_dir + "/battcp.dat");
	battcp = new File(si.script_dir + "/battcp.dat","text");
	var opts = "readWrite";
	if (!battcp.exists) {
		opts += ",create";
	} else {
		let cur = new Date();
		dprintf(dlevel,"last mod: %s, current: %s\n", battcp.lastModified, new Date());
		var diff = Math.abs((new Date().getTime() - battcp.lastModified.getTime()) / 1000);
		dprintf(dlevel,"diff: %s\n", diff);
		if (diff > 60) opts += ",create,truncate";
	}
	dprintf(dlevel,"opts: %s\n", opts);
	if (battcp.open(opts)) {
		dprintf(dlevel,"size: %d\n", battcp.size);
		if (battcp.size) {
			var cpinfo = battcp.readln().split(",");
//			dumpobj(cpinfo);
			let i = 0;
			si.battery_ah = parseFloat(cpinfo[i++]);
			si.battery_in = parseFloat(cpinfo[i++]);
			si.battery_out = parseFloat(cpinfo[i++]);
			si.battery_empty = (cpinfo[i++] == 'true');
			si.battery_full = (cpinfo[i++] == 'true');
			dprintf(0,"Restored: ah: %f, in: %f, out: %f, empty: %s, full: %s\n", si.battery_ah, si.battery_in, si.battery_out, si.battery_empty, si.battery_full);
		}
	}
}

// checkpoint batt in/out/etc in case of restart
function soc_checkpoint() {

	let dlevel = 1;

	// Checkpoint total power
	if (si.battcp) {
		if (typeof(battcp) == "undefined" || !battcp.exists) open_battcp();
		if (battcp.isOpen) {
			battcp.seek(0);
			let line = sprintf("%f,%f,%f,%s,%s\n",si.battery_ah,si.battery_in,si.battery_out,si.battery_empty,si.battery_full);
			dprintf(dlevel,"line: %s", line);
			battcp.write(line);
			battcp.flush();
		}
	}
}

function soc_battery_empty() {

	let dlevel = -1;

	dprintf(dlevel,"battery_ah: %f, charge_start_ah: %f\n", si.battery_ah, si.charge_start_ah);

	dprintf(dlevel,"battery_full: %s\n", si.battery_full);
	if (si.battery_full) {
		let ov = si.discharge_efficiency;
		dprintf(dlevel,"ov: %f\n", ov);
		let m = si.charge_start_ah / si.battery_ah;
		dprintf(dlevel,"m: %f\n", m);
		val = ov * m;
		dprintf(dlevel,"val: %f\n", val);
		if (val > 1.0) val = 1.0;
		if (influx.enabled && influx.connected) influx.write(si.discharge_efficiency_table,{} = { value: val });
		let nv = soc_kf_deff.filter(val,1);
		dprintf(dlevel,"nv: %f, ov: %f\n", nv, ov);
		if (!double_equals(nv,ov)) {
			log_verbose("Setting discharge_effiency to: %f\n", nv);
			si.discharge_efficiency = nv;
			config.save();
		}
	}
	si.battery_full = false;
	si.battery_empty = true;

	dprintf(dlevel,"empty_command: %s\n", si.empty_command);
	if (si.empty_command.length > 0) system(si.empty_command);

	// Start counting all over again
	si.battery_ah = si.charge_start_ah;
}

function soc_battery_full() {

	let dlevel = -1;

	dprintf(dlevel,"battery_ah: %f, charge_end_ah: %f\n", si.battery_ah, si.charge_end_ah);

	dprintf(dlevel,"battery_empty: %s\n", si.battery_empty);
	if (si.battery_empty) {
		let ov = si.charge_factor;
		let m = si.charge_end_ah / si.battery_ah;
		dprintf(dlevel,"m: %f\n", m);
		val = ov * m;
		dprintf(dlevel,"val: %f\n", val);
		if (influx.enabled && influx.connected) influx.write(si.charge_factor_table,{} = { value: val });
		let nv = soc_kf_ceff.filter(val,1);
		dprintf(dlevel,"nv: %f, ov: %f\n", nv, ov);
		if (!double_equals(nv,ov)) {
			log_verbose("Setting charge_effiency to: %f\n", nv);
			si.charge_factor = nv;
			config.save();
		}
	}
	si.battery_empty = false;
	si.battery_full = true;

	dprintf(dlevel,"full_command: %s\n", si.full_command);
	if (si.full_command.length > 0) system(si.full_command);

	// pin AH to charge end (fix SOC)
	si.battery_ah = si.charge_end_ah;
}

// Prime efficiency kalman filter
function soc_prime_kf(kf,table_name,prop) {

	let dlevel = 1;

	dprintf(dlevel,"table_name: %s, prop.name: %s\n", table_name, prop.name);

	if (!influx) return;
	dprintf(dlevel,"influx: enabled: %s, connected: %s\n", influx.enabled, influx.connected);
	if (influx.enabled && influx.connected) {
		let query = "SELECT value FROM " + table_name + " ORDER BY time";
		dprintf(dlevel,"query: %s\n", query);
		let results = influx2arr(influx.query(query));
		dprintf(dlevel,"results: %s\n", results);
		let nv,ov;
		nv = ov = prop.value;
		dprintf(dlevel,"ov: %f\n", ov);
		if (results) {
			let count = results.length;
			dprintf(dlevel,"count: %d\n", count);
			for(let i=0; i < count; i++) {
				dprintf(dlevel,"results[%d].value: %f\n", i, results[i].value);
				nv = kf.filter(results[i].value,1);
			}
			dprintf(dlevel,"nv: %f, ov: %f\n", nv, ov);
			if (!double_equals(nv,ov)) {
				log_warning("computed initial %s (%f) != saved value (%f)\n", prop.name, nv, ov);
				prop.value = nv;
				config.save();
			}
		}
	}
	dprintf(dlevel,"done!\n");
}

function soc_event_handler(name,module,action) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, agent.name: %s\n", name, agent.name);
	if (name != agent.name) return;

	dprintf(dlevel,"module: %s\n", module);
	if (module == "Battery") {
		dprintf(dlevel,"action: %s\n", action);
		if (action == "Full") soc_battery_full();
		else if (action == "Empty") soc_battery_empty();
	}

	// Any change in charge status reset the time remaining kalman filter */
	if (module == "Charge" && soc_kf_secs) soc_kf_secs.reset();
}

function soc_init() {

	SI_SOC_LINEAR = 0;
	SI_SOC_USABLE = 1;
	SI_SOC_TOTAL = 2;

	var soc_props = [
		[ "discharge_efficiency_table", DATA_TYPE_STRING, "si_discharge_efficiency", 0 ],
		[ "discharge_efficiency", DATA_TYPE_DOUBLE, 0.945, 0 ],
		[ "charge_factor_table", DATA_TYPE_STRING, "si_charge_factor", 0 ],
		[ "charge_factor", DATA_TYPE_DOUBLE, 1.04, 0 ],
		[ "battery_capacity", DATA_TYPE_DOUBLE, null, 0, soc_battery_capacity_trigger ],
		[ "battery_ah", DATA_TYPE_DOUBLE, NaN, CONFIG_FLAG_PRIVATE ],
		[ "battery_level", DATA_TYPE_DOUBLE, "0.0", CONFIG_FLAG_PRIVATE ],
		[ "battcp", DATA_TYPE_BOOL, "true", 0 ],
		[ "soc_type", DATA_TYPE_INT, SI_SOC_USABLE.toString(), 0 ],
	];
	var soc_funcs = [
		[ "battery_empty", soc_battery_empty, 0 ],
		[ "battery_full", soc_battery_full, 0 ],
	];

	config.add_props(si,soc_props,si.driver_name);
	config.add_funcs(si,soc_funcs,si.driver_name);

	event.handler(soc_event_handler,"si","all","all");

	si.battery_empty = false;
	si.battery_full = false;
	si.battery_in = 0.0;
	si.battery_out = 0.0;
	si.battery_ah = NaN;
	si.remain_time = "";
	si.remain_text = "";

	if (si.battcp) soc_open_battcp();

	soc_kf_secs = new KalmanFilter();
	soc_kf_deff = new KalmanFilter(1.5);
	soc_kf_ceff = new KalmanFilter(1.5);

	soc_prime_kf(soc_kf_deff,si.discharge_efficiency_table,config.get_property("discharge_efficiency"));
	log_verbose("Initial discharge efficiency: %f\n", si.discharge_efficiency);
	soc_prime_kf(soc_kf_ceff,si.charge_factor_table,config.get_property("charge_factor"));
	log_verbose("Initial charge_factor: %f\n", si.charge_factor);
}

function report_remain(func,secs) {
	var diff,hours,mins;

	var dlevel = 3;

	dprintf(dlevel,"secs: %f\n", secs);
	var d = new Date();
	d.setSeconds(d.getSeconds() + secs);
	t = d.toString();
	days = parseInt(secs / 86400);
	dprintf(dlevel,"days: %d\n", days);
	if (days > 0) {
		secs -= (days * 86400);
		dprintf(dlevel,"new secs: %d\n", secs);
	}
	hours = parseInt(secs / 3600);
	dprintf(dlevel,"hours: %d\n", hours);
	if (hours > 0) {
		secs -= (hours * 3600.0);
		dprintf(dlevel,"new secs: %d\n", secs);
	}
	mins = parseInt(secs / 60);
	dprintf(dlevel,"mins: %d\n", mins);
	if (mins) {
		secs -= parseInt(mins * 60);
		dprintf(dlevel,"new secs: %d\n", secs);
	}
	if (days > 1000) secs = (-1);
	if (secs >= 0) {
		var s = "";
		if (days > 0) s += sprintf("%d days", days);
		if (hours > 0) {
			if (s.length) s += ", ";
			if (hours == 1) s += "1 hour";
			else s += sprintf("%d hours", hours);
		}
		if (mins > 0) {
			if (s.length) s += ", ";
			if (mins == 1) s += "1 minute";
			else s += sprintf("%d minutes", mins);
		}
if (0) {
		if (s.length) s += ", ";
		if (secs == 1) s+= "1 second";
		else s += sprintf("%d seconds", secs);
}
	} else {
		s = "~";
		t = "~";
	}
	si.remain_time = t;
	si.remain_text = s;
	dprintf(dlevel,"%s time: %s (%s)\n", func, si.remain_text, si.remain_time);
}

function init_battery_ah() {

	let dlevel = 0;

	// XXX use a table for the initial val?
	dprintf(dlevel,"battery_voltage: %f, min_voltage: %f, max_voltage: %f\n",
		data.battery_voltage, si.min_voltage, si.max_voltage);
	let vsoc = ((data.battery_voltage - si.min_voltage) / (si.max_voltage - si.min_voltage)) * 0.85;
	dprintf(dlevel,"vsoc: %f\n", vsoc);
	si.battery_ah = si.battery_capacity * vsoc;
	if (!si.battery_ah) si.battery_ah = 0;
	dprintf(dlevel,"initial battery_ah: %f\n", si.battery_ah);
}

function soc_main() {

	var dlevel = 1;

	dprintf(dlevel,"battery_capacity: %d\n", si.battery_capacity);
	if (!si.battery_capacity) {
		si.remain_text = "(battery_capacity not set)";
		return;
	}

	// Set initial battery_ah
	dprintf(dlevel,"battery_ah: %f\n", si.battery_ah);
	if (isNaN(si.battery_ah)) init_battery_ah();

	// XXX If CC charging and the battery was NOT empty, adjust ah
	dprintf(dlevel,"charge_mode: %d, si.battery_empty: %s\n", si.charge_mode, si.battery_empty);
	if (si.charge_mode == 1 && si.battery_empty == false) {
		dprintf(dlevel,"battery_voltage: %f, min_voltage: %f\n", data.battery_voltage, si.min_voltage);
		let rel = data.battery_voltage - si.min_voltage;
		dprintf(dlevel,"rel: %f, spread: %f\n", rel, si.spread);
		pct = rel / si.spread;
		dprintf(dlevel,"pct: %f\n", pct);
		dprintf(dlevel,"charge_factor: %f\n", si.charge_factor);
		let mult = .85 * si.charge_factor;
		dprintf(dlevel,"mult: %f\n", mult);
		adj = pct * mult;
		dprintf(dlevel,"adj: %f\n", adj);
		dprintf(dlevel,"battery_capacity: %f\n", si.battery_capacity);
		si.battery_ah = si.battery_capacity * adj;
		dprintf(dlevel,"new battery_ah: %f\n", si.battery_ah);
	}

	// battery AH counter
	let amps = data.battery_current;
	dprintf(dlevel,"amps: %f, discharge_efficiency: %f, charge_factor: %f\n",
		amps, si.discharge_efficiency, si.charge_factor);
	let val = (amps > 0 ? amps / si.discharge_efficiency : amps * si.charge_factor);
	dprintf(dlevel,"val: %f\n", val);
	let mult = (si.interval / 3600);
	dprintf(dlevel,"mult: %f\n", mult);
	let ah = val * mult;
	dprintf(dlevel,"ah: %f\n", ah);
	si.battery_ah -= ah;
	dprintf(dlevel,"NEW battery_ah: %f\n", si.battery_ah);
	var obt = si.battery_ah;
	// something went wrong
	if (isNaN(si.battery_ah)) si.battery_ah = si.charge_start_ah;
	if (si.battery_ah < 0) si.battery_ah = 0;
	if (si.battery_ah > si.battery_capacity) si.battery_ah = si.battery_capacity;
	if (si.battery_ah != obt) dprintf(dlevel,"FIXED battery_ah: %f\n", si.battery_ah);

	// Checkpoint in case restart
	soc_checkpoint();

if (0) {
//	new_soc = Sk - (n * Ts / As) * Ik
	top = (0.98 * si.interval);
	bot = (si.battery_capacity * 3600);
	i = Math.abs(data.battery_current);
	dprintf(0,"top: %f, bot: %f, i: %f\n", top, bot, i);
	sk = (top / bot) * i;
	dprintf(0,"sk: %f\n", sk);
}

	let new_soc = (si.battery_ah / si.battery_capacity) * 100.0;
	dprintf(dlevel,"new_soc: %.1f\n", new_soc);

	// XXX sanity check 
	let obl = new_soc;
	if (new_soc > 100.0) new_soc = 100.0;
	dprintf(dlevel,"new_soc: %f, charge_start_level: %f, battery_voltage: %f, charge_start_voltage: %f\n",
		new_soc, si.charge_start_level, data.battery_voltage, si.charge_start_voltage);
	if (new_soc < si.charge_start_level && pround(data.battery_voltage,1) > si.charge_start_voltage) {
		dprintf(dlevel,"new_soc: %f, charge_start_level: %f, battery_voltage: %f, charge_start_voltage: %f\n",
			new_soc, si.charge_start_level, pround(data.battery_voltage,1), si.charge_start_voltage);
		new_soc = si.charge_start_level;
	}
	if (pround(data.battery_voltage,1) < si.charge_start_voltage) {
		printf("new_soc: %f, si.soc: %f\n", new_soc, si.soc);
		let p = pct(new_soc,si.soc);
		printf("p: %f\n", p);
		if (p >= 10) new_soc = si.soc;
	}
	if (new_soc != obl) dprintf(dlevel,"FIXED: new_soc: %.1f\n", new_soc);
	si.soc = si.battery_level = new_soc;

	dprintf(dlevel,"new battery level: %f\n", si.battery_level);

	// Calculate remaining ah
	let remain,func;
	dprintf(dlevel,"charge_mode: %d\n", si.charge_mode);
	if (si.charge_mode) {
		dprintf(dlevel,"charge_end_ah: %f\n", si.charge_end_ah);
		remain = si.charge_end_ah - si.battery_ah;
		func = "Charge";
	} else {
		dprintf(dlevel,"charge_start_ah: %f\n", si.charge_start_ah);
		remain = si.battery_ah - si.charge_start_ah;
		func = "Battery";
	}
	dprintf(dlevel,"remain: %f\n", remain);

	// Calculate remaining seconds
	if (si.charge_mode && amps > 0) amps = 0.00001;
	else if (amps < 0) amps *= (-1);
	dprintf(dlevel,"amps: %f\n", amps);
	let secs = (remain / amps) * 3600;
	dprintf(dlevel,"secs: %f\n", secs);
	if (isNaN(secs) || !isFinite(secs)) secs = -1;

	// If we're in CV mode and there's a time remaining
	if (si.charge_mode == CHARGE_MODE_CV && si.cv_time) {
		let diff = (si.cv_start_time + MINUTES(si.cv_time)) - time();
		dprintf(dlevel,"diff: %f, secs: %f\n", diff, secs);
		if (diff < secs || secs < 0) secs = diff;
	}
	dprintf(dlevel,"secs: %f\n", secs);
	secs = soc_kf_secs.filter(secs);
	dprintf(dlevel,"NEW secs: %f\n", secs);

	report_remain(func,secs);
}
