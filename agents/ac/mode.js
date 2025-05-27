
function mode_location_event_handler(name,module,action) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, my name: %s, module: %s, action: %s\n", name, ac.name, module, action);
	if (name != ac.name) return;

	if (action == "Set") {
		dprintf(dlevel,"location: %s\n", ac.location);
		if (ac.day_start == DEFAULT_DAY_START) ac.day_start = "sunrise+30";
		if (ac.night_start == DEFAULT_NIGHT_START) ac.night_start = "sunset+120";
	} else if (action == "Clear") {
		if (ac.day_start == DEFAULT_DAY_LSTART) ac.day_start = DEFAULT_DAY_START;
		if (ac.night_start == DEFAULT_NIGHT_LSTART) ac.night_start = DEFAULT_NIGHT_START;
	}
	dprintf(dlevel,"day_start: %s, night_start: %s\n", ac.day_start, ac.night_start);
}

// use outside temp with a (100-day_weight)% night / (day_weight)% day weighted average over 3 days
// if below heat_mode_average, set heat mode, else cool
function get_wt() {

	let dlevel = 1;

	// D: 3 days ago at day_start to 3 days ago at night_start
	// N: 3 days ago at night_start to 2 days ago at day_start
	// D: 2 days ago at day_start to 2 days ago at night_start
	// N: 2 days ago at night_start to 1 day ago at day_start
	// D: 1 day ago at day_start to 1 day ago at night_start
	// N: 1 day ago at night_start to today at day_start

	let night_weight = 100 - ac.day_weight;
	dprintf(dlevel,"day_weight: %d, night_weight: %d\n", ac.day_weight, night_weight);

	function get_timespec(loc,spec,label,date) {
		// location: lat,lon
		// timespec format: [sunrise|sunset|0-23:0-59][[+|-]# of seconds]
		// what: timespec name
		// next: if date < current, use next day
		// date: Date to use
		dprintf(dlevel,"loc: %s, spec: %s, label: %s, date: %s\n", loc, spec, label, date);
		let dt = get_date(loc,spec,label,false,date);
		dprintf(dlevel,"%s: %s\n", label, dt);
		if (!dt) return undefined;
		let str = sprintf("%04d-%02d-%02d %02d:%02d:00",dt.getYear()+1900,dt.getMonth()+1,dt.getDate(),dt.getHours(),dt.getMinutes())
		dprintf(dlevel,"str: %s\n", str);
		return str;
	}

	let dividend = 0;
	let divisor = 0;
	let tz = SOLARD_TZNAME.length ? " tz('"+SOLARD_TZNAME+"')" : "";

	for(let k = 3; k > 0; k--) {

		// Get the date k days ago and k-1 days ago
		let day = new Date();
		day.setDate(day.getDate() - k);
		dprintf(dlevel,"day: %s\n", day);
		let day2 = new Date();
		day2.setDate(day2.getDate() - (k-1));
		dprintf(dlevel,"day2: %s\n", day2);

		// Day
		let p1 = get_timespec(ac.location,ac.day_start,"day_start",day);
		dprintf(dlevel,"p1: %s\n", p1);
		if (!p1) {
			log_error("get_wt: unable to get timespec for day_start %d days ago\n", k);
			return INVALID_TEMP;
		}
		let p2 = get_timespec(ac.location,ac.night_start,"night_start",day);
		dprintf(dlevel,"p2: %s\n", p2);
		if (!p2) {
			log_error("get_wt: unable to get timespec for night_start %d days ago\n", k);
			return INVALID_TEMP;
		}
		let q1 = sprintf("select mean(f) from outside_temp where time >= '%s' and time < '%s'%s", p1, p2, tz);
		dprintf(dlevel,"q1: %s\n", q1);
		let r1 = influx2arr(influx.query(q1));
		dprintf(dlevel,"r1: %s\n", r1);
	//	dumpobj(r1);
		if (r1) {
			let v1 = r1[0].mean;
			dprintf(dlevel,"v1: %f\n", v1);
			dividend += (v1 * ac.day_weight);
			divisor += ac.day_weight;
		}

		// Night
		let p3 = get_timespec(ac.location,ac.day_start,"day_start2",day2);
		dprintf(dlevel,"p3: %s\n", p3);
		if (!p3) {
			log_error("get_wt: unable to get timespec for day_start %d days ago\n", k-1);
			return INVALID_TEMP;
		}
		let q2 = sprintf("select mean(f) from outside_temp where time >= '%s' and time < '%s'%s", p2, p3, tz);
		dprintf(dlevel,"q2: %s\n", q2);
		let r2 = influx2arr(influx.query(q2));
		dprintf(dlevel,"r2: %s\n", r2);
	//	dumpobj(r2);
		if (r2) {
			let v2 = r2[0].mean;
			dprintf(dlevel,"v2: %f\n", v2);
			dividend += (v2 * night_weight);
			divisor += night_weight;
		}
	}

	dprintf(dlevel,"dividend: %f, divisor: %f\n", dividend, divisor);
	let wt = dividend/divisor;
	dprintf(dlevel,"wt: %f\n", wt);
	if (typeof(wt) == "undefined") wt = INVALID_TEMP;
	return wt;
}

