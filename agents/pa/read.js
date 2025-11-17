
function read_main() {
	let dlevel = 1;
	dprintf(dlevel,"*** PA READ ***\n");

	// Send out revocations
	dprintf(dlevel,"revokes.length: %d\n", pa.revokes.length);
	for(let i = 0; i < pa.revokes.length; i++ ) {
		pa_revoke(pa.revokes[i]);
		pa.revokes.splice(i,1);
	}

	// Process mqtt messages
	pa.have_grid_power = pa.have_charge_mode = pa.have_pv_power = pa.have_battery_power = false;
	pa_process_messages("inverter",pa.inverter_mqtt,pa.inverter_topic,pa_get_inverter_data);
	pa_process_messages("pvc",pa.pv_mqtt,pa.pv_topic,pa_get_pv_data);
	pa_process_messages("battery",pa.battery_mqtt,pa.battery_topic,pa_get_battery_data);

	// If freq not set, use nominal
	if (!pa.have_frequency) pa.frequency = pa.nominal_frequency;

	// Handle night/day mode transitions
	let is_night = pa_is_night_period();

	// Initialize mode if undefined
	if (typeof(pa.mode) == "undefined") {
		pa.mode = is_night ? "night" : "day";
		log_info("Mode initialized to: %s\n", pa.mode);
	}

	// Update mode based on time period
	if (is_night && pa.mode == "day") {
		pa.mode = "night";
		log_info("Night mode activated\n");
	} else if (!is_night && pa.mode == "night") {
		pa.mode = "day";
		log_info("Day mode activated\n");
	}

	// Determine which budget to use based on mode
	let budget = (pa.mode == "night" && pa.night_budget > 0) ? pa.night_budget : pa.budget;

	dprintf(dlevel,"mode: %s, pa.budget: %.1f, pa.night_budget: %.1f, effective budget: %.1f\n",
		pa.mode, pa.budget, pa.night_budget, budget);

	dprintf(dlevel,"budget: %d\n", budget);
	if (!budget) {
		let power = 0.0;

		// if we're pulling from the grid, power is neg
		dprintf(dlevel,"have_grid_power: %s, grid_power: %s\n", pa.have_grid_power, pa.grid_power);
		if (pa.have_grid_power && pa.grid_power < 0.0) {
			power += pa.grid_power;
			dprintf(dlevel,"NEW power(grid): %s\n", power);
		}

		// If we're pulling from the batt, power is neg
		dprintf(dlevel,"have_battery_power: %s, battery_power: %s\n", pa.have_battery_power, pa.battery_power);
		if (pa.have_battery_power && pa.battery_power < 0.0) {
			power += pa.battery_power;
			dprintf(dlevel,"NEW power(batt): %s\n", power);
		}

		// If we have pv power, subtract the load from it and add to avail power
		dprintf(dlevel,"have_pv_power: %s, pv_power: %s\n", pa.have_pv_power, pa.pv_power);
		if (pa.have_pv_power) {
			dprintf(dlevel,"have_load_power: %s, load_power: %s\n", pa.have_load_power, pa.load_power);
			if (!pa.have_load_power) {
				let used = 0.0;

				// Feeding to the grid
				dprintf(dlevel,"have_grid_power: %s, grid_power: %s\n", pa.have_grid_power, pa.grid_power);
				if (pa.have_grid_power && pa.grid_power > 0.0) {
					used += pa.grid_power;
					dprintf(dlevel,"NEW used(grid): %s\n", used);
				}
				dprintf(dlevel,"have_battery_power: %s, battery_power: %s\n", pa.have_battery_power, pa.battery_power);

				// Note: Charging batteries (battery_power > 0) is NOT considered "used" 
				// since it represents available power that could be redirected elsewhere

				pa.load_power = pa.pv_power - used;
				dprintf(dlevel,"==> load_power: %f\n", pa.load_power);
			}

			// Adjust pv_power if fspc is active
			dprintf(dlevel,"fspc: %s, frequency: %.1f, start: %.1f\n", pa.fspc, pa.frequency, pa.fspc_start_frq);
			if (pa.fspc && pa.have_frequency && pa.frequency >= pa.fspc_start_frq) {
				dprintf(dlevel,"fspc_end_frq: %s\n", pa.fspc_end_frq);
				let diff = pa.fspc_end_frq - pa.frequency;
				dprintf(dlevel,"diff: %s, pv_power: %s\n", diff, pa.pv_power);
				if (diff < 0) diff = 1;
				pa.pv_power /= diff;
				dprintf(dlevel,"NEW pv_power(fspc): %.1f\n", pa.pv_power);
			}
			if (pa.pv_power > 0.0) {
				let pv_contribution = pa.pv_power - pa.load_power;
				power += pv_contribution;
				dprintf(dlevel,"PV calculation: %.1f - %.1f = %.1f, total power: %.1f\n", pa.pv_power, pa.load_power, pv_contribution, power);
			}
			
			// Add battery charging power as available (can be redirected) unless protect_charge is enabled
			if (pa.have_battery_power && pa.battery_power > 0.0 && !pa.protect_charge) {
				power += pa.battery_power;
				dprintf(dlevel,"NEW power(+batt charging): %.1f\n", power);
			} else if (pa.have_battery_power && pa.battery_power > 0.0 && pa.protect_charge) {
				dprintf(dlevel,"battery charging power protected, not adding to available: %.1f\n", pa.battery_power);
			}
		}

		dprintf(dlevel,"==> power: %s\n", power);

		pa.values[pa.idx++] = power;
		dprintf(dlevel,"idx: %d, samples: %d\n", pa.idx, pa.samples);
		if (pa.idx > pa.samples) pa.idx = 0;
		let total = 0.0;
		for(let i=0; i < pa.samples; i++) total += pa.values[i];
		dprintf(dlevel,"total: %.1f\n", total);
		pa.avail = total / pa.samples;
		dprintf(dlevel,"averaged power: %s\n", pa.avail);

		// If charging and charge protect, set power negative
		if (pa.avail > 0.0) {
			dprintf(dlevel,"have_charge_mode: %s, charge_mode: %d\n", pa.have_charge_mode, (pa.have_charge_mode ? pa.charge_mode : 0));
			let charging;
			if (pa.have_charge_mode) {
				charging = pa.charge_mode == 1 ? true : false;
			} else if (pa.have_battery_power && pa.battery_power > 0.0) {
				charging = true;
			} else {
				charging = false;
			}
			dprintf(dlevel,"charging: %s, protect_charge: %s\n", charging, pa.protect_charge);
			if (charging && pa.protect_charge) pa.avail = -1;
		}
	} else {
		// Budget is set - calculate avail from budget
		pa.avail = budget - pa.reserved;
	}

	dprintf(dlevel,"avail: %.1f\n", pa.avail);
	if (pa.avail == 0.0) {
		dprintf(dlevel,"setting pa.avail to -1!\n");
		pa.avail = -1;
	}

	// Check battery power limits - only if we have BOTH battery and grid data this cycle
	dprintf(dlevel,"battery_hard_limit: %.1f, battery_soft_limit: %.1f, have_battery_power: %s, have_grid_power: %s\n",
		pa.battery_hard_limit, pa.battery_soft_limit, pa.have_battery_power, pa.have_grid_power);
	if (pa.have_battery_power && pa.have_grid_power && pa.battery_power < 0) {
		let battery_discharge = Math.abs(pa.battery_power);
		dprintf(dlevel,"battery_discharge: %.1f\n", battery_discharge);

		// Check if we're not pulling from grid (grid_power >= 0)
		let on_grid = pa.grid_power < 0;
		dprintf(dlevel,"on_grid: %s (grid_power: %.1f)\n", on_grid, pa.grid_power);

		if (!on_grid && pa.battery_hard_limit > 0 && battery_discharge > pa.battery_hard_limit) {
			log_warning("Battery HARD limit exceeded: %.1f W > %.1f W - revoking all reservations\n",
				battery_discharge, pa.battery_hard_limit);

			// Queue all reservations for immediate revocation
			for(let i = pa.reservations.length - 1; i >= 0; i--) {
				let res = pa.reservations[i];
				dprintf(dlevel,"queuing immediate revoke: res[%d]: id: %s, amount: %.1f, pri: %d\n",
					i, res.id, res.amount, res.pri);
				pa.revokes.push(res);
				pa.reservations.splice(i, 1);
				pa.reserved -= res.amount;
			}
		} else if (!on_grid && pa.battery_soft_limit > 0 && battery_discharge > pa.battery_soft_limit) {
			dprintf(dlevel,"Battery SOFT limit exceeded: %.1f W > %.1f W - setting avail negative\n",
				battery_discharge, pa.battery_soft_limit);
			pa.avail = -1;
		}
	}

	// If we run out of power for X seconds, start revoking
	if (pa.avail < 0) {
		dprintf(dlevel,"neg_power_time: %s, avail: %.1f, limit: %.1f, load_power: %.1f\n", pa.neg_power_time, pa.avail, pa.limit, pa.load_power);
		if (pa.neg_power_time == 0) {
			pa.neg_power_time = time();
		} else {
			let diff = time() - pa.neg_power_time;
			dprintf(dlevel,"diff: %d, timeout: %d\n", diff, pa.deficit_timeout);
			if (diff >= pa.deficit_timeout) {
				// Start revoking
				// Check if we should use night-time approval settings
				let approve_p1 = pa.approve_p1;
				if (pa.night_budget > 0 && pa.mode == "night") {
					approve_p1 = pa.night_approve_p1;
				}
				
				// Check if deficit exceeds limit
				let exceed_limit = (pa.limit > 0 && Math.abs(pa.avail) > pa.limit);
				if (exceed_limit) {
					dprintf(dlevel,"Deficit (%.1f) exceeds limit (%.1f), overriding P1 protection\n", 
						Math.abs(pa.avail), pa.limit);
				}
				
				for(let i=0; i < pa.reservations.length; i++) {
					let res = pa.reservations[i];
					dprintf(dlevel,"revocation check: res[%d]: id: %s, amount: %.1f, pri: %d\n", i, res.id, res.amount, res.pri);
					// Dont revoke p1 when approve is true UNLESS we exceed the limit
					if (res.pri == 1 && approve_p1 && !exceed_limit) continue;
					pa_revoke(res);
				}
			}
		}
	} else {
		pa.neg_power_time = 0;
	}

	let out = sprintf("reserved: %.1f, avail: %.1f", pa.reserved, pa.avail);
	if (typeof(pa.last_out) == "undefined") pa.last_out = "";
	if (out != pa.last_out) {
		log_info("%s\n", out);
		pa.last_out = out;
	}
}
