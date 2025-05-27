
function sc_event_handler(name,module,action) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, module: %s, action: %s\n", name, module, action);

	// handle repub events (re-init)
	if (module == "Agent" && action == "repub") {
	}
}

function start_main() {

        // Sub to the Info topic of all agents
	mqtt.sub(SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/+/"+SOLARD_FUNC_INFO);

	// Sub to the Data topic of all agents for monitoring
	mqtt.sub(SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/+/"+SOLARD_FUNC_DATA);

	// Dont purge unprocessed messages
	agent.purge = false;

	// handle events
	agent.event_handler(sc_event_handler,agent.name);

	// load agents
//	sc_load_agents();
//	agents_load();
}
