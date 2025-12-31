
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
		[ "cool_high_temp", DATA_TYPE_INT, 60, 0 ],
		[ "cool_low_temp", DATA_TYPE_INT, 35, 0 ],
		[ "heat_high_temp", DATA_TYPE_INT, 125, 0 ],
		[ "heat_low_temp", DATA_TYPE_INT, 100, 0 ],
		[ "charge_threshold", DATA_TYPE_INT, 3, 0 ],
		[ "repri_interval", DATA_TYPE_FLOAT, 1.0, 0 ],
	];
	config.add_props(ac,props,ac.driver_name);
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

	let dlevel = 1;

	let val = 0;
	let pri = 100;
	if (ac.mode == AC_MODE_COOL) {
		val = ((ac.cool_high_temp - ac.water_temp) / (ac.cool_high_temp - ac.cool_low_temp));
	} else {
		val = ((ac.water_temp - ac.heat_low_temp) / (ac.heat_high_temp - ac.heat_low_temp));
	}
	dprintf(dlevel,"val: %f\n", val);
	let m = (val * 100.0) + 1;
	dprintf(dlevel,"m: %f\n", m);
	pri = parseInt(Math.ceil(m));
	dprintf(dlevel,"pri: %f\n", pri);
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
			dprintf(-1,"NEW last_pri_temp: %s\n", unit.last_pri_temp);
			unit.charging = true;
			unit.charge_state = CHARGE_STATE_WAIT_START;
			if (unit_start(name)) {
				unit.charge_state = CHARGE_STATE_STOPPED;
				continue;
			}
			unit.charge_start_time = time();
		case CHARGE_STATE_WAIT_START:
			// Keep the priority updated since it may have changed
			unit.charge_priority = charge_get_pri();
			dprintf(dlevel,"charge_priority: %d\n", unit.charge_priority);
			dprintf(dlevel,"unit[%s]: state: %s\n", name, unit_statestr(unit.state));
			if (unit.state != UNIT_STATE_RUNNING) {
				// If another unit cooled/heated while we waiting...
				if (do_stop) unit.charge_state = CHARGE_STATE_STOP;
				continue;
			}
			unit.charge_state = CHARGE_STATE_RUNNING;
			unit.last_pri_temp = ac.water_temp;
			dprintf(-1,"NEW last_pri_temp: %s\n", unit.last_pri_temp);
		case CHARGE_STATE_RUNNING:
			dprintf(dlevel,"unit[%s]: state: %s, reserve: %d\n", name, unit_statestr(unit.state), unit.reserve);
			if (unit.state == UNIT_STATE_RUNNING) {
				if (unit.priority == 0 && unit.reserve) {
					let pump = pumps[unit.pump];
					if (pump.settled && pump.temp_in >= -50 && pump.temp_in < 150) {
if (1) {
						if (unit.last_pri_temp < -50 || unit.last_pri_temp > 150) {
							unit.last_pri_temp = pump.temp_in;
							dprintf(-1,"NEW last_pri_temp: %s\n", unit.last_pri_temp);
						}
}
if (1) {
						if (typeof(ac.cic) == 'undefined') ac.cic = (60/ac.interval) + 1;
						if (ac.cic++ >= (60/ac.interval)) {
							diff = Math.abs(pump.temp_in - unit.last_pri_temp);
							log_info("charge_main: temp_in: %.1f, last_pri_temp: %.1f, diff: %.1f, interval: %.1f\n", pump.temp_in, unit.last_pri_temp, diff, ac.repri_interval);
							ac.cic = 1;
						}
}

						if ((unit.mode == AC_MODE_COOL && pump.temp_in > unit.last_pri_temp+0.3) || (unit.mode == AC_MODE_HEAT && pump.temp_in < unit.last_pri_temp-0.3)) {
							unit.last_pri_temp = pump.temp_in;
							dprintf(-1,"NEW last_pri_temp: %s\n", unit.last_pri_temp);
						}
						diff = Math.abs(pump.temp_in - unit.last_pri_temp);
						dprintf(dlevel,"pri temp diff: %.1f, repri_interval: %.1f\n", diff, ac.repri_interval);
						if (diff >= ac.repri_interval) {
							let pri = charge_get_pri();
							dprintf(dlevel,"NEW pri: %.1f, current_pri: %.1f\n", pri, unit.charge_priority);
							if (!double_equals(pri,unit.charge_priority)) {
								unit.charge_priority = pri;
								if (pa_client_repri(ac,"unit",name,unit.reserve,pri)) {
									// ANY issues with repri/sdconfig, we must stop
									do_stop = true;
								} else {
									unit.last_pri_temp = pump.temp_in;
									dprintf(-1,"NEW last_pri_temp: %s\n", unit.last_pri_temp);
								}
							} else {
								unit.last_pri_temp = pump.temp_in;
								dprintf(-1,"NEW last_pri_temp: %s\n", unit.last_pri_temp);
							}
						}
					}
				}
			} else if (unit.state == UNIT_STATE_STOPPED) {
				unit.charge_state = CHARGE_STATE_STOPPED;
			}
			dprintf(dlevel,"do_stop: %s\n", do_stop);
			if (!do_stop) continue;
			unit.charge_state = CHARGE_STATE_STOP;
		case CHARGE_STATE_STOP:
			// min_runtime is now enforced by UNIT_STATE_MIN_RUNTIME_WAIT
			unit_stop(name);
			unit.charge_state = CHARGE_STATE_STOPPED;
			unit.charging = false;
		}
	}
}
