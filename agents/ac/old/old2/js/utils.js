
function can_get_sensor(id,offset,wait) {

	let dlevel = 4;

	if (typeof(wait) == "undefined") wait = false;
	dprintf(dlevel,"id: %x, offset: %d, wait: %s\n", id, offset, wait);
	// each sensor is 2 bytes, so valid offset is 0-3
	let action = sprintf("%04x:%02d",id,offset);
	if (offset < 0 || offset > 3) {
		error_set("can_get_sensor",action,sprintf("CAN read id %x: invalid offset: %d", id, offset));
		return;
	}
	let a = ac.can_read(id,wait);
//	dumpobj(a);
	if (!a) {
		error_set("can_get_sensor",action,sprintf("CAN read id %x offset %d failed", id, offset));
		return;
	}
	dprintf(dlevel,"offset: %d\n", offset);
	let i = parseInt(offset)*2;
	dprintf(dlevel,"i: %d\n", i);
	let v = a[i] << 8 | a[i+1];
	dprintf(dlevel,"v: %x\n", v);
	let f = parseFloat(v / 10);
	dprintf(dlevel,"f: %f\n", f);
	return f;
}

function get_sensor(spec,wait) {

	let dlevel = 4;

	if (typeof(wait) == "undefined") wait = false;
	dprintf(dlevel,"spec: %s, wait: %s\n", spec, wait);
	if (!spec) {
		// XXX dont set an error, just return (cuts down on ifs)
//		error_set("utils","get_sensor","internal error: get_sensor: spec is undefined!");
		return;
	}
	if (!spec.length) {
		error_set("utils","get_sensor","internal error: get_sensor: spec is empty");
		return;
	}
	let a = spec.split(",");
	let driver = a[0];
	let target = a[1];
	let offset = a[2];
	dprintf(dlevel,"driver: %s, target: %s, offset: %s\n", driver, target, offset);

	let r;
	switch(driver) {
	case "can":
		r = can_get_sensor(target, offset, wait);
		break;
	case "func":
	case "function":
		dprintf(dlevel,"target type: %s\n", typeof(window[target]));
		if (typeof(window[target]) != "function") {
			error_set("utils","get_sensor",sprintf("get_sensor: func driver target '%s' is not a function", target));
			return;
		}
		r = window[target](offset);
		break;
	default:
		error_set("utils","get_sensor",sprintf("get_sensor: driver %s is not supported", driver));
		return;
	}
	return r;
}

function set_pin(pin,state) {

	let dlevel = -1;

	dprintf(dlevel,"pin: %d, state: %d\n", pin, state);
	if (pin < 0) return 0;
if (0) {
	if (pin < 0) {
		error_set("utils","set_pin",sprintf("set_pin: unable to set pin %d: invalid pin number", pin));
		return 1;
	}
}
	let pin_state = digitalRead(pin);
	dprintf(dlevel,"pin_state: %d\n", pin_state);
	if (pin_state == state) return 0;
	digitalWrite(pin,state);
	delay(500);
	pin_state = digitalRead(pin);
	dprintf(dlevel,"pin_state: %d\n", pin_state);
	if (pin_state != state) {
		error_set("utils","set_pin",sprintf("set_pin: unable to set pin %d to %d from %d\n", pin, state, pin_state));
		return 1;
	}
	return 0;
}

function compare_temps(a,b) {
	dprintf(dlevel,"a: %f, b: %f\n", a, b);
	if (a == INVALID_TEMP || b == INVALID_TEMP) return -1;
	let r = 100.0 - ((a > b ? Math.abs(b / a) : Math.abs(a / b)) * 100.0);
	dprintf(dlevel,"r: %f\n", r);
	return r;
}

// get the temp from the prod ac influx instance (for testing/sim'ing)
function get_influx(info) {

	let dlevel = 4;

	if (typeof(info) == 'undefined') info = "";
	let arr = info.split("|");
	let hn = arr[0];
	let db = arr[1];
	let fn = arr[2];
	if (hn == 'undefined') hn = "http://localhost:8086";
	if (db == 'undefined') db = "hvac";
	if (fn == 'undefined') {
		error_set("utils","get_influx: field name must be specified (hostname:dbname:fieldname)\n");
		return 1;
	}
	dprintf(dlevel,"hn: %s\n", hn);
	let idx = hn.indexOf("://");
	dprintf(dlevel,"idx: %d\n", idx);
	if (idx >= 0) {
		hn = hn.substr(idx+3);
		dprintf(dlevel,"NEW hn: %s\n", hn);
	}
	idx = hn.indexOf(":");
	dprintf(dlevel,"idx: %d\n", idx);
	if (idx >= 0) {
		hn = hn.substr(0,idx);
		dprintf(dlevel,"NEW hn: %s\n", hn);
	}

	vname = "influx_" + hn + "_" + db;
	dprintf(dlevel,"vname: %s\n", vname);
	let i = ac[vname];
	dprintf(dlevel,"i: %s\n", i);
	if (typeof(i) == "undefined") {
		i = ac[vname] = new Influx(hn,db);
		i.enabled = true;
		i.connect();
	}
	let query = sprintf("SELECT last(%s) FROM ac", fn);
	dprintf(dlevel,"query: %s\n", query);
	let results = influx2arr(i.query(query));
	dprintf(dlevel,"results: %s\n", results);
	let temp = undefined;
	if (results) temp = results[0].last;
	dprintf(dlevel,"temp: %s\n", temp);
	return temp;
}
