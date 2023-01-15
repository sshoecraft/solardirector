
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

function pub_main() {

	let dlevel = 1;

//	dprintf(dlevel,"can_connected: %s, smanet_connected: %s\n", si.can_connected, si.smanet_connected);
//	if (!si.can_connected && !si.smanet_connected) return(0);
	dprintf(dlevel,"running: %s\n", data.Run);
	if (!data.Run) return;

	function addstat(str,val,text) {
		if (val) str += "[" + text + "]";
		return str;
	}

	var status = "";
	status = addstat(status,si.can_connected,"can");
	status = addstat(status,si.readonly,"readonly");
	status = addstat(status,si.mirror,"mirror");
	status = addstat(status,si.smanet_connected,"smanet");
	status = addstat(status,data.GdOn,"grid");
	status = addstat(status,data.GnOn,"gen");
	status = addstat(status,si.charge_mode,"charge");
	status = addstat(status,si.charge_mode == 1,"CC");
	status = addstat(status,si.charge_mode == 2,"CV");
	status = addstat(status,si.feed_enabled,"feed");
	status = addstat(status,(si.input_source != CURRENT_SOURCE_NONE && si.feed_enabled && si.charge_mode && si.dynfeed && data.GdOn),"dynfeed");
	status = addstat(status,(si.charge_mode && si.dyngrid && data.GdOn),"dyngrid");
	status = addstat(status,(si.charge_mode && si.dyngen && data.GnOn),"dyngen");

	avail = data.input_power;
	if (avail < 0) avail = 0;

if (0 == 1) {
	// Calculate available power
        dprintf(dlevel,"input_source: %d, feed_enabled: %d, GdOn: %d\n", si.input_source, si.feed_enabled, data.GdOn);
	var avail = 0;
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

	var tab = [
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
			[ "available_power",	avail ],
			[ "remain_text",	si.remain_text ],
			[ "status",		status ],
	];
	var mydata = {};
	for(let j=0; j < tab.length; j++) mydata[tab[j][0]] = tab[j][1];
	//for(let key in mydata) printf("%20.20s: %s\n", key, mydata[key]);


	var format = 3;
	switch(format) {
	case 0:
		out = sprintf("%s Battery: Voltage: %.1f, Current: %.1f, Level: %.1f",
			status, mydata.battery_voltage, mydata.battery_current, mydata.battery_level);
		break;
	case 1:
		out = sprintf("%s Battery: Voltage: %.1f, Current: %.1f, Power: %.1f, Level: %.1f",
			status, mydata.battery_voltage, mydata.battery_current, mydata.battery_power, mydata.battery_level);
		break;
	case 2:
		out = sprintf("%s Battery: Voltage: %.1f, Power: %.1f, Level: %.1f",
			status, mydata.battery_voltage, mydata.battery_power, mydata.battery_level);
		break;
	case 3:
		out = sprintf("%s Battery: Voltage: %.1f, Level: %.1f", status, mydata.battery_voltage, mydata.battery_level);
		break;
	}
	if (typeof(last_out) == "undefined") last_out = "";
	if (out != last_out) {
		printf("%s\n",out);
		last_out = out;
	}

	// JSON.stringfy leaks a ton of mem
	var j = JSON.stringify(mydata,fields,4);
//	printf("j: %s\n", j);

	mqtt.pub(data_topic,j,0);

	dprintf(dlevel,"influx: enabled: %s\n", influx.enabled);
	if (influx.enabled) {
		dprintf(dlevel,"influx: connected: %s\n", influx.connected);
		if (!influx.connected) influx.connect();
		if (influx.connected) influx.write("inverter",mydata);
	}
}
