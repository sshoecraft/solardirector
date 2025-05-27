#!/opt/sd/bin/sdjs

// Dirs
script_dir = dirname(script_name);
lib_dir = (script_dir == "." ? "../../lib" : SOLARD_LIBDIR);
sdlib_dir = lib_dir + "/sd";

// sdlib
include(sdlib_dir+"/init.js");

var pa_version = "1.0";

// use these fields to determine how much power is avail
/* 
grid_power (grid)
load_power
battery_power
pv_power
frequency (if solar output is being restricted)

positive values indicate excess power
negative values indicate overuse

*/

function pa_budget_trigger(a,p,o) {

	let dlevel = 1;

	if (!pa) return;
	dprintf(dlevel,"new budget: %.1f\n", p.value);
	if (typeof pa.reserved != 'undefined') {
		pa.avail = pa.budget - pa.reserved;
		dprintf(dlevel,"NEW avail: %.1f\n", pa.avail);
	}
}

function pa_server_config_trigger(a,p,o) {

	let dlevel = 1;

	dprintf(dlevel,"new val: %s, old val: %s\n", p.value, o);

	dprintf(dlevel,"p.name: %s\n", p.name);
	let vname = p.name.substr(0,p.name.indexOf("_server_config")) + "_mqtt";
	dprintf(dlevel,"vname: %s\n", vname);
	let m = a[vname];
	let isdef = (typeof(m) != 'undefined');
	dprintf(dlevel,"isdef: %s\n", isdef);
	let subs = [];
	if (isdef) {
		dprintf(dlevel,"connected: %s\n", m.connected);
		if (m.connected) m.disconnect();
		dprintf(dlevel,"m.subs: %s\n", m.subs);
		for(let i = 0; i < m.subs.length; i++) {
			let sub = m.subs[i];
			dprintf(dlevel,"unsubbing: %s\n", sub);
			subs.push(sub);
			m.unsub(sub);
		}
		delobj(a[vname]);
	}

	dprintf(dlevel,"config: %s(%d)\n", p.value, p.value.length);
	if (p.value.length) {
		a[vname] = new MQTT(p.value);
		if (!a[vname]) {
			log_error("error: unable to create %s\n", vname);
			return 1;
		}
		m = a[vname];
		m.enabled = true;
		for(let i = 0; i < subs.length; i++) {
			let sub = subs[i];
			dprintf(dlevel,"subbing: %s\n", sub);
			m.sub(sub);
		}
		dprintf(dlevel,"connecting...\n");
		m.connect();
//		dumpobj(m);
	}
	return 0;
}

function pa_topic_trigger(a,p,o) {

	let dlevel = 1;

	dprintf(dlevel,"new val: %s, old val: %s\n", p.value, o);

	dprintf(dlevel,"p.name: %s\n", p.name);
	let vname = p.name.substr(0,p.name.indexOf("_topic")) + "_mqtt";
	dprintf(dlevel,"vname: %s\n", vname);
	let isdef = (typeof(a[vname]) != 'undefined');
	dprintf(dlevel,"isdef: %s\n", isdef);

	let m = a[vname];
	dprintf(dlevel,"m: %s\n", m);
	if (isdef) {
		if (m.connected) m.disconnect();
		if (typeof(o) != "undefined") m.unsub(o);
		m.sub(p.value);
		m.connect();
	}
	return 0;
}

function pa_property_trigger(a,p,o) {

	let dlevel = 2;

	dprintf(dlevel,"new val: %s, old val: %s\n", p.value, o);

	dprintf(dlevel,"p.name: %s\n", p.name);
	let name = p.name.substr(0,p.name.indexOf("_property"));
	dprintf(dlevel,"name: %s\n", name);
	a["have_" + name] = false;
	a[name] = 0.0;
}

function pa_set_fspc(a,p,o) {

	let dlevel = 2;

	dprintf(dlevel,"fspc: %s\n", pa.fspc);
	if (typeof(pa.fspc) == "undefined") return;
	if (!pa.fspc) return;
	dprintf(dlevel,"fspc_start: %s\n", pa.fspc_start);
	if (typeof(pa.fspc_start) == "undefined") return;
	dprintf(dlevel,"fspc_end: %s\n", pa.fspc_end);
	if (typeof(pa.fspc_end) == "undefined") return;
	if (pa.fspc_start > pa.fspc_end) {
		config.errmsg = "fspc_start must be < fspc_end";
		return 1;
	}
	dprintf(dlevel,"nominal_frequency: %s\n", pa.nominal_frequency);
	if (typeof(pa.nominal_frequency) == "undefined") return;

	pa.fspc_start_frq = pa.nominal_frequency + pa.fspc_start;
	pa.fspc_end_frq = pa.nominal_frequency + pa.fspc_end;
	dprintf(dlevel,"fspc_start_frq: %.02f, fspc_end_frq: %.02f\n", pa.fspc_start_frq, pa.fspc_end_frq);
	return 0;
}

