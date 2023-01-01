#!/opt/sd/bin/sdjs

include(dirname(script_name)+"/../../core/utils.js");

function agent_main() {
	var dummy = new Driver("dummy");
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
	dummy.info = function() {
		dprintf(0,"info...\n");
	}
	dummy.config = function() {
		dprintf(0,"config...\n");
	}
	dummy.run = function() {
//		dprintf(0,"*** RUN ***\n");
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
		[ "boolval", DATA_TYPE_BOOL, "false", 0 ],
	];
	var funcs = [];

	agent = new Agent(argv,dummy,"1.0",props,funcs);
	config = agent.config;
	mqtt = agent.mqtt;
	influx = agent.influx;
	agent.run();
	printf("exiting\n");
}
