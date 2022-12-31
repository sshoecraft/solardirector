
printf("name: %s\n", agent.name);

var format="%-20.20s %s %s\n";

var agents = sd.agents;

printf("len: %d\n", agents.length);
for(var i=0; i < agents.length; i++) {
	printf(format,agents[i].name,agents[i].role,agents[i].path);
}
