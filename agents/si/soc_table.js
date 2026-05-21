
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

/*
 * Adaptive SOC Correction (EV-style continuous)
 *
 * PHILOSOPHY: Coulomb counting (battery_ah) is the SOURCE OF TRUTH for SOC.
 * The adaptive table is the battery MODEL. Every interval, we compare coulomb-
 * counted battery_ah against the model's expected value and apply a small
 * correction — no threshold, no minimum drift. Like an EKF in an EV.
 *
 * Correction modes by operating state:
 *   CC charge:   voltage → expected Ah (from charge table)
 *   Discharge:   voltage → expected Ah (from discharge table)
 *   At rest:     OCV → expected Ah (average of both tables, highest confidence)
 *   CV charge:   current decay → expected Ah (from CV current table)
 *
 * The adaptive table is updated from complete anchor-to-anchor cycles.
 * Each voltage/current bin has its own Kalman filter that smooths day-to-day
 * variation from temperature, load, and battery aging.
 *
 * The correction gain is small enough that individual corrections are invisible
 * on a graph, but over minutes the estimate converges. battery_ah should arrive
 * naturally close to anchor values without visible jumps.
 */

// Static tables loaded from ah_table.dat (used to seed adaptive table on first run)
var CHARGE_TABLE = [];
var DISCHARGE_TABLE = [];
var ah_table_loaded = false;
var ah_table_metadata = {};

// Adaptive table with per-bin Kalman state
var adaptive_table = {
	charge: {},		// keyed by voltage bin string, e.g. "45.00", "45.25"
	discharge: {},		// keyed by voltage bin string
	cv: {},			// keyed by current bin string, e.g. "200", "195", "190"
	cycle_count: { charge: 0, discharge: 0 },
	last_updated: ""
};
var adaptive_table_loaded = false;

// Kalman filter constants for per-bin updates
var KALMAN_R = 25;	// Process noise: battery changes slowly between cycles (~5 Ah drift)
var KALMAN_Q = 100;	// Measurement noise: individual cycles are noisy (~10 Ah uncertainty)

// Voltage bin size
var VOLTAGE_BIN_SIZE = 0.25;

// CV current bin size (amps)
var CV_CURRENT_BIN_SIZE = 5;

/*
 * Snap a voltage to the nearest bin
 */
function voltage_to_bin(v) {
	return (Math.round(v / VOLTAGE_BIN_SIZE) * VOLTAGE_BIN_SIZE).toFixed(2);
}

/*
 * Snap a current to the nearest bin (for CV table)
 */
function current_to_bin(amps) {
	return (Math.round(amps / CV_CURRENT_BIN_SIZE) * CV_CURRENT_BIN_SIZE).toFixed(0);
}

/*
 * Kalman update for a single voltage bin
 * Inline implementation (not using KalmanFilter objects) for serializability
 */
function kalman_update_bin(bin_entry, observation) {

	var dlevel = 1;

	if (isNaN(bin_entry.kf_x)) {
		// First observation: initialize
		bin_entry.kf_x = observation;
		bin_entry.kf_cov = KALMAN_Q;
		dprintf(dlevel, "Kalman init: x=%.1f, cov=%.1f\n", bin_entry.kf_x, bin_entry.kf_cov);
	} else {
		// Predict (A=1, B=0, so prediction = prior state)
		var pred_x = bin_entry.kf_x;
		var pred_cov = bin_entry.kf_cov + KALMAN_R;

		// Update
		var K = pred_cov / (pred_cov + KALMAN_Q);
		bin_entry.kf_x = pred_x + K * (observation - pred_x);
		bin_entry.kf_cov = (1 - K) * pred_cov;

		dprintf(dlevel, "Kalman update: obs=%.1f, K=%.3f, x=%.1f -> %.1f\n",
			observation, K, pred_x, bin_entry.kf_x);
	}

	bin_entry.ah_estimate = bin_entry.kf_x;
	bin_entry.samples++;
}

/*
 * Fold a completed cycle's curve points into the adaptive table
 * Called from soc.js when a full anchor-to-anchor cycle completes
 */
