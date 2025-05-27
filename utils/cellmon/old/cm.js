#!/opt/sd/bin/sdjs

include("../../lib/sd/utils.js");
include("./display.js");

m = new MQTT("192.168.1.142");
//printf("m: %s\n", m);
m.sub(SOLARD_TOPIC_ROOT + "/" + SOLARD_TOPIC_AGENTS + "/#");
//dumpobj(m);

//topic:                         SolarD/Agents/pack_01/Info
//name:                          pack_01
//func:                          Info
//data:                          {
//    "agent_name": "jbd",
//    "agent_role": "Battery",

var batteries = [];
var bidx=0;
var agents = {};
while(1) {
for(let i=0; i < m.mq.length; i++) {
	msg = m.mq[i];
//	printf("msg[%d]: %s\n", i, msg);
//	dumpobj(msg);
	let func = basename(msg.topic);
//	printf("func: %s\n", func);
	if (msg.func == "Info") {
		let info = JSON.parse(msg.data);
		if (info.agent_role != "Battery") continue;
//		printf("adding: %s\n", msg.name);
		agents[msg.name] = {};
		agents[msg.name].info = info;
	} else if (msg.func == "Data" && agents[msg.name]) {
		if (agents[msg.name].info.agent_role != "Battery") continue;
//		printf("getting data for %s\n", msg.name);
		agents[msg.name].data = JSON.parse(msg.data);
		agents[msg.name].data.last_update = time();
	}
}
	m.purgemq();
	display(agents);
	sleep(1);
}