function pa_samples_trigger(a,p,o) {

    let dlevel = 1;

	dprintf(dlevel,"new value: %d\n", a.samples);
	for(let i=0; i < a.samples; i++) a.values[i] = 0;
	a.idx = 0;
}

function pa_sample_period_trigger(a,p,o) {

    let dlevel = 1;

	dprintf(dlevel,"new value: %d\n", a.sample_period);
    dprintf(dlevel,"interval: %d\n", a.interval);
    a.samples = a.sample_period / a.interval;
    return 0;
}

function pa_reserved_trigger(a,p,o) {

	let dlevel = 2;

	if (!pa) return;
	dprintf(dlevel,"budget: %d\n", pa.budget);
	if (pa.budget) {
		dprintf(dlevel,"new reserved: %.1f\n", p.value);
		pa.avail = pa.budget - pa.reserved;
		dprintf(dlevel,"NEW avail: %.1f\n", pa.avail);
	}
}

function pa_reserve(agent_name,module,item,str_amount,str_pri) {

	let dlevel = 2;

	dprintf(dlevel,"agent_name: %s, module: %s, item: %s, amount: %s, pri: %s\n", agent_name, module, item, str_amount, str_pri);

	let id = agent_name;
	if (module.length) id += PA_ID_DELIMITER + module;
	if (item.length) id += PA_ID_DELIMITER + item;
	let amount = parseFloat(str_amount);
	let pri = parseInt(str_pri);
	if (pri < 1) pri = 1;
	if (pri > 100) pri = 100;
	dprintf(dlevel,"id: %s, amount: %s, priority: %d\n", id, amount, pri);

	// If a reservation exists for the same id + amount delete it
	for(let i=0; i < pa.reservations.length; i++) {
		let res = pa.reservations[i];
		dprintf(dlevel,"check exist: res[%d]: id: %s, amount: %.1f, pri: %d\n", i, res.id, res.amount, res.pri);
		if (res.id == id && res.amount == amount) {
			pa.reserved  -= res.amount;
			pa.reservations.splice(i,1);
		}
	}

	if (!pa.budget) {
		// Deny everything until last_reserve_time + reserve_delay has passed
		dprintf(dlevel,"last_reserve_time: %s\n", pa.last_reserve_time);
		let diff = time() - pa.last_reserve_time;
		dprintf(dlevel,"diff: %d, reserve_delay: %d\n", diff, pa.reserve_delay);
		if (diff < pa.reserve_delay) {
			dprintf(dlevel,"waiting for %d seconds before next reserve\n", pa.reserve_delay - diff);
			config.errmsg = "denied";
			return 1;
		}
	}

	dprintf(dlevel,"budget: %d, avail: %.1f, power: %.1f\n", pa.budget, pa.avail, pa.power);
	dprintf(dlevel,"approve_p1: %s, pri: %d\n", pa.approve_p1, pri);
	let avail = (pa.budget ? pa.avail : pa.power);
	if (avail >= amount || pa.approve_p1 && pri == 1) {
		log_info("adding reservation for: id: %s, amount: %.1f, pri: %d\n", id, amount, pri);
		let newres = {};
		newres.id = id;
		newres.amount = amount;
		newres.pri = pri;
		newres.marked = false;
		pa.reservations.push(newres);
		pa.reservations.sort(function(a,b) { return a.amount - b.amount});
		pa.reserved += amount;
		pa.last_reserve_time = time();
		return 0;
	} else {
		config.errmsg = "denied";
		return 1;
	}
/*
	let r = 0;
	let do_add = false;
	let avail = (pa.budget ? pa.avail : pa.power);
	if (avail >= amount) {
		do_add = true;
	} else {
		// Are there lower pri reservations that can be revoked?
		// Can't do an sdconfig request (revoke) inside of a sdconfig request, so q it up
		let freed = 0;
		for(let i=0; i < pa.reservations.length; i++) {
			let res = pa.reservations[i];
			dprintf(dlevel,"adding to revoke list: res[%d]: id: %s, amount: %.1f, pri: %d\n", i, res.id, res.amount, res.pri);
			dprintf(dlevel,"marked: %s\n", res.marked);
			if (!res.marked && pri < res.pri) {
				res.marked = true;
				pa.revokes.push(res);
			}
			freed += res.amount;
			if (freed >= amount) break;
		}
		dprintf(dlevel,"approve_p1: %s\n", pa.approve_p1);
		if (pa.approve_p1 && pri == 1) {
			do_add = true;
		} else {
			config.errmsg = "denied";
			r = 1;
		}
	}
	dprintf(dlevel,"do_add: %s\n", do_add);
	if (do_add) {
		let newres = {};
		newres.id = id;
		newres.amount = amount;
		newres.pri = pri;
//		newres.marked = false;
		pa.reservations.push(newres);
//		pa.reservations.sort(function(a,b) { return a.amount - b.amount});
		pa.reserved += amount;
		pa.last_reserve_time = time();
	}
	dprintf(dlevel,"r: %d\n", r);
	return r;
*/
}