function fold_cycle_into_table(cycle_type, curve_points) {

	var dlevel = 0;

	if (!curve_points || curve_points.length == 0) {
		dprintf(dlevel, "fold_cycle: no curve points to fold\n");
		return false;
	}

	var table = (cycle_type == "charge") ? adaptive_table.charge : adaptive_table.discharge;

	dprintf(dlevel, "Folding %d points into %s table\n", curve_points.length, cycle_type);

	for (var i = 0; i < curve_points.length; i++) {
		var pt = curve_points[i];
		var key = pt.voltage;

		if (!table[key]) {
			table[key] = {
				ah_estimate: NaN,
				kf_x: NaN,
				kf_cov: NaN,
				samples: 0
			};
		}

		kalman_update_bin(table[key], pt.ah_from_anchor);
	}

	adaptive_table.cycle_count[cycle_type]++;
	adaptive_table.last_updated = new Date().toISOString();

	dprintf(dlevel, "Adaptive table updated: %s cycle %d, %d bins\n",
		cycle_type, adaptive_table.cycle_count[cycle_type], curve_points.length);

	save_adaptive_table();
	return true;
}

/*
 * Fold CV current→Ah curve points into the adaptive CV table
 * Called from soc.js when a charge cycle with CV phase completes
 */
function fold_cv_into_table(cv_curve_points) {

	var dlevel = 0;

	if (!cv_curve_points || cv_curve_points.length == 0) {
		dprintf(dlevel, "fold_cv: no CV curve points to fold\n");
		return false;
	}

	if (!adaptive_table.cv) adaptive_table.cv = {};

	dprintf(dlevel, "Folding %d CV points into CV table\n", cv_curve_points.length);

	for (var i = 0; i < cv_curve_points.length; i++) {
		var pt = cv_curve_points[i];
		var key = pt.current_bin;

		if (!adaptive_table.cv[key]) {
			adaptive_table.cv[key] = {
				ah_estimate: NaN,
				kf_x: NaN,
				kf_cov: NaN,
				samples: 0
			};
		}

		kalman_update_bin(adaptive_table.cv[key], pt.ah_since_cv_start);
	}

	dprintf(dlevel, "CV table updated: %d bins\n", cv_curve_points.length);

	// Saved as part of the main adaptive_table save
	return true;
}

/*
 * Look up Ah from the adaptive table at a given voltage
 * Returns the Kalman-filtered ah_estimate with linear interpolation between bins
 */
function lookup_adaptive_ah(table_obj, voltage) {

	var dlevel = 2;

	// Collect all valid bins and sort by voltage
	var keys = [];
	for (var k in table_obj) {
		if (table_obj.hasOwnProperty(k) && !isNaN(table_obj[k].ah_estimate)) {
			keys.push(parseFloat(k));
		}
	}

	if (keys.length == 0) return NaN;

	keys.sort(function(a, b) { return a - b; });

	// Below lowest bin
	if (voltage <= keys[0]) {
		return table_obj[keys[0].toFixed(2)].ah_estimate;
	}

	// Above highest bin
	if (voltage >= keys[keys.length - 1]) {
		return table_obj[keys[keys.length - 1].toFixed(2)].ah_estimate;
	}

	// Find bracketing bins and interpolate
	for (var i = 0; i < keys.length - 1; i++) {
		if (voltage >= keys[i] && voltage <= keys[i + 1]) {
			var ratio = (voltage - keys[i]) / (keys[i + 1] - keys[i]);
			var ah1 = table_obj[keys[i].toFixed(2)].ah_estimate;
			var ah2 = table_obj[keys[i + 1].toFixed(2)].ah_estimate;
			var ah = ah1 + ratio * (ah2 - ah1);
			dprintf(dlevel, "lookup_adaptive: %.2fV -> %.1f Ah (interp %.2f-%.2f)\n",
				voltage, ah, keys[i], keys[i + 1]);
			return ah;
		}
	}

	return NaN;
}

/*
 * Get the maximum Ah value in an adaptive table (endpoint of the curve)
 */
function get_max_table_ah(table_obj) {
	var max_ah = 0;
	for (var k in table_obj) {
		if (table_obj.hasOwnProperty(k) && !isNaN(table_obj[k].ah_estimate)) {
			if (table_obj[k].ah_estimate > max_ah) {
				max_ah = table_obj[k].ah_estimate;
			}
		}
	}
	return max_ah;
}

/*
 * Look up Ah-since-CV-start from the CV current table
 * Current decays monotonically during CV: high current = early, low = near full
 * Returns interpolated ah_estimate
 */
