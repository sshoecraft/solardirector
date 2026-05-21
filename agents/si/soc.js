
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
			let i = 0;
			si.battery_ah = parseFloat(cpinfo[i++]);
			si.battery_in = parseFloat(cpinfo[i++]);
			si.battery_out = parseFloat(cpinfo[i++]);
			dprintf(0,"Restored: ah: %f, in: %f, out: %f\n", si.battery_ah, si.battery_in, si.battery_out);
		}
	}
}

// checkpoint batt in/out/etc in case of restart
function soc_checkpoint() {

	let dlevel = 1;

	// Checkpoint total power
	if (si.battcp) {
		if (typeof(battcp) == "undefined" || !battcp.exists) soc_open_battcp();
		if (battcp.isOpen) {
			battcp.seek(0);
			let line = sprintf("%f,%f,%f\n",si.battery_ah,si.battery_in,si.battery_out);
			dprintf(dlevel,"line: %s", line);
			battcp.write(line);
			battcp.flush();
		}
	}
}

function soc_event_handler(name,module,action) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, agent.name: %s\n", name, agent.name);
	if (name != agent.name) return;

	// Any change in charge status reset the time remaining kalman filter
	if (module == "Charge" && soc_kf_secs) soc_kf_secs.reset();
}

function soc_init() {

	SI_SOC_LINEAR = 0;
	SI_SOC_USABLE = 1;
	SI_SOC_TOTAL = 2;

	var soc_props = [
		[ "battery_capacity", DATA_TYPE_DOUBLE, null, 0, soc_battery_capacity_trigger ],
		[ "battery_ah", DATA_TYPE_DOUBLE, NaN, CONFIG_FLAG_PRIVATE ],
		[ "battery_level", DATA_TYPE_DOUBLE, "0.0", CONFIG_FLAG_PRIVATE ],
		[ "battcp", DATA_TYPE_BOOL, "true", 0 ],
		[ "soc_type", DATA_TYPE_INT, SI_SOC_USABLE.toString(), 0 ],
	];

	config.add_props(si,soc_props,si.driver_name);

	event.handler(soc_event_handler,"si","all","all");

	si.battery_in = 0.0;
	si.battery_out = 0.0;
	si.battery_ah = NaN;
	si.remain_time = "";
	si.remain_text = "";
	si.anchor_set_this_interval = false;

	if (si.battcp) soc_open_battcp();

	soc_kf_secs = new KalmanFilter();
}

// Cycle tracking state for adaptive table learning
var cycle_state = {
	in_cycle: false,
	cycle_type: "",               // "charge" or "discharge"
	start_time: 0,
	start_voltage: 0.0,
	start_ah: 0.0,
	integrated_ah: 0.0,
	curve_points: [],             // recorded voltage→Ah curve during this cycle
	last_sample_bin: "",          // avoid duplicate samples in same voltage bin
	cv_curve_points: [],          // recorded current→Ah curve during CV phase
	cv_last_sample_bin: ""        // avoid duplicate samples in same current bin
};

// Snap voltage to nearest 0.25V bin key
function soc_voltage_bin(v) {
	return (Math.round(v / 0.25) * 0.25).toFixed(2);
}

// Start tracking a charge cycle (called on Battery.Empty event)
function soc_start_charge_cycle() {
	var dlevel = 1;

	if (!si.battery_capacity) return;

	cycle_state.in_cycle = true;
	cycle_state.cycle_type = "charge";
	cycle_state.start_time = time();
	cycle_state.start_voltage = data.battery_voltage || 0;
	cycle_state.start_ah = si.battery_ah;
	cycle_state.integrated_ah = 0.0;
	cycle_state.curve_points = [];
	cycle_state.last_sample_bin = "";
	cycle_state.cv_curve_points = [];
	cycle_state.cv_last_sample_bin = "";

	dprintf(dlevel, "Cycle: Started charge cycle tracking (battery_ah=%.1f)\n", si.battery_ah);
}

// Complete a charge cycle and update adaptive table (called on Battery.Full event)
function soc_complete_charge_cycle() {
	var dlevel = 1;

	if (!cycle_state.in_cycle || cycle_state.cycle_type != "charge") {
		dprintf(dlevel, "Cycle: Not in charge cycle, ignoring Full event\n");
		return;
	}

	var duration_secs = time() - cycle_state.start_time;
	var duration_mins = duration_secs / 60.0;
	var observed_ah = cycle_state.integrated_ah;

	dprintf(0, "Cycle: Completed charge cycle: %.1f Ah in %.0f minutes (%d curve points)\n",
		observed_ah, duration_mins, cycle_state.curve_points.length);

	// Validate cycle (must be >30 min and >10% capacity)
	if (duration_mins < 30 || observed_ah < si.battery_capacity * 0.1) {
		log_warning("Cycle: Charge cycle too short or insufficient Ah (%.0f min, %.1f Ah), discarding\n",
			duration_mins, observed_ah);
		cycle_state.in_cycle = false;
		return;
	}

	// Fold voltage curve into adaptive table
	if (typeof(fold_cycle_into_table) == "function") {
		fold_cycle_into_table("charge", cycle_state.curve_points);
	}

	// Fold CV current curve into adaptive CV table
	if (typeof(fold_cv_into_table) == "function" && cycle_state.cv_curve_points.length > 0) {
		fold_cv_into_table(cycle_state.cv_curve_points);
		dprintf(dlevel, "Cycle: Folded %d CV curve points\n", cycle_state.cv_curve_points.length);
	}

	cycle_state.in_cycle = false;
}

