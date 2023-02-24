#!/opt/sd/bin/sdjs

var script_dir=dirname(script_name)+"/";

include(script_dir+"../core/utils.js");

function set_astring() {
	printf("new astring: %s\n", sc.astring);
}

function agent_main() {
	sc = new Driver("sc");
	sc.init = function(tempagent) {
		dprintf(0,"*** INIT ***\n");
		dprintf(0,"tempagent: %s\n", tempagent);
	//	dumpobj(tempagent);
	}
	sc.get_info = function() {
		dprintf(0,"*** GET_INFO ****\n");
		var info = {};
		info.agent_name = "sc";
		info.agent_class = "Management";
		info.agent_role = "Controller";
		info.agent_description = "Site Controller";
		info.agent_version = "1.0";
		info.agent_author = "Stephen P. Shoecraft"
		var j = JSON.stringify(info);
	//	printf("j: %s\n", j);
		return j;
	}

	var props = [
		[ "astring", DATA_TYPE_STRING, "adef", 0, set_astring ],
	];
	var funcs = [];

	agent = new Agent(argv,sc,"1.0",props,funcs);
	config = agent.config;
	sc.astring = "atest";
	config.save();
	agent.pubconfig();
	mqtt = agent.mqtt;
	influx = agent.influx;
	agent.run();
	printf("exiting\n");
}
