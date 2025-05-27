
function cycle_init() {

	let s = 0;
	CYCLE_STATE_STOPPED = s++;
	CYCLE_STATE_WAIT_PUMP = s++;
	CYCLE_STATE_RUNNING = s++;

	let props = [
		[ "temp_sensor", DATA_TYPE_STRING, "", 0 ],
		[ "cycle_start", DATA_TYPE_INT, 0, 0 ],
		[ "cycle_interval", DATA_TYPE_INT, 1800, 0 ],
		[ "cycle_duration", DATA_TYPE_INT, 180, 0 ],
	];

	config.add_props(ac,props,ac.driver_name);
	ac.cycle_state = CYCLE_STATE_STOPPED;
	ac.temp = INVALID_TEMP;
	ac.humidity = 0;
	ac.pressure = 0;
}

function cycle_statestr(state) {

	let dlevel = 4;

	let str = "";
	dprintf(dlevel,"state: %d\n", state);
	switch(state) {
	case CYCLE_STATE_STOPPED:
		str = "Stopped";
		break;
	case CYCLE_STATE_WAIT_PUMP:
		str = "Wait pump";
		break;
	case CYCLE_STATE_RUNNING:
		str = "Running";
		break;
	default:
		str = sprintf("Unknown(%d)",state);
		break;
	}

	dprintf(dlevel,"returning: %s\n", str);
	return str;
}

function cycle_main() {

	let dlevel = 1;

	// ALWAYS get the temp sensor
	dprintf(dlevel,"temp_sensor length: %d\n", ac.temp_sensor.length);
	if (ac.temp_sensor.length) {
		let r = get_sensor(ac.temp_sensor);
		dprintf(dlevel,"r: %s\n", r);
//		dumpobj(r);
		if (r && r.status == 0) {
			ac.temp = r.temp;
			ac.humidity = r.humidity;
			ac.pressure = r.pressure;
		}
	}

	dprintf(dlevel,"interval: %d\n", ac.cycle_interval);
	if (ac.cycle_interval < 1) return;

	// Dont bother if we dont have a valid temp
	dprintf(dlevel,"temp: %s\n", ac.temp);
	if (ac.temp == INVALID_TEMP) return;

	// Only cycle when below freezing
	if (ac.temp >= ac.cycle_start) return;

	let cycle_interval = ac.cycle_interval;
	let m = ac.temp / ac.cycle_start;
	dprintf(dlevel,"m: %f\n", m);
	cycle_interval = parseInt(cycle_interval * m);
	if (cycle_interval < 0) cycle_interval = 0;
	dprintf(dlevel,"NEW cycle_interval: %d\n", cycle_interval);

	let diff;
	dprintf(dlevel,"current_time: %s\n", new Date(time() * 1000));
	for(let name in pumps) {
		dprintf(dlevel,"pump: %s\n", name)
		let pump = pumps[name];
		dprintf(dlevel,"pump: direct: %s, enabled: %s, state: %s\n",
			pump.direct, pump.enabled, pump_statestr(pump.state));
                if (pump.direct || !pump.enabled || pump.error) continue;
		if (pump_is_primer(name)) continue;
		if (typeof(pump.cycle_state) == "undefined") pump.cycle_state = CYCLE_STATE_STOPPED;
		dprintf(dlevel,"cycle_state: %s\n", cycle_statestr(pump.cycle_state));
		switch(pump.cycle_state) {
		case CYCLE_STATE_STOPPED:
			dprintf(dlevel,"cycle_time: %s\n", new Date(pump.cycle_time * 1000));
			diff = time() - pump.cycle_time;
			dprintf(dlevel,"diff: %d\n", diff);
			if (diff >= cycle_interval) {
				dprintf(dlevel,"*** STARTING PUMP: %s ***\n", name);
				pump_start(name);
				pump.cycle_state = CYCLE_STATE_WAIT_PUMP;
			}
			break;
		case CYCLE_STATE_WAIT_PUMP:
			if (pump.state == PUMP_STATE_RUNNING) {
				pump.cycle_state = SAMPLE_STATE_RUNNING;
				pump.cycle_start_time = time();
			}
			break;
		case CYCLE_STATE_RUNNING:
			dprintf(dlevel,"cycle_start_time: %d\n", pump.cycle_start_time);
			diff = time() - pump.cycle_start_time;
			dprintf(dlevel,"diff: %d, cycle_duration: %d\n", diff, ac.cycle_duration);
			// Only stop if duration is exceeded or
			// the time it takes to start the pump is less than the interval
			let adj = ac.cycle_duration+pump.wait_time;
			dprintf(dlevel,"cycle_interval: %d, pump.wait_time: %d, adjusted duration: %d\n",
				cycle_interval, pump.wait_time, adj);
			if (diff >= ac.cycle_duration && cycle_interval > adj) {
				pump_stop(name);
				pump.cycle_state = CYCLE_STATE_STOPPED;
				pump.cycle_time = time();
//				jconfig_save("pump",pumps,pump_props);
			}
			break;
		}
	}
	return 0;
}
