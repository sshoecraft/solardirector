
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

function addstat(str,val,text) {
	if (val) str += "[" + text + "]";
	return str;
}

function pub_main() {
	include(core_dir+"/utils.js");

	let dlevel = 1;

	dprintf(dlevel,"running: %s\n", data.Run);
	if (!data.Run) return;

	let pub = {};
	pub.status = "";
	pub.status = addstat(pub.status,si.can_connected,"can");
	pub.status = addstat(pub.status,si.readonly,"readonly");
	pub.status = addstat(pub.status,si.mirror,"mirror");
	pub.status = addstat(pub.status,si.smanet_connected,"smanet");
	pub.status = addstat(pub.status,data.GdOn,"grid");
	pub.status = addstat(pub.status,data.GnOn,"gen");
	pub.status = addstat(pub.status,si.charge_mode,"charge");
	pub.status = addstat(pub.status,si.charge_mode == 1,"CC");
	pub.status = addstat(pub.status,si.charge_mode == 2,"CV");
	pub.status = addstat(pub.status,si.feed_enabled,"feed");
	pub.status = addstat(pub.status,(si.input_source != CURRENT_SOURCE_NONE && si.feed_enabled && si.charge_mode && si.dynfeed && data.GdOn),"dynfeed");
	if (!si.feed_enabled) {
		pub.status = addstat(pub.status,(si.charge_mode && si.dyngrid && data.GdOn),"dyngrid");
		pub.status = addstat(pub.status,(si.charge_mode && si.dyngen && data.GnOn),"dyngen");
	}

	// ?
	if (!si.battery_level) si.battery_level = 0;
	if (data.GnOn) data.input_power = 0;

        let feeding = si.input_source != CURRENT_SOURCE_NONE && si.feed_enabled && data.GdOn ? 1 : 0;
	pub.data = {
		name: 			agent.name,
		battery_voltage:	data.battery_voltage,
		battery_power:		data.battery_power,
		battery_temp:		data.battery_temp,
		battery_level:		si.battery_level,
		charging:		si.charge_mode ? 1 : 0,
		charge_mode:		si.charge_mode,
		charge_voltage:		si.charge_voltage,
		charge_current:		si.charge_amps,
		charge_power:		si.charge_voltage * si.charge_amps,
		feeding:		feeding,
		input_voltage:		data.input_voltage,
		input_frequency:	data.input_frequency,
		input_current:		data.input_current,
		input_power:		data.input_power,
		output_voltage:		data.output_voltage,
		output_frequency:	data.output_frequency,
		output_current:		data.output_current,
		output_power:		data.output_power,
		remain_text:		si.remain_text,
		status:			pub.status,
	};

	let format = 3;
	switch(format) {
	case 0:
		pub.out = sprintf("%s Battery: Voltage: %.1f, Current: %.1f, Level: %.1f",
			pub.status, pub.data.battery_voltage, pub.data.battery_current, pub.data.battery_level);
		break;
	case 1:
		pub.out = sprintf("%s Battery: Voltage: %.1f, Current: %.1f, Power: %.1f, Level: %.1f",
			pub.status, pub.data.battery_voltage, pub.data.battery_current, pub.data.battery_power, pub.data.battery_level);
		break;
	case 2:
		pub.out = sprintf("%s Battery: Voltage: %.1f, Power: %.1f, Level: %.1f",
			pub.status, pub.data.battery_voltage, pub.data.battery_power, pub.data.battery_level);
		break;
	case 3:
		pub.out = sprintf("%s Battery: Voltage: %.1f, Level: %.1f",
			pub.status, pub.data.battery_voltage, pub.data.battery_level);
		break;
	}
	if (typeof(last_out) == "undefined") last_out = "";
	if (pub.out != last_out) {
		printf("%s\n",pub.out);
		last_out = pub.out;
	}

//	if (mqtt) mqtt.pub(si.topic,JSON.stringify(pub.data),0);
	if (mqtt) mqtt.pub(si.topic,pub.data);

	if (influx) dprintf(dlevel,"influx: enabled: %s, connected: %s\n", influx.enabled, influx.connected);
	if (influx && influx.enabled && influx.connected) influx.write("inverter",pub.data);
	for(let key in pub) delete pub[key];
}
