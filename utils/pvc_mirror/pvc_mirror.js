#!+BINDIR+/sdjs

include(SOLARD_LIBDIR + "/sd/utils.js");

var m = new MQTT("192.168.1.142");
m.sub(SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/pvc/"+SOLARD_FUNC_DATA);
influx = new Influx("localhost","power");
influx.enabled = true;
influx.connect();
var fields = [
	"name",
	"input_voltage",
	"input_current",
	"input_power",
	"output_voltage",
	"output_frequency",
	"output_current",
	"output_power",
	"total_yield",
	"daily_yield"
];
while(true) {
	if (!m.connected) m.reconnect();
	let mq = m.mq;
//	printf("len: %d\n", mq.length);
	for(let i=mq.length-1; i >= 0; i--) {
		if (mq[i].func == "Data") {
			let pvc = JSON.parse(mq[i].data);
//			dumpobj(pvc);
//			printf("enabled: %s, connected: %s\n", influx.enabled, influx.connected);
			if (influx.enabled && influx.connected) influx.write("pvinverter",pvc);
			break;
		}
	}
	m.purgemq();
	sleep(5);
}