function lookup_adaptive_cv_ah(current_amps) {

	var dlevel = 2;

	var table_obj = adaptive_table.cv;
	if (!table_obj) return NaN;

	var keys = [];
	for (var k in table_obj) {
		if (table_obj.hasOwnProperty(k) && !isNaN(table_obj[k].ah_estimate)) {
			keys.push(parseFloat(k));
		}
	}

	if (keys.length == 0) return NaN;

	keys.sort(function(a, b) { return a - b; });

	// Below lowest current bin (past end of recorded data)
	if (current_amps <= keys[0]) {
		return table_obj[keys[0].toFixed(0)].ah_estimate;
	}

	// Above highest current bin (just started CV)
	if (current_amps >= keys[keys.length - 1]) {
		return table_obj[keys[keys.length - 1].toFixed(0)].ah_estimate;
	}

	// Interpolate between bins
	for (var i = 0; i < keys.length - 1; i++) {
		if (current_amps >= keys[i] && current_amps <= keys[i + 1]) {
			var ratio = (current_amps - keys[i]) / (keys[i + 1] - keys[i]);
			var ah1 = table_obj[keys[i].toFixed(0)].ah_estimate;
			var ah2 = table_obj[keys[i + 1].toFixed(0)].ah_estimate;
			var ah = ah1 + ratio * (ah2 - ah1);
			dprintf(dlevel, "lookup_cv: %.0fA -> %.1f Ah (interp %.0f-%.0f)\n",
				current_amps, ah, keys[i], keys[i + 1]);
			return ah;
		}
	}

	return NaN;
}

/*
 * Get expected battery_ah during CC charging (voltage-based)
 */
function get_charge_expected_ah(voltage) {
	var ah_from_empty = lookup_adaptive_ah(adaptive_table.charge, voltage);
	if (isNaN(ah_from_empty)) return NaN;
	var max_ah = get_max_table_ah(adaptive_table.charge);
	if (max_ah <= 0) return NaN;
	var fraction = ah_from_empty / max_ah;
	return si.charge_start_ah + fraction * (si.charge_end_ah - si.charge_start_ah);
}

/*
 * Get expected battery_ah during discharge (voltage-based)
 */
function get_discharge_expected_ah(voltage) {
	var ah_from_full = lookup_adaptive_ah(adaptive_table.discharge, voltage);
	if (isNaN(ah_from_full)) return NaN;
	var max_ah = get_max_table_ah(adaptive_table.discharge);
	if (max_ah <= 0) return NaN;
	var fraction = ah_from_full / max_ah;
	return si.charge_end_ah - fraction * (si.charge_end_ah - si.charge_start_ah);
}

/*
 * Get expected battery_ah at rest (OCV — average both tables)
 * At rest, OCV is the best SOC indicator. Charge table voltages are slightly
 * higher than OCV (under load), discharge slightly lower. Averaging cancels
 * some of this bias.
 */
function get_rest_expected_ah(voltage) {
	var charge_ah = get_charge_expected_ah(voltage);
	var discharge_ah = get_discharge_expected_ah(voltage);

	if (!isNaN(charge_ah) && !isNaN(discharge_ah)) {
		return (charge_ah + discharge_ah) / 2.0;
	}
	if (!isNaN(charge_ah)) return charge_ah;
	if (!isNaN(discharge_ah)) return discharge_ah;
	return NaN;
}

/*
 * Get expected battery_ah during CV phase (current-based)
 * During CV, voltage is held constant — useless for SOC. But current decays
 * as the battery fills, so current level predicts progress through CV.
 */
function get_cv_expected_ah() {
	if (!adaptive_table.cv) return NaN;
	if (typeof(si.cv_start_ah) == "undefined" || isNaN(si.cv_start_ah)) return NaN;

	var current_amps = Math.abs(data.battery_current);
	var expected_pumped = lookup_adaptive_cv_ah(current_amps);
	if (isNaN(expected_pumped)) return NaN;

	return si.cv_start_ah + expected_pumped;
}

/*
 * Check if adaptive table has data
 */
function adaptive_table_has_data() {
	var has_charge = false;
	var has_discharge = false;
	for (var k in adaptive_table.charge) {
		if (adaptive_table.charge.hasOwnProperty(k)) { has_charge = true; break; }
	}
	for (var k in adaptive_table.discharge) {
		if (adaptive_table.discharge.hasOwnProperty(k)) { has_discharge = true; break; }
	}
	return has_charge || has_discharge;
}

/*
 * Load static ah_table.dat (used to seed adaptive table on first run)
 */
