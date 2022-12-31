
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

function set_auto_on_date(n) {

	var dlevel = 1;

	dprintf(dlevel,"n: %s\n", n);
	if (typeof(n) == "undefined") return;

	var dev = rheem.devices[n];
	dprintf(dlevel,"auto_on: %s\n", dev.auto_on);
	if (!dev.auto_on.length) dev.auto_on_date = undefined;
	else dev.auto_on_date = get_date(rheem.location,dev.auto_on,"auto_on",true);
	if (dev.auto_on_date) dprintf(dlevel,"NEW auto_on_date: %s\n",dev.auto_on_date);
}

function set_aoltd(n) {

	var dlevel = 1;

	dprintf(dlevel,"n: %s\n", n);
	if (typeof(n) == "undefined") return;

	var dev = rheem.devices[n];
	dprintf(dlevel,"auto_off_level_timeout: %s, auto_off_date: %s\n", dev.auto_off_level_timeout, dev.auto_off_date);
	if (!dev.auto_off_level_timeout) dev.aoltd = undefined;
	else if (dev.auto_off_date) {
		// Auto Off Level Timeout Date
		dev.aoltd = new Date(dev.auto_off_date.getTime());
		dev.aoltd.setMinutes(dev.aoltd.getMinutes() + dev.auto_off_level_timeout);
		dprintf(dlevel,"aoltd: %s\n", dev.aoltd);
		if (dev.aoltd) dprintf(dlevel,"NEW aoltd: %s\n", dev.aoltd);
	}
}

function set_auto_off_date(n) {

	var dlevel = 1;

	dprintf(dlevel,"n: %s\n", n);
	if (typeof(n) == "undefined") return;

	var dev = rheem.devices[n];
	dprintf(dlevel,"auto_off: %s\n", dev.auto_off);
	if (!dev.auto_off.length) dev.auto_off_date = undefined;
	else dev.auto_off_date = get_date(rheem.location,dev.auto_off,"auto_off",true);
	if (dev.auto_off_date) dprintf(dlevel,"NEW auto_off_date: %s\n",dev.auto_off_date);
	if (dev.auto_off_level_timeout) set_aoltd(n);
}

function set_flbd(n) {

	var dlevel = 0;

	dprintf(dlevel,"n: %s\n", n);
	if (typeof(n) == "undefined") return;

	var dev = rheem.devices[n];
	dprintf(dlevel,"force_level_before: %s\n", dev.force_level_before);
	if (!dev.force_level_before.length) dev.flbd = undefined;
	else {
		let nextday = (dev.flbd ? true : false);
		dev.flbd = get_date(rheem.location,dev.force_level_before,"flbd",nextday);
	}
	if (dev.flbd) dprintf(dlevel,"NEW flbd: %s\n",dev.flbd);
}

