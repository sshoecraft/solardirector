
function read_main() {

	let dlevel = -1;

//	dprintf(0,"*** IN READ ***\n");

	run(script_dir+"/monitor.js");

//	dprintf(-1,"purging...\n");
	let mq = agent.messages;
	dprintf(-1,"mq.length: %d\n", mq.length);
	for(let i=0; i < mq.length; i++) {
		msg = mq[i];
//		dumpobj(msg);
//		dprintf(-1,"func: %s\n", msg.func);
		if (msg.func == "Data") agent.delete_message(msg);
	}
//	report_mem();
}
