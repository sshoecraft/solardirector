
include(dirname(script_name)+"/../../core/utils.js");
include(dirname(script_name)+"/display.js");

var t = SOLARD_TOPIC_ROOT + "/" + SOLARD_TOPIC_AGENTS + "/#";
printf("t: %s\n", t);

var agents = {};
mqtt.sub(t);
while(1) {
//	dumpobj(agent.messages);
	printf("len: %d\n", agent.messages.length);
	var messages = agent.messages;
	for(i=0; i < messages.length; i++) {
		var msg = messages[i];
//		dumpobj(msg);
		if (msg.func == "Info") {
			var info = JSON.parse(msg.data);
			if (info.agent_role != "Battery") continue;
			printf("adding: %s\n", msg.name);
			agents[msg.name] = {};
			agents[msg.name].info = info;
		} else if (msg.func == "Data" && agents[msg.name]) {
			if (agents[msg.name].info.agent_role != "Battery") continue;
			printf("getting data for %s\n", msg.name);
			agents[msg.name].data = JSON.parse(msg.data);
			agents[msg.name].data.last_update = time();
		}
	}
	agent.purgemq();
	display(agents);
	sleep(1);
}
