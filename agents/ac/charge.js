
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
		[ "water_temp", DATA_TYPE_DOUBLE, INVALID_TEMP, CONFIG_FLAG_PRIVATE ],
		[ "water_temp_time", DATA_TYPE_U32, 0, CONFIG_FLAG_PRIVATE ],
		[ "cool_high_temp", DATA_TYPE_INT, 60, 0 ],
		[ "cool_low_temp", DATA_TYPE_INT, 35, 0 ],
		[ "heat_high_temp", DATA_TYPE_INT, 125, 0 ],
		[ "heat_low_temp", DATA_TYPE_INT, 100, 0 ],
		[ "charge_threshold", DATA_TYPE_INT, 5, 0 ],
		[ "charge_priority_scale", DATA_TYPE_INT, 100, 0 ],
		[ "repri_interval", DATA_TYPE_INT, 3, 0 ],
	];
	config.add_props(ac,props,ac.driver_name);
//	ac.water_temp_time = 0;
	ac.last_repri = INVALID_TEMP;

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
			unit.last_pri_temp = ac.water_temp;
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
					unit.last_pri_temp = ac.water_temp;
				}
				continue;
			}
			unit.charge_state = CHARGE_STATE_RUNNING;
			unit.charge_start = time();
		case CHARGE_STATE_RUNNING:
			dprintf(dlevel,"unit[%s]: state: %s, reserve: %d\n", name, unit_statestr(unit.state), unit.reserve);
			if (unit.state == UNIT_STATE_RUNNING) {
				if (unit.priority == 0 && unit.reserve) {
					let pump = pumps[unit.pump];
					if (pump.temp_in != INVALID_TEMP) {
						if (typeof(ac.cic) == 'undefined') ac.cic = (60/ac.interval) + 1;
						if (ac.cic++ >= (60/ac.interval)) {
							log_info("temp_in: %.1f, last_pri_temp: %.1f\n", pump.temp_in, unit.last_pri_temp);
							ac.cic = 1;
						}

						// if the temp_in is > last_pri_temp + 1 degree, do a repri
						if (unit.mode == AC_MODE_COOL && pump.temp_in > unit.last_pri_temp+0.3) unit.last_pri_temp = pump.temp_in;
						else if (unit.mode == AC_MODE_HEAT && pump.temp_in < unit.last_pri_temp-0.3) unit.last_pri_temp = pump.temp_in;
						diff = Math.abs(pump.temp_in - unit.last_pri_temp);
						dprintf(dlevel,"pri temp diff: %.1f, repri_interval: %.1f\n", diff, ac.repri_interval);
						if (diff >= ac.repri_interval) {
							let pri = charge_get_pri();
							dprintf(dlevel,"NEW pri: %d, current_pri: %d\n", pri, unit.charge_priority);
							if (pri != unit.charge_priority) {
								unit.charge_priority = pri;
								if (!pa_client_repri(ac,"unit",name,unit.reserve,pri)) {
									unit.last_pri_temp = pump.temp_in;
								}
							}
						}
					}
				}
			} else if (unit.state == UNIT_STATE_STOPPED) {
				unit.charge_state = CHARGE_STATE_STOPPED;
			}
			if (!do_stop) continue;
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
