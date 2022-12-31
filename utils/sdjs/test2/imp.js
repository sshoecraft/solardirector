function imp_main() {
include("../../core/utils.js");
var ifrom = new INFLUX("http://serv:8086","power");
//ifrom.convert_time = true;
//var ito = new INFLUX("http://localhost:8086","power");
var vals = ifrom.query("select * from pvinverter");
dumpobj(vals);
printf("count: %d\n", vals.length);
for(i=0; i < vals.length; i++) {
	printf("vals[%d]: %s\n", i, vals[i]);
	dumpobj(vals[i]);
	printf("length: %d\n", vals[i].length);
	for(j=0; j < vals[i].values.length; j++) {
		printf("values[%d]: %s\n", j, vals[i].values[j]);
	}
}
}