function pa_release(agent_name,module,item,str_amount) {

	let dlevel = 2;

	let id = agent_name;
	if (module.length) id += PA_ID_DELIMITER + module;
	if (item.length) id += PA_ID_DELIMITER + item;
	let amount = parseFloat(str_amount);
	dprintf(dlevel,"id: %s, amount: %s\n", id, amount);

	let r = 0;
	dprintf(dlevel,"reservation count: %d\n", pa.reservations.length);
	for(let i=0; i < pa.reservations.length; i++) {
		let res = pa.reservations[i];
		if (res.id == id && res.amount == amount) {
			log_info("removing reservation for: id: %s, amount: %.1f\n", id, amount);
			dprintf(dlevel,"found\n");
			pa.reservations.splice(i,1);
			pa.reserved -= amount;
			// XXX no reserves until timeout
			pa.last_reserve_time = time();
			r = 0;
			break;
		}
	}

	if (r) config.errmsg = "release: reservation not found";
	return r;
}

function pa_repri(agent_name,module,item,str_amount,str_pri) {

	let dlevel = -1;

	dprintf(dlevel,"agent_name: %s, module: %s, item: %s, amount: %s, pri: %s\n", agent_name, module, item, str_amount, str_pri);

	let id = agent_name;
	if (module.length) id += PA_ID_DELIMITER + module;
	if (item.length) id += PA_ID_DELIMITER + item;
	let amount = parseFloat(str_amount);
	let pri = parseInt(str_pri);
	if (pri < 1) pri = 1;
	if (pri > 100) pri = 100;
	dprintf(dlevel,"id: %s, amount: %s, priority: %d\n", id, amount, pri);

	// If a reservation exists for the same id + amount delete it
	let found = false;
	for(let i=0; i < pa.reservations.length; i++) {
		let res = pa.reservations[i];
		dprintf(dlevel,"check exist: res[%d]: id: %s, amount: %.1f, pri: %d\n", i, res.id, res.amount, res.pri);
		if (res.id == id && res.amount == amount) {
			found = true;
			res.pri = pri;
			break;
		}
	}
	dprintf(dlevel,"found: %s\n", found);
	if (!found) config.errmsg = "repri: reservation not found";
	return found ? 0 : 1;
}

function pa_get_number(data,name,invert) {

	let dlevel = 2;

	dprintf(dlevel,"name: %s, invert: %s\n", name, invert);
	let pfield = name + "_property";
	dprintf(dlevel,"pfield: %s\n", pfield);
	let pname = pa[pfield];
	dprintf(dlevel,"pname: %s\n", pname);
	let val = 0.0;
	let vname = "have_" + name;
	let found = false;
	dprintf(dlevel,"data[%s]: %s\n", pname, data[pname]);
	if (pname.length && typeof(data[pname]) != "undefined") {
		val = parseFloat(data[pname]);
		dprintf(dlevel,"val: %s\n", val);
		if (invert) {
			val *= -1;
			dprintf(dlevel,"NEW val: %s\n", val);
		}
		found = true;
	}
	pa[name] = val;
	pa["have_" + name] = found;
	dprintf(dlevel,"pa.%s: %s\n", name, val);
}

function pa_get_inverter_data(data) {
	pa_get_number(data,"grid_power",pa.invert_grid_power);
	pa_get_number(data,"load_power",pa.invert_load_power);
	pa_get_number(data,"frequency",false);
}

