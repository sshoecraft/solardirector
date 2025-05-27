
function run_main() {

	let dlevel = -1;

	for(let name in agents) {
		let service = agents[name];
//		dumpobj(service);
		dprintf(dlevel,"%s: status: %s\n", name, agent_statusstr(service.status));
		switch(service.status) {
		case AGENT_STATUS_STOPPED:
			dprintf(dlevel,"autostart: %s\n", service.autostart);
			break;
		}
	}
}
