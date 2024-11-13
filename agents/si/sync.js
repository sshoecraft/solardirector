
/*
Copyright (c) 2023, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

function sync_init() {
	var sync_props = [
		[ "sync", DATA_TYPE_BOOL, "false", 0 ],
		[ "sync_interval", DATA_TYPE_INT, "30", 0 ],
		[ "have_smanet_info", DATA_TYPE_BOOL, "false", CONFIG_FLAG_PRIVATE ],
	];

	config.add_props(si,sync_props);
}

function sync_main() {

	let dlevel = 1;

	dprintf(dlevel,"connected: %s\n", smanet.connected);
	if (!smanet.connected) return;

	// When connected to SMANET 1st time, get some info
	if (!si.have_smanet_info) {
		let vals = smanet.get([ "ExtSrc","BatCpyNom","ClstCfg" ]);
		if (typeof(vals) == "undefined") {
			log_error("unable to get smanet info: %s\n", smanet.errmsg);
		} else {
			let do_save = false;
			dprintf(dlevel,"si.charge_source: %s\n", si.charge_source);
			if (!si.charge_source) {
				si.charge_source = vals.ExtSrc;
				printf("Setting charge_source to: %s (from SMANET.ExtSrc)\n", si.charge_source);
				do_save = true;
			}
			dprintf(dlevel,"si.battery_capacity: %s\n", si.battery_capacity);
			if (!si.battery_capacity) {
				si.battery_capacity = parseInt(vals.BatCpyNom) * 10;
				printf("Setting battery_capacity to: %d (from SMANET.BatCpyNom)\n", si.battery_capacity);
				do_save = true;
			}
			if (!si.cluster_config) {
				si.cluster_config = vals.ClstCfg;
				si.phases = 1;
				if (vals.ClstCfg == "1Phase2") {
					si.units = 2;
				} else if (vals.ClstCfg == "1Phase3") {
					si.units = 3;
				} else if (vals.ClstCfg == "1Phase4") {
					si.units = 4;
				} else if (vals.ClstCfg == "2Phase2") {
					si.phases = 2;
					si.units = 2;
				} else if (vals.ClstCfg == "2Phase4") {
					si.phases = 2;
					si.units = 4;
				} else if (vals.ClstCfg == "3Phase") {
					si.phases = 3;
					si.units = 3;
				} else {
					printf("warning: unknown ClstCfg: %s\n", vals.ClstCfg);
					si.phases = 1;
					si.units = 1;
				}
				printf("Setting phases to: %d, and units to: %d\n", si.phases, si.units);
				do_save = true;
			}
			if (do_save) config.save();
			si.have_smanet_info = true;
		}
	}

	dprintf(dlevel,"sync: %s, sync_interval: %d\n", si.sync, si.sync_interval);
	if (si.sync && si.sync_interval) {
		let my_dt = new Date();
if (0) {
		let t = my_dt.getTime();
		let i = si.sync_interval * 60000;
		let r = t % i;
		let v = si.interval * 1000;
		let m = my_dt.getMinutes();
		let s = my_dt.getSeconds();
		dprintf(-1,"%02d:%02d t: %s, i: %s, r: %s, v: %s\n", m, s, t, i, r, v);
}
		// Every sync_interval minutes sync our internal settings vs SI
		if ((my_dt.getTime() % (si.sync_interval * 60000)) < (si.interval * 1000)) {
			let vals = smanet.get([ "GdManStr","GnManStr","GdMod","Dt","Tm" ]);
			if (typeof(vals) == "undefined") {
				log_error("unable to get smanet info: %s\n", smanet.errmsg);
			} else {
				dumpobj(vals);
				dprintf(dlevel,"GdManStr: %s, grid_started: %s\n", vals.GdManStr, si.grid_started);
				let doit = false;
				if (vals.GdManStr == "Start" && !si.grid_started) {
					dprintf(dlevel,"Starting grid...\n");
					if (doit) charge_start_grid(true);
					else si.grid_started = true;
				} else if (vals.GdManStr != "Start" && si.grid_started) {
					dprintf(dlevel,"Stopping grid...\n");
					if (doit) si_stop_grid(true);
					else si.grid_started = false;
				}
				dprintf(dlevel,"GnManStr: %s, gen_started: %s\n", vals.GnManStr, si.gen_started);
				if (vals.GnManStr == "Start" && !si.gen_started) {
					dprintf(dlevel,"Starting gen...\n");
					if (doit) si_start_gen(true);
					else si.gen_started = true;
				} else if (vals.GnManStr != "Start" && si.gen_started) {
					dprintf(dlevel,"Stopping gen...\n");
					if (doit) si_stop_gen(true);
					else si.gen_started = false;
				}
				dprintf(dlevel,"GdMod: %s, feed_enabled: %s\n", vals.GdMod, si.feed_enabled);
				if (vals.GdMod == "GridFeed" && !si.feed_enabled) {
					dprintf(dlevel,"Starting Feed...\n");
					if (doit) feed_start(true);
					else si.feed_enabled = true;
				} else if (vals.GdMod == "GridCharge" && si.feed_enabled) {
					dprintf(dlevel,"Stopping Feed...\n");
					if (doit) feed_stop(true);
					else si.feed_enabled = false;
				}

				// Date format: YYYYMMDD
				let si_dt = new Date();
				let si_dstr = vals.Dt.toString();
				dprintf(dlevel,"si_dstr: %s\n", si_dstr);
				si_dt.setYear(parseInt(si_dstr.substr(0,4),10));
				si_dt.setMonth(parseInt(si_dstr.substr(4,2),10)-1);
				si_dt.setDate(parseInt(si_dstr.substr(6,2),10));
				dprintf(dlevel,"si_dt: %s\n", si_dt);
				// Time format: [H]HMMSS
				let si_tstr = sprintf("%06d",vals.Tm);
				dprintf(dlevel,"si_tstr: %s\n", si_tstr);
				si_dt.setHours(parseInt(si_tstr.substr(0,2),10));
				si_dt.setMinutes(parseInt(si_tstr.substr(2,2),10));
				si_dt.setSeconds(parseInt(si_tstr.substr(4,2),10));

				dprintf(dlevel,"si_dt: %s\n", si_dt);
				dprintf(dlevel,"my_dt: %s\n", my_dt);

				// If there is a 23 second difference, update the SI
				dprintf(dlevel,"si_time: %s, my_time: %s\n", si_dt.getTime(), my_dt.getTime());
				let diff = (my_dt.getTime() - si_dt.getTime()) / 1000;
				dprintf(dlevel,"diff: %s\n", diff);
				if (diff < -23 || diff > 23) {
					let my_dstr = my_dt.getFullYear().toString() + sprintf("%02d",my_dt.getMonth()+1) + sprintf("%02d",my_dt.getDate());
					dprintf(dlevel,"si_dstr: %s, my_dstr: %s\n", si_dstr, my_dstr);
					if (si_dstr != my_dstr) {
						dprintf(dlevel,"Setting Date... from: %s, to: %s\n", si_dstr, my_str);
						if (si_smanet_set_value("Dt",my_dstr,2))
							log_error("error setting date: %s\n",smanet.errmsg);
					}

					let my_tstr = sprintf("%d",my_dt.getHours()) + sprintf("%02d",my_dt.getMinutes()) + sprintf("%02d",my_dt.getSeconds());
					dprintf(dlevel,"Setting Time...from: %s, to: %s\n", si_tstr, my_tstr);
					if (si_smanet_set_value("Tm",my_tstr,2))
						log_error("error setting date: %s\n",smanet.errmsg);
				}
			}
		}
	}

	return 0;
}
