
function agent_get_role(name,info) {
	let mq,msg;

	let dlevel = 1;

	dprintf(dlevel,"name: %s\n", name);

	info.role = "";
        mq = agent.messages;
        dprintf(dlevel,"mq len: %d\n", mq.length);
        for(let j=0; j < mq.length; j++) {
                msg = mq[j];
		dprintf(dlevel,"name: %s, func: %s\n", msg.name, msg.func);
		if (msg.func == "Info") {
			let data = JSON.parse(msg.data);
			dprintf(dlevel+1,"agent_name: %s\n", data.agent_name);
			if (data.agent_name == name) {
				dprintf(dlevel,"setting role to: %s\n", data.agent_role);
				info.role = data.agent_role;
				break;
			}
		}
	}

	dprintf(dlevel,"role: %s(%d)\n", info.role, info.role.length);
	if (!info.role.length && info.managed) {
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
//			dprintf(dlevel,"out_line: %s\n", out_line);
			let info = JSON.parse(out_line);
			dprintf(dlevel,"info type: %s\n", typeof(info));
			dprintf(dlevel,"info.name: %s, name: %s\n", info.agent_name, name);
			if (info.agent_name == name) {
				dprintf(dlevel,"setting role to: %s\n", info.agent_role);
				info.role = info.agent_role;
			}
		}
	}

	return (info.role.length ? 0 : 1);
}

function sc_agent_get_conf(name,info) {
	if (new File(SOLARD_ETCDIR + "/" + name + ".json").exists)
		info.conf = SOLARD_ETCDIR + "/" + name + ".json";
	else if (new File(SOLARD_ETCDIR + "/" + name + ".conf").exists)
		info.conf = SOLARD_ETCDIR + "/" + name + ".conf";
	else if (new File(SOLARD_ETCDIR + "/" + name + ".json").exists)
		info.conf = SOLARD_ETCDIR + "/" + name + ".json";
	else if (new File(SOLARD_ETCDIR + "/" + name + ".conf").exists)
		info.conf = SOLARD_ETCDIR + "/" + name + ".conf";
	else
		info.conf = SOLARD_ETCDIR + "/" + name + ".json";
	dprintf(dlevel,"NEW conf: %s\n", info.conf);
}

function sc_agent_verify(name,info) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s\n", name);

	dprintf(dlevel,"managed: %s\n", info.managed);
	if (info.managed) {
		// Program path
		dprintf(dlevel,"path: %s\n", info.path);
		if (!info.path.length) {
			info.path = SOLARD_BINDIR + "/" + name;
			dprintf(dlevel,"NEW path: %s\n", info.path);
		} else if (info.path[0] != '/') {
			info.path = SOLARD_BINDIR + "/" + name;
			dprintf(dlevel,"NEW path: %s\n", info.path);
		}
		if (!new File(info.path).exists) {
			config.errmsg = "error: agent path " + info.path + " does not exist";
			printf("%s\n",config.errmsg);
			return 1;
		}

		// Role
		dprintf(dlevel,"role.length: %d\n", info.role.length);
		if (!info.role.length) {
			agent_get_role(name,info);
			if (!info.role.length) printf("warning: unable to get role for agent %s\n", name);
		}

		// Conf
		dprintf(dlevel,"conf: %s\n", info.conf);
		if (!info.conf.length) sc_agent_get_conf(name,info);
		if (!new File(info.conf).exists) log_warning("warning: conf file '%s' for %s does not exist\n",info.conf,name);

		// Log
		dprintf(dlevel,"log.length: %d\n", info.log.length);
		if (!info.log.length) {
			// use SC name vs agent name
			info.log = SOLARD_LOGDIR + "/" + name + ".log";
			dprintf(dlevel,"NEW log: %s\n", info.log);
		}
	}

	// Topic
	if (!info.topic.length) info.topic = sprintf("%s/%s/%s/%s",SOLARD_TOPIC_ROOT,SOLARD_TOPIC_AGENTS,name,SOLARD_FUNC_DATA);

	// Timeouts
	if (!info.warning_time) info.warning_time = -1;
	if (!info.error_time) info.error_time = -1;
	if (!info.notify_time) info.notify_time = -1;
	return 0;
}

// jconfig funcs
function _agent_init(name,info) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s\n", name);
//	dumpobj(info);

	if (sc_agent_verify(name,info)) return 1;

	// is it managed?  is the process running?  get the pid if so
	info.pid = sc.pidofpath(info.path);
	dprintf(dlevel,"pid: %d\n", info.pid);
	if (info.pid < 0) info.pid = sc.pidofname(name);
	if (info.pid >= 0) info.status = AGENT_STATUS_STARTED;
	else info.status = AGENT_STATUS_STOPPED;

	return 0;
}

function agent_add(name,opts) {
	return jconfig_add(name,opts,"agent",agents,agent_props,_agent_init);
}

function agent_del(name) {
	return jconfig_del(name,"agent",agents,agent_props);
}

function agent_get(name) {
	return jconfig_get(name,"agent",agents,agent_props);
}

function agent_set(name,opts) {
	return jconfig_set(name,opts,"agent",agents,agent_props,sc_agent_verify);
}

