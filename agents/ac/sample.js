
function sample_init() {

	let s = 0;
	SAMPLE_STATE_STOPPED = s++;
	SAMPLE_STATE_WAIT_PUMP = s++;
	SAMPLE_STATE_RUNNING = s++;

	let props = [
		[ "sample_interval", DATA_TYPE_INT, 0, 0 ],
		[ "sample_duration", DATA_TYPE_INT, 180, 0 ],
		[ "sample_state", DATA_TYPE_INT, SAMPLE_STATE_STOPPED, CONFIG_FLAG_PRIVATE ],
		// Times must be unsigned 32-bit
		[ "sample_time", DATA_TYPE_U32, 0, CONFIG_FLAG_PRIVATE ],
	];

	config.add_props(ac,props,ac.driver_name);
	ac.sample_pump = "";
	ac.sample_state = SAMPLE_STATE_STOPPED;
}

function sample_statestr(state) {

	let dlevel = 4;

	let str = "";
	dprintf(dlevel,"state: %d\n", state);
	switch(state) {
	case SAMPLE_STATE_STOPPED:
		str = "Stopped";
		break;
	case SAMPLE_STATE_WAIT_PUMP:
		str = "Wait pump";
		break;
	case SAMPLE_STATE_RUNNING:
		str = "Running";
		break;
	default:
		str = sprintf("Unknown(%d)",state);
		break;
	}

	dprintf(dlevel,"returning: %s\n", str);
	return str;
}

function sample_select_pump() {

	let dlevel = 1;

	ac.sample_pump = "";
	for(let name in units) {
		let unit = units[name];
		dprintf(dlevel,"unit: %s, pump: %s\n", name, unit.pump);
		let pump = pumps[unit.pump];
		dprintf(dlevel,"pump: direct: %s, enabled: %s, state: %s\n",
			pump.direct, pump.enabled, pump_statestr(pump.state));
		if (pump.direct || !pump.enabled || pump.error) continue;
		// XXX needs to have a temp_in sensor
		if (!pump.temp_in_sensor.length) continue;
		ac.sample_pump = unit.pump;
		break;
	}
	dprintf(dlevel,"selected pump: %s\n", ac.sample_pump);
}

function sample_main() {

	let dlevel = 1;

	if (ac.sample_interval < 1) return;

	dprintf(dlevel,"current_time: %s\n", new Date(time() * 1000));
	let pump,diff;
	dprintf(dlevel,"sample_state: %s\n", sample_statestr(ac.sample_state));
	switch(ac.sample_state) {
	case SAMPLE_STATE_STOPPED:
		dprintf(dlevel,"water_temp_time: %s\n", new Date(ac.water_temp_time * 1000));
		dprintf(dlevel,"sample_time: %s\n", new Date(ac.sample_time * 1000));
		let ref = ac.water_temp_time > ac.sample_time ? ac.water_temp_time : ac.sample_time;
		diff = time() - ref;
		dprintf(dlevel,"diff: %d, sample_interval: %d\n", diff, ac.sample_interval);
		if (diff >= ac.sample_interval) {
			sample_select_pump();
			if (ac.sample_pump.length) {
				dprintf(dlevel,"*** STARTING SAMPLE PUMP: %s ***\n", ac.sample_pump);
				ac.signal("Sample "+ac.sample_pump,"Start");
				if (!pump_start(ac.sample_pump)) {
					ac.sample_state = SAMPLE_STATE_WAIT_PUMP;
				}
			}
		}
		break;
	case SAMPLE_STATE_WAIT_PUMP:
		pump = pumps[ac.sample_pump];
		dprintf(dlevel,"pump_state: %s\n", pump_statestr(pump.state));
		if (pump.state == PUMP_STATE_RUNNING) {
			ac.sample_state = SAMPLE_STATE_RUNNING;
			ac.sample_start_time = time();
		}
		break;
	case SAMPLE_STATE_RUNNING:
		// Wait until the pump settles or sample_duration is exceeded
		dprintf(dlevel,"sample_start_time: %d\n", ac.sample_start_time);
		diff = time() - ac.sample_start_time;
		pump = pumps[ac.sample_pump];
		dprintf(dlevel,"settled: %s, diff: %d, sample_duration: %d\n", pump.settled, diff, ac.sample_duration);
		if (pump.settled || diff >= ac.sample_duration) {
			pump_stop(ac.sample_pump);
			ac.sample_state = SAMPLE_STATE_STOPPED;
			ac.sample_time = time();
			ac.signal("Sample "+ac.sample_pump,"Stop");
//			config.save();
			// update cycle_time for the pump too
			pumps[ac.sample_pump].cycle_time = time();
//			jconfig_save("pump",pumps,pump_props);
			ac.sample_pump = "";
		}
		break;
	}
}