function mode_auto() {

	let dlevel = -1;

	// Try to determine what the mode should be for the storage
	let new_mode;
	// search the air handlers and see how many times each mode was requested over the time period
	let wt = get_wt();
	dprintf(dlevel,"wt: %s\n", wt);
	if (wt != INVALID_TEMP) {
		dprintf(dlevel,"threshold: %d\n", ac.mode_threshold);
		if (wt <= ac.mode_threshold) new_mode = AC_MODE_HEAT;
		else new_mode = AC_MODE_COOL;
	} else {
		new_mode = AC_MODE_COOL;
	}
	ac.mode = new_mode;
	dprintf(dlevel,"NEW mode: %s(%d)\n", ac_modestr(ac.mode), ac.mode);
}

function mode_set(mode_str) {
	dprintf(-1,"mode_str: %s\n", mode_str.toLowerCase());
	switch(mode_str.toLowerCase()) {
	case "auto":
		mode_auto();
		break;
	case "cool":
		ac.mode = AC_MODE_COOL;
		break;
	case "heat":
		ac.mode = AC_MODE_HEAT;
		break;
	default:
		config.errmsg = sprintf("set_mode: unknown mode: %s\n", mode_str);
		return 1;
	}
	config.save();
	return 0;
}

function mode_set_trigger(arg,prop,old_val) {
	dprintf(1,"new val: %s, old val: %s\n", prop.value, old_val);
	if (typeof old_val != 'undefined' && prop.value != old_val) ac.signal("Mode","Set");
}

function mode_init() {

	let s = 0;
	AC_MODE_NONE = s++;
	AC_MODE_COOL = s++;
	AC_MODE_HEAT = s++;

	DEFAULT_DAY_START = "06:00";
	DEFAULT_DAY_LSTART = "sunrise+30";
	DEFAULT_NIGHT_START = "20:00";
	DEFAULT_NIGHT_LSTART = "sunset+120";

	let props = [
		[ "mode", DATA_TYPE_INT, AC_MODE_COOL, 0, mode_set_trigger ],
		[ "mode_threshold", DATA_TYPE_INT, 60, 0 ],
		[ "night_start", DATA_TYPE_STRING, DEFAULT_NIGHT_START, 0 ],
		[ "day_start", DATA_TYPE_STRING, DEFAULT_DAY_START, 0 ],
		[ "day_weight", DATA_TYPE_INT, 40, 0 ],
	];

	let funcs = [
		[ "set_mode", mode_set, 1 ],
	];

	config.add_props(ac,props,ac.driver_name);
	config.add_funcs(ac,funcs,ac.driver_name);
	event.handler(mode_location_event_handler,ac.name,"Location","all");
}

function ac_modestr(mode) {
	let dlevel = 4;

	dprintf(dlevel,"mode: %d\n", mode);
	switch(mode) {
	case AC_MODE_NONE:
		str = "None";
		break;
	case AC_MODE_COOL:
		str = "Cool";
		break;
	case AC_MODE_HEAT:
		str = "Heat";
		break;
	default:
		str = sprintf("Unknown(%d)",mode);
		break;
	}

	dprintf(dlevel,"returning: %s\n", str);
	return str;
}
