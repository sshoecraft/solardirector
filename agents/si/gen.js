
function gen_init() {
	var gen_props = [
		[ "dyngen", DATA_TYPE_BOOL, "false", 0 ],
		[ "gen_max_power", DATA_TYPE_FLOAT, "24000", 0 ],
	];

	config.add_props(si,gen_props);
}

// dynamically modify chargs amps to keep gen under max power
function dynamic_gen() {

	var dlevel = 1;

	// when charging with gen, battery_power is not counted in "load_power"
	// makes it easy to calculate avail power for batt
	dprintf(dlevel,"load_power: %.1f\n", data.load_power);
	var avail_power = si.gen_max_power - data.load_power;
	dprintf(dlevel,"avail_power: %f\n", avail_power);
	nca = avail_power / data.battery_voltage;
	dprintf(dlevel,"nca: %f\n", nca);
	if (nca < 0) nca = 0;
	if (nca < si.min_charge_amps) nca = si.min_charge_amps;
	if (nca > si.max_charge_amps) nca = si.max_charge_amps;
	dprintf(dlevel,"nca: %f\n", nca);
	si.charge_amps = nca;
	si.force_charge_amps = 1;
}

function gen_main() {

	var dlevel = 1;

	// If we're charging AND gen_hold_soc is set, set soc to hold value
	dprintf(dlevel,"gen_hold_soc: %.1f, soc: %.1f\n", si.gen_hold_soc, si.soc);
	if (si.gen_hold_soc > 0 && si.gen_hold_soc < si.soc) {
		dprintf(dlevel,"adjusting soc to: %.1f\n", si.gen_hold_soc);
		si.soc = si.gen_hold_soc;
	}

	// Dynamic generator charging
	dprintf(dlevel,"dyngen: %d\n", si.dyngen);
	if (si.dyngen) dynamic_gen();

}