function soc_table_load_static() {

	var dlevel = 1;
	var table_path = si.script_dir + "/ah_table.dat";

	try {
		var f = new File(table_path, "text");
		if (f.exists) {
			if (f.open("read")) {
				var lines = f.readAll();
				f.close();

				if (lines && lines.length > 0) {
					var content = lines.join("\n");
					var data = JSON.parse(content);

					if (data && data.charge_table && data.discharge_table) {
						CHARGE_TABLE = data.charge_table;
						DISCHARGE_TABLE = data.discharge_table;
						ah_table_metadata = data.metadata || {};
						ah_table_loaded = true;

						dprintf(0, "Loaded static Ah table from %s\n", table_path);
						dprintf(dlevel, "  Charge table: %d entries\n", CHARGE_TABLE.length);
						dprintf(dlevel, "  Discharge table: %d entries\n", DISCHARGE_TABLE.length);
						return true;
					}
				}
			}
		} else {
			dprintf(dlevel, "Static table %s not found (optional)\n", table_path);
		}
	} catch(e) {
		dprintf(dlevel, "Could not load static table: %s\n", e);
	}

	return false;
}

/*
 * Seed adaptive table from static ah_table.dat
 * Uses high initial covariance so first real cycle observation dominates
 */
function seed_from_static_table() {

	var dlevel = 0;

	if (!ah_table_loaded) return false;

	var SEED_COVARIANCE = 200;	// High uncertainty for seeded values

	// Convert charge table entries
	for (var i = 0; i < CHARGE_TABLE.length; i++) {
		var entry = CHARGE_TABLE[i];
		var key = voltage_to_bin(entry.voltage);
		adaptive_table.charge[key] = {
			ah_estimate: entry.ah_from_empty,
			kf_x: entry.ah_from_empty,
			kf_cov: SEED_COVARIANCE,
			samples: 1
		};
	}

	// Convert discharge table entries
	for (var i = 0; i < DISCHARGE_TABLE.length; i++) {
		var entry = DISCHARGE_TABLE[i];
		var key = voltage_to_bin(entry.voltage);
		adaptive_table.discharge[key] = {
			ah_estimate: entry.ah_from_full,
			kf_x: entry.ah_from_full,
			kf_cov: SEED_COVARIANCE,
			samples: 1
		};
	}

	adaptive_table.last_updated = new Date().toISOString();

	dprintf(dlevel, "Seeded adaptive table from static table (%d charge, %d discharge bins)\n",
		CHARGE_TABLE.length, DISCHARGE_TABLE.length);

	return true;
}

/*
 * Load adaptive table from file
 */
function load_adaptive_table() {

	var dlevel = 1;
	var table_path = si.script_dir + "/adaptive_table.dat";

	try {
		var f = new File(table_path, "text");
		if (f.exists) {
			if (f.open("read")) {
				var lines = f.readAll();
				f.close();

				if (lines && lines.length > 0) {
					var content = lines.join("\n");
					var loaded = JSON.parse(content);

					if (loaded && typeof loaded === 'object') {
						if (loaded.charge) adaptive_table.charge = loaded.charge;
						if (loaded.discharge) adaptive_table.discharge = loaded.discharge;
						if (loaded.cv) adaptive_table.cv = loaded.cv;
						if (loaded.cycle_count) adaptive_table.cycle_count = loaded.cycle_count;
						if (loaded.last_updated) adaptive_table.last_updated = loaded.last_updated;

						adaptive_table_loaded = true;
						dprintf(0, "Loaded adaptive table from %s\n", table_path);
						dprintf(dlevel, "  Charge cycles: %d, Discharge cycles: %d\n",
							adaptive_table.cycle_count.charge,
							adaptive_table.cycle_count.discharge);
						dprintf(dlevel, "  Last updated: %s\n", adaptive_table.last_updated);
						return true;
					}
				}
			}
		}
	} catch(e) {
		log_warning("Could not load adaptive table: %s\n", e);
	}

	dprintf(dlevel, "No adaptive table found\n");
	return false;
}

/*
 * Save adaptive table to file (atomic write)
 */
function save_adaptive_table() {

	var table_path = si.script_dir + "/adaptive_table.dat";
	var tmp_path = table_path + ".tmp";

	try {
		var f = new File(tmp_path, "text");
		if (f.open("write")) {
			var json = JSON.stringify(adaptive_table, null, 2);
			f.write(json);
			f.close();

			// Atomic rename
			var cmd = sprintf("mv '%s' '%s'", tmp_path, table_path);
			exec(cmd);

			return true;
		}
	} catch(e) {
		log_error("Could not save adaptive table: %s\n", e);
	}

	return false;
}

/*
 * Initialize the adaptive SOC table system
 */