function pa_get_pv_data(data) {
	pa_get_number(data,"pv_power",pa.invert_pv_power);
}

function pa_get_battery_data(data) {
	pa_get_number(data,"battery_power",pa.invert_battery_power);
	pa_get_number(data,"battery_level",false);
}

function pa_process_messages(m,t,f) {

	let dlevel = 1;

//	dumpobj(m);
	let mq = m.mq;
	dprintf(dlevel,"mq.len: %d\n", mq.length);
	for(let i=mq.length-1; i >= 0; i--) {
		let msg = mq[i];
//		dumpobj(msg);
		let data = JSON.parse(msg.data);
//		dumpobj(data);
		if (msg.topic == t) f(data);
	}
	m.purgemq();
}

function pa_revoke(res) {

	let dlevel = 2;

	log_info("revoking reservation for: id: %s, amount: %.1f\n", res.id, res.amount);
	let arr = res.id.split(PA_ID_DELIMITER);
	let agent_name = arr[0];
	let module = arr[1];
	let item = arr[2];
	let r = sdconfig(mqtt,agent_name,"revoke",module,item,res.amount);
	if (r.status) dprintf(dlevel,"revoke failed: %s\n", r.message);
	return r.status;
}

function pa_revoke_all(res) {

	let dlevel = 2;

	for(let i=0; i < pa.reservations.length; i++) {
		let res = pa.reservations[i];
		dprintf(dlevel,"revoking: res[%d]: id: %s, amount: %.1f, pri: %d\n", i, res.id, res.amount, res.pri);
		pa.revokes.push(res);
	}
	return 0;
}

