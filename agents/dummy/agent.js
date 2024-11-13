#!/opt/sd/bin/sdjs

include(dirname(script_name)+"/../../lib/sd/utils.js");

function set_astring() {
	printf("new astring: %s\n", dummy.astring);
}

function agent_main() {
	dummy = new Driver("dummy");
	dummy.init = function(tempagent) {
		dprintf(0,"*** INIT ***\n");
		dprintf(0,"tempagent: %s\n", tempagent);
	//	dumpobj(tempagent);
	}
	dummy.read = function(control,buflen) {
		dprintf(0,"read: control: %s, buflen: %s\n", control, buflen);
		dprintf(0,"len: %s\n", this.agents.length);
		dprintf(0,"new line!\n");
	}
	dummy.write = function(control,buflen) {
		dprintf(0,"write: control: %s, buflen: %s\n", control, buflen);
	}
	dummy.run = function() {
		dprintf(0,"*** RUN ***\n");
		return;
	}
	dummy.get_info = function() {
		dprintf(0,"*** GET_INFO ****\n");
		var info = {};
		info.driver_name = "dummy";
		info.driver_type = "Example";
		var j = JSON.stringify(info);
	//	printf("j: %s\n", j);
		return j;
	}

	var props = [
		[ "astring", DATA_TYPE_STRING, "adef", 0, set_astring ],
	];
	var funcs = [];

	agent = new Agent(argv,dummy,"1.0",props,funcs);
	config = agent.config;
	dummy.astring = "atest";
	config.save();
	agent.pubconfig();
	mqtt = agent.mqtt;
	influx = agent.influx;
	agent.run();
	printf("exiting\n");
}
