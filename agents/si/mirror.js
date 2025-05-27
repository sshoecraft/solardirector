
function mirror_event(data) {

	let dlevel = 0;

	dprintf(dlevel+1,"data: %s\n", data);
	m = JSON.parse(data);
	dprintf(dlevel,"agent: %s, module: %s, action: %s\n", m.agent, m.module, m.action);
	if (m.module == "Charge" && m.action == "Start") charge_start(false);
	else if (m.module == "Charge" && m.action == "StartCV") charge_start_cv(false);
	else if (m.module == "Charge" && m.action == "Stop") charge_stop(false);
	else if (m.module == "Feed" && m.action == "Start") feed_start(false);
	else if (m.module == "Feed" && m.action == "Stop") feed_stop(false);
	else if (m.module == "Battery" && m.action == "Empty") soc_battery_empty();
	else if (m.module == "Battery" && m.action == "Full") soc_battery_full();
}

function mirror_get_data() {

	let dlevel = 1;

	dprintf(dlevel,"mirror_info: %s\n", si.mirror_info);
	if (!si.mirror_info) {
		printf("mirror_get_data: mirror_info is not defined!\n");
		return 1;
	}
	info = si.mirror_info;
//	dumpobj(info);

	if (!info.init) {
		if (info.type == "influx") {
			info.db = new Influx(info.conx,info.db_name);
			if (!info.db.connected) log_error("unable to connect to mirror_source %s: %s\n",
				info.conx, info.db.errmsg);
		} else if (info.type == "mqtt") {
			info.mqtt = new MQTT(info.conx);
			dprintf(dlevel,"connected: %s\n", info.mqtt.connected);
			if (!info.mqtt.connected) log_error("unable to connect to mirror_source %s: %s\n",
				info.conx, info.mqtt.errmsg);
			info.topic = SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/" + info.name;
			info.mqtt.sub(info.topic + "/" + SOLARD_FUNC_DATA);
			info.mqtt.sub(info.topic + "/" + SOLARD_FUNC_EVENT);
		} else {
			return 1;
		}
		printf("Mirroring from: %s\n",si.mirror_source);
		info.init = true;
	}
//	dumpobj(info);

	let inverter = undefined;
	if (info.type == "mqtt") {
		dprintf(dlevel,"connected: %s\n", info.mqtt.connected);
		if (!info.mqtt.connected) info.mqtt.reconnect();
		let mq = info.mqtt.mq;
		dprintf(dlevel,"mq.length: %d\n", mq.length);
		for(let i=0; i < mq.length; i++) {
			dprintf(dlevel,"mq[%d].func: %s\n", i, mq[i].func);
			if (mq[i].func == "Event") mirror_event(mq[i].data);
		}
		for(let i=mq.length-1; i >= 0; i--) {
			if (mq[i].func == "Data") {
				inverter = JSON.parse(mq[i].data);
				break;
			}
		}
		info.mqtt.purgemq();
	} else if (info.type == "influx") {
		query = "select last(*) from inverter";
		if (info.name && info.name.length) query += " WHERE \"name\" = '" + info.name + "'";
		dprintf(dlevel,"query: %s\n", query);
		inverter = influx2obj(info.db.query(query));
	}

	// Convert inverter to si_data
	dprintf(dlevel,"inverter: %s\n", inverter);
	if (!inverter) {
		dprintf(dlevel,"diff: %d, timeout: %d\n", time() - info.last_inverter_time, si.mirror_timeout);
		if (time() - info.last_inverter_time > si.mirror_timeout) {
			return 0;
		}
		inverter = info.last_inverter;
	} else {
		info.last_inverter = inverter;
		info.last_inverter_time = time();
	}
//	dumpobj(inverter);

	if (!inverter) return;

	data.Run = true;
	data.battery_voltage = inverter.battery_voltage;
	data.battery_current = inverter.battery_current;
	data.battery_power = inverter.battery_power;
	data.battery_temp = inverter.battery_temp;
	data.battery_soc = inverter.battery_level;
	si.soc = inverter.battery_level;
	data.ac2_voltage = inverter.input_voltage;
	data.ac2_current = inverter.input_current;
	data.ac2_frequency = inverter.input_frequency;
	data.ac2_power = inverter.input_power;
	data.TotLodPwr = inverter.load_power;
	data.ac1_voltage = inverter.output_voltage;
	data.ac1_current = inverter.output_current;
	data.ac1_frequency = inverter.output_frequency;
	data.ac1_power = inverter.output_power;
//	dprintf(0,"status: %s\n", inverter.status);
	data.GdOn = inverter.status.search("grid") >= 0 ? true : false;
	data.GnOn = inverter.status.search("gen") >= 0 ? true : false;
	if (inverter.status.search("CC") >= 0 && si.charge_mode != 1) charge_start(false);
	else if (inverter.status.search("CV") >= 0 && si.charge_mode != 2) charge_start_cv(false);
	si.feed_enabled = inverter.status.search("feed") >= 0 ? true : false;
	si.dynfeed = inverter.status.search("dynfeed") >= 0 ? true : false;
	si.dyngrid = inverter.status.search("dyngrid") >= 0 ? true : false;
	si.dyngen = inverter.status.search("dyngen") >= 0 ? true : false;
	return 0;
}