function agent_get_config(name) {
	return jconfig_get_config(name,"agent",agents,agent_props);
}

function agent_set_config(config) {
	return jconfig_set_config(config,"agent",agents,agent_props,load_agents);
}

function load_agents() {
	jconfig_load("agents",agent_props,_agent_init);
//	dumpobj(agents);
}

/*************************************************************************************************/

function agent_get_state(name) {
	return common_get_state(name,agents);
}

function agent_exec(name,info) {

	let dlevel = -1;

	dprintf(dlevel,"pid: %d\n", info.pid);
	if (info.pid < 0) {
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
		info.pid = sc.exec(info.path,args);
		if (info.pid < 0) {
			log_syserr("unable to exec: " + info.path);
			return 1;
		}
		info.started = time();
		info.status = AGENT_STATUS_STARTED;
	}
	return 0;
}

function agent_start(name) {
	let info = agents[name];
	if (info.status == AGENT_STATUS_STARTED) {
		config.errmsg = "error: agent already started";
		return 1;
	}
	agent_exec(name,info);

//	return common_start(name,"agent",agents,agent_on);
}

function agent_off(name,agent) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, state: %d\n", name, agent.state);
	let r = sdconfig(mqtt,name,"exit");
//	dprintf(dlevel,"r: %s\n", r);
	if (debug >= dlevel) dumpobj(r,"sdconfig result");
	if (r.status != 0) {
		error_set("agent",name,r.message);
		agent.enabled = false;
	}
	agent_set_state(name,FAN_STATE_STOPPED);
	return r.status;
}

function agent_cooldown(name,agent) {

        let dlevel = 1;

	// Turn the pump off first
	if (agent.pump.length) pump_stop(agent.pump);

        dprintf(dlevel,"name: %s, cooldown: %d\n", name, agent.cooldown);
        if (!agent.cooldown) return agent_off(name,agent);
        agent.time = time();
        agent_set_state(name,FAN_STATE_COOLDOWN);
        return 0;
}

function agent_stop(name,force) {
        return common_stop(name,"agent",agents,agent_cooldown,force)
}

function force_agent_stop(name) {
	if (agents[name].pump.length) pump_stop(agents[name].pump,true);
	return common_stop(name,"agent",agents,agent_off,true)
}

function agent_restart(name) {
	agent_stop(name,false);
	agent_start(name);
}

function agents_init() {

	let dlevel = 1;

	dprintf(dlevel,"*** AGENTS INIT ****\n");

	let s = 0;
	AGENT_STATUS_NONE = s++;
	AGENT_STATUS_STARTED = s++;
	AGENT_STATUS_STOPPED = s++;

	// Declare the per-agent props here as a global
	agent_props = [
		[ "autostart", DATA_TYPE_BOOL, "true", 0 ],
		[ "enabled", DATA_TYPE_BOOL, "true", 0 ],
		[ "managed", DATA_TYPE_BOOL, "true", 0 ],
		[ "monitor_topic", DATA_TYPE_STRING, "", 0 ],
		[ "monitor_interval", DATA_TYPE_INT, -1, 0 ],
		[ "role", DATA_TYPE_STRING, "", 0 ],
		[ "path", DATA_TYPE_STRING, "", 0 ],
		[ "conf", DATA_TYPE_STRING, "", 0 ],
		[ "log", DATA_TYPE_STRING, "", 0 ],
		[ "log_append", DATA_TYPE_BOOL, "false", 0 ],
		[ "monitor", DATA_TYPE_BOOL, "true", 0 ],
		[ "topic", DATA_TYPE_STRING, "", 0 ],
		[ "notify", DATA_TYPE_BOOL, "true", 0 ],
		[ "notify_path", DATA_TYPE_STRING, "", 0 ],
		[ "warning_time", DATA_TYPE_INT, "-1", 0 ],
		[ "error_time", DATA_TYPE_INT, "-1" , 0 ],
		[ "notify_time", DATA_TYPE_INT, "-1" , 0 ],
		[ "status", DATA_TYPE_INT, AGENT_STATUS_NONE.toString(), CONFIG_FLAG_PRIVATE ],
	];

	let funcs = [
		[ "add_agent", agent_add, 2 ],
		[ "del_agent", agent_del, 1 ],
		[ "get_agent", agent_get, 1 ],
		[ "set_agent", agent_set, 2 ],
		[ "get_agent_confgg", agent_get_config, 2 ],
		[ "set_agent_config", agent_set_config, 2 ],
		[ "start_agent", agent_start, 1 ],
		[ "stop_agent", agent_stop, 1 ],
		[ "restart_agent", agent_restart, 1 ],
	];

	config.add_funcs(sc,funcs);

	dprintf(dlevel,"done!\n");
	return 0;
}

function agent_statusstr(status) {

	let str;
	switch(status) {
	case AGENT_STATUS_NONE:
		str = "None";
		break;
	case AGENT_STATUS_STARTED:
		str = "Started";
		break;
	case AGENT_STATUS_STOPPED:
		str = "Stopped";
		break;
	default:
		str = sprintf("Unknown(%d)",status);
		break;
	}
	return str;
}
