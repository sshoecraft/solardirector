
function grid_init() {
	var grid_props = [
		[ "dyngrid", DATA_TYPE_BOOL, "false", 0 ],
		[ "grid_max_power", DATA_TYPE_FLOAT, "300", 0 ],
	];

	config.add_props(si,grid_props);
}

// dynamically modify chargs amps to keep grid under max power
function dynamic_grid() {

	var dlevel = 1;

	dprintf(dlevel,"ac2_power: %.1f\n", data.ac2_power);
	if (isNaN(data.ac2_power)) return 0;
	let grid = data.ac2_power * (-1);
	dprintf(dlevel,"grid: %f\n", grid);
	let avail = si.grid_max_power - grid;
	dprintf(dlevel,"avail: %.1f\n", avail);

	let bp = data.battery_power * (-1);
	dprintf(dlevel,"bp: %f\n", bp);
	bp += avail;
	dprintf(dlevel,"bp: %f\n", bp);
	let nca = bp / data.battery_voltage;
	dprintf(dlevel,"nca: %f\n", nca);

	var old_nca = nca;
	if (nca < si.min_charge_amps) nca = si.min_charge_amps;
	if (nca > si.max_charge_amps) nca = si.max_charge_amps;
	// Use a KF to "smooth" out the values
	if (typeof(kf_dg) == "undefined") kf_dg = new KalmanFilter();
	nca = kf_dg.filter(nca);
	if (nca != old_nca) dprintf(dlevel,"nca: %f\n", nca);
	si.charge_amps = nca;
	si.force_charge_amps = true;
}

function grid_main() {

	var dlevel = 1;

	// Dynamic grid charging
	dprintf(dlevel,"dyngrid: %d\n", si.dyngrid);
	if (si.dyngrid) dynamic_grid();
}
