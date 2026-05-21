
function start_main() {
	let dlevel = -1;

	log_info("Startup: initializing state\n");

	// Stop all direct groups FIRST - coordinates fan/unit/valve shutdown
	for(let name in directs) {
		let dg = directs[name];
		dprintf(dlevel,"direct[%s]: pin1: %d, pin2: %d, state: %s\n", name, dg.pin1, dg.pin2, direct_statestr(dg.state));
		// Stop primer if configured
		if (dg.primer && dg.primer.length) pump_stop(dg.primer);
		dg_valves_off(name);
		dg.state = DIRECT_STATE_STOPPED;
		dg.active = false;
		dg.pending_fan = "";
		dg.active_fan = "";
		dg.initial_temp = INVALID_TEMP;
		dg.primer_start_time = 0;
	}

	// Stop all units
	for(let name in units) {
                dprintf(dlevel,"name: %s\n", name);
                let unit = units[name];
                dprintf(dlevel,"unit[%s]: enabled: %d, refs: %d\n", name, unit.enabled, unit.refs);
		unit_force_stop(name);
	}

	// Stop all fans
        for(let name in fans) {
                dprintf(dlevel,"name: %s\n", name);
                let fan = fans[name];
                dprintf(dlevel,"fan[%s]: enabled: %d, refs: %d, state: %s\n", name, fan.enabled, fan.refs, fan_statestr(fan.state));
		fan_force_stop(name);
	}

	// Stop all pumps
        for(let name in pumps) {
                dprintf(dlevel,"name: %s\n", name);
                let pump = pumps[name];
                dprintf(dlevel,"pump[%s]: enabled: %d, refs: %d\n", name, pump.enabled, pump.refs);
		pump_force_stop(name);
	}
}