function main(argv) {

	let dlevel = 1;

	let driver = new Driver("pa");
	driver.read = function(control,buflen) {
		dprintf(dlevel,"*** READ ***\n");

		// Send out revocations
		dprintf(dlevel,"revokes.length: %d\n", pa.revokes.length);
		for(let i = 0; i < pa.revokes.length; i++ ) {
			pa_revoke(pa.revokes[i]);
			pa.revokes.splice(i,1);
		}

		// Process mqtt messages
		pa.have_grid_power = pa.have_pv_power = pa.have_battery_power = false;
		pa_process_messages(pa.inverter_mqtt,pa.inverter_topic,pa_get_inverter_data);
		pa_process_messages(pa.pv_mqtt,pa.pv_topic,pa_get_pv_data);
		pa_process_messages(pa.battery_mqtt,pa.battery_topic,pa_get_battery_data);

		// If freq not set, use nominal
		if (!pa.have_frequency) pa.frequency = pa.nominal_frequency;

		dprintf(dlevel,"budget: %d\n", pa.budget);
		if (!pa.budget) {
			let power = 0.0;

			// if we're pulling from the grid, power is neg
			dprintf(dlevel,"have_grid_power: %s, grid_power: %s\n", pa.have_grid_power, pa.grid_power);
			if (pa.have_grid_power && pa.grid_power < 0.0) {
				power += pa.grid_power;
				dprintf(dlevel,"NEW power(grid): %s\n", power);
			}

			// If we're pulling from the batt, power is neg
			dprintf(dlevel,"have_battery_power: %s, battery_power: %s\n", pa.have_battery_power, pa.battery_power);
			if (pa.have_battery_power && pa.battery_power < 0.0) {
				power += pa.battery_power;
				dprintf(dlevel,"NEW power(batt): %s\n", power);
			}

			// If we have pv power, subtract the load from it and add to avail power
			dprintf(dlevel,"have_pv_power: %s, pv_power: %s\n", pa.have_pv_power, pa.pv_power);
			if (pa.have_pv_power) {
				dprintf(dlevel,"have_load_power: %s, load_power: %s\n", pa.have_load_power, pa.load_power);
				if (!pa.have_load_power) {
					let used = 0.0;

					// Feeding to the grid
					dprintf(dlevel,"have_grid_power: %s, grid_power: %s\n", pa.have_grid_power, pa.grid_power);
					if (pa.have_grid_power && pa.grid_power > 0.0) {
						used += pa.grid_power;
						dprintf(dlevel,"NEW used(grid): %s\n", used);
					}
					dprintf(dlevel,"have_battery_power: %s, battery_power: %s\n", pa.have_battery_power, pa.battery_power);

					// Charging batteries
					if (pa.have_battery_power && pa.battery_power > 0.0) {
						used += pa.battery_power;
						dprintf(dlevel,"NEW used(batt): %s\n", used);
					}

					pa.load_power = pa.pv_power - used;
					dprintf(dlevel,"==> load_power: %f\n", pa.load_power);
				}

				// Adjust pv_power if fspc is active
				dprintf(dlevel,"fspc: %s, frequency: %.1f, start: %.1f\n", pa.fspc, pa.frequency, pa.fspc_start_frq);
				if (pa.fspc && pa.have_frequency && pa.frequency >= pa.fspc_start_frq) {
					dprintf(dlevel,"fspc_end_frq: %s\n", pa.fspc_end_frq);
					let diff = pa.fspc_end_frq - pa.frequency;
					dprintf(dlevel,"diff: %s, pv_power: %s\n", diff, pa.pv_power);
					if (diff < 0) diff = 1;
					pa.pv_power /= diff;
					dprintf(dlevel,"NEW pv_power(fspc): %.1f\n", pa.pv_power);
				}
				if (pa.pv_power > 0.0) {
					power += (pa.pv_power - pa.load_power);
					dprintf(dlevel,"NEW power(pv): %.1f\n", power);
				}
			}

			// Subtract batt power if charging and protected
			dprintf(dlevel,"power: %f, have_battery_power: %s, battery_power: %s, protected: %s\n",
				power, pa.have_battery_power, pa.battery_power, pa.protect_charge);
			if (power > 0.0 && pa.have_battery_power && pa.battery_power > 0.0 && pa.protect_charge) {
				power -= pa.battery_power;
				dprintf(dlevel,"NEW power(charge protect): %s\n", power);
			}

			dprintf(dlevel,"==> power: %s\n", power);

			pa.values[pa.idx++] = power;
			dprintf(dlevel,"idx: %d, samples: %d\n", pa.idx, pa.samples);
			if (pa.idx > pa.samples) pa.idx = 0;
			let total = 0.0;
			for(let i=0; i < pa.samples; i++) total += pa.values[i];
			dprintf(dlevel,"total: %.1f\n", total);
			pa.power = total / pa.samples;
			dprintf(dlevel,"averaged power: %s\n", pa.power);
			pa.avail = pa.power;
		}

		// If we run out of power for X seconds, start revoking
		if (pa.avail < 0) {
			dprintf(dlevel,"neg_power_time: %s\n", pa.neg_power_time);
			if (pa.neg_power_time == 0) {
				pa.neg_power_time = time();
			} else {
				let diff = time() - pa.neg_power_time;
				dprintf(dlevel,"diff: %d, timeout: %d\n", diff, pa.deficit_timeout);
				if (diff >= pa.deficit_timeout) {
					// Start revoking
					for(let i=0; i < pa.reservations.length; i++) {
						let res = pa.reservations[i];
						dprintf(dlevel,"neg check: res[%d]: id: %s, amount: %.1f, pri: %d\n", i, res.id, res.amount, res.pri);
						// Dont revoke p1 when approve is true
						if (res.pri == 1 && pa.approve_p1) continue;
						pa_revoke(res);
					}
				}
			}
		} else {
			pa.neg_power_time = 0;
		}

		let out;
		if (pa.budget) out = sprintf("budget: %.1f, reserved: %.1f, avail: %.1f", pa.budget, pa.reserved, pa.avail);
		else out =  sprintf("reserved: %.1f, avail: %.1f, pv_power: %.1f", pa.reserved, pa.power, pa.have_pv_power ? pa.pv_power : 0.0);
		last_out = 0;
		if (typeof(last_out) == "undefined") last_out = "";
		if (out != last_out) {
			log_info("%s\n", out);
			last_out = out;
		}
        printf("read done!\n");
	}
	driver.write = function(control,buflen) {
		dprintf(dlevel,"*** WRITE ***\n");
		let data;
		if (pa.budget) data = { budget: pa.budget, reserved: pa.reserved, avail: pa.avail };
		else data = { reserved: pa.reserved, avail: pa.power };
		mqtt.pub(pa.pub_topic,data);
	}
	driver.get_info = function() {
		dprintf(dlevel,"*** GET_INFO ****\n");
		var info = {};
		info.agent_name = "pa";
		info.agent_role = "Utility";
		info.agent_description = "Power Advisor";
		info.agent_version = pa_version;
    		info.agent_author = "Stephen P. Shoecraft";
		return JSON.stringify(info);
	}

	pa = agent = new Agent(argv,driver,pa_version,0,0,0);

	config = pa.config;
	mqtt = pa.mqtt;
	influx = pa.influx;

	pa.reservations = [];
	pa.revokes = [];
	pa.values = [];
	pa.power = 0;
	pa.values = [];
	pa.neg_power_time = 0;

	PA_ID_DELIMITER = "/";

	let props = [
		// Inverter power
		[ "inverter_server_config", DATA_TYPE_STRING, "localhost", 0, pa_server_config_trigger, pa ],
		[ "inverter_topic", DATA_TYPE_STRING, SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/si/"+SOLARD_FUNC_DATA, 0, pa_topic_trigger, pa ],
		[ "grid_power_property", DATA_TYPE_STRING, "input_power", 0, pa_property_trigger, pa ],
		[ "invert_grid_power", DATA_TYPE_BOOL, "no", 0 ],
		[ "load_power_property", DATA_TYPE_STRING, "load_power", 0, pa_property_trigger, pa ],
		[ "invert_load_power", DATA_TYPE_BOOL, "no", 0 ],
		[ "frequency_property", DATA_TYPE_STRING, "output_frequency", 0 ],

		// Solar power
		[ "pv_server_config", DATA_TYPE_STRING, "localhost", 0, pa_server_config_trigger, pa ],
		[ "pv_topic", DATA_TYPE_STRING, SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/pvc/"+SOLARD_FUNC_DATA, 0, pa_topic_trigger, pa ],
		[ "pv_power_property", DATA_TYPE_STRING, "output_power", 0, pa_property_trigger, pa ],
		[ "invert_pv_power", DATA_TYPE_BOOL, "no", 0 ],
		[ "fspc", DATA_TYPE_BOOL, "false", 0, pa_set_fspc ],
		[ "fspc_start", DATA_TYPE_DOUBLE, 1, 0, pa_set_fspc ],
		[ "fspc_end", DATA_TYPE_DOUBLE, 2, 0, pa_set_fspc ],
		[ "nominal_frequency", DATA_TYPE_DOUBLE, 60, 0, pa_set_fspc ],

		// Battery level
		[ "battery_server_config", DATA_TYPE_STRING, "localhost", 0, pa_server_config_trigger, pa ],
		[ "battery_topic", DATA_TYPE_STRING, SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/si/"+SOLARD_FUNC_DATA, 0, pa_topic_trigger, pa ],
		[ "battery_power_property", DATA_TYPE_STRING, "battery_power", 0, pa_property_trigger, pa ],
		[ "invert_battery_power", DATA_TYPE_BOOL, "no", 0 ],
		[ "battery_level_property", DATA_TYPE_STRING, "battery_level", 0, pa_property_trigger, pa ],

		[ "buffer", DATA_TYPE_INT, 0, 0 ],
		[ "samples", DATA_TYPE_INT, 0, CONFIG_FLAG_PRIVATE, pa_samples_trigger, pa ],
		[ "sample_period", DATA_TYPE_INT, 90, 0, pa_sample_period_trigger, pa ],
		[ "reserve_delay", DATA_TYPE_INT, 180, 0 ],
		[ "protect_charge", DATA_TYPE_BOOL, "no", 0, 0 ],
		[ "approve_p1", DATA_TYPE_BOOL, "yes", 0 ],
		[ "deficit_timeout", DATA_TYPE_INT, 300, 0 ],
		[ "pub_topic", DATA_TYPE_STRING, SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/" + agent.name + "/"+SOLARD_FUNC_DATA, 0 ],
		[ "reserved", DATA_TYPE_INT, 0, CONFIG_FLAG_PRIVATE, pa_reserved_trigger, pa ],
		[ "budget", DATA_TYPE_FLOAT, "0.0", 0, pa_budget_trigger, pa ],
		[ "battery_limit", DATA_TYPE_FLOAT, "0.0", 0 ],
		[ "battery_scale", DATA_TYPE_BOOL, "yes", 0 ],
		[ "interval", DATA_TYPE_INT, 15, CONFIG_FLAG_NOWARN, pa_sample_period_trigger, pa ]
	];
	config.add_props(pa,props,driver.name);

	let funcs = [
		[ "reserve", pa_reserve, 5 ], 
		[ "release", pa_release, 4 ], 
		[ "repri", pa_repri, 5 ], 
		[ "revoke_all", pa_revoke_all, 0 ], 
	];
	config.add_funcs(agent,funcs,driver.name);
	agent.repub();

	// Init non-config vars
	pa.last_reserve_time = time() - pa.reserve_delay;

	agent.run();
}
