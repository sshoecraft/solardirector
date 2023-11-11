
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

function set_feed_start_date() {

        var dlevel = 1;

	dprintf(dlevel,"feed_start_time: %s\n", si.feed_start_time);
        if (!si.feed_start_time.length) si.feed_start_date = undefined;
        else si.feed_start_date = get_date(si.location,si.feed_start_time,"feed_start_time",true);
        if (si.feed_start_date) dprintf(dlevel,"NEW feed_start_date: %s\n",si.feed_start_date);
}

function set_feed_stop_date() {

        var dlevel = 1;

	dprintf(dlevel,"feed_stop_time: %s\n", si.feed_stop_time);
        if (!si.feed_stop_time.length) si.feed_stop_date = undefined;
        else si.feed_stop_date = get_date(si.location,si.feed_stop_time,"feed_stop_time",true);
        if (si.feed_stop_date) dprintf(dlevel,"NEW feed_stop_date: %s\n",si.feed_stop_date);
}

function set_feed_timeout_after_date() {

        var dlevel = 0;

	dprintf(dlevel,"feed_timeout_after: %s\n", si.feed_timeout_after);
        if (!si.feed_timeout_after.length) si.feed_timeout_after_date = undefined;
        else si.feed_timeout_after_date = get_date(si.location,si.feed_timeout_after,"feed_timeout_after",true);
        if (si.feed_timeout_after_date) dprintf(dlevel,"NEW feed_timeout_after_date: %s\n",si.feed_timeout_after_date);
}

function feed_event_handler(name,module,action) {

	let dlevel = 1;

	dprintf(dlevel,"name: %s, module: %s, action: %s\n", name, module, action);
	if (name.match(/si.*/) && module == "Location" && action == "Set") {
		set_feed_start_date();
		set_feed_stop_date();
		set_feed_timeout_after_date();
	}
}

function feed_init() {
	var feed_props = [
		[ "feed_enabled", DATA_TYPE_BOOL, "false", CONFIG_FLAG_NOSAVE ],
		[ "dynfeed", DATA_TYPE_BOOL, "false", 0 ],
		[ "feed_start_time", DATA_TYPE_STRING, null, 0, set_feed_start_date ],
		[ "feed_stop_time", DATA_TYPE_STRING, null, 0, set_feed_stop_date ],
		[ "feed_timeout", DATA_TYPE_INT, "300", 0 ],
		[ "feed_timeout_after", DATA_TYPE_STRING, null, 0, set_feed_timeout_after_date ],
	];

	config.add_props(si,feed_props);
	si.feed_timeout_start = 0;

	agent.event_handler(feed_event_handler,"si","Location","Set");
}

function feed_start(force) {

	let dlevel = 0;

	let lforce = force;
	if (typeof(lforce) == "undefined") lforce = false;
	dprintf(dlevel,"feed_enabled: %s, force: %s\n", si.feed_enabled, lforce);
	if (si.feed_enabled && !lforce) return 0;

	// Dont start feed if we're charging and dyngrid is enabled
	dprintf(-1,"charge_mode: %d, dyngrid: %s\n", si.charge_mode, si.dyngrid);
	if (!lforce && si.charge_mode != 0 && si.dyngrid) return 0;

	dprintf(dlevel,"GnOn: %s\n", data.GnOn);
	if (data.GnOn) {
		si.errmsg = "Generator running, not starting feed...\n";
		log_error("%s\n",si.errmsg);
		return 1;
	}

	agent.event("Feed","Start");

	if (si_smanet_set_value("GdMod","GridFeed",2)) {
		si.errmsg = "error setting GdMod to GridFeed";
		log_error("%s\n",si.errmsg);
		return 1;
	}
	// Use charge start/stop for grid
	si_start_grid();

	si.feed_enabled = true;
	si.feed_timeout_start = 0;
	if (si.feed_timeout_after.length) set_feed_timeout_after_date();

	return 0;
}

function feed_stop(force) {

	let dlevel = 0;

	let lforce = force;
	if (typeof(lforce) == "undefined") lforce = false;
	dprintf(dlevel,"feed_enabled: %s, force: %s\n", si.feed_enabled, lforce);
	if (!si.feed_enabled && !lforce) return 0;

	agent.event("Feed","Stop");

	if (si_smanet_set_value("GdMod","GridCharge",2)) {
		si.errmsg = "error setting GdMod to GridCharge";
		log_error("%s\n",si.errmsg);
		return 1;
	}
	// Use charge start/stop for grid
	si_stop_grid(false);

	si.feed_enabled = false;
	si.feed_timeout_start = 0;

	return 0;
}

