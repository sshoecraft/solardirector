
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
	include(core_dir+"utils.js");

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
	pub.status = addstat(pub.status,(si.charge_mode && si.dyngrid && data.GdOn),"dyngrid");
	pub.status = addstat(pub.status,(si.charge_mode && si.dyngen && data.GnOn),"dyngen");

	pub.avail = data.input_power;
	if (pub.avail < 0) pub.avail = 0;

if (0 == 1) {
	// Calculate available power
        dprintf(dlevel,"input_source: %d, feed_enabled: %d, GdOn: %d\n", si.input_source, si.feed_enabled, data.GdOn);
        if (si.input_source != CURRENT_SOURCE_NONE && si.feed_enabled && data.GdOn) {
		// We're feeding: use input power as a base
		avail = data.input_power;
		if (avail < 0) avail = 0;
	}
	// we need solar input for this to work
	if (si.solar_source != CURRENT_SOURCE_NONE && si.ac1_frequency >= 61.0) {
		let m = 62 - si.ac1_frequency;
		dprintf(0,"m: %f\n", m);
	}
}

	// In the event SI is powered off
	if (si.soc < 0) si.soc = 0.0;

	pub.tab = [
			[ "name",		agent.name ],
			[ "battery_voltage",	data.battery_voltage ],
			[ "battery_current",	data.battery_current ],
			[ "battery_power",	data.battery_power ],
			[ "battery_temp",	data.battery_temp ],
			[ "battery_level",	si.soc ],
			[ "charge_mode",	si.charge_mode ],
			[ "charge_voltage",	si.charge_voltage ],
			[ "charge_current",	si.charge_amps ],
			[ "charge_power",	si.charge_voltage * si.charge_amps ],
			[ "input_voltage",	data.input_voltage ],
			[ "input_frequency",	data.input_frequency ],
			[ "input_current",	data.input_current ],
			[ "input_power",	data.input_power ],
			[ "output_voltage",	data.output_voltage ],
			[ "output_frequency",	data.output_frequency ],
			[ "output_current",	data.output_current ],
			[ "output_power",	data.output_power ],
			[ "load_power",		data.load_power ],
			[ "available_power",	pub.avail ],
			[ "remain_text",	si.remain_text ],
			[ "status",		pub.status ],
	];
	pub.data = {};
	for(let j=0; j < pub.tab.length; j++) pub.data[pub.tab[j][0]] = pub.tab[j][1];
	//for(let key in pub.data) printf("%20.20s: %s\n", key, pub.data[key]);

	let format = 3;
	switch(format) {
	case 0:
		pub.out = sprintf("%s Battery: Voltage: %.1f, Current: %.1f, Level: %.1f",
			status, pub.data.battery_voltage, pub.data.battery_current, pub.data.battery_level);
		break;
	case 1:
		pub.out = sprintf("%s Battery: Voltage: %.1f, Current: %.1f, Power: %.1f, Level: %.1f",
			status, pub.data.battery_voltage, pub.data.battery_current, pub.data.battery_power, pub.data.battery_level);
		break;
	case 2:
		pub.out = sprintf("%s Battery: Voltage: %.1f, Power: %.1f, Level: %.1f",
			status, pub.data.battery_voltage, pub.data.battery_power, pub.data.battery_level);
		break;
	case 3:
		pub.out = sprintf("%s Battery: Voltage: %.1f, Level: %.1f", pub.status, pub.data.battery_voltage, pub.data.battery_level);
		break;
	}
	if (typeof(last_out) == "undefined") last_out = "";
	if (pub.out != last_out) {
		printf("%s\n",pub.out);
		last_out = pub.out;
	}

	// JSON.stringfy leaks a ton of mem
	pub.json = JSON.stringify(pub.data,fields,4);
//	pub.json = toJSONString(pub.data,fields,4);
//	printf("json: %s\n", pub.json);

	if (mqtt) mqtt.pub(data_topic,pub.json,0);

	if (influx) dprintf(dlevel,"influx: enabled: %s, connected: %s\n", influx.enabled, influx.connected);
	if (influx && influx.enabled && influx.connected) influx.write("inverter",pub.data);
	for(let key in pub) delete pub[key];
}
