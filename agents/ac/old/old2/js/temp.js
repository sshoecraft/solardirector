
function temp_init() {

	let props = [
		[ "outside_temp_sensor", DATA_TYPE_STRING, "", 0 ],
		[ "outside_temp", DATA_TYPE_INT, INVALID_TEMP, CONFIG_FLAG_PRIVATE ],
		[ "pump_temp_sensor", DATA_TYPE_STRING, "", 0 ],
		[ "pump_temp", DATA_TYPE_INT, INVALID_TEMP, CONFIG_FLAG_PRIVATE ],
	];

	config.add_props(ac,props);

	var outside_temp_data = {};
	var pump_temp_data = {};
}

function temp_main() {

	let dlevel = 1;

	let sensors = [
		{ name: "outside_temp", spec: ac.outside_temp_sensor, data: "outside_temp_data" },
		{ name: "pump_temp", spec: ac.pump_temp_sensor, data: "pump_temp_data" },
	];

	dprintf(dlevel,"sensors.length: %d\n", sensors.length);
	for(let i=0; i < sensors.length; i++) {
		let sensor = sensors[i];
//		dumpobj(sensor);
		ac[sensor.name] = INVALID_TEMP;
		if (!sensor.spec.length) continue;
		dprintf(dlevel,"getting sensor: name: %s, spec: %s\n", sensor.name, sensor.spec);
		let r = get_sensor(sensor.spec);
		dprintf(dlevel,"r: %s\n", r);
		if (r && r.status == 0) {
			ac[sensor.data] = r;
			ac[sensor.name] = (ac.standard == AC_UNIT_STANDARD_US ? r.f : r.c);
			dprintf(dlevel,"NEW %s: %s\n", sensor.name, ac[sensor.name]);
		}
	}

	dprintf(dlevel,"influx: %s, influx.enabled: %s, influx.connected: %s\n", influx, influx.enabled, influx.connected);
	if (influx && influx.enabled && influx.connected) {
		dprintf(dlevel,"outsie_temp: %d\n", ac.outside_temp);
		if (ac.outside_temp != INVALID_TEMP) influx.write("outside_temp",ac.outside_temp_data);
		dprintf(dlevel,"pump_temp: %d\n", ac.pump_temp);
		if (ac.pump_temp != INVALID_TEMP) influx.write("pump_temp",ac.pump_temp_data);
	}

	// Make sure both are set even if only 1 is set
	if (ac.pump_temp == INVALID_TEMP && ac.outside_temp != INVALID_TEMP) ac.pump_temp = ac.outside_temp;
	else if (ac.pump_temp != INVALID_TEMP && ac.outside_temp == INVALID_TEMP) ac.outside_temp = ac.pump_temp;
}
