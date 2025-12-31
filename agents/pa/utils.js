
function pa_get_number(data,name,invert) {
	let dlevel = 2;

	dprintf(dlevel,"name: %s, invert: %s\n", name, invert);
	let pfield = name + "_property";
	dprintf(dlevel,"pfield: %s\n", pfield);
	let pname = pa[pfield];
	dprintf(dlevel,"pname: %s\n", pname);
	let val = 0.0;
	let vname = "have_" + name;
	let found = false;
	dprintf(dlevel,"data[%s]: %s\n", pname, data[pname]);
	if (pname.length && typeof(data[pname]) != "undefined") {
		val = parseFloat(data[pname]);
		dprintf(dlevel,"val: %s\n", val);
		if (invert) {
			val *= -1;
			dprintf(dlevel,"NEW val: %s\n", val);
		}
		found = true;
	}
	pa[name] = val;
	pa["have_" + name] = found;
	// Update timestamp when data is received
	if (found) {
		pa[name + "_time"] = time();
	}
	dprintf(dlevel,"pa.%s: %s\n", name, val);
}

function pa_get_inverter_data(data) {
	pa_get_number(data,"grid_power",pa.invert_grid_power);
	pa_get_number(data,"charge_mode",false);
	pa_get_number(data,"load_power",pa.invert_load_power);
	pa_get_number(data,"frequency",false);
}

function pa_get_pv_data(data) {
	pa_get_number(data,"pv_power",pa.invert_pv_power);
}

function pa_get_battery_data(data) {
	pa_get_number(data,"battery_power",pa.invert_battery_power);
	pa_get_number(data,"battery_level",false);
}

function pa_process_messages(n,m,t,f) {

	let dlevel = 1;

    dprintf(dlevel,"n: %s,m : %s\n", n, m);
	dprintf(dlevel,"m.connected: %s\n", m.connected);
	if (!m.connected) m.reconnect();
	let mq = m.mq;
	dprintf(dlevel,"mq.len: %d\n", mq.length);
	for(let i=mq.length-1; i >= 0; i--) {
		let msg = mq[i];
		let data = JSON.parse(msg.data);
		if (msg.topic == t) f(data);
	}
	m.purgemq();
}

function pa_revoke(revoke_item) {
	let dlevel = 1;

	// Handle both old format (just res) and new format ({ res, immediate })
	let res = revoke_item.res || revoke_item;
	let immediate = revoke_item.immediate || false;

	log_info("revoking reservation for: id: %s, amount: %.1f, immediate: %s\n", res.id, res.amount, immediate);
	let arr = res.id.split(PA_ID_DELIMITER);
	let agent_name = arr[0];
	let module = arr[1];
	let item = arr[2];
	if (typeof(pa.mqtt) == 'undefined') pa.mqtt = mqtt;
	let r = sdconfig(pa,agent_name,"revoke",module,item,res.amount,immediate);
	if (r.status) dprintf(dlevel,"revoke failed: %s\n", r.message);
	return r.status;
}

function pa_parse_time_string(timestr) {
	// Simple parser for HH:MM format when location is not available
	let match = timestr.match(/^(\d{1,2}):(\d{2})$/);
	if (match) {
		let now = new Date();
		let result = new Date(now);
		result.setHours(parseInt(match[1]), parseInt(match[2]), 0, 0);
		return result;
	}
	return null;
}

function checkIfNight(night_start, day_start) {

	let dlevel = 1;

	dprintf(dlevel,"night_start: %s\n", night_start);
	dprintf(dlevel,"day_start: %s\n", day_start);

  // Both night_start and day_start are strings like "22:00" or "06:30"
  const now = new Date();

  // Helper to convert "HH:MM" to a Date object for today
  function timeStringToDate(timeStr) {
    const [hours, minutes] = timeStr.split(":").map(Number);
    const d = new Date(now);
    d.setHours(hours, minutes, 0, 0);
    return d;
  }

  const nightStart = timeStringToDate(night_start.toString());
  const dayStart = timeStringToDate(day_start.toString());

  let is_night;

  if (nightStart < dayStart) {
    // Night is within the same day (e.g., 20:00 to 06:00 — invalid here)
    is_night = now >= nightStart && now < dayStart;
  } else {
    // Night spans over midnight (e.g., 22:00 to 06:00)
    is_night = now >= nightStart || now < dayStart;
  }

  return is_night;
}

function pad(n) {
  return n < 10 ? '0' + n : '' + n;
}

function formatTimeHHMM(date) {
  const hours = pad(date.getHours());
  const minutes = pad(date.getMinutes());
  return hours + ':' + minutes;
}

function pa_is_night_period() {
	let dlevel = 1;
	
	if (!pa.night_budget) return false;

//	let now = new Date();
	
	// Always try get_date first - it should work for HH:MM format regardless of location
	// Location is only required for sunrise/sunset calculations
	let night_date = get_date(pa.location, pa.night_start, "night_start", false);
	let day_date = get_date(pa.location, pa.day_start, "day_start", false);
	
	// Fallback to simple parsing only if get_date failed
	if (!night_date) night_date = pa_parse_time_string(pa.night_start);
	if (!day_date) day_date = pa_parse_time_string(pa.day_start);
	
	let is_night = checkIfNight(formatTimeHHMM(night_date), formatTimeHHMM(day_date));

if (0) {
	dprintf(dlevel,"now: %s\n", now.toTimeString());
	dprintf(dlevel,"location: %s\n", pa.location || "not set");
	dprintf(dlevel,"night_start: %s -> %s\n", pa.night_start, night_date ? night_date.toTimeString() : "null");
	dprintf(dlevel,"day_start: %s -> %s\n", pa.day_start, day_date ? day_date.toTimeString() : "null");
	
	if (!night_date || !day_date) {
		dprintf(dlevel,"missing time calculation, returning false\n");
		return false;
	}
	
	let is_night = false;
	
	// Determine if night period crosses midnight (normal case)
	let night_hour = night_date.getHours();
	let day_hour = day_date.getHours();
	
	if (night_hour > day_hour || (night_hour == day_hour && night_date.getMinutes() >= day_date.getMinutes())) {
		// Night period crosses midnight (e.g., 20:00 to 06:00)
		is_night = (now.getHours() >= night_hour || now.getHours() < day_hour || 
		           (now.getHours() == night_hour && now.getMinutes() >= night_date.getMinutes()) ||
		           (now.getHours() == day_hour && now.getMinutes() < day_date.getMinutes()));
	} else {
		// Night period within same day (unusual case)
		is_night = (now >= night_date && now < day_date);
	}
}
	
	dprintf(dlevel,"is_night: %s\n", is_night);
	return is_night;
}
