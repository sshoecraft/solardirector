#!+BINDIR+/sdjs

include(SOLARD_LIBDIR + "/sd/utils.js");

m = new MQTT("192.168.1.142");
m.sub(SOLARD_TOPIC_ROOT+"/"+SOLARD_TOPIC_AGENTS+"/btc/"+SOLARD_FUNC_DATA);
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
printf("mirring from: %s\n", m.uri);
while(true) {
//	dprintf(-1,"connected: %s\n", m.connected);
	if (!m.connected) m.reconnect();
	let mq = m.mq;
//	printf("len: %d\n", mq.length);
	for(let i=mq.length-1; i >= 0; i--) {
		if (mq[i].func == "Data") {
			let btc = JSON.parse(mq[i].data);
			for(let i=0; i < btc.temps.length; i++) {
				f = sprintf("temp_%02d", i);
				btc[f] = btc.temps[i];
			}
			delete btc.temps;
			for(let i=0; i < btc.cellvolt.length; i++) {
				f = sprintf("cell_%02d", i);
				btc[f] = btc.cellvolt[i];
			}
			delete btc.cellvolt;
			for(let i=0; i < btc.cellres.length; i++) {
				f = sprintf("res_%02d", i);
				btc[f] = btc.cellres[i];
			}
			delete btc.cellres;
//			dumpobj(btc);
//			printf("enabled: %s, connected: %s\n", influx.enabled, influx.connected);
			if (influx.enabled && influx.connected) influx.write("battery",btc);
			break;
		}
	}
	m.purgemq();
	sleep(5);
}
