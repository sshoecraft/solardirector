
function mirror_event(data) {

	let dlevel = 1;

	dprintf(dlevel,"data: %s\n", data);
	m = JSON.parse(data);
	dprintf(dlevel,"agent: %s, module: %s, action: %s\n", m.agent, m.module, m.action);
	if (m.module == "Charge" && m.action == "Start") charge_start(false);
	else if (m.module == "Charge" && m.action == "StartCV") charge_start_cv(false);
	else if (m.module == "Charge" && m.action == "Stop") charge_stop(false);
	else if (m.module == "Feed" && m.action == "Start") feed_start(false);
	else if (m.module == "Feed" && m.action == "Stop") feed_stop(false);
	else if (m.module == "Battery" && m.action == "Empty") charge_empty();
	else if (m.module == "Battery" && m.action == "Full") charge_full();
}

function mirror_get_data() {

	let dlevel = 1;

	let inverter = undefined;
	if (mirror_type == "mqtt") {
		dprintf(dlevel,"connected: %s\n", mirror_mqtt.connected);
		if (!mirror_mqtt.connected) {
			mirror_mqtt.reconnect();
			mirror_mqtt.resub();
		}
		if (!mirror_mqtt.connected) return 1;
		let mq = mirror_mqtt.mq;
		let inverter_data = undefined;
		dprintf(dlevel,"mq.length: %d\n", mq.length);
		for(let i=0; i < mq.length; i++) {
			dprintf(dlevel,"mq[%d].func: %s\n", i, mq[i].func);
			if (mq[i].func == "Event") mirror_event(mq[i].data);
			else if (mq[i].func == "Data") inverter_data = mq[i].data;
		}
		dprintf(dlevel+2,"inverter_data: %s\n", inverter_data);
		if (inverter_data) inverter = JSON.parse(inverter_data);
		mirror_mqtt.purgemq();
	} else if (mirror_type == "influx") {
		let query = "select last(*) from inverter";
		if (mirror_name && mirror_name.length) query += " WHERE \"name\" = '" + mirror_name + "'";
		dprintf(dlevel,"query: %s\n", query);
		inverter = influx2obj(mirror_influx.query(query));
	}

	// Convert inverter to si_data
	if (!inverter) {
		dprintf(dlevel,"diff: %d, timeout: %d\n", time() - last_inverter_time, si.mirror_timeout);
		if (time() - last_inverter_time > si.mirror_timeout) return 1;
		inverter = last_inverter;
	} else {
		last_inverter = inverter;
		last_inverter_time = time();
	}
//	dumpobj(inverter);

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
//	if (inverter.status.search("CC") >= 0 && si.charge_mode != 1) charge_start(false);
//	else if (inverter.status.search("CV") >= 0 && si.charge_mode != 2) charge_start_cv(false);
	si.feed_enabled = inverter.status.search("feed") >= 0 ? true : false;
	si.dynfeed = inverter.status.search("dynfeed") >= 0 ? true : false;
	si.dyngrid = inverter.status.search("dyngrid") >= 0 ? true : false;
	si.dyngen = inverter.status.search("dyngen") >= 0 ? true : false;

	return 0;
}

function mirror_source_trigger() {

	let dlevel = 1;

	mirror_conx = undefined;

	dprintf(dlevel,"mirror_source(%d): %s\n", si.mirror_source.length, si.mirror_source);
	if (si.mirror_info) delete si.mirror_info;
	if (!si.mirror_source.length) return;
	// Source,conx,name
	// influx,192.168.1.142,power
	// mqtt,192.168.1.4,si
	let tf = si.mirror_source.split(",");
	mirror_type = tf[0];
	dprintf(dlevel,"type: %s\n", mirror_type);
	if (mirror_type != 'influx' && mirror_type != 'mqtt') {
		log_error("mirror_source type must be mqtt or influx\n");
		si.mirror = false;
		return;
	}
	let conx = tf[1];
	dprintf(dlevel,"conx: %s\n", conx);
	if (!conx) conx = "localhost";

	if (mirror_type == "influx") {
		let mirror_db = tf[2];
		dprintf(dlevel,"db: %s\n", mirror_db);
		if (!mirror_db) mirror_db = "solardirector";
		mirror_name = tf[3];
		dprintf(dlevel,"name: %s\n", mirror_name);
		mirror_influx = new Influx(conx,mirror_db);
	} else if (mirror_type == "mqtt") {
		mirror_name = tf[2];
		dprintf(dlevel,"name: %s\n", mirror_name);
		mirror_mqtt = new MQTT(conx);
		mirror_mqtt.enabled = true;
		mirror_mqtt.connect();
		if (!mirror_mqtt.connected) log_error("unable to connect to mirror_source %s: %s\n", conx, mirror_mqtt.errmsg);
		mirror_mqtt.sub(SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/" + mirror_name + "/"+SOLARD_FUNC_DATA);
		mirror_mqtt.sub(SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/" + mirror_name + "/"+SOLARD_FUNC_EVENT);
	}
}

function mirror_init() {
	var mirror_props = [
		[ "mirror", DATA_TYPE_BOOL, "false", 0 ],
		[ "mirror_source", DATA_TYPE_STRING, null, 0, mirror_source_trigger ],
		[ "mirror_timeout", DATA_TYPE_INT, 30, 0 ],
	];

	config.add_props(si,mirror_props);
	last_inverter = undefined;
	last_inverter_time = 0;
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
