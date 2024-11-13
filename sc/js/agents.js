
var dlevel = -1;

function sc_add_agent(name,opts) {
	dprintf(-1,"name: %s, opts: %s\n", name, opts);
	let newagent = new Class(name);
	newagent.config = config;
	config.add_props(newagent,agent_props);
	let d = JSON.parse(opts);
	sc.agents += sprintf("%s%s",(sc.agents.length ? "," : ""),name);
	dprintf(-1,"NEW agents: %s\n", sc.agents);
	config.save();
	return 0;
}

function sc_del_agent(name) {
	dprintf(-1,"name: %s\n", name);
	let names = sc.agents.split(",");
	dprintf(0,"names length: %d\n", names.length);
	let name;
	agents = [];
	for(let i=0; i < names.length; i++) {
	}
	return 0;
}

function sc_agent_enable(name) {
	dprintf(-1,"name: %s\n", name);
}

function agents_init() {

	dprintf(dlevel,"*** AGENTS INIT ****\n");

	agent_props = [
		[ "enabled", DATA_TYPE_BOOL, "true", 0 ],
		[ "managed", DATA_TYPE_BOOL, "true", 0 ],
		[ "name", DATA_TYPE_STRING, null, 0 ],
		[ "path", DATA_TYPE_STRING, null, 0 ],
		[ "conf", DATA_TYPE_STRING, null, 0 ],
		[ "log", DATA_TYPE_STRING, null, 0 ],
		[ "log_append", DATA_TYPE_BOOL, "true", 0 ],
		[ "monitor", DATA_TYPE_BOOL, "true", 0 ],
		[ "topic", DATA_TYPE_STRING, null, 0 ],
		[ "notify", DATA_TYPE_BOOL, "true", 0 ],
		[ "notify_path", DATA_TYPE_STRING, null, 0 ],
		[ "warning_time", DATA_TYPE_INT, null, 0 ],
		[ "error_time", DATA_TYPE_INT, null, 0 ],
		[ "notify_time", DATA_TYPE_INT, null, 0 ]
	];

	var agent_funcs = [
		[ "enable", sc_agent_enable, 1 ]
	];

	let sc_agent_props = [
		[ "agents", DATA_TYPE_STRING, null, 0 ],
	];
	let sc_agent_funcs = [
		[ "add", sc_add_agent, 2 ],
		[ "del", sc_del_agent, 1 ]
	];
	config.add_props(sc,sc_agent_props);
	dprintf(-1,"sc.agents type: %s\n", typeof(sc.agents));
	config.add_funcs(sc,sc_agent_funcs);
}

function sc_load_agents() {
	let names = sc.agents.split(",");
	dprintf(dlevel,"names length: %d\n", names.length);
	let name;
	agents = [];
	for(let i=0; i < names.length; i++) {
		name = names[i];
		dprintf(dlevel,"name: %s\n", name);
		agents[name] = new Class(name);
		agents[name].config = config;
		config.add_props(agents[name],agent_props);
	}
	agents["one"].conf = "/one.cfg";
	agents["two"].conf = "/two.json";
	config.write();
}

function agents_main() {
	if (typeof(sc) == "undefined") sc = new Driver("sc");
	dprintf(-1,"calling init...\n");
	agents_init();
}
