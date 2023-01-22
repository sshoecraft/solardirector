
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

function set_auto_on_date(n,p,o) {

	let dlevel = 1;

	dprintf(dlevel,"n: %s, p: %s, o: %s\n", n, p, o);
	if (typeof(n) == "undefined") return;

	var dev = rheem.devices[n];
	dprintf(dlevel,"auto_on: %s, added: %s\n", dev.auto_on, dev.added);
	if (!dev.auto_on.length) dev.auto_on_date = undefined;
	else dev.auto_on_date = get_date(rheem.location,dev.auto_on,"auto_on",dev.added);
	if (dev.auto_on_date) dprintf(dlevel,"NEW auto_on_date: %s\n",dev.auto_on_date);
}

function set_aoltd(n,p,o) {

	let dlevel = 1;

	dprintf(dlevel,"n: %s, p: %s, o: %s\n", n, p, o);
	if (typeof(n) == "undefined") return;

	var dev = rheem.devices[n];
	dprintf(dlevel,"auto_off_level_timeout: %s, auto_off_date: %s, added: %s\n",
		dev.auto_off_level_timeout, dev.auto_off_date, dev.added);
	if (!dev.auto_off_level_timeout) dev.aoltd = undefined;
	else if (dev.auto_off_date) {
		// Auto Off Level Timeout Date
		dev.aoltd = new Date(dev.auto_off_date.getTime());
		dev.aoltd.setMinutes(dev.aoltd.getMinutes() + dev.auto_off_level_timeout);
		dprintf(dlevel,"aoltd: %s\n", dev.aoltd);
		if (dev.aoltd) dprintf(dlevel,"NEW aoltd: %s\n", dev.aoltd);
	}
}

function set_auto_off_date(n,p,o) {

	let dlevel = 1;

	dprintf(dlevel,"n: %s, p: %s, o: %s\n", n, p, o);
	if (typeof(n) == "undefined") return;

	var dev = rheem.devices[n];
	dprintf(dlevel,"auto_off: %s, added: %s\n", dev.auto_off, dev.added);
	if (!dev.auto_off.length) dev.auto_off_date = undefined;
	else dev.auto_off_date = get_date(rheem.location,dev.auto_off,"auto_off",dev.added);
	if (dev.auto_off_date) dprintf(dlevel,"NEW auto_off_date: %s\n",dev.auto_off_date);
	if (dev.auto_off_level_timeout) set_aoltd(n);
}

function set_override(n,p,o) {

	let dlevel = 1;

	dprintf(dlevel,"n: %s, p: %s, o: %s\n", n, p, o);
	if (typeof(n) == "undefined") return;

	var dev = rheem.devices[n];
	dprintf(dlevel,"override: %s\n", dev.override);
	if (dev.override.length) {
		dprintf(dlevel,"setting mode to: %s\n", dev.override);
		dev.mode = dev.override;
	}
}

function set_location() {
	dprintf(dlevel,"count: %d\n", rheem.devices.length);
	for(i=0; i < rheem.devices.length; i++) {
		set_auto_on_date(i);
		set_auto_off_date(i);
	}
}

function get_agent(name) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s\n", name);

	let agents = client.agents;
	dprintf(dlevel,"agents count: %d\n", agents.length);
	for(let i=0; i < agents.length; i++) {
		let agent = agents[i];
		dprintf(dlevel,"agent name: %s\n", agent.name);
		let info = agent.info;
		dprintf(dlevel,"agent role: %s\n", info.agent_role);
		if (info && info.agent_role == "Inverter") {
			// If name empty, just take 1st one found
			if (!name.length) return agent;
			else if (agent.name == name) return agent;
		}
	}
	return undefined;
}

