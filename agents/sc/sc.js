#!/opt/sd/bin/sdjs

function main(argv) {

	let dlevel = -1;

	let my_name = "sc";
	let my_version = "1.0";

	let paths = [ ".","..","../..","../../..",SOLARD_LIBDIR ]; for(let idx in paths) { let inc=paths[idx]+"/lib/sd/init.js"; if (new File(inc).exists) { include(inc); break; } }
	driver = new Driver(my_name);
	driver.init = function(agent) {
		dprintf(dlevel,"*** INIT ***\n");

		agent.config.read("test.json");

		// Call init funcs
		let init_funcs = [
			"temp",
			"mode",
			"snap",
			"errors",
			"fan",
			"pump",
			"unit",
			"cycle",
			"sample",
			"charge",
		];

/*
		// make the std objects global
		config = agent.config;
		mqtt = agent.mqtt;
		influx = agent.influx;
		event = agent.event;
*/

		if (common_init(agent,init_funcs)) abort(1);
	}
        driver.get_info = function(agent) {
//		dprintf(dlevel,"*** INFO ****\n");
		var info = {};
		info.agent_name = my_name;
		info.agent_role = "Controller";
		info.agent_description = "Service Controller";
		info.agent_version = my_version;
    		info.agent_author = "Stephen P. Shoecraft";
		return JSON.stringify(info);
	}
	driver.read = function(agent,control,buflen) {
		printf("*** READ ***\n");
	}
	driver.write = function(agent,control,buflen) {
		printf("*** WRITE ***\n");
		report_mem();
	}
//	driver.run = function(agent) {
//		printf("*** RUN ***\n");
//		agent.test++;
//	}

	let props = [
		[ "interval", DATA_TYPE_INT, "15", CONFIG_FLAG_NOWARN ],
	];

	let sc = new Agent(argv,driver,my_version,0,props,0);
	printf("name: %s\n", sc.name);
	printf("interval: %d\n", sc.interval);

	sc.run();
	sc.destroy();
}
