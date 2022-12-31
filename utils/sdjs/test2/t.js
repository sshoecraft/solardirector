
function t_main() {
	include(dirname(script_name)+"/../../core/utils.js");
	printf("agent: %s\n",agent);
	dumpobj(agent.mqtt);
}