// Start tracking a discharge cycle (called on Battery.Full event)
function soc_start_discharge_cycle() {
	var dlevel = 1;

	if (!si.battery_capacity) return;

	cycle_state.in_cycle = true;
	cycle_state.cycle_type = "discharge";
	cycle_state.start_time = time();
	cycle_state.start_voltage = data.battery_voltage || 0;
	cycle_state.start_ah = si.battery_ah;
	cycle_state.integrated_ah = 0.0;
	cycle_state.curve_points = [];
	cycle_state.last_sample_bin = "";

	dprintf(dlevel, "Cycle: Started discharge cycle tracking (battery_ah=%.1f)\n", si.battery_ah);
}

// Complete a discharge cycle and update adaptive table (called on Battery.Empty event)
function soc_complete_discharge_cycle() {
	var dlevel = 1;

	if (!cycle_state.in_cycle || cycle_state.cycle_type != "discharge") {
		dprintf(dlevel, "Cycle: Not in discharge cycle, ignoring Empty event\n");
		return;
	}

	var duration_secs = time() - cycle_state.start_time;
	var duration_mins = duration_secs / 60.0;
	var observed_ah = cycle_state.integrated_ah;

	dprintf(0, "Cycle: Completed discharge cycle: %.1f Ah in %.0f minutes (%d curve points)\n",
		observed_ah, duration_mins, cycle_state.curve_points.length);

	// Validate cycle (must be >30 min and >10% capacity)
	if (duration_mins < 30 || observed_ah < si.battery_capacity * 0.1) {
		log_warning("Cycle: Discharge cycle too short or insufficient Ah (%.0f min, %.1f Ah), discarding\n",
			duration_mins, observed_ah);
		cycle_state.in_cycle = false;
		return;
	}

	// Fold curve into adaptive table
	if (typeof(fold_cycle_into_table) == "function") {
		fold_cycle_into_table("discharge", cycle_state.curve_points);
	}

	cycle_state.in_cycle = false;
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

	// Use table lookup (accurate for NMC chemistry)
	if (typeof(soc_table_get_raw_soc) == "function") {
		let table_soc = soc_table_get_raw_soc();
		if (!isNaN(table_soc)) {
			si.battery_ah = si.battery_capacity * (table_soc / 100.0);
			dprintf(dlevel, "initial battery_ah (from table): %f\n", si.battery_ah);
			return;
		}
	}

	// Fallback to linear interpolation
	dprintf(dlevel,"battery_voltage: %f, min_voltage: %f, max_voltage: %f\n",
		data.battery_voltage, si.min_voltage, si.max_voltage);
	let vsoc = ((data.battery_voltage - si.min_voltage) / (si.max_voltage - si.min_voltage)) * 0.85;
	dprintf(dlevel,"vsoc: %f\n", vsoc);
	si.battery_ah = si.battery_capacity * vsoc;
	if (!si.battery_ah) si.battery_ah = 0;
	dprintf(dlevel,"initial battery_ah (fallback): %f\n", si.battery_ah);
}

var soc_checkpoint_validated = false;

