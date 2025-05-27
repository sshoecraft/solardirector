
function sc_get_info(name) {

	let dlevel = 1;

//	dprintf(dlevel,"name: %s\n", name);

	let names = sc.agents.esplit(",");
	for(let i=0; i < names.length; i++) {
		if (names[i] == name) {
			var info = agents[name];
			if (typeof(info) == "undefined") {
				config.errmsg = "internal error: agentinfo not defined!";
				return undefined;
			}
			return info;
		}
	}
	config.errmsg = "error: agent not found";
	return undefined;
}

function agent_get_role(info) {
	let mq,msg;

	let dlevel = 1;

	dprintf(dlevel,"info.name: %s\n", info.name);

        mq = agent.messages;
        dprintf(dlevel,"mq len: %d\n", mq.length);
        for(let j=0; j < mq.length; j++) {
                msg = mq[j];
		dprintf(dlevel,"name: %s, func: %s\n", msg.name, msg.func);
		if (msg.func == "Info") {
			let agent_info = JSON.parse(msg.data);
			dprintf(dlevel,"agent_info.name: %s\n", agent_info.agent_name);
			if (agent_info.agent_name == info.name) {
				dprintf(dlevel,"setting role to: %s\n", agent_info.agent_role);
				info.role = agent_info.agent_role;
				return true;
			}
		}
	}
	if (info.managed) {
		// Didnt get it from mqtt, get from driver
		let cmd = info.path + " -I";
		dprintf(dlevel,"cmd: %s\n", cmd);
		let out = run_command(cmd);
		if (!out || !out.length) return false;
		dprintf(dlevel,"out type: %s\n", typeof(out));
		dprintf(dlevel,"out.length: %d\n", out.length);
		for(j=0; j < out.length; j++) {
			dprintf(dlevel,"out[%d]: %s\n", j, out[j]);
			if (out[j] == "{") break;
		}
		dprintf(dlevel,"j: %d\n", j);
		if (j) out.splice(0,j);
		dprintf(dlevel,"NEW out.length: %d\n", out.length);
		if (out.length) {
			let out_line = out.join("\n");
			dprintf(dlevel,"out_line: %s\n", out_line);
			let agent_info = JSON.parse(out_line);
			dprintf(dlevel,"agent_info type: %s\n", typeof(agent_info));
			dprintf(dlevel,"agent_info.name: %s, info.name: %s\n", agent_info.agent_name, info.name);
			if (agent_info.agent_name == info.name) {
				dprintf(dlevel,"setting role to: %s\n", agent_info.agent_role);
				info.role = agent_info.agent_role;
				return true;
			}
		}
	}

	return false;
}