function soc_table_init() {

	var dlevel = 1;

	// Add configuration properties for continuous correction
	var props = [
		[ "correction_gain", DATA_TYPE_DOUBLE, "0.01", 0 ],
		[ "cc_correction_confidence", DATA_TYPE_DOUBLE, "1.0", 0 ],
		[ "discharge_correction_confidence", DATA_TYPE_DOUBLE, "0.8", 0 ],
		[ "rest_correction_confidence", DATA_TYPE_DOUBLE, "1.5", 0 ],
		[ "cv_correction_confidence", DATA_TYPE_DOUBLE, "0.5", 0 ],
	];
	config.add_props(si, props, si.driver_name);

	// Try to load existing adaptive table
	if (!load_adaptive_table()) {
		// No adaptive table yet — try to seed from static table
		soc_table_load_static();
		if (ah_table_loaded) {
			seed_from_static_table();
			save_adaptive_table();
			adaptive_table_loaded = true;
		} else {
			dprintf(0, "No static table to seed from. Starting with empty adaptive table.\n");
			dprintf(0, "Drift correction will begin after first complete cycle.\n");
		}
	}

	dprintf(dlevel, "soc_table_init done, adaptive_loaded: %s\n",
		adaptive_table_loaded ? "yes" : "no");
}

/*
 * Get raw voltage-based SOC estimate (for initialization only)
 * Uses simple linear interpolation between min and max voltages
 */
function soc_table_get_raw_soc() {

	var dlevel = 2;

	if (!si_isvrange(data.battery_voltage)) return NaN;

	var v = data.battery_voltage;
	var v_min = si.min_voltage || 41.0;
	var v_max = si.max_voltage || 58.1;

	var soc = ((v - v_min) / (v_max - v_min)) * 100.0;
	soc = Math.max(0, Math.min(100, soc));

	dprintf(dlevel, "raw_soc: %.1f (linear estimate)\n", soc);
	return soc;
}

/*
 * Entry point for continuous SOC correction (called from soc.js every interval)
 */
function soc_table_correct(coulomb_soc) {
	return soc_continuous_correct(coulomb_soc);
}

/*
 * Continuous model-based SOC correction (EV-style).
 *
 * Every interval (10s), compare coulomb-counted battery_ah against
 * the adaptive table's expected value and apply a small proportional
 * correction. No threshold, no minimum drift — every measurement
 * nudges toward the model.
 *
 * The correction gain × confidence weighting controls how fast we
 * converge. With gain=0.01:
 *   After 10 min (~60 intervals): corrects ~45% of error
 *   After 30 min: ~83%
 *   After 1 hour: ~97%
 *
 * This ensures battery_ah arrives naturally close to anchor values.
 */
function soc_continuous_correct(coulomb_soc) {

	var dlevel = 1;

	// Must have adaptive table data
	if (!adaptive_table_has_data()) {
		dprintf(dlevel, "No adaptive table data, skipping correction\n");
		return false;
	}

	var voltage = data.battery_voltage;
	if (!si_isvrange(voltage)) return false;

	var expected_ah = NaN;
	var confidence = 1.0;

	// CV mode: use current decay as correction signal (voltage is constant, useless)
	if (si.charge_mode == CHARGE_MODE_CV) {
		expected_ah = get_cv_expected_ah();
		confidence = si.cv_correction_confidence;
	} else {
		var power = data.battery_power || 0;
		var is_charging = (power < -100);
		var is_discharging = (power > 100);

		if (is_charging) {
			expected_ah = get_charge_expected_ah(voltage);
			confidence = si.cc_correction_confidence;
		} else if (is_discharging) {
			expected_ah = get_discharge_expected_ah(voltage);
			confidence = si.discharge_correction_confidence;
		} else {
			// At rest: OCV is the most reliable SOC indicator
			expected_ah = get_rest_expected_ah(voltage);
			confidence = si.rest_correction_confidence;
		}
	}

	if (isNaN(expected_ah)) {
		dprintf(dlevel, "Could not determine expected_ah, skipping\n");
		return false;
	}

	// Continuous correction: nudge toward expected value
	var error = expected_ah - si.battery_ah;
	var correction = si.correction_gain * confidence * error;

	// Apply if above floating point noise
	if (Math.abs(correction) > 0.001) {
		var old_ah = si.battery_ah;
		si.battery_ah += correction;

		dprintf(dlevel, "SOC correct: %+.2f Ah (expected=%.1f, was=%.1f, conf=%.1f)\n",
			correction, expected_ah, old_ah, confidence);

		// Log significant corrections at info level
		if (Math.abs(correction) > 1.0) {
			log_info("SOC: correcting %.1f -> %.1f Ah (expected %.1f, error %.1f)\n",
				old_ah, si.battery_ah, expected_ah, error);
		}
		return true;
	}

	return false;
}
