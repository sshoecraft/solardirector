
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

function read_main() {

	let dlevel = 1;

	dprintf(dlevel,"mirroring: %s\n", si.mirror);
	if (si.mirror) run(script_dir+"/mirror.js");
//	else if (si.sim) run(script_dir+"/sim.js");

	// If not running, leave now
	dprintf(dlevel,"running: %s\n", data.Run);
	if (!data.Run) return;

	// When connected to SMANET 1st time, get some info
	if (typeof(si.have_smanet_info) == "undefined") si.have_smanet_info = false;
	if (!si.have_smanet_info && smanet.connected) {
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
	
	// Dynamic feed
	run(script_dir+"/feed.js");

	// Dynamic grid/gen
	dprintf(dlevel,"charge_mode: %d, feed_enabled: %d\n", si.charge_mode, si.feed_enabled);
	if (si.charge_mode && !si.feed_enabled) {
		dprintf(dlevel,"GdOn: %d, GnOn: %d\n", data.GdOn, data.GnOn);
		if (data.GdOn) run(script_dir+"/grid.js");
		else if (data.GnOn) run(script_dir+"/gen.js");
	}

	// Charge control
	run(script_dir+"/charge.js");

	// Set SoC / remain
	run(script_dir+"/soc.js");

	// @ 2:01am, set the date + time
	dprintf(1,"smanet.connected: %s\n", smanet.connected);
	if (smanet.connected && 0 == 1) {
		let d = new Date();
		dprintf(1,"hours: %d, mins: %d\n", d.getHours(), d.getMinutes());
		if (typeof(time_set) == "undefined") time_set = false;
		if (d.getHours() == 2 && d.getMinutes() == 1) {
			dprintf(1,"time_set: %s\n", time_set);
			if (!time_set) {
				// Date format: YYYYMMDD
				let si_dt = d.getFullYear().toString() + sprintf("%02d",d.getMonth()+1) + sprintf("%02d",d.getDate());
				dprintf(1,"si_dt: %s\n", si_dt);
				if (smanet_set_and_verify("Dt",si_dt,3)) {
					log_error("error setting date: %s\n",smanet.errmsg);
				} else {
					// Time format: HHMMSS
					let si_tm = sprintf("%02d",d.getHours()) + sprintf("%02d",d.getMinutes()) + sprintf("%02d",d.getSeconds());
					dprintf(1,"si_tm: %s\n", si_tm);
					if (smanet_set_and_verify("Tm",si_tm,3)) log_error("error setting time: %s\n",si_tm);
					else time_set = true;
				}
			}
		} else {
			time_set = false;
		}
        }

	return 0;
}