function dynamic_feed() {

	var dlevel = 1;

	dprintf(dlevel,"battery_power: %.1f\n", data.battery_power);
	dprintf(dlevel,"ac2_power: %.1f\n", data.ac2_power);
	if (isNaN(data.ac2_power)) return 0;
	var diff = data.ac2_power - data.battery_power;
	dprintf(dlevel,"diff: %f\n", diff);
	nca = diff / data.battery_voltage;
	dprintf(dlevel,"nca: %f\n", nca);
	if (nca < 0) nca = 0;
	if (nca) {
		var r = (10 - (Math.log(nca) * Math.LOG2E))/100;
		dprintf(dlevel,"r: %f\n", r);
		nca -= (nca * r);
		dprintf(dlevel,"nca: %f\n", nca);
	}
	if (nca < si.min_charge_amps) nca = si.min_charge_amps;
	if (nca > si.max_charge_amps) nca = si.max_charge_amps;
	dprintf(dlevel,"nca: %f\n", nca);
	si.charge_amps = nca;
	si.force_charge_amps = true;
}

function feed_timeout() {

	var dlevel = 1;

	let cur = new Date();
	dprintf(dlevel,"current_date: %s\n", cur);
	dprintf(dlevel,"feed_timeout_after_date: %s\n", si.feed_timeout_after_date);
	if (si.feed_timeout_after_date && (cur.getTime() < si.feed_timeout_after_date.getTime())) {
		si.feed_timeout_start = 0;
		return 0;
	}

	dprintf(dlevel,"feed_timeout_start: %d\n", si.feed_timeout_start);
	if (!si.feed_timeout_start) si.feed_timeout_start = time();
	var diff = time() - si.feed_timeout_start;
	dprintf(dlevel,"diff: %d, timeout: %d\n", diff, si.feed_timeout);
//	return (diff > si.feed_timeout ? true : false);
	let r = diff > si.feed_timeout ? true : false;
	dprintf(dlevel,"returning: %s\n", r);
	return r;
}

function feed_main() {

	var dlevel = 1;

	// feed auto start/stop
	let cur = new Date();
	dprintf(dlevel,"current_date: %s\n", cur);
	dprintf(dlevel,"feed_start_date: %s\n", si.feed_start_date);
	if (si.feed_start_date) dprintf(dlevel,"cur: %s, start: %s, diff: %s\n",
		cur.getTime(), si.feed_start_date.getTime(), si.feed_start_date.getTime() - cur.getTime());
	if (si.feed_start_date && cur.getTime() >= si.feed_start_date.getTime()) {
		feed_start(false);
		set_feed_start_date();
	}

	dprintf(dlevel,"feed_stop_date: %s\n", si.feed_stop_date);
	if (si.feed_stop_date) {
		dprintf(dlevel,"charge_mode: %d, charge_feed: %s\n", si.charge_mode, si.charge_feed);
		dprintf(dlevel,"cur: %s, stop: %s, diff: %s\n",
			cur.getTime(), si.feed_stop_date.getTime(), si.feed_stop_date.getTime() - cur.getTime());
	}
	if (si.feed_stop_date && cur.getTime() >= si.feed_stop_date.getTime()) {
		if (!(si.charge_mode && si.charge_feed)) feed_stop(false);
		set_feed_stop_date();
	}

	// check if feed enabled (in case of a restart)
	if (typeof(feed_check) == "undefined") feed_check = false;
	dprintf(dlevel,"connected: %d, feed_enabled: %d, feed_check: %d\n", smanet.connected, si.feed_enabled, feed_check);
	if (smanet.connected && !si.feed_enabled && !feed_check) {
		var vals = smanet.get(["GdMod","GdManStr"]);
		dprintf(dlevel,"vals type: %s\n", typeof(vals));
		if (typeof(vals) != "undefined") {
			dprintf(dlevel,"vals.GdMod: %s, GdManStr: %s\n", vals.GdMod, vals.GdManStr);
			if (vals.GdMod == "GridFeed" && vals.GdManStr == "Start") si.feed_enabled = true;
			feed_check = true;
		}
	}

	dprintf(dlevel,"input_source: %d, feed_enabled: %d, GdOn: %d\n", si.input_source, si.feed_enabled, data.GdOn);
	if (si.input_source != CURRENT_SOURCE_NONE && si.feed_enabled && data.GdOn) {
		// If ac2_power has been < 0 longer than timeout, disable feed (only if not force charging)
		dprintf(dlevel,"charge_mode: %d, charge_feed: %s, ac2_power: %.1f\n",
			si.charge_mode, si.charge_feed, data.ac2_power);
		if (!(si.charge_mode && si.charge_feed) && data.ac2_power < 0) {
			if (feed_timeout()) {
				dprintf(dlevel,"timed out, stopping feed\n");
				feed_stop(false);
			}
		} else {
			si.feed_timeout_start = 0;
		}

		// Feed timeout could have disabled feeding
		dprintf(dlevel,"charge_mode: %d, dynfeed: %d\n", si.charge_mode, si.dynfeed);
		if (si.feed_enabled && si.charge_mode && si.dynfeed) dynamic_feed();
	}
}
