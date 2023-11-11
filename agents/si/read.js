
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

	// Sync
	run(script_dir+"/sync.js");

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

	return 0;
}
