
function write_main() {

	let dlevel = 1;

//	run(script_dir + "/snap.js");

	run(script_dir + "/pub.js");

//	report_mem();
	return;

	var n = 0;
	var out = [];
	out[n++] = "*****************************************************************";
	out[n++] = sprintf("Mode: %s, Water temp: %d", ac_modestr(ac.mode), ac.water_temp);
	out[n++] = sprintf("Cycle: %s, Sample: %s", cycle_statestr(ac.cycle_state), sample_statestr(ac.sample_state));
	["fan", "pump", "unit"].forEach(function(which) {
		let an = which + "s";
		let state_func = which + "_statestr";
		let line = ""
		for(let name in global[an]) {
			let item = global[an][name];
			let state;
			if (item.error) state = "Error";
			else if (!item.enabled) state = "Disabled";
			else state = global[state_func](item.state);
			if (line.length) line += sprintf(", ");
			let modestr = (typeof(item.mode) != 'undefined' ? ac_modestr(item.mode)+":" : "");
			// See if the fan is stopped due to water temp
			let opstr = "";
			if (which == "fan" && item.state == FAN_STATE_STOPPED && item.refs) {
				dprintf(dlevel,"water_temp: %s\n", ac.water_temp);
				if (ac.water_temp != INVALID_TEMP) {
					let cool_thresh = ac.cool_high_temp-ac.charge_threshold;
					let heat_thresh = ac.heat_low_temp+ac.charge_threshold;
					dprintf(dlevel,"mode: %s, water_temp: %d, cool_thresh: %d, heat_thresh: %d\n", ac_modestr(ac.mode), ac.water_temp, cool_thresh, heat_thresh);
					if ((ac.mode == AC_MODE_COOL && ac.water_temp > cool_thresh) || (ac.mode == AC_MODE_HEAT && ac.water_temp < high_thresh)) opstr = "(water_temp)";
				}
			} else if (which == "unit" && item.charging) {
				opstr = "(Charging)";
			}
 			line += sprintf("%s: %s%s%s", name, modestr, state, opstr);
		}
		if (line.length) out[n++] = sprintf("%s: %s", an, line);
	});
	out[n++] = "*****************************************************************";

	let outline = "";
	out.forEach(function(line) { outline += sprintf("%s\n", line); });
	if (typeof last_out_line == "undefined") last_out_line = "";
	if (outline != last_out_line) {
//		printf("\n%s", outline);
		for(let i=0; i < out.length; i++) log_write("%s\n",out[i]);
		last_out_line = outline;
	}
//	delobj(out);
	for(let i=0; i < out.length; i++) out.splice(i,1);
}
