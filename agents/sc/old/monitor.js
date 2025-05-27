
function monitor_init() {
	dprintf(1,"*** MONITOR INIT ***\n");
	monitor_props = [
		[ "monitor_disp", DATA_TYPE_BOOL, "false", 0 ],
                [ "battery_voltage_min", DATA_TYPE_DOUBLE, 33.6, 0 ],
                [ "battery_voltage_max", DATA_TYPE_DOUBLE, 58.8, 0 ],
                [ "battery_temp_min", DATA_TYPE_DOUBLE, 0, 0 ],
                [ "battery_temp_max", DATA_TYPE_DOUBLE, 90, 0 ],
	];

	config.add_props(sc,monitor_props);
//	config.add_funcs(sc,monitor_funcs);
}

function do_notify(info, reason) {

	let dlevel = -1;

	agent.event("Monitor",reason);
	let notify = (sc.notify && info.notify);
	dprintf(dlevel,"notify: %s\n", notify);
	dprintf(dlevel,"sc.notify_path: %s\n", sc.notify_path);
	dprintf(dlevel,"info.notify_path: %s\n", info.notify_path);
	let path = info.notify_path ? info.notify_path : sc.notify_path;
	dprintf(dlevel,"path: %s\n", path);
	if (typeof(path) == "undefined") {
		path = SOLARD_BINDIR + "/notify";
		dprintf(dlevel,"FORCED path: %s\n", path);
	}
	if (!new File(path).exists) {
		printf("ERROR: notify path '%s' for %s does not exist!\n",path,info.key);
		return 1;
	}
	let args = [ path, "\"" + reason + "\"" ];
	dprintf(dlevel,"args: %s\n", args);
	info.pid = sc.exec(path,args,true);
	if (info.pid < 0) {
		log_syserr("unable to exec: " + path);
		return 1;
	}

	return 0;
}

function notify(list) {

	let dlevel = 1;

//	dprintf(-1,"list.length: %d\n", list.length);
	// get the notify_paths
	let paths = [];
	dprintf(dlevel,"sc.notify_path: %s\n", sc.notify_path);
	for(let i=0; i < list.length; i++) {
		let info = list[i].info;
		dprintf(dlevel,"info.notify_path: %s\n", info.notify_path);
		let path = info.notify_path ? info.notify_path : sc.notify_path;
		dprintf(dlevel,"path: %s\n", path);
		if (!new File(path).exists) {
			printf("ERROR: notify path '%s' for %s does not exist!\n",path,info.key);
			continue;
		}
		paths.push(path);
	}

	dprintf(dlevel,"paths.length: %d\n", paths.length);
	if (!paths.length) return 1;
	// sort the paths and keep unique
	paths = arrayUnique(paths).sort();
	dprintf(dlevel,"NEW paths.length: %d\n", paths.length);
	// now notify for every unique path
	for(i=0; i < paths.length; i++) {
		dprintf(dlevel,"paths[%d]: %s\n", i, paths[i]);
		let args = [];
		let pa = false;
		for(let j=0; j < list.length; j++) {
			let info = list[j].info;
			let path = info.notify_path ? info.notify_path : sc.notify_path;
			dprintf(dlevel,"path: %s\n", path);
			if (path == paths[i]) {
				if (!pa) {
					args.push(path);
					pa = true;
				}
				let reason = list[j].reason;
				args.push("\"" + reason + "\"");
			}
		}
		dprintf(dlevel,"args: %s\n", args);
		let pid = sc.exec(paths[i],args,true);
		dprintf(dlevel,"pid: %d\n",pid);
		if (pid < 0) {
			log_syserr("unable to exec: " + paths[i]);
			return 1;
		}
	}
}