function validate_checkpoint() {
	// Validate checkpoint against OCV table
	// If they differ by more than 20%, checkpoint is garbage - use table value

	let dlevel = 0;

	if (soc_checkpoint_validated) return;
	soc_checkpoint_validated = true;

	// Need table function and valid voltage (table is optional, skip if not available)
	if (typeof(soc_table_get_raw_soc) != "function") return;
	if (!si_isvrange(data.battery_voltage)) return;
	if (!si.battery_capacity) return;

	// Get what battery_ah SHOULD be based on table/voltage
	let table_soc = soc_table_get_raw_soc();
	if (isNaN(table_soc)) return;

	let table_ah = si.battery_capacity * (table_soc / 100.0);
	let checkpoint_ah = si.battery_ah;

	dprintf(dlevel, "Validating checkpoint: checkpoint_ah: %.1f, table_ah: %.1f\n", checkpoint_ah, table_ah);

	// If checkpoint is NaN, just use table
	if (isNaN(checkpoint_ah)) {
		si.battery_ah = table_ah;
		dprintf(dlevel, "Checkpoint was NaN, using table value: %.1f Ah\n", table_ah);
		return;
	}

	// Calculate difference as percentage of capacity
	let diff_pct = Math.abs(checkpoint_ah - table_ah) / si.battery_capacity * 100.0;
	dprintf(dlevel, "Checkpoint vs table difference: %.1f%%\n", diff_pct);

	// If more than 20% different, checkpoint is garbage
	if (diff_pct > 20.0) {
		log_warning("Checkpoint (%.1f Ah / %.1f%%) vs OCV table (%.1f Ah / %.1f%%) differ by %.1f%% - using table value\n",
			checkpoint_ah, (checkpoint_ah / si.battery_capacity) * 100.0,
			table_ah, table_soc, diff_pct);
		si.battery_ah = table_ah;
	} else {
		dprintf(dlevel, "Checkpoint validated OK (%.1f%% difference)\n", diff_pct);
	}
}

function soc_main() {

	var dlevel = 1;

	dprintf(dlevel,"battery_capacity: %d\n", si.battery_capacity);
	if (!si.battery_capacity) {
		si.remain_text = "(battery_capacity not set)";
		return;
	}

	// Validate checkpoint against OCV table on first run
	validate_checkpoint();

	// Set initial battery_ah
	dprintf(dlevel,"battery_ah: %f\n", si.battery_ah);
	if (isNaN(si.battery_ah)) init_battery_ah();

	// Raw coulomb counting
	let amps = data.battery_current;
	dprintf(dlevel, "amps: %f\n", amps);
	let ah = amps * (si.interval / 3600);
	dprintf(dlevel, "ah: %f\n", ah);
	si.battery_ah -= ah;
	dprintf(dlevel, "battery_ah after coulomb: %f\n", si.battery_ah);

	// Cycle tracking - accumulate Ah and record voltage curve during active cycles
	if (cycle_state.in_cycle) {
		if (cycle_state.cycle_type == "charge" && ah < 0) {
			// Charging: negative ah means power going IN
			cycle_state.integrated_ah += Math.abs(ah);
		} else if (cycle_state.cycle_type == "discharge" && ah > 0) {
			// Discharging: positive ah means power going OUT
			cycle_state.integrated_ah += ah;
		}

		// Record curve points for adaptive table learning
		if (si.charge_mode != CHARGE_MODE_CV) {
			// CC/discharge: record voltage→Ah curve
			var power = data.battery_power || 0;
			if (Math.abs(power) > 100) {
				var bin = soc_voltage_bin(data.battery_voltage);
				if (bin != cycle_state.last_sample_bin) {
					cycle_state.curve_points.push({
						voltage: bin,
						ah_from_anchor: cycle_state.integrated_ah
					});
					cycle_state.last_sample_bin = bin;
				}
			}
		} else if (cycle_state.cycle_type == "charge") {
			// CV mode: record current→Ah curve (current decays as battery fills)
			var cv_current = Math.abs(data.battery_current);
			if (cv_current > 1 && !isNaN(si.cv_start_ah)) {
				var cbin = current_to_bin(cv_current);
				if (cbin != cycle_state.cv_last_sample_bin) {
					var ah_since_cv = si.battery_ah - si.cv_start_ah;
					cycle_state.cv_curve_points.push({
						current_bin: cbin,
						ah_since_cv_start: ah_since_cv
					});
					cycle_state.cv_last_sample_bin = cbin;
				}
			}
		}
	}

	// Voltage-based drift correction (skip if anchor was set this interval)
	if (typeof(soc_table_correct) == "function" && !si.anchor_set_this_interval) {
		let coulomb_soc = (si.battery_ah / si.battery_capacity) * 100.0;
		soc_table_correct(coulomb_soc);
		dprintf(dlevel, "battery_ah after correction: %f\n", si.battery_ah);
	}
	si.anchor_set_this_interval = false;

	// Clamp to valid range
	if (isNaN(si.battery_ah)) si.battery_ah = si.charge_start_ah;
	if (si.battery_ah < 0) si.battery_ah = 0;
	if (si.battery_ah > si.battery_capacity) si.battery_ah = si.battery_capacity;

	// Checkpoint in case restart
	soc_checkpoint();

	// Calculate SOC
	let new_soc = (si.battery_ah / si.battery_capacity) * 100.0;
	dprintf(dlevel,"new_soc: %.1f\n", new_soc);
	if (new_soc > 100.0) new_soc = 100.0;
	si.soc = si.battery_level = new_soc;

	dprintf(dlevel,"battery level: %f\n", si.battery_level);

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
