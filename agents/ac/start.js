
function start_main() {
	let dlevel = -1;

	log_info("Startup: initializing state\n");

	// Stop all units
	for(let name in units) {        
                dprintf(dlevel,"name: %s\n", name);
                let unit = units[name];
//              dumpobj(unit);  
                 
                dprintf(dlevel,"unit[%s]: enabled: %d, refs: %d\n", name, unit.enabled, unit.refs);
		unit_force_stop(name);
	}

	// Stop all fans
        for(let name in fans) {
                dprintf(dlevel,"name: %s\n", name);
                let fan = fans[name];
//              dumpobj(fan);

                dprintf(dlevel,"fan[%s]: enabled: %d, refs: %d, state: %s\n", name, fan.enabled, fan.refs, fan_statestr(fan.state));
		fan_force_stop(name);
	}

	// Stop all pumps
        for(let name in pumps) {
                dprintf(dlevel,"name: %s\n", name);
                let pump = pumps[name];
//              dumpobj(pump);

                dprintf(dlevel,"pump[%s]: enabled: %d, refs: %d\n", name, pump.enabled, pump.refs);
		pump_force_stop(name);
	}
}