function monitor_main() {

	let dlevel = 1;

	notify_list = [];

if (0) {
	let names = sc.agents.esplit(",");
	names.sort();
	let name_len_max = 0;
	dprintf(dlevel,"names.length: %d\n", names.length);
	for(let i=0; i < names.length; i++) {
		let name = names[i];
		dprintf(dlevel,"name: %s\n", name);
		if (!name.length) continue;
		let info = agents[name];
		if (typeof(info) == "undefined") {
			printf("internal error: agent info is undefined for name: %s\n",name);
			continue;
		}
		dprintf(dlevel,"enabled: %s, managed: %s\n", info.enabled, info.managed);
		if (!info.enabled || !info.managed) continue;
		dprintf(dlevel,"status: %d\n", info.status);
		let cur = time();
		if (info.status == AGENT_STATUS_NONE) {
			_start_agent(info);
			info.updated = time();
		} else if (info.status == AGENT_STATUS_STARTED) {
			// Check the process status
			let r = (info.pid ? sc.checkpid(info.pid) : 0);
			dprintf(dlevel,"r: %d\n", r);
			if (r) info.status = AGENT_STATUS_NONE;
		}
		if (name.length > name_len_max) name_len_max = name.length;
	}

	mq = agent.messages;
	dprintf(dlevel,"mq len: %d\n", mq.length);
	for(let j=0; j < mq.length; j++) {
		msg = mq[j];
//		printf("func: %s\n", msg.func);
		if (msg.func == "Data") {
			// Is this one of ours?
			let info = sc_get_info(msg.name);
			if (!info) continue;
			dprintf(dlevel,"getting data for %s\n", msg.name);
			info.data = JSON.parse(msg.data);
			info.updated = time();
		}
	}
	agent.purgemq();

	if (sc.monitor_disp) printf("======================================================================\n");
	let line = "";
	let out,fmt;
	let code,diff;
	// Report agents that havent updated
	for(i=0; i < names.length; i++) {
		let name = names[i];
		dprintf(dlevel,"name: %s\n", name);
		if (!name.length) continue;
		let info = sc_get_info(name);
		dprintf(dlevel,"info: %s\n", info);
		if (!info) continue;
		dprintf(dlevel,"status: %d\n", info.status);
		switch(info.status) {
		case AGENT_STATUS_NONE:
			code = "*";
			diff = 0;
			break;
		case AGENT_STATUS_STARTED:
			if (info.monitor) {
				dprintf(dlevel,"updated: %s\n", info.updated);
				if (typeof(info.updated) == "undefined") info.updated = time();
				diff = time() - info.updated;
//				dprintf(-1,"\ninfo.key: %s\n", info.key);
				dprintf(dlevel,"diff: %d\n", diff);
				let notify_time = (info.notify_time < 0 ? sc.notify_time : info.notify_time);
				dprintf(dlevel,"notify_time: %d\n", notify_time);
				let error_time = (info.error_time < 0 ? sc.error_time : info.error_time);
				dprintf(dlevel,"error_time: %d\n", error_time);
				let warning_time = (info.warning_time < 0 ? sc.warning_time : info.warning_time);
				dprintf(dlevel,"warning_time: %d\n", warning_time);
				dprintf(dlevel,"sc.interval: %d\n", sc.interval);
				dprintf(dlevel,"%s: notified: %s, errormsg: %s, warned: %s\n", name, info.notified, info.errormsg, info.warned);
				if (notify_time && diff >= notify_time && !info.notified) {
					let reason = sprintf("agent '%s' has not responded in %s seconds!",name,diff);
//					do_notify(info,reason);
					notify_list.push({ info: info, reason: reason});
					info.notified = true;
				} else if (error_time && diff >= error_time && !info.errormsg) {
					if (!sc.monitor_disp) printf("agent '%s' has not responded in %s seconds!\n",name,diff);
					info.errormsg = true;
				} else if (warning_time && diff >= warning_time && !info.warned) {
					if (!sc.monitor_disp) printf("agent '%s' has not responded in %s seconds!\n",name,diff);
					info.warned = true;
//				} else if (diff < sc.interval) {
				} else if (notify_time && diff < notify_time) {
					info.notified = info.errormsg = info.warned = false;
				}
				if (info.notified) code = "N";
				else if (info.errormsg) code = "E";
				else if (info.warned) code = "W";
				else code = "+";
			} else {
				code = "+";
				diff = 0;
			}
			break;
		case AGENT_STATUS_STOPPED:
			code = "-";
			diff = 0;
			break;
		}
		if (sc.monitor_disp) {
			fmt = sprintf("%%%ds [%%1s] %%09d",name_len_max);
			dprintf(dlevel,"fmt: %s\n", fmt);
			out = sprintf(fmt,name,code,diff);
			dprintf(dlevel,"out(%d): %s\n", out.length, out);
//			dprintf(dlevel,"line.length: %d, out.length: %d\n", line.length, out.length);
			if (line.length + out.length > 78) {
				printf("%s\n",line);
				line = "";
			}
			line += out + "   ";
		}
	}
	if (sc.monitor_disp) {
		if (line.length) printf("%s\n",line);
//		printf("Legend: -: stopped, +: running, W: warning, E: error, N: notified\n");
	}
//	dprintf(-1,"influx: %s\n", influx);
//	if (influx) dprintf(-1,"influx: enabled: %s, connected: %s\n", influx.enabled, influx.connected);

	// Get average voltage of all batteries
	let total = 0;
	let count = 0;
	let batts = [];
	for(i=0; i < names.length; i++) {
		let name = names[i];
		dprintf(dlevel,"name: %s\n", name);
		if (!name.length) continue;
		let info = sc_get_info(name);
		dprintf(dlevel,"info: %s\n", info);
		if (!info) continue;
		if (info.role == "Battery") {
			dprintf(dlevel,"info.data: %s\n", info.data);
			if (typeof(info.data) == "undefined") continue;
//			dumpobj(info.data);
			dprintf(dlevel,"updated: %s\n", info.updated);
			if (typeof(info.updated) == "undefined") continue;
			diff = time() - info.updated;
			dprintf(dlevel,"diff: %s\n", diff);
			// We only want fresh data
			if (diff < 60) {
//				dumpobj(info.data);
				total += info.data.voltage;
				batts.push(info);
			}
		}
	}
	dprintf(dlevel,"total: %s, count: %d\n", total, batts.length);
	if (batts.length) {
		let avg = pround(total / batts.length,2);
		dprintf(dlevel,"avg: %s\n", avg);

		for(j=0; j < batts.length; j++) {
			let info = batts[j]
			let bp = info.data;
//			dumpobj(bp);

			// Check voltage
//			printf("[%s] %s voltage: %.1f\n",timestamp(),bp.name,bp.voltage);
			let p = pround(pct(bp.voltage, avg),2);
			dprintf(dlevel,"%s: voltage: %s, avg: %s, pct: %.2f%%\n", bp.name, bp.voltage, avg, p);
			if (p >= 2.0) {
				let reason = sprintf("Battery %s voltage (%.1f) differs from average (%.1f) by %.2f%%",
					bp.name,bp.voltage,avg,p);
//				do_notify(info,reason);
				notify_list.push({ info: info, reason: reason});
			}

			// Check cell diff
			dprintf(dlevel,"cell_diff: %s\n", bp.cell_diff);
			if (bp.cell_diff > 1.0) {
				let reason = sprintf("Battery %s cell diff is %s!", bp.cell_diff);
//				do_notify(info,reason);
				notify_list.push({ info: info, reason: reason});
			}

			// Check temp
			dprintf(dlevel,"temp_min: %f, temp_max: %f\n", sc.battery_temp_min, sc.battery_temp_max);
			dprintf(dlevel,"%s: temps.length: %d\n", bp.name, bp.temps.length);
			for(let k=0; k < bp.temps.length; k++) {
				let temp = bp.temps[k];
				dprintf(dlevel,"%s: temp[%d]: %f\n", bp.name, k, temp);
				if (temp < sc.battery_temp_min || temp > sc.battery_temp_max) {
					let reason = sprintf("Battery %s temp is %s!", bp.name, temp);
//					do_notify(info,reason);
					notify_list.push({ info: info, reason: reason});
					break;
				}
			}
		}
	}

	dprintf(dlevel,"notify_list.length: %d\n", notify_list.length);
	if (notify_list.length) notify(notify_list);
}
	return;
}
