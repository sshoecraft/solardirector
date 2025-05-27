
function pub_main() {

	return;

	let dlevel = 1;

	dprintf(dlevel,"mqtt: enabled: %s, connected: %s\n", mqtt.enabled, mqtt.connected);
	dprintf(dlevel,"influx: enabled: %s, connected: %s\n", influx.enabled, influx.connected);

	let pub = {};

	// AC data
	let ac_data = {};
	ac_data.mode = ac_modestr(ac.mode);
	ac_data.mode = ac_modestr(ac.mode);
	ac_data.pump_temp = ac.pump_temp;
//	ac_data.cycle_time = (ac.cycle_end_time ? new Date(ac.cycle_end_time*1000) : "");
	ac_data.cycle_time = ac.cycle_end_time;
//	ac_data.sample_time = (ac.sample_end_time ? new Date(ac.sample_end_time*1000) : "");
	ac_data.sample_time = ac.sample_end_time;
	ac_data.water_temp = ac.water_temp;
//	ac_data.water_temp_time = (ac.water_temp_time ? new Date(ac.water_temp_time*1000) : "");
	ac_data.water_temp_time = ac.water_temp_time;
	influx.write("ac",ac_data);
	pub.ac = ac_data;

	// Errors
	influx.query("DELETE FROM errors");
	let errors = get_errors();
	dprintf(dlevel,"error count: %d\n", errors.length);
	for(let i=0; i < errors.length; i++) influx.write("errors",{} = { value: errors[i] });
	pub.errors = errors;

	// Fans
	pub.fans = [];
	for(let name in fans) {
		let fan = fans[name];
		let fan_data = {};
		fan_data.name = name;
		fan_data.enabled = fan.enabled ? 1 : 0;
		fan_data.running = 0;
		if (fan.error) fan_data.state = "Error";
		else if (!fan.enabled) fan_data.state = "Disabled";
		else fan_data.state = fan_statestr(fan.state);
		fan_data.error = 0;
		fan_data.mode = ac_modestr(fan.mode);

		// Temps
		fan_data.temp_in = fan.temp_in;
		fan_data.temp_out = fan.temp_out;
		dprintf(dlevel+1,"fan[%s]: temp_in: %s, temp_out: %s\n", name, fan_data.temp_in, fan_data.temp_out);
		if (typeof(fan_data.temp_in) == "undefined") fan_data.temp_in = INVALID_TEMP;
		if (typeof(fan_data.temp_out) == "undefined") fan_data.temp_out = INVALID_TEMP;
		if (influx && influx.enabled && influx.connected) influx.write("fans",fan_data);
		pub.fans.push(fan_data);
	}

	// Pumps
	pub.pumps = [];
	for(let name in pumps) {
		let pump = pumps[name];
		let pump_data = {};
		pump_data.name = name;
		pump_data.enabled = pump.enabled ? 1 : 0;
		pump_data.running = 0;
		if (pump.error) pump_data.state = "Error";
		else if (!pump.enabled) pump_data.state = "Disabled";
		else pump_data.state = pump_statestr(pump.state);
		pump_data.error = 0;

		// Temp and flow
		dprintf(dlevel+1,"pump[%s]: flow_sensor: %s\n", name, pump.flow_sensor);
		pump_data.flow_rate = get_sensor(pump.flow_sensor,false);
		dprintf(dlevel+1,"pump[%s]: flow_rate: %s\n", name, pump_data.flow_rate);
		if (typeof(pump_data.flow_rate) == "undefined") pump_data.flow_rate = 0;
		dprintf(dlevel+1,"pump[%s]: temp_in_sensor: %s\n", name, pump.temp_in_sensor);
		pump_data.temp_in = get_sensor(pump.temp_in_sensor,false);
		dprintf(dlevel+1,"pump[%s]: temp_in: %s\n", name, pump_data.temp_in);
		if (typeof(pump_data.temp_in) == "undefined") pump_data.temp_in = INVALID_TEMP;
		dprintf(dlevel+1,"pump[%s]: temp_out_sensor: %s\n", name, pump.temp_out_sensor);
		pump_data.temp_out = get_sensor(pump.temp_out_sensor,false);
		dprintf(dlevel+1,"pump[%s]: temp_out: %s\n", name, pump_data.temp_out);
		if (typeof(pump_data.temp_out) == "undefined") pump_data.temp_out = INVALID_TEMP;
		if (influx && influx.enabled && influx.connected) influx.write("pumps",pump_data);
		pub.pumps.push(pump_data);
	}

	// Units
	pub.units = [];
	for(let name in units) {
		let unit = units[name];
		let unit_data = {};
		unit_data.name = name;
		unit_data.enabled = (unit.enabled ? 1 : 0);;
		unit_data.running = 0;
		if (unit.error) unit_data.state = "Error";
		else if (!unit.enabled) unit_data.state = "Disabled";
		else unit_data.state = unit_statestr(unit.state);
		unit_data.error = 0;
		unit_data.mode = ac_modestr(unit.mode);

		// Temps
		dprintf(dlevel+1,"unit[%s]: liquid_temp_sensor: %s\n", name, unit.liquid_temp_sensor);
		unit_data.liquid_temp = get_sensor(unit.liquid_temp_sensor,false);
		dprintf(dlevel+1,"unit[%s]: liquid_temp: %s\n", name, unit_data.liquid_temp);
		if (typeof(unit_data.liquid_temp) == "undefined") unit_data.liquid_temp = INVALID_TEMP;
		dprintf(dlevel+1,"unit[%s]: vapor_temp_sensor: %s\n", name, unit.vapor_temp_sensor);
		unit_data.vapor_temp = get_sensor(unit.vapor_temp_sensor,false);
		dprintf(dlevel+1,"unit[%s]: vapor_temp: %s\n", name, unit_data.vapor_temp);
		if (typeof(unit_data.vapor_temp) == "undefined") unit_data.vapor_temp = INVALID_TEMP;
		if (influx && influx.enabled && influx.connected) influx.write("units",unit_data);
		pub.units.push(unit_data);
	}

	// Pub it 
if (0) {
	if (mqtt && mqtt.enabled && mqtt.connected) {
//		pub.j = JSON.stringify(pub);
//		pub.j = recursiveJsonStringify(pub);
		mqtt.pub(data_topic,pub);
	}
}

	delobj(pub);
}