function mirror_enabled_trigger(a,p,o) {
	let n = p ? p.value : undefined;
	dprintf(1,"new val: %s, old val: %s\n", n, o);

	if (n == false && o == true) {
		info = si.mirror_info;
		dprintf(1,"type: %s\n", info.type);
		if (info.type == "mqtt") {
			dprintf(1,"connected: %s\n", info.mqtt.connected);
			info.mqtt.disconnect();
			info.mqtt.purgemq();
		} else if (info.type == "influx") {
			dprintf(1,"connected: %s\n", info.db.connected);
			info.db.disconnect();
		}
	}
}

function mirror_source_trigger() {

	let dlevel = 1;

	dprintf(dlevel,"mirror_source(%d): %s\n", si.mirror_source.length, si.mirror_source);
	if (si.mirror_info) {
		// Delete the old objects
		delete si.mirror_info.mqtt;
		delete si.mirror_info.db;
		delete si.mirror_info;
	}
	if (!si.mirror_source.length) return;
	si.mirror_info = new Object();
	info = si.mirror_info;

	// Source,conx,name
	// influx,myinfluxdb,power
	// mqtt,mymqtt,si
	let tf = si.mirror_source.split(",");
	info.type = tf[0];
	dprintf(dlevel,"type: %s\n", info.type);
	if (info.type != 'influx' && info.type != 'mqtt') {
		log_error("mirror_source type must be mqtt or influx\n");
		delete si.mirror_info;
		si.mirror_info = undefined;
		return;
	}
	info.conx = tf[1];
	dprintf(dlevel,"conx: %s\n", info.conx);
	if (!info.conx) info.conx = "localhost";

	if (info.type == "influx") {
		info.db = tf[2];
		dprintf(dlevel,"db: %s\n", info.db_name);
		if (!info.db_name) info.db_name = "solardirector";
		info.name = tf[3];
	} else if (info.type == "mqtt") {
		info.name = tf[2];
	} else {
		log_error("unknown mirror type: %s\n", info.type);
		info.error = 1;
	}
	dprintf(dlevel,"name: %s\n", info.name);
	info.init = false;
//	dumpobj(info);
}

function mirror_init() {
	var mirror_props = [
		[ "mirror", DATA_TYPE_BOOL, "false", 0, mirror_enabled_trigger ],
		[ "mirror_source", DATA_TYPE_STRING, null, 0, mirror_source_trigger ],
		[ "mirror_timeout", DATA_TYPE_INT, 30, 0 ],
	];

	config.add_props(si,mirror_props,si.driver_name);
	if (si.mirror_info) {
		si.mirror_info.last_inverter = undefined;
		si.mirror_info.last_inverter_time = 0;
	}
}

function mirror_main() {

	var dlevel = 1;

	dprintf(dlevel,"mirror_source: %s\n", si.mirror_source);
	if (!si.mirror_source) {
		log_error("mirror enabled yet no mirror_source set, mirror disabled.\n");
		si.mirror = false;
		return 1;
	}
	return mirror_get_data();
}
