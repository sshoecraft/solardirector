
function pub_main() {

	let dlevel = 1;

	dprintf(dlevel,"mqtt: enabled: %s, connected: %s\n", mqtt.enabled, mqtt.connected);
	dprintf(dlevel,"influx: enabled: %s, connected: %s\n", influx.enabled, influx.connected);

	let pub = {};

	// AC data
	let ac_data = {};
	ac_data.mode = ac_modestr(ac.mode);
	ac_data.temp = ac.temp;
	ac_data.humidity = ac.humidity;
	ac_data.pressure = ac.pressure;
//	ac_data.cycle_time = ac.cycle_end_time;
//	ac_data.cycle_time = (ac.cycle_end_time ? new Date(ac.cycle_end_time*1000) : "");
//	ac_data.sample_time = ac.sample_end_time;
//	ac_data.sample_time = (ac.sample_end_time ? new Date(ac.sample_end_time*1000) : "");
	dprintf(dlevel,"water_temp: %s - valid: %s\n", ac.water_temp, is_valid_temp(ac.water_temp));
	if (is_valid_temp(ac.water_temp)) ac_data.water_temp = ac.water_temp;
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
		else if ((fan.heat_state == "on" || fan.cool_state == "on") && fan.stop_wt && fan.wt_warned && fan.state != FAN_STATE_RUNNING) {
			// Check if in direct mode - if so, show actual state not "Temp"
			let in_direct = false;
			if (fan.direct_group) {
				let dg = directs[fan.direct_group];
				if (dg && dg.active && dg.active_fan == name) in_direct = true;
			}
			fan_data.state = in_direct ? fan_statestr(fan.state) : "Temp";
		}
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
//		if (typeof(pump_data.temp_in) == "undefined") pump_data.temp_in = INVALID_TEMP;
		if (pump_data.temp_in < -50 || pump_data.temp_in > 150) delete pump_data.temp_in;
		dprintf(dlevel+1,"pump[%s]: temp_out_sensor: %s\n", name, pump.temp_out_sensor);
		pump_data.temp_out = get_sensor(pump.temp_out_sensor,false);
		dprintf(dlevel+1,"pump[%s]: temp_out: %s\n", name, pump_data.temp_out);
//		if (typeof(pump_data.temp_out) == "undefined") pump_data.temp_out = INVALID_TEMP;
		if (pump_data.temp_out < -50 || pump_data.temp_out > 150) delete pump_data.temp_out;
		if (influx && influx.enabled && influx.connected) influx.write("pumps",pump_data);
		pub.pumps.push(pump_data);
	}

	// Direct groups
	pub.directs = [];
	for(let name in directs) {
		let dg = directs[name];
		let dg_data = {};
		dg_data.name = name;
		dg_data.enabled = dg.enabled ? 1 : 0;
		if (!dg.enabled) dg_data.state = "Disabled";
		else dg_data.state = direct_statestr(dg.state);
		dg_data.active = dg.active ? 1 : 0;
		dg_data.pin1 = dg.pin1;
		dg_data.pin1_state = (dg.pin1 >= 0) ? digitalRead(dg.pin1) : 0;
		dg_data.pin2 = dg.pin2;
		dg_data.pin2_state = (dg.pin2 >= 0) ? digitalRead(dg.pin2) : 0;
		dg_data.fan = dg.active_fan || dg.pending_fan || "";
		dg_data.unit = dg.unit;
		dg_data.mode = fan_modestr(dg.target_mode || dg.mode);
		if (influx && influx.enabled && influx.connected) influx.write("directs",dg_data);
		pub.directs.push(dg_data);
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
		// Report mode based on actual rvpin state if available, else use unit.mode
		if (unit.rvpin >= 0) {
			let rv_state = digitalRead(unit.rvpin);
			// rvcool=false: LOW=cool, HIGH=heat; rvcool=true: HIGH=cool, LOW=heat
			let actual_mode = unit.rvcool ? (rv_state ? AC_MODE_COOL : AC_MODE_HEAT) : (rv_state ? AC_MODE_HEAT : AC_MODE_COOL);
			unit_data.mode = ac_modestr(actual_mode);
		} else {
			unit_data.mode = ac_modestr(unit.mode);
		}

		// Temps
		dprintf(dlevel+1,"unit[%s]: liquid_temp_sensor: %s\n", name, unit.liquid_temp_sensor);
		unit_data.liquid_temp = get_sensor(unit.liquid_temp_sensor,false);
		dprintf(dlevel+1,"unit[%s]: liquid_temp: %s\n", name, unit_data.liquid_temp);
//		if (typeof(unit_data.liquid_temp) == "undefined") unit_data.liquid_temp = INVALID_TEMP;
		if (unit_data.liquid_temp < -50 || unit_data.liquid_temp > 200) delete unit_data.liquid_temp;
		dprintf(dlevel+1,"unit[%s]: vapor_temp_sensor: %s\n", name, unit.vapor_temp_sensor);
		unit_data.vapor_temp = get_sensor(unit.vapor_temp_sensor,false);
		dprintf(dlevel+1,"unit[%s]: vapor_temp: %s\n", name, unit_data.vapor_temp);
//		if (typeof(unit_data.vapor_temp) == "undefined") unit_data.vapor_temp = INVALID_TEMP;
		if (unit_data.vapor_temp < -50 || unit_data.vapor_temp > 200) delete unit_data.vapor_temp;
		if (influx && influx.enabled && influx.connected) influx.write("units",unit_data);
		pub.units.push(unit_data);
	}

	// Pub it 
	mqtt.pub(ac.topic,pub);
//	delobj(pub);
	for(let key in pub) delete pub[key];
}
