#!+BINDIR+/sdjs

function sdconfig_main(argv) {
	script_dir = dirname(script_name);
	lib_dir = (script_dir == "." ? "../../lib" : SOLARD_LIBDIR);
	sdlib_dir = lib_dir + "/sd";
	include(sdlib_dir+"/init.js");
	include(sdlib_dir+"/getopt.js");

	let opts = [
		[ "-L|list agent variables", "list_vars", DATA_TYPE_BOOL, "false" ],
		[ "-F|list agent functions", "list_funcs", DATA_TYPE_BOOL, "false" ],
		[ ":name|agent name", "agent_name", DATA_TYPE_STRING, "" ],
	];

	let client = new Client(argv,"sdconfig","2.0",opts);
	if (typeof(client.opts) == "undefined") return 1;
//	dumpobj(client.opts);

	let agent_name = client.opts.agent_name;
	if (!agent_name.length) {
		log_error("agent name is required\n");
		return 1;
	}
	let optind = client.opts.optind-1;

	if (client.opts.list_vars) {
		r = sdconfig(client,agent_name,"***VARS***");
		if (r.status) {
			printf("%s\n", r.message);
			exit(1);
		}
		let vars = r.results.variables;
		for(let key in vars) {
			dprintf(2,"key: %s\n", key);
			let ci = vars[key];
			printf("%-30.30s %-10.10s %-10s %s\n", "Name", "Type", "Size", "Default");
			dprintf(2,"len: %s\n", ci.length);
			for(let i = 0; i < ci.length; i++) {
				let obj = ci[i];
				if (typeof(obj.size) == "undefined") obj.size = "";
				if (typeof(obj.default) == "undefined") obj.default = "";
				printf("%-30.30s %-10.10s %-10s %s\n", obj.name, obj.type.replace(/^DATA_TYPE_/, ""), obj.size, obj.default);
			}
			break;
		}
	} else if (client.opts.list_funcs) {
		r = sdconfig(client,agent_name,"***FUNCS***");
		if (r.status) {
			printf("%s\n", r.message);
			exit(1);
		}
		printf("%s functions:\n", agent_name);
		printf("%-30.30s %s\n", "Name", "# args");
		for(let i=0; i < r.results.functions.length; i++) {
			let func = r.results.functions[i];
			printf("%-30.30s %d\n", func.name, func.nargs);
		}
	} else {
		let cmd = "r = sdconfig(client,agent_name";
		dprintf(2,"optind: %d, argv.length: %d\n", optind, argv.length);
		if (optind >= argv.length) {
			log_error("function name is required when not using -L or -F\n");
			return 1;
		}
		for(let i=optind; i < argv.length; i++) {
			cmd += ",\"" + argv[i] + "\"";
		}
		cmd += ");"
		dprintf(2,"cmd: %s\n", cmd);
		eval(cmd);
	//	new Function(cmd)();
		if (r.status) {
			printf("%s\n", r.message);
			exit(1);
		}
		dumpobj(r.results);
	}
}
