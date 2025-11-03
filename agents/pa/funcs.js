// PA Agent Function Implementations
// Power reservation management functions

// Include helper functions
//include(script_dir+"/utils.js");

function pa_reserve(agent_name,module,item,str_amount,str_pri) {
	let dlevel = 1;

	dprintf(dlevel,"agent_name: %s, module: %s, item: %s, amount: %s, pri: %s\n", agent_name, module, item, str_amount, str_pri);

	let id = agent_name + PA_ID_DELIMITER + module + PA_ID_DELIMITER + item;
//	if (module.length) id += PA_ID_DELIMITER + module;
//	if (item.length) id += PA_ID_DELIMITER + item;
	let amount = parseFloat(str_amount);
	let pri = parseInt(str_pri);
	if (pri < 1) pri = 1;
	if (pri > 100) pri = 100;
	dprintf(dlevel,"id: %s, amount: %s, priority: %d\n", id, amount, pri);

	// Check battery level if configured
	dprintf(dlevel,"battery_level_min: %.1f, have_battery_level: %s, battery_level: %.1f\n",
		pa.battery_level_min, pa.have_battery_level, pa.battery_level);
	if (pa.battery_level_min > 0 && pa.have_battery_level && pa.battery_level < pa.battery_level_min) {
		config.errmsg = sprintf("battery level too low (%.1f%% < %.1f%%)", pa.battery_level, pa.battery_level_min);
		dprintf(dlevel,"denying reservation: %s\n", config.errmsg);
		return 1;
	}

	// If a reservation exists for the same id + amount delete it
	for(let i=0; i < pa.reservations.length; i++) {
		let res = pa.reservations[i];
		dprintf(dlevel,"check exist: res[%d]: id: %s, amount: %.1f, pri: %d\n", i, res.id, res.amount, res.pri);
		if (res.id == id && res.amount == amount) {
			pa.reserved  -= res.amount;
			pa.reservations.splice(i,1);
		}
	}

	if (!pa.budget) {
		// Deny everything until last_reserve_time + reserve_delay has passed
		dprintf(dlevel,"last_reserve_time: %s\n", pa.last_reserve_time);
		let diff = time() - pa.last_reserve_time;
		dprintf(dlevel,"diff: %d, reserve_delay: %d\n", diff, pa.reserve_delay);
		if (diff < pa.reserve_delay) {
			config.errmsg = sprintf("waiting for %d seconds before next reserve", pa.reserve_delay - diff);
			return 1;
		}
	}

	dprintf(dlevel,"budget: %d, avail: %.1f\n", pa.budget, pa.avail);

	// Check if we should use night-time approval settings
	let approve_p1 = pa.approve_p1;
	if (pa.night_budget && pa_is_night_period()) {
		approve_p1 = pa.night_approve_p1;
	}
	
	let r = 0;
	if (pa.avail >= amount || approve_p1 && pri == 1) {
		// Check P1 limit when in deficit (no-budget mode only)
		if (!pa.budget && pri == 1 && approve_p1 && pa.avail < amount && pa.limit > 0) {
			// Calculate total deficit after approval
			let current_deficit = pa.avail < 0 ? Math.abs(pa.avail) : 0;
			let additional_deficit = amount - (pa.avail > 0 ? pa.avail : 0);
			let total_deficit = current_deficit + additional_deficit;
			
			dprintf(dlevel,"P1 limit check: current_deficit: %.1f, additional: %.1f, total: %.1f, limit: %.1f\n",
				current_deficit, additional_deficit, total_deficit, pa.limit);
			
			if (total_deficit > pa.limit) {
				config.errmsg = sprintf("P1 reservation would exceed deficit limit (%.1f > %.1f)", 
					total_deficit, pa.limit);
				return 1;
			}
		}
		
		log_info("adding reservation for: id: %s, amount: %.1f, pri: %d\n", id, amount, pri);
		let newres = {};
		newres.id = id;
		newres.amount = amount;
		newres.pri = pri;
		newres.marked = false;
		pa.reservations.push(newres);
		pa.reservations.sort(function(a,b) { return a.amount - b.amount});
		pa.reserved += amount;
		pa.last_reserve_time = time();
	} else {
		config.errmsg = "denied";
		r = 1;
	}

	dprintf(dlevel,"returning: %d (msg: %s)\n", r, config.errmsg);
	return r;
}