function inverter_source_trigger() {

	let dlevel = 1;

	dprintf(dlevel,"inverter_source(%d): %s\n", rheem.inverter_source.length, rheem.inverter_source);
	if (!rheem.inverter_source.length) return;
	// Source,conx,name
	// influx,192.168.1.142,power
	// mqtt,192.168.1.4,si
	let tf = rheem.inverter_source.split(",");
//	dumpobj(tf);
	inverter_type = tf[0];
	dprintf(dlevel,"type: %s\n", inverter_type);
	if (inverter_type != 'influx' && inverter_type != 'mqtt') {
		log_error("inverter_source type must be mqtt or influx\n");
		return;
	}
	let conx = tf[1];
	dprintf(dlevel,"conx: %s\n", conx);
	if (!conx) conx = "localhost";

	if (inverter_type == "influx") {
		let inverter_db = tf[2];
		dprintf(dlevel,"db: %s\n", inverter_db);
		if (!inverter_db) inverter_db = "solardirector";
		inverter_name = tf[3];
		dprintf(dlevel,"name: %s\n", inverter_name);
		inverter_influx = new Influx(conx,inverter_db);
	} else if (inverter_type == "mqtt") {
		inverter_name = tf[2];
		dprintf(dlevel,"name: %s\n", inverter_name);
		inverter_mqtt = new MQTT(conx);
		inverter_mqtt.enabled = true;
		inverter_mqtt.connect();
		if (!inverter_mqtt.connected) log_error("unable to connect to inverter_source %s: %s\n", conx, inverter_mqtt.errmsg);
		inverter_mqtt.sub(SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/" + inverter_name + "/"+SOLARD_FUNC_DATA);
		inverter_mqtt.sub(SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/" + inverter_name + "/"+SOLARD_FUNC_EVENT);
	}
}

function read_main() {

//	printf("*** IN READ ****\n");

	include(dirname(script_name)+"/../../core/utils.js");
	include(dirname(script_name)+"/../../core/kalman.js");
	include(dirname(script_name)+"/../../core/suncalc.js");

	let dlevel = 1;

	dprintf(dlevel,"props_added: %s\n", rheem.props_added);
	if (!rheem.props_added) {
		var props = [
			[ "inverter_source", DATA_TYPE_STRING, "mqtt,localhost,si", 0, inverter_source_trigger ],
			[ "inverter_timeout", DATA_TYPE_INT, 30, 0 ],
		];

		config.add_props(rheem,props);
		last_inverter = undefined;
		last_inverter_time = 0;
		rheem.props_added = true;
	}

	let inverter = undefined;
	if (rheem.inverter_source && rheem.inverter_source.length) {
		if (inverter_type == "mqtt") {
//			dumpobj(inverter_mqtt);
			dprintf(dlevel,"connected: %s\n", inverter_mqtt.connected);
			if (!inverter_mqtt.connected) inverter_mqtt.reconnect();
			let mq = inverter_mqtt.mq;
			dprintf(dlevel,"mq.length: %d\n", mq.length);
			for(let i=mq.length-1; i >= 0; i--) {
				dprintf(dlevel,"mq[%d].func: %s\n", i, mq[i].func);
				if (mq[i].func == "Data") {
					inverter = JSON.parse(mq[i].data);
					break;
				}
			}
			inverter_mqtt.purgemq();
		} else if (inverter_type == "influx") {
			let query = "select last(*) from inverter";
			if (inverter_name && inverter_name.length) query += " WHERE \"name\" = '" + inverter_name + "'";
			dprintf(dlevel,"query: %s\n", query);
			inverter = influx2obj(inverter_influx.query(query));
		}
	}

	if (!inverter) {
		dprintf(dlevel,"diff: %s, timeout: %s\n", time() - last_inverter_time, rheem.inverter_timeout);
		if (time() - last_inverter_time < rheem.inverter_timeout) inverter = last_inverter;
	} else {
		last_inverter = inverter;
		last_inverter_time = time();
	}

	// If we have the inverter data, get battery_power
	let battery_power = 0;
	let avail_power = 0;
	dprintf(dlevel,"inverter: %s\n", inverter);
	if (inverter) {
		dprintf(dlevel,"battery_power: %f\n", inverter.battery_power);
		if (typeof(kf_bp) == "undefined") kf_bp = new KalmanFilter();
		battery_power = kf_bp.filter(inverter.battery_power);
		dprintf(dlevel,"available_power: %f\n", inverter.available_power);
		if (typeof(kf_avail) == "undefined") kf_avail = new KalmanFilter();
		avail_power = kf_avail.filter(inverter.available_power);
	}
	dprintf(dlevel,"battery_power: %f, avail_power: %f\n", battery_power, avail_power);

	var devices = rheem.devices;
	dprintf(dlevel,"devices.length: %d\n", devices.length);
	for(i=0; i < devices.length; i++) {
		var dev = devices[i];
		dprintf(dlevel,"id: %s, added: %s\n", dev.id, dev.added);
		if (!dev.added) {
//			printf("***** ADDING DEV PROPS ****\n");
			let deftemp = 122;
			var dev_props = [
				[ "defmode", DATA_TYPE_STRING, "energy_saver", 0 ],
				[ "deftemp", DATA_TYPE_INT, deftemp, 0 ],
				[ "override", DATA_TYPE_STRING, null, 0, set_override, i ],
				[ "force_level", DATA_TYPE_INT, "-1", 0 ],
				[ "force_level_mode", DATA_TYPE_STRING, "high_demand", 0 ],
				[ "force_level_temp", DATA_TYPE_INT, deftemp, 0 ],
				[ "auto_on", DATA_TYPE_STRING, null, 0, set_auto_on_date, i ],
				[ "auto_off", DATA_TYPE_STRING, null, 0, set_auto_off_date, i ],
				[ "auto_off_level", DATA_TYPE_INT, null, 0 ],
				[ "auto_off_level_timeout", DATA_TYPE_INT, 120, 0, set_aoltd, i ],
				[ "reserve_power", DATA_TYPE_INT, 5000, 0 ],
				[ "avail_mode", DATA_TYPE_STRING, "electric", 0 ],
				[ "avail_temp", DATA_TYPE_INT, deftemp, 0 ],
				[ "battery_power", DATA_TYPE_INT, 300, 0 ],
				[ "battery_mode", DATA_TYPE_STRING, null, 0 ],
				[ "battery_temp", DATA_TYPE_INT, deftemp, 0 ],
			];

			config.add_props(dev,dev_props,dev.id);
			dev.reserved = false;
			dev.added = true;
			dev.auto_off_level_start = 0;
			agent.pubinfo();
			agent.pubconfig();
		}
//		dumpobj(dev);

		// If we're readonly, dont try to do anything here
//		dprintf(dlevel,"readonly: %s\n", rheem.readonly);
//		if (rheem.readonly) continue;

		let cur = new Date();
		dprintf(dlevel,"Current date: %s\n", cur);

		// Override?
		dprintf(dlevel,"override: %s\n", dev.override);
		if (dev.override.length) {
			if (dev.mode != dev.override) {
				dprintf(dlevel,"setting mode to: %s\n", dev.override);
				dev.mode = dev.override;
			}
			continue;
		}

//		if (dev.mode == "off") {
			// auto on
			let doit = false;
			dprintf(dlevel,"auto_on_date: %s\n", dev.auto_on_date);
			if (dev.auto_on_date) dprintf(dlevel,"cur: %s, auto_on: %s, diff: %s\n",
				cur.getTime(), dev.auto_on_date.getTime(), dev.auto_on_date.getTime() - cur.getTime());
			if (dev.auto_on_date && cur.getTime() >= dev.auto_on_date.getTime()) {
				set_auto_on_date(i);
				doit = true;
			}
		dprintf(dlevel,"mode: %s, doit: %s\n", dev.mode, doit);
			if (dev.mode == "off" && !doit) continue;
//		}

		// auto off
		dprintf(dlevel,"auto_off_date: %s\n", dev.auto_off_date);
		if (dev.auto_off_date) dprintf(dlevel,"cur: %s, auto_off: %s, diff: %s\n",
			cur.getTime(), dev.auto_off_date.getTime(), dev.auto_off_date.getTime() - cur.getTime());
		if (dev.auto_off_date && cur.getTime() >= dev.auto_off_date.getTime()) {
			let doit = true;
			dprintf(dlevel,"level: %d, auto_off_level: %d\n", dev.level, dev.auto_off_level);
			if (dev.auto_off_level && dev.level < dev.auto_off_level) {
				dprintf(dlevel,"auto off level timeout date: %s\n", dev.aoltd);
				if (dev.aoltd) dprintf(dlevel,"cur: %s, aoltd: %s, diff: %s\n", cur.getTime(), dev.aoltd.getTime(), dev.aoltd.getTime() - cur.getTime());
				if (dev.aoltd && cur.getTime() < dev.aoltd.getTime()) doit = false;
			}
			dprintf(dlevel,"doit: %s\n", doit);
			if (doit) {
				set_auto_off_date(i);
				dev.mode = "off";
				continue;
			}
		}

		// If we're on battery power
		dprintf(dlevel,"battery_power: %s, battery_mode: %s\n", dev.battery_power, dev.battery_mode);
		if (battery_power >= dev.battery_power && dev.battery_mode) {
			dev.mode = dev.battery_mode;
			dev.temp = dev.battery_temp;
			continue;
		}

		// Avail power
		dprintf(dlevel,"reserved: %s, reserve_power: %s, avail_power: %s\n",
			dev.reserved, dev.reserve_power, avail_power);
		if (!dev.reserved && dev.reserve_power && avail_power > dev.reserve_power) {
			dev.mode = dev.avail_mode;
			dev.temp = dev.avail_temp;
			avail_power -= dev.reserve_power;
			dev.reserved = true;
			continue;

		// If reserved, keep it until avail_power < reserve_power
		} else if (dev.reserved) {
			dprintf(dlevel,"running: %s, avail_power: %s, reserve_power: %f\n",
				dev.running, avail_power, dev.reserve_power);
			if (dev.running && avail_power + dev.reserve_power > dev.reserve_power) continue;
			if (!dev.runnning && avail_power > dev.reserve_power) continue;
			dev.reserved = false;
		}

		// force level
		dprintf(dlevel,"level: %s, force_level: %s\n", dev.level, dev.force_level);
		if (dev.level <= dev.force_level) {
			dev.mode = dev.force_level_mode;
			dev.temp = dev.force_level_temp;
			continue;
		}

		// If we got to here, set default mode/temp
		dev.mode = dev.defmode;
		dev.temp = dev.deftemp;
	}
}