function sc_verify_agent(name,info) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s\n", name);

	dprintf(dlevel,"managed: %s\n", info.managed);
	if (info.managed) {
		dprintf(dlevel,"path: %s\n", info.path);
		if (!info.path.length) {
			info.path = SOLARD_BINDIR + "/" + info.name;
			dprintf(dlevel,"NEW path: %s\n", info.path);
		} else if (info.path[0] != '/') {
			info.path = SOLARD_BINDIR + "/" + info.name;
			dprintf(dlevel,"NEW path: %s\n", info.path);
		}
		if (!new File(info.path).exists) {
			config.errmsg = "error: agent path " + info.path + " does not exist";
			printf("%s\n",config.errmsg);
			return 1;
		}
		// must get role after path set
		dprintf(dlevel,"role.length: %d\n", info.role.length);
		if (!info.role.length) {
			agent_get_role(info);
			if (!info.role.length) printf("warning: unable to get role for agent %s\n", name);
		}
		dprintf(dlevel,"conf: %s\n", info.conf);
		if (!info.conf.length) {
			if (new File(SOLARD_ETCDIR + "/" + name + ".json").exists)
				info.conf = SOLARD_ETCDIR + "/" + name + ".json";
			else if (new File(SOLARD_ETCDIR + "/" + name + ".conf").exists)
				info.conf = SOLARD_ETCDIR + "/" + name + ".conf";
			else if (new File(SOLARD_ETCDIR + "/" + info.name + ".json").exists)
				info.conf = SOLARD_ETCDIR + "/" + info.name + ".json";
			else if (new File(SOLARD_ETCDIR + "/" + info.name + ".conf").exists)
				info.conf = SOLARD_ETCDIR + "/" + info.name + ".conf";
			else
				info.conf = SOLARD_ETCDIR + "/" + info.name + ".json";
			dprintf(dlevel,"NEW conf: %s\n", info.conf);
		} else if (info.conf[0] != '/') {
			info.conf = SOLARD_ETCDIR + "/" + info.conf;
			dprintf(dlevel,"NEW conf: %s\n", info.conf);
		}
		if (!new File(info.conf).exists) {
			printf("%s\n","warning: conf file '%s' for %s does not exist",info.conf,name);
		}
		dprintf(dlevel,"log.length: %d\n", info.log.length);
		if (!info.log.length) {
			// use SC name vs agent name
			info.log = SOLARD_LOGDIR + "/" + name + ".log";
			dprintf(dlevel,"NEW log: %s\n", info.log);
		} else if (info.log[0] != '/') {
			info.log = SOLARD_LOGDIR + "/" + info.log;
			dprintf(dlevel,"NEW log: %s\n", info.log);
		}
	}
	if (!info.topic.length) info.topic = sprintf("%s/%s/%s/%s",SOLARD_TOPIC_ROOT,SOLARD_TOPIC_AGENTS,name,SOLARD_FUNC_DATA);
	if (!info.warning_time) info.warning_time = -1;
	if (!info.error_time) info.error_time = -1;
	if (!info.notify_time) info.notify_time = -1;
	return 0;
}

function sc_set_agent(name,opt_str) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, opt_str: %s\n", name, opt_str);

	let names = sc.agents.esplit(",");
	let found = false;
	for(let i=0; i < names.length; i++) {
		if (names[i] == name) {
			found=true;
		}
	}
	dprintf(dlevel,"found: %s\n", found);
	if (!found) {
		config.errmsg = "error: name not found";
		return 1;
	}
	let info = agents[name];
	if (typeof(info) == "undefined") {
		config.errmsg = "internal error: agentinfo not defined!";
		return 1;
	}

	// opts are key=value pair delimited by colon (:)
	let opts = opt_str.esplit(":");
	dprintf(dlevel,"opts.length: %d\n", opts.length);
	for(let i=0; i < opts.length; i++) {
		let opt = opts[i];
		dprintf(dlevel,"opt: %s\n", opt);
		let kv = opt.esplit("=");
		let key = kv[0];
		let value = kv[1];
		dprintf(dlevel,"key: %s, value: %s\n", key, value);
		if (typeof(key) == "undefined" || typeof(value) == "undefined") {
			config.errmsg = sprintf("error: invalid key/value pair: %s",opt_str);
			printf("adding %s: %s\n",name,config.errmsg);
			return 1;
		}
		// make sure key is in agents
		let found = false;
		for(kn in info) {
			dprintf(dlevel,"kn: %s\n",kn);
			if (kn == key) {
				found=true;
				break;
			}
		}
		if (!found) {
			config.errmsg = "error: key '" + key + "' not found.";
			printf("adding %s: %s\n",name,config.errmsg);
			return 1;
		}
		info[key] = value;
	}
	if (typeof(info.name) == "undefined") {
		config.errmsg = "error: agent name must be specified";
		printf("adding %s: %s\n",name,config.errmsg);
		return 1;
	}
	if (sc_verify_agent(name,info)) return 1;
	config.write();
	return 0;
}

