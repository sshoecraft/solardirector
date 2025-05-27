#!/opt/sd/bin/sdjs -U512K

function main(argv) {
	let paths = [ ".","..","../..","../../..",SOLARD_LIBDIR ]; for(let idx in paths) { let inc=paths[idx]+"/lib/sd/init.js"; if (new File(inc).exists) { include(inc); break; } }
	driver = new Driver("test");
	driver.init = function(agent) {
		printf("*** INIT ***\n");
		checkargs(arguments,{ agent: "number", a: "number" });
	}
        driver.get_info = function(agent) {
		printf("*** INFO ***\n");
		return "{}";
        }
	driver.read = function(agent,control,buflen) {
		printf("*** READ ***\n");
	}
	driver.write = function(agent,control,buflen) {
		printf("*** WRITE ***\n");
		report_mem();
	}
	driver.run = function(agent) {
		printf("*** RUN ***\n");
		agent.test++;
	}

	/* trigger is in same scope as ta so has access to it */
	function test_trigger(arg,prop,old_value) {
		dprintf(-1,"test value: %d\n", prop.value);
		if (!prop.value) ta.test = 1;
	}

	let props = [
		[ "interval", DATA_TYPE_INT, "7", CONFIG_FLAG_NOWARN ],
		[ "test", DATA_TYPE_INT, "-1", 0, test_trigger ],
	];

	let ta = new Agent(argv,driver,"1.0",0,props,0);

	ta.test = 3;

	function test_handler(name,module,action) {
        	dprintf(-1,"name: %s, module: %s, action: %s\n", name, module, action);
	}
	ta.event.handler(test_handler);
	ta.event.signal(ta.name,"Test","Started");
//	dumpobj(ta.messages);

	ta.run();
	printf("final value: %d\n", ta.test);
	ta.destroy();
}
