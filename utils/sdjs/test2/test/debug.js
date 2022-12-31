
function myfunc() {
	dprintf(1,"<== func name\n");
}

agent.debug = 0;
dprintf(1,"test\n");
myfunc();
agent.debug = 1;
dprintf(1,"test\n");
myfunc();