function sc_add_agent(name,opts) {

	let dlevel = 1;

	let names = sc.agents.esplit(",");
	for(let i=0; i < names.length; i++) {
		if (names[i] == name) {
			config.errmsg = "error: name exists";
			return 1;
		}
	}
	dprintf(dlevel,"name: %s, opts: %s\n", name, opts);
	agents[name] = new Class(name);
	let info = agents[name];
	info.config = config;
	info.key = name;
	config.add_props(info,agent_props);
	printf("names: %s\n", names);
	names.push(name);
	let names_save = sc.agents;
	sc.agents = names.join(",");
	dprintf(dlevel,"NEW names: %s\n", sc.agents);
	if (sc_set_agent(name,opts)) {
		sc.agents = names_save;
		delete agents[name];
		return 1;
	}
	return 0;
}

function sc_del_agent(name) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s\n", name);
	let names = sc.agents.esplit(",");
	let i = names.indexOf(name);
	if (i >= 0) {
		printf("names: %s\n", names);
		names.splice(i,1);
		printf("NEW names: %s\n", names);
		sc.agents = names.join(",");
		config.delete_section(name);
		config.write();
		return 0;
	}
	config.errmsg = "error: name not found";
	return 1;
}

function _start_agent(info) {

	let dlevel = 1;

	dprintf(dlevel,"status: %d\n", info.status);
	if (info.status == AGENT_STATUS_NONE) {
		// dont start if started less than 30 sec ago
		let cur = time();
		let started = (typeof(info.started) == "undefined") ? cur - 30 : info.started;
		let diff = cur - started;
		dprintf(dlevel,"diff: %d\n", diff);
		if (diff < 30) return 0;
		// Start it
		let args = [ info.path, "-c", info.conf, "-l", info.log ];
		if (info.log_append) args.push("-a");
		dprintf(dlevel,"debug: %d\n", debug);
		if (debug > 0) {
			args.push("-d");
			args.push(debug.toString());
		}
		dprintf(dlevel,"mqtt_uri: %s(%d)\n", sc.mqtt_uri, sc.mqtt_uri.length);
		dprintf(dlevel,"sc.mqtt_username: %s(%d)\n", sc.mqtt_username, sc.mqtt_username.length);
		dprintf(dlevel,"sc.mqtt_password: %s(%d)\n", sc.mqtt_password, sc.mqtt_password.length);
		if (sc.mqtt_uri.length || sc.mqtt_username.length || sc.mqtt_password.length) {
			args.push("-m");
			args.push(sc.mqtt_uri + ",," + sc.mqtt_username + "," + sc.mqtt_password);
		}
		dprintf(dlevel,"args: %s\n", args);
if (0) {
		info.pid = sc.exec(info.path,args);
		if (info.pid < 0) {
			log_syserr("unable to exec: " + info.path);
			return 1;
		}
}
		info.started = time();
		info.status = AGENT_STATUS_STARTED;
	}
	return 0;
}

function sc_start_agent(name) {

	let dlevel = 1;

	let info = sc_get_info(name);
	if (!info) return 1;

	if (info.status == AGENT_STATUS_STARTED) {
		config.errmsg = "error: agent already started";
		return 1;
	}

	return _start_agent(info);
}

function sc_stop_agent(name) {

	let dlevel = -1;

	dprintf(dlevel,"name: %s\n", name);
}