function pa_release(agent_name,module,item,str_amount) {
	let dlevel = -1;

	let id = agent_name;
	if (module.length) id += PA_ID_DELIMITER + module;
	if (item.length) id += PA_ID_DELIMITER + item;
	let amount = parseFloat(str_amount);
	dprintf(dlevel,"id: %s, amount: %s\n", id, amount);

	let r = 0;
	dprintf(dlevel,"reservation count: %d\n", pa.reservations.length);
	for(let i=0; i < pa.reservations.length; i++) {
		let res = pa.reservations[i];
		if (res.id == id && res.amount == amount) {
			log_info("removing reservation for: id: %s, amount: %.1f\n", id, amount);
			dprintf(dlevel,"found\n");
			pa.reservations.splice(i,1);
			pa.reserved -= amount;
			// XXX no reserves until timeout
			pa.last_reserve_time = time();
			r = 0;
			break;
		}
	}

	if (r) config.errmsg = "release: reservation not found";
	return r;
}

function pa_repri(agent_name,module,item,str_amount,str_pri) {
	let dlevel = -1;

	dprintf(dlevel,"agent_name: %s, module: %s, item: %s, amount: %s, pri: %s\n", agent_name, module, item, str_amount, str_pri);

	let id = agent_name;
	if (module.length) id += PA_ID_DELIMITER + module;
	if (item.length) id += PA_ID_DELIMITER + item;
	let amount = parseFloat(str_amount);
	let pri = parseInt(str_pri);
	if (pri < 1) pri = 1;
	if (pri > 100) pri = 100;
	dprintf(dlevel,"id: %s, amount: %s, priority: %d\n", id, amount, pri);

	// If a reservation exists for the same id + amount delete it
	let found = false;
	dprintf(dlevel,"reservation count: %d\n", pa.reservations.length);
	for(let i=0; i < pa.reservations.length; i++) {
		let res = pa.reservations[i];
		dprintf(dlevel,"check exist: res[%d]: id: %s, amount: %.1f, pri: %d\n", i, res.id, res.amount, res.pri);
		if (res.id == id && res.amount == amount) {
			found = true;
			res.pri = pri;
			break;
		}
	}
	dprintf(dlevel,"found: %s\n", found);
	if (!found) {
		// Check battery level if configured (only for new reservations)
		dprintf(dlevel,"battery_level_min: %.1f, have_battery_level: %s, battery_level: %.1f\n",
			pa.battery_level_min, pa.have_battery_level, pa.battery_level);
		if (pa.battery_level_min > 0 && pa.have_battery_level && pa.battery_level < pa.battery_level_min) {
			config.errmsg = sprintf("battery level too low (%.1f%% < %.1f%%)", pa.battery_level, pa.battery_level_min);
			dprintf(dlevel,"denying new reservation via repri: %s\n", config.errmsg);
			return 1;
		}

		// Check P1 limit when in deficit (no-budget mode only)
		let approve_p1 = pa.approve_p1;
		if (pa.night_budget && pa_is_night_period()) {
			approve_p1 = pa.night_approve_p1;
		}
		
		if (!pa.budget && pri == 1 && approve_p1 && pa.avail < amount && pa.limit > 0) {
			// Calculate total deficit after approval
			let current_deficit = pa.avail < 0 ? Math.abs(pa.avail) : 0;
			let additional_deficit = amount - (pa.avail > 0 ? pa.avail : 0);
			let total_deficit = current_deficit + additional_deficit;
			
			dprintf(dlevel,"repri P1 limit check: current_deficit: %.1f, additional: %.1f, total: %.1f, limit: %.1f\n",
				current_deficit, additional_deficit, total_deficit, pa.limit);
			
			if (total_deficit > pa.limit) {
				config.errmsg = sprintf("repri P1 reservation would exceed deficit limit (%.1f > %.1f)", 
					total_deficit, pa.limit);
				return 1;
			}
		}
		
		log_info("adding reservation for: id: %s, amount: %.1f, pri: %d\n", id, amount, pri);
		let newres = {};
		newres.id = id;
		newres.amount = amount;
		newres.pri = pri;
		newres.marked = false;
		pa.reservations.push(newres);
		pa.reservations.sort(function(a,b) { return a.amount - b.amount});
		pa.reserved += amount;
		pa.last_reserve_time = time();
	}

	return 0;
}

function pa_revoke_all(res) {
	let dlevel = 2;

	for(let i=0; i < pa.reservations.length; i++) {
		let res = pa.reservations[i];
		dprintf(dlevel,"revoking: res[%d]: id: %s, amount: %.1f, pri: %d\n", i, res.id, res.amount, res.pri);
		pa.revokes.push(res);
	}
	return 0;
}
