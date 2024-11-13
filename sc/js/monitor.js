
function monitor_init() {
}

function monitor_main() {

	let dlevel = -1;

	mq = m.mq;
	printf("mq len: %d\n", mq.length);
	for(j=0; j < mq.length; j++) {
		msg = mq[j];
//		dumpobj(msg);
//		printf("func: %s\n", msg.func);
		if (msg.func == "Data") {
			printf("getting data for %s\n", msg.name);
//			agents[msg.name].data = JSON.parse(msg.data);
//			agents[msg.name].data.last_update = time();
		}
	}
//	dumpobj(mq[0]);
	m.purgemq();
	return;
}
