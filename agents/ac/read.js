
function read_main() {

	let dlevel = 1;

	// See if any unit pumps are running set water temp from one
	let have_temp = false;
	for (name in units) {
		let unit = units[name];
		dprintf(dlevel,"unit[%s]: pump: %s\n", name, unit.pump);
		if (!unit.pump.length) continue;
		let pump = pumps[unit.pump];
		dprintf(dlevel,"pump[%s]: %s\n", unit.pump, pump);
		if (typeof(pump) == "undefined") continue;
		// Pump settled will only be set if it has a valid temp_in sensor
		dprintf(dlevel,"pump[%s]: state: %s, settled: %s\n", unit.pump, pump_statestr(pump.state), pump.settled);
		if (pump.state == PUMP_STATE_RUNNING) {
			if (pump.settled && pump.temp_in >= -50 && pump.temp_in < 150) ac.water_temp = pump.temp_in;
			have_temp = true;
			break;
		}
	}

	// If no unit pumps running, try a fan pump
//	dlevel = -1;
	dprintf(dlevel,"have_temp: %s\n", have_temp);
	if (!have_temp) {
		for (name in fans) {
			let fan = fans[name];
			dprintf(dlevel,"fan[%s]: pump: %s\n", name, fan.pump);
			if (!fan.pump.length) continue;
			let pump = pumps[fan.pump];
			dprintf(dlevel,"pump[%s]: %s\n", fan.pump, pump);
			if (typeof(pump) == "undefined") continue;
			dprintf(dlevel,"pump[%s]: state: %s\n", fan.pump, pump_statestr(pump.state));
			if (pump.state == PUMP_STATE_RUNNING && pump.settled && pump.temp_in >= -50 && pump.temp_in < 150) {
				ac.water_temp = pump.temp_in;
				have_temp = true;
				break;
			}
		}
	}

	run(script_dir+"/cycle.js");
	run(script_dir+"/sample.js");
	run(script_dir+"/charge.js");
}
