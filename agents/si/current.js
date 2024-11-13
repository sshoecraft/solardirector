
/*
Copyright (c) 2024, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

function current_init() {
	let current_props = [
		[ "collect_current", DATA_TYPE_BOOL, "false", 0 ],
	];

	config.add_props(si,current_props);
	si.current_init = true;
}

function current_main() {
	let current_chans = [
		"InvCur",
		"InvCurSlv1",
		"InvCurSlv2",
		"InvCurSlv3",
		"TotInvCur",
		"ExtCur",
		"ExtCurSlv1",
		"ExtCurSlv2",
		"ExtCurSlv3",
		"TotExtCur"
	];

	let dlevel = -1;

	// this is so i can put it in place while running
	if (typeof(si.current_init) == "undefined") si.current_init = false;
	dprintf(dlevel,"init: %s\n", si.current_init);
	if (!si.current_init) current_init();

	// requires SMANET
	dprintf(dlevel,"connected: %s\n", smanet.connected);
	if (!smanet.connected) return;

	dprintf(dlevel,"collection enabled: %s\n", si.collect_current);
	if (!si.collect_current) return;

	let vals = smanet.get(current_chans);
	if (typeof(vals) == "undefined") {
		log_error("unable to get current data: %s\n", smanet.errmsg);
		return;
	}

	// We write the vals to the influx measurement "current"
        if (influx) dprintf(dlevel,"influx: enabled: %s, connected: %s\n", influx.enabled, influx.connected);
        if (influx && influx.enabled && influx.connected) influx.write("current",vals);

	return 0;
}
