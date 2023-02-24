
function read_main() {
	include(dirname(script_name)+"/../../core/utils.js")
	include(dirname(script_name)+"/../../core/battery.js")

	// Need a copy in order to flatten
	var mydata = clone(data);

	// Select temp which is furthest from 25c
	var max_delta = 0.0;
	var max_temp = 0.0;
	for(i=0; i < data.ntemps; i++) {
		t = data.temps[i];
		var v = 0;
		if (t > 25) v = t - 25;
		else if (t < 25) v = 25 - t;
		if (v > max_delta) {
			max_delta = v;
			max_temp = t;
		}
	}
	dprintf(1,"max_delta: %.1f, max_temp: %.1f\n", max_delta, max_temp);
	// Convert to F
	//data.temp = max_temp * (9/5) + 32;
	mydata.temp = max_temp;

	var fields = clone(battery_fields);
	dprintf(1,"flatten: %s\n", jk.flatten);
	if (jk.flatten) {
		for(i=0; i < mydata.ntemps; i++) {
			f = sprintf("temp_%02d",i+1);
			mydata[f] = mydata.temps[i];
			fields.push(f);
		}
		delete mydata.ntemps;
		delete mydata.temps;
		for(i=0; i < mydata.ncells; i++) {
			f = sprintf("cell_%02d",i+1);
			mydata[f] = mydata.cellvolt[i];
			fields.push(f);
		}
		delete mydata.cellvolt;
		for(i=0; i < data.ncells; i++) {
			f = sprintf("cellres_%02d",i);
			mydata[f] = mydata.cellres[i];
			fields.push(f);
		}
		delete mydata.ncells;
		delete mydata.cellres;
	}
	mydata.power = mydata.voltage * mydata.current;

	function addstate(str,val,text) {
		if (val) {
			if (str.length) str += ",";
			str += text;
		}
		return str;
	}

	var state = "";
	var statebits = mydata.state;
	dprintf(1,"statebits: %x\n", statebits);
	state = addstate(state,statebits & JK_STATE_CHARGING,"Charging");
	state = addstate(state,statebits & JK_STATE_DISCHARGING,"Discharging");
	state = addstate(state,statebits & JK_STATE_BALANCING,"Balancing");
	dprintf(1,"state: %s\n", state);
	mydata.state = state;

//	dumpobj(mydata);

	j = JSON.stringify(mydata,fields,4);
	printf("j: %s\n", j);
	if (mqtt.pub(agent.mktopic(SOLARD_FUNC_DATA),j)) log_error("mqtt.pub: %s\n",mqtt.errmsg);
	if (influx.connected && influx.write("battery",mydata)) log_error("influx.write: %s\n",influx.errmsg);

	if (typeof(last_power) == "undefined") last_power = 0;
	if (mydata.power != last_power) log_info("%.1f\n",mydata.power);
	last_power = mydata.power;
}
