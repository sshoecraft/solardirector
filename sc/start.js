
function sc_event_handler(name,module,action) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, module: %s, action: %s\n", name, module, action);

	// handle repub events (re-init)
	if (module == "Agent" && action == "repub") init_main();
}

function start_main() {

	// Sub to all agents data topics for monitoring
	topic = sprintf("%s/%s/+/%s",SOLARD_TOPIC_ROOT,SOLARD_TOPIC_AGENTS,SOLARD_FUNC_DATA);
	dprintf(1,"==> subscribing to: %s\n", topic);
	mqtt.sub(topic);
	topic = sprintf("%s/%s/+/%s",SOLARD_TOPIC_ROOT,SOLARD_TOPIC_AGENTS,SOLARD_FUNC_INFO);
	dprintf(1,"==> subscribing to: %s\n", topic);
	mqtt.sub(topic);

	// Dont purge unprocessed messages
	agent.purge = false;

	// handle events
	agent.event_handler(sc_event_handler,agent.name);

	// load agents
	sc_load_agents();
}
