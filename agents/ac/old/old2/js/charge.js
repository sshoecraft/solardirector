
function charge_mode_event_handler(name,module,action) {

	let dlevel = 1;

        dprintf(dlevel,"name: %s, my name: %s, module: %s, action: %s\n", name, ac.name, module, action);
        if (name != ac.name) return;

	if (module == "Mode" && action == "Set") {
		dprintf(-1,"new mode: %s\n", ac_modestr(ac.mode));
	}
}

function charge_init() {

	let s = 0;
	CHARGE_STATE_STOPPED = s++;
	CHARGE_STATE_START = s++;
	CHARGE_STATE_WAIT_START = s++;
	CHARGE_STATE_RUNNING = s++;
	CHARGE_STATE_STOP = s++;

	let props = [
		[ "water_temp", DATA_TYPE_INT, INVALID_TEMP, CONFIG_FLAG_PRIVATE ],
		[ "cool_high_temp", DATA_TYPE_INT, 60, 0 ],
		[ "cool_low_temp", DATA_TYPE_INT, 35, 0 ],
		[ "heat_high_temp", DATA_TYPE_INT, 125, 0 ],
		[ "heat_low_temp", DATA_TYPE_INT, 100, 0 ],
		[ "charge_threshold", DATA_TYPE_INT, 5, 0 ],
		[ "charge_priority_scale", DATA_TYPE_INT, 100, 0 ],
	];
	config.add_props(ac,props);
	ac.water_temp_time = 0;

	event.handler(charge_mode_event_handler,ac.name,"Mode","Set");
}

function charge_statestr(state) {

	let str = "";
	switch(state) {
	case CHARGE_STATE_STOPPED:
		str = "Stopped";
		break;
	case CHARGE_STATE_WAIT_START:
		str = "Wait Start";
		break;
	case CHARGE_STATE_START:
		str = "Start";
		break;
	case CHARGE_STATE_RUNNING:
		str = "Running";
		break;
	case CHARGE_STATE_STOP:
		str = "Stop";
		break;
	default:
		str = sprintf("Unknown(%s)",state);
		break;
	}
	return str;
}

function charge_get_pri() {
	let pri = 100;
	if (ac.mode == AC_MODE_COOL)
		pri = parseInt(((ac.cool_high_temp - ac.water_temp) / (ac.cool_high_temp - ac.cool_low_temp)) * ac.charge_priority_scale);
	else
		pri = parseInt(((ac.water_temp - ac.heat_low_temp) / (ac.heat_high_temp - ac.heat_low_temp)) * ac.charge_priority_scale);
	if (pri < 1) pri = 1;
	else if (pri > 100) pri = 100;
	return pri;
}

function charge_main() {

	let dlevel = 1;

	dprintf(dlevel,"water_temp: %s\n", ac.water_temp);
	if (ac.water_temp == INVALID_TEMP) return;

/*
	// Should be updated once/second via run.js so interval is plenty
	dprintf(dlevel,"water_temp_time: %s\n", new Date(ac.water_temp_time * 1000));
	let diff = time() - ac.water_temp_time;
	dprintf(dlevel,"diff: %d, interval: %d\n", diff, ac.interval);
	let water_temp = ac.water_temp;
	if (diff > ac.interval) {
		water_temp = ac.mode == AC_MODE_COOL ? ac.cool_low_temp : ac.heat_high_temp;
		dprintf(dlevel,"NEW water_temp: %.1f\n", water_temp);
	}
*/

	let low_thresh = ac.cool_low_temp+ac.charge_threshold;
	let high_thresh = ac.heat_high_temp-ac.charge_threshold;
	dprintf(dlevel,"low_thresh: %d, high_thresh: %d\n", low_thresh, high_thresh);

	dprintf(dlevel,"mode: %s, cool_low_temp: %d, heat_high_temp: %d\n", ac_modestr(ac.mode), low_thresh, high_thresh);
	let do_start = (ac.mode == AC_MODE_COOL && ac.water_temp >= low_thresh || ac.mode == AC_MODE_HEAT && ac.water_temp <= high_thresh ? true : false);
	let do_stop = (ac.mode == AC_MODE_COOL && ac.water_temp <= ac.cool_low_temp || ac.mode == AC_MODE_HEAT && ac.water_temp >= ac.heat_high_temp ? true : false);
	dprintf(dlevel,"do_start: %s, do_stop: %s\n", do_start, do_stop);

	for(let name in units) {
		let unit = units[name];
		dprintf(dlevel,"unit[%s]: direct: %s, enabled: %s, error: %d\n", name, unit.direct, unit.enabled, unit.error);
		if (unit.direct || !unit.enabled || unit.error) {
			unit.charging = false;
			unit.charge_state = CHARGE_STATE_STOPPED;
			continue;
		}

		// This is a fall-through switch - on purpose!
		dprintf(dlevel,"unit[%s]: charge_state: %s\n", name, charge_statestr(unit.charge_state));
		switch(unit.charge_state) {
		case CHARGE_STATE_STOPPED:
			if (!do_start) continue;
			charge_state = CHARGE_STATE_START;
		case CHARGE_STATE_START:
			// Set the charge priority of the unit
			unit.charge_priority = charge_get_pri();
			dprintf(dlevel,"charge_priority: %d\n", unit.charge_priority);
			unit.charging = true;
			unit.charge_state = CHARGE_STATE_WAIT_START;
			if (unit_start(name)) {
				unit.charge_state = CHARGE_STATE_STOPPED;
				continue;
			}
			unit.charge_start_time = time();
		case CHARGE_STATE_WAIT_START:
			dprintf(dlevel,"unit[%s]: state: %s\n", name, unit_statestr(unit.state));
			if (unit.state != UNIT_STATE_RUNNING) {
				// If another unit cooled/heated while we waiting...
				if (do_stop) {
					unit.charge_state = CHARGE_STATE_STOP;
				} else {
					// Keep the priority updated since it may have changed
					unit.charge_priority = charge_get_pri();
					dprintf(dlevel,"charge_priority: %d\n", unit.charge_priority);
				}
				continue;
			}
			unit.charge_state = CHARGE_STATE_RUNNING;
			unit.charge_start = time();
		case CHARGE_STATE_RUNNING:
			if (unit.state == UNIT_STATE_RUNNING && !do_stop) continue;
			unit.charge_state = CHARGE_STATE_STOP;
		case CHARGE_STATE_STOP:
			dprintf(dlevel,"min_runtime: %d\n", unit.min_runtime);
			if (unit.min_runtime) {
				let diff = time() - unit.charge_start;
				dprintf(dlevel,"diff: %d\n", diff);
				if (diff < unit.min_runtime) continue;
			}
			unit_stop(name);
			unit.charge_state = CHARGE_STATE_STOPPED;
			unit.charging = false;
		}
	}
}
