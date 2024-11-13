#!/opt/sd/bin/sdjs

/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

function sc_main() {

	let dlevel = -1;

	dprintf(dlevel,"==> making driver\n");
	sc = new Driver("sc");
	dprintf(dlevel,"==> sc: %s\n", sc);
	dprintf(dlevel,"==> adding funcs...\n");
	sc.init = function(tempagent) {
		dprintf(0,"*** INIT ***\n");
//		dprintf(0,"tempagent: %s\n", tempagent);
		agent = tempagent;
		config = agent.config;
		mqtt = agent.mqtt;
		influx = agent.influx;
	}
	sc.get_info = function() {
//		dprintf(0,"*** GET_INFO ****\n");
		if (!sc.json_info) {
			let info = {};
			info.agent_name = "sc";
			info.agent_class = "Management";
			info.agent_role = "Controller";
			info.agent_description = "Site Controller";
			info.agent_version = "1.0";
			info.agent_author = "Stephen P. Shoecraft"
			sc.json_info = JSON.stringify(info);
//			dprintf(dlevel,"json_info: %s\n", sc.json_info);
		}
		return sc.json_info;
	}
	sc.read = function(control,buflen) {
		dprintf(0,"read: control: %s, buflen: %s\n", control, buflen);
		run(script_dir+"/monitor.js");
	}
	sc.write = function(control,buflen) {
		dprintf(0,"write: control: %s, buflen: %s\n", control, buflen);
	}
//	sc.run = function() {
//		dprintf(0,"*** RUN ***\n");
//		return;
//	}
//	dumpobj(driver);

	dprintf(dlevel,"==> creating agent...\n");
	argv.push("-c");
	argv.push("sctest.json");
        let sc_agent_props = [
                [ "agents", DATA_TYPE_STRING, null, 0 ],
        ];

	agent = new Agent(argv,sc,"1.0",sc_agent_props,null);
//	sc.config.read(sc.config.filename);
	dprintf(-1,"sc.agents type: %s\n", typeof(sc.agents));
	dprintf(-1,"sc.agents: %s\n", sc.agents);
	config = agent.config;
	mqtt = agent.mqtt;
	influx = agent.influx;

	sc_load_agents();

	// Sub to the topic for all agents
	m = new MQTT(mqtt.uri);
	m.disconnect();
	m.username = mqtt.username;
	m.password = mqtt.password;
	m.connect();
	m.enabled = true;
	topic = sprintf("%s/%s/+/#",SOLARD_TOPIC_ROOT,SOLARD_TOPIC_AGENTS);
	dprintf(dlevel,"==> subscribing to: %s\n", topic);
	m.sub(topic);

	dprintf(dlevel,"==> running...\n");
	agent.run();
	printf("exiting\n");
}
