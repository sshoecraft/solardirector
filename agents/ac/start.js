
function start_main() {
	let dlevel = -1;

	log_info("Startup: initializing state\n");

	// Stop all direct groups FIRST - coordinates fan/unit/valve shutdown
	for(let name in directs) {
		let dg = directs[name];
		dprintf(dlevel,"direct[%s]: pin: %d, state: %s\n", name, dg.pin, direct_statestr(dg.state));
		dg_valve_off(name);
		dg.state = DIRECT_STATE_STOPPED;
		dg.active = false;
		dg.pending_fan = "";
		dg.active_fan = "";
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
