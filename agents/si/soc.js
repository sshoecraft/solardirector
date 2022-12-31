
function soc_init() {

//	var soc_props = [ ];

//	dprintf(1,"adding props...\n");
//	config.add_props(si, soc_props);

	si.remain_time = "";
	si.remain_text = "";
	si.kf_secs = new KalmanFilter();
}

function report_remain(func,secs) {
	var diff,hours,mins;

	var dlevel = 3;

	dprintf(dlevel,"secs: %f\n", secs);
	var d = new Date();
	d.setSeconds(d.getSeconds() + secs);
	t = d.toString();
	days = parseInt(secs / 86400);
	dprintf(dlevel,"days: %d\n", days);
	if (days > 0) {
		secs -= (days * 86400);
		dprintf(dlevel,"new secs: %d\n", secs);
	}
	hours = parseInt(secs / 3600);
	dprintf(dlevel,"hours: %d\n", hours);
	if (hours > 0) {
		secs -= (hours * 3600.0);
		dprintf(dlevel,"new secs: %d\n", secs);
	}
	mins = parseInt(secs / 60);
	dprintf(dlevel,"mins: %d\n", mins);
	if (mins) {
		secs -= parseInt(mins * 60);
		dprintf(dlevel,"new secs: %d\n", secs);
	}
	if (days > 1000) secs = (-1);
	if (secs >= 0) {
		var s = "";
		if (days > 0) s += sprintf("%d days", days);
		if (hours > 0) {
			if (s.length) s += ", ";
			if (hours == 1) s += "1 hour";
			else s += sprintf("%d hours", hours);
		}
		if (mins > 0) {
			if (s.length) s += ", ";
			if (mins == 1) s += "1 minute";
			else s += sprintf("%d minutes", mins);
		}
if (0) {
		if (s.length) s += ", ";
		if (secs == 1) s+= "1 second";
		else s += sprintf("%d seconds", secs);
}
	} else {
		s = "~";
		t = "~";
	}
	si.remain_time = t;
	si.remain_text = s;
	dprintf(dlevel,"%s time: %s (%s)\n", func, si.remain_text, si.remain_time);
}

function soc_main() {

	var dlevel = 1;

	dprintf(dlevel,"battery_capacity: %d\n", si.battery_capacity);
	if (!si.battery_capacity) {
		si.remain_text = "(battery_capacity not set)";
		return;
	}

//	new_soc = Sk - (n * Ts / As) * Ik
if (0) {
	top = (0.98 * si.interval);
	bot = (si.battery_capacity * 3600);
	i = Math.abs(data.battery_current);
	dprintf(0,"top: %f, bot: %f, i: %f\n", top, bot, i);
	sk = (top / bot) * i;
	dprintf(0,"sk: %f\n", sk);
}

	// Calculate the level using AH
	dprintf(dlevel,"battery_ah: %f\n", si.battery_ah);
	let new_soc = (si.battery_ah / si.battery_capacity) * 100.0;
	dprintf(dlevel,"new_soc: %.1f\n", new_soc);

	// Dont let level < 0 or > 100
	let obl = new_soc;
	if (new_soc < 0) new_soc = 0;
	if (new_soc > 100.0) new_soc = 100;
	if (new_soc != obl) dprintf(dlevel,"FIXED: new_soc: %.1f\n", new_soc);
	si.soc = new_soc;

	// Calculate remaining ah
	let remain,func;
	dprintf(dlevel,"charge_mode: %d\n", si.charge_mode);
	if (si.charge_mode) {
		dprintf(dlevel,"charge_end_ah: %f\n", si.charge_end_ah);
		remain = si.charge_end_ah - si.battery_ah;
		func = "Charge";
	} else {
		dprintf(dlevel,"charge_start_ah: %f\n", si.charge_start_ah);
		remain = si.battery_ah - si.charge_start_ah;
		func = "Battery";
	}
	dprintf(dlevel,"remain: %f\n", remain);

	// Calculate remaining seconds
	let amps = data.battery_current;
	dprintf(dlevel,"amps: %f\n", amps);
	if (si.charge_mode && amps > 0) amps = 0.00001;
	else if (amps < 0) amps *= (-1);
	dprintf(dlevel,"amps: %f\n", amps);
	let secs = (remain / amps) * 3600;
	dprintf(dlevel,"secs: %f\n", secs);
	if (isNaN(secs) || !isFinite(secs)) secs = -1;

	// If we're in CV mode and there's a time remaining
	if (si.charge_mode == CHARGE_MODE_CV && si.cv_time) {
		let diff = (si.cv_start_time + MINUTES(si.cv_time)) - time();
		dprintf(0,"diff: %f, secs: %f\n", diff, secs);
		if (diff < secs || secs < 0) secs = diff;
	}
	dprintf(dlevel,"secs: %f\n", secs);
	secs = si.kf_secs.filter(secs);
	dprintf(dlevel,"NEW secs: %f\n", secs);

	report_remain(func,secs);
}