function set_override(n) {

	var dlevel = 1;

	dprintf(dlevel,"n: %s\n", n);
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

function read_main() {

//	printf("*** IN READ ****\n");

	include(dirname(script_name)+"/../../core/utils.js");
	include(dirname(script_name)+"/../../core/kalman.js");
	include(dirname(script_name)+"/../../core/suncalc.js");
	include(dirname(script_name)+"/../../core/inverter.js");

	var dlevel = 0;

	let inverter = undefined;
	if (rheem.inverter_source == "mqtt") {
		// See if there's an inverter agent
		let invagent = get_agent(rheem.inverter_name);
		dprintf(dlevel,"invagent: %s\n", invagent);
		if (!invagent) {
			if (rheem.agent.length)
				log_warning("agent not found: %s\n", rheem.inverter);
			else
				log_warning("no agents found\n");
		} else {
			// Get the data if available
			invagent.addmq = true;
			let mq = invagent.messages;
			dprintf(dlevel,"mq.length: %d\n", mq.length);
//			for(let i=0; i < mq.length; i++) {
			for(let i=mq.length-1; i >= 0; i--) {
				dprintf(dlevel,"mq[%d].func: %s\n", i, mq[i].func);
				if (mq[i].func == "Data") {
//					dprintf(dlevel,"mq[%d].data: %s\n", i, mq[i].data);
					inverter = JSON.parse(mq[i].data);
					break;
				}
			}
			invagent.purgemq();
		}
	} else if (rheem.inverter_source == "influx") {
		let query = "select last(*) from inverter";
		if (rheem.inverter_name.length) query += " WHERE \"name\" = '" + rheem.inverter_name + "'";
		dprintf(dlevel,"query: %s\n", query);
		inverter = influx2obj(influx.query(query));
//		dumpobj(inverter);
	}

	// If we have the inverter data, get battery_power
	let battery_power = 0;
	let avail_power = 0;
	dprintf(dlevel,"inverter: %s\n", inverter);
	if (inverter) {
		dprintf(dlevel,"battery_power: %f\n", inverter.battery_power);
		if (!rheem.kf_bp) rheem.kf_bp = new KalmanFilter();
		battery_power = rheem.kf_bp.filter(inverter.battery_power);
		dprintf(dlevel,"available_power: %f\n", inverter.available_power);
		if (!rheem.kf_avail) rheem.kf_avail = new KalmanFilter();
		avail_power = rheem.kf_avail.filter(inverter.available_power);
	}
	dprintf(dlevel,"battery_power: %f, avail_power: %f\n", battery_power, avail_power);

	var devices = rheem.devices;
	dprintf(dlevel,"devices.length: %d\n", devices.length);
	for(i=0; i < devices.length; i++) {
		var dev = devices[i];
		dprintf(dlevel,"id: %s, added: %s\n", dev.id, dev.added);
		if (!dev.added) {
//			printf("***** ADDING DEV PROPS ****\n");
			var dev_props = [
				[ "defmode", DATA_TYPE_STRING, "energy_saver", 0 ],
				[ "deftemp", DATA_TYPE_INT, 123, 0 ],
				[ "override", DATA_TYPE_STRING, null, 0, set_override, i ],
				[ "force_level", DATA_TYPE_INT, "-1", 0 ],
				[ "force_level_mode", DATA_TYPE_STRING, "high_demand", 0 ],
				[ "force_level_before", DATA_TYPE_STRING, "sunset", 0, set_flbd, i ],
				[ "auto_on", DATA_TYPE_STRING, null, 0, set_auto_on_date, i ],
				[ "auto_off", DATA_TYPE_STRING, null, 0, set_auto_off_date, i ],
				[ "auto_off_level", DATA_TYPE_INT, null, 0 ],
				[ "auto_off_level_timeout", DATA_TYPE_INT, "120", 0, set_aoltd, i ],
				[ "reserve_power", DATA_TYPE_INT, 5000, 0 ],
				[ "avail_mode", DATA_TYPE_STRING, "electric", 0 ],
				[ "avail_temp", DATA_TYPE_INT, "140", 0 ],
				[ "battery_mode", DATA_TYPE_STRING, null, 0 ],
				[ "battery_temp", DATA_TYPE_INT, 123, 0 ],
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
		dprintf(dlevel,"readonly: %s\n", rheem.readonly);
		if (rheem.readonly) continue;

		dprintf(dlevel,"enabled: %s, mode: %s, level: %d\n", dev.enabled, dev.mode, dev.level);

		let cur = new Date();
		dprintf(dlevel,"current_date: %s\n", cur);

		// Override?
		dprintf(dlevel,"override: %s\n", dev.override);
		if (dev.override.length) {
			if (dev.mode != dev.override) {
				dprintf(dlevel,"setting mode to: %s\n", dev.override);
				dev.mode = dev.override;
			}
		} else if (dev.mode == "off") {
			// auto on
			dprintf(dlevel,"auto_on_date: %s\n", dev.auto_on_date);
			if (dev.auto_on_date) dprintf(dlevel,"cur: %s, auto_on: %s, diff: %s\n",
				cur.getTime(), dev.auto_on_date.getTime(), dev.auto_on_date.getTime() - cur.getTime());
			if (dev.auto_on_date && cur.getTime() >= dev.auto_on_date.getTime()) {
				set_auto_on_date(i);
				// Only doit if on time < off time
				let doit = true;
				if (dev.auto_off_date && cur.getTime() >= dev.auto_off_date.getTime()) doit = false;
				dprintf(dlevel+1,"doit: %s\n", doit);
				if (doit) {
					dprintf(dlevel,"setting mode to: %s\n", dev.defmode);
					dev.mode = dev.defmode;
					if (dev.temp != dev.deftemp) dev.temp = dev.deftemp;
				}
			}
		} else {
			// auto off
			dprintf(dlevel,"auto_off_date: %s\n", dev.auto_off_date);
			dprintf(dlevel,"reserved: %s\n", dev.reserved);
			dprintf(dlevel,"force_level: %s\n", dev.force_level);
			dprintf(dlevel,"defmode: %s\n", dev.defmode);
			if (dev.auto_off_date) dprintf(dlevel,"cur: %s, auto_off: %s, diff: %s\n",
				cur.getTime(), dev.auto_off_date.getTime(), dev.auto_off_date.getTime() - cur.getTime());
			if (dev.auto_off_date && cur.getTime() >= dev.auto_off_date.getTime()) {
				// If auto_off > auto_on time, skip it
				if (dev.auto_on_date) dprintf(dlevel,"cur: %s, auto_on: %s, diff: %s\n",
					cur.getTime(), dev.auto_on_date.getTime(), dev.auto_on_date.getTime() - cur.getTime());
				if (dev.auto_on_date && cur.getTime() >= dev.auto_on_date.getTime()) {
					set_auto_off_date(i);
				} else {
					// Only turn off if auto_off_level is >= level
					let doit = true;
					dprintf(dlevel,"auto_off_level: %s\n", dev.auto_off_level);
					if (dev.auto_off_level && dev.level < dev.auto_off_level) {
						if (dev.aoltd) {
							dprintf(dlevel,"aoltd: %s\n", dev.aoltd);
							dprintf(dlevel,"cur: %s, aoltd: %s, diff: %s\n", cur.getTime(), dev.aoltd.getTime(), dev.aoltd.getTime() - cur.getTime());
						}
						if (!(dev.aoltd && cur.getTime() >= dev.aoltd.getTime()))
							doit = false;
					}
					dprintf(dlevel+1,"doit: %s\n", doit);
					if (doit) {
						dprintf(dlevel,"setting mode to: off\n");
						dev.mode = "off";
						set_auto_off_date(i);
						continue;
					}
				}
			}

			// If we're on battery power
			dprintf(dlevel,"battery_power: %s, battery_mode: %s\n", battery_power, dev.battery_mode);
			if (battery_power > 100 && dev.battery_mode) {
				if (dev.mode != dev.battery_mode) {
					dprintf(dlevel,"setting mode to %s\n",dev.battery_mode);
					dev.mode = dev.battery_mode;
				}
				if (dev.temp != dev.battery_temp) {
					dprintf(dlevel,"setting temp to %s\n",dev.battery_temp);
					dev.temp = dev.battery_temp;
				}
				continue;
			}

			// Check force level?
			let check_force_level = false;
			if (dev.flbd) {
				dprintf(dlevel,"cur: %s, flbd: %s, diff: %s\n",
					cur.getTime(), dev.flbd.getTime(), dev.flbd.getTime() - cur.getTime());
				if (cur.getTime() < dev.flbd.getTime()) {
					check_force_level = true;
				} else {
					set_flbd(i);
				}
			} else {
				check_force_level = true;
			}
			dprintf(dlevel,"check_force_level: %s\n", check_force_level);

			// Avail power
			if (!dev.reserved && avail_power > dev.reserve_power) {
				dprintf(dlevel,"setting mode to %s and temp to %d\n",dev.avail_mode,dev.avail_temp);
				dev.mode = dev.avail_mode;
				dev.temp = dev.avail_temp;
				avail_power -= dev.reserve_power;
				dev.reserved = true;

			// If reserved, keep it until avail_power < reserve_power
			} else if (dev.reserved) {
				dprintf(dlevel,"avail_power + reserve_power: %f\n",avail_power + dev.reserve_power);
				if (avail_power + dev.reserve_power <= dev.reserve_power) {
					dprintf(dlevel,"setting mode to %s and temp to %d\n",dev.defmode,dev.deftemp);
					dev.mode = dev.defmode;
					dev.temp = dev.deftemp; 
					dev.reserved = false;
				}

			// force level
			} else if (dev.level <= dev.force_level && check_force_level) {
				if (dev.mode != dev.force_level_mode) {
					dprintf(dlevel,"setting mode to: %s\n", dev.force_level_mode);
					dev.mode = dev.force_level_mode;
				}

			// Default mode
			} else if (dev.mode != dev.defmode || dev.temp != dev.deftemp) {
				if (dev.mode != dev.defmode) {
					dprintf(dlevel,"setting mode to %s\n", dev.defmode);
					dev.mode = dev.defmode;
				}
				if (dev.temp != dev.deftemp) {
					dprintf(dlevel,"setting temp to: %s\n", dev.deftemp);
					dev.temp = dev.deftemp;
				}
			}
		}

	}
}