function agents_init() {

	let dlevel = 1;

	dprintf(dlevel,"*** AGENTS INIT ****\n");

	AGENT_STATUS_NONE = 0;
	AGENT_STATUS_STARTED = 1;
	AGENT_STATUS_STOPPED = 2;

	// Declare the per-agent props here as a global
	agent_props = [
		[ "enabled", DATA_TYPE_BOOL, "true", 0 ],
		[ "managed", DATA_TYPE_BOOL, "true", 0 ],
		[ "name", DATA_TYPE_STRING, null, 0 ],
		[ "role", DATA_TYPE_STRING, null, 0 ],
		[ "path", DATA_TYPE_STRING, null, 0 ],
		[ "conf", DATA_TYPE_STRING, null, 0 ],
		[ "log", DATA_TYPE_STRING, null, 0 ],
		[ "log_append", DATA_TYPE_BOOL, "false", 0 ],
		[ "monitor", DATA_TYPE_BOOL, "true", 0 ],
		[ "topic", DATA_TYPE_STRING, null, 0 ],
		[ "notify", DATA_TYPE_BOOL, "true", 0 ],
		[ "notify_path", DATA_TYPE_STRING, null, 0 ],
		[ "warning_time", DATA_TYPE_INT, "-1", 0 ],
		[ "error_time", DATA_TYPE_INT, "-1" , 0 ],
		[ "notify_time", DATA_TYPE_INT, "-1" , 0 ],
		[ "status", DATA_TYPE_INT, AGENT_STATUS_NONE.toString(), CONFIG_FLAG_PRIVATE ],
	];

//	let sc_agent_props = [
//		[ "agents", DATA_TYPE_STRING, null, 0 ],
//	];
//	let sc_agent_funcs = [
//		[ "add_agent", sc_add_agent, 2 ],
//		[ "del_agent", sc_del_agent, 1 ],
//		[ "get_agent", sc_get_agent, 2 ],
//		[ "set_agent", sc_set_agent, 2 ],
//		[ "get_agent_config", sc_get_agent_config, 2 ],
//		[ "set_agent_config", sc_set_agent_config, 2 ],
//		[ "start_agent", sc_start_agent, 1 ],
//		[ "stop_agent", sc_stop_agent, 1 ]
//		[ "restart_agent", sc_stop_agent, 1 ]
//	];

//	config.add_props(sc,sc_agent_props);
//	config.add_funcs(sc,sc_agent_funcs);

	var agents = {};
}

function sc_load_agents() {

	let dlevel = 1;

	dprintf(dlevel,"agents: %s\n", sc.agents);
	let names = sc.agents.esplit(",");
	dprintf(dlevel,"names length: %d\n", names.length);
	let name;
	agents = [];
	let updated = false;
	for(let i=0; i < names.length; i++) {
		name = names[i];
		dprintf(dlevel,"name: %s(%d)\n", name, name.length);
		if (!name.length) {
			printf("error: name is empty!\n");
			continue;
		}
		agents[name] = new Class(name);
		let info = agents[name];
		info.config = config;
		info.key = name;
		config.add_props(info,agent_props);
		if (!info.name.length) info.name = name;
		let before = JSON.stringify(info);
//		dprintf(dlevel+2,"before: %s\n", before);
		if (sc_verify_agent(name,info)) {
			dprintf(dlevel,"verify failed for %s!\n", name);
			printf("names: %s\n", names);
			names.splice(i,1);
			printf("NEW names: %s\n", names);
			sc.agents = names.join(",");
			config.delete_section(name);
			delete agents[name];
			return 1;
		}
		let after = JSON.stringify(info);
//		dprintf(dlevel+2,"after: %s\n", after);
		if (after != before) updated = true;
	}
	dprintf(dlevel,"updated: %s\n", updated);
	if (updated) config.write();
        agent.pubinfo();
        agent.pubconfig();
	return 0;
}

function agents_load() {

	let dlevel = 1;

//	dprintf(dlevel,"name: %s\n", name);
	agents = read_json(ac.etcdir + "/" + agent.name + "_agents.json",true);
}


function agents_main() {

	let dlevel = -1;

	if (typeof(sc) == "undefined") {
		sc = new Driver("sc");
		sc.agents = "";
	}
	if (typeof(agent) == "undefined") {
//		agent = new Agent([ "-d", "8", "-c", "sctest.json" ],sc,"1.0",null,null);
		agent = new Agent([ "-c", "sctest.json" ],sc,"1.0",null,null);
	}
	config = agent.config;
	dprintf(-1,"calling init\n");
	agents_init();
	dprintf(-1,"loading agents...\n");
//	sc_load_agents();
	agents_load();
	dumpobj(agents);
}
