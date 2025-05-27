
/* best viewed in window with at least 128 chars and tabstops set to 4
012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678
*/

function run_main() {

	let dlevel = 1;

	let diff,pump_state;

	// Fan can be controlled by air handler/thermostat
	// fans messages come in as mqtt
	let mq = agent.messages;
	dprintf(dlevel,"mq len: %d\n", mq.length);
	for(let j=0; j < mq.length; j++) {
		let msg = mq[j];
//		dumpobj(msg);
		dprintf(dlevel,"func: %s\n", msg.func);
		if (msg.func == "Data") {
			dprintf(dlevel,"getting data for %s\n", msg.name);
			let data = JSON.parse(msg.data);
//			dumpobj(data);
			let name = data.name;
			dprintf(dlevel,"name: %s\n", name);
			// make sure it's a fan by ensuring the "fan_state" field is present
			let fs_type = typeof(data.fan_state);
			dprintf(dlevel,"fan_state type: %s\n", fs_type);
			if (fs_type == "undefined") continue;
			let fan = fans[name];
			if (!fan) {
				dprintf(dlevel,"fan not found\n");
				continue;
			}
//			dumpobj(fan);
			dprintf(dlevel,"fan[%s]: pump: %s\n", name, fan.pump);
			if (!fan.pump) continue;
			let pump = pumps[fan.pump];
			if (!pump) {
				error_set("fan",fan.pump,sprintf("fan %s specifies pump %s, which doesnt exist", name, fan.pump));
				continue;
			}

			// fan is started from the unit/thermostat, so we can't follow normal process
			dprintf(dlevel,"data.fan_state: %s, fan.fan_state: %s\n", data.fan_state, fan.fan_state);
			if (data.fan_state != fan.fan_state) {
				log_info("fan[%s] fan_state: %s -> %s\n", name, fan.fan_state, data.fan_state);
				fan.fan_state = data.fan_state;
			}

			// Get the air in/out
			fan.temp_in = data.air_in;
			fan.temp_out = data.air_out;

			dprintf(dlevel,"data.heat_state: %s, fan.heat_state: %s\n", data.heat_state, fan.heat_state);
			dprintf(dlevel,"data.cool_state: %s, fan.cool_state: %s\n", data.cool_state, fan.cool_state);
			dprintf(dlevel,"fan[%s]: mode: %s\n", name, fan_modestr(fan.mode));
			if (data.heat_state == "on") {
				if (fan.heat_state != "on") {
					log_info("fan[%s] heat_state: %s -> %s\n", name, fan.heat_state, data.heat_state);
					if (!fan_set_mode(name,FAN_MODE_HEAT)) fan.heat_state = data.heat_state;
				}
			} else if (data.cool_state == "on") {
				if (fan.cool_state != "on") {
					log_info("fan[%s] cool_state: %s -> %s\n", name, fan.cool_state, data.cool_state);
					if (!fan_set_mode(name,FAN_MODE_COOL)) fan.cool_state = data.cool_state;
				}
			}
			if (data.heat_state == "off" && fan.heat_state == "on") {
				log_info("fan[%s] heat_state: %s -> %s\n", name, fan.heat_state, data.heat_state);
				if (!fan_set_mode(name,FAN_MODE_NONE)) fan.heat_state = data.heat_state;
			} else if (data.cool_state == "off" && fan.cool_state == "on") {
				log_info("fan[%s] cool_state: %s -> %s\n", name, fan.cool_state, data.cool_state);
				if (!fan_set_mode(name,FAN_MODE_NONE)) fan.cool_state = data.cool_state;
			}
		}
	}

	for(let name in fans) {
		dprintf(dlevel,"name: %s\n", name);
		let fan = fans[name];
//		dumpobj(fan);

		dprintf(dlevel,"fan[%s]: enabled: %d, refs: %d, state: %s\n", name, fan.enabled, fan.refs, fan_statestr(fan.state));
		if ((fan.error || !fan.enabled || !fan.refs) && fan.state < FAN_STATE_COOLDOWN) continue;

		dprintf(dlevel,"fan[%s]: state: %s\n", name, fan_statestr(fan.state));
		switch(fan.state) {
		// Start the pump first
		case FAN_STATE_STOPPED:
			// Dont start unless under water temp within limits
			dprintf(dlevel,"fan[%s]: stop_wt: %s, water_temp: %s\n", name, fan.stop_wt, ac.water_temp);
			if (fan.stop_wt && ac.water_temp != INVALID_TEMP) {
				dprintf(dlevel,"mode: %s, water_temp: %d, cool_high_temp: %d, heat_low_temp: %d\n", ac_modestr(ac.mode), ac.water_temp, ac.cool_high_temp, ac.heat_low_temp);
				dprintf(dlevel,"fan[%s]: wt_warned: %s\n", name, fan.wt_warned);
				if ((ac.mode == AC_MODE_COOL && ac.water_temp > ac.cool_high_temp) || (ac.mode == AC_MODE_HEAT && ac.water_temp < ac.heat_low_temp)) {
					if (!fan.wt_warned) {
						log_warning("fan %s not started due to water_temp out of range\n",name);
						fan.wt_warned = true;
					}
					continue;
				} else if (fan.wt_warned) {
					fan.wt_warned = false;
				}
			}
			dprintf(dlevel,"fan.pump: %s\n", fan.pump);
			if (fan.reserve) fan_set_state(name,FAN_STATE_RESERVE);
			else if (fan.pump.length) fan_set_state(name,FAN_STATE_START_PUMP);
			else fan_set_state(name,FAN_STATE_START);
			break;

		// Reserve power
		case FAN_STATE_RESERVE:
			if (!pa_client_reserve(ac,"fan",name,fan.reserve,fan.priority))  {
				if (fan.pump.length) fan_set_state(name,FAN_STATE_START_PUMP);
				else fan_set_state(name,FAN_STATE_START);
			}
			break;

		// Start the pump
		case FAN_STATE_START_PUMP:
			if (pump_start(fan.pump)) {
				error_set("fan",name,sprintf("fan %s: unable to start pump: %s", name, fan.pump));
				fan_set_state(name,FAN_STATE_ERROR);
			} else {
				fan.pump_start_time = time();
				fan_set_state(name,FAN_STATE_WAIT_PUMP);
			}
			break;

		// Wait <pump_timeout> seconds for the pump to change state to RUNNING
		case FAN_STATE_WAIT_PUMP:
			pump_state = pump_get_state(fan.pump);
			dprintf(dlevel,"fan[%s]: pump_state: %s\n", name, pump_statestr(pump_state));
			if (pump_state == PUMP_STATE_RUNNING) {
				fan_set_state(name,FAN_STATE_START);
			} else {
				dprintf(dlevel,"fan[%s]: time: %s, fan.pump_start_time: %s\n", name, time(), fan.pump_start_time);
				diff = time() - fan.pump_start_time;
				dprintf(dlevel,"fan[%s]: diff: %s, pump_timeout: %s\n", name, diff, fan.pump_timeout);
				if (diff >= fan.pump_timeout) {
					error_set("fan",name,sprintf("fan %s: timeout waiting for pump %s", name, fan.pump));
					fan_set_state(name,FAN_STATE_ERROR);

				// If we went through a few loops and pump is stuck in reserve start over
				} else if (diff > 3 && pump_state == PUMP_STATE_RESERVE) {
					fan_set_state(name,FAN_STATE_RELEASE);
				}
			}
			break;

		// Start the fan
		case FAN_STATE_START:
			if (fan_on(name,fan)) {
				error_set("fan",name,"unable to power on fan "+name);
				fan_set_state(name,FAN_STATE_ERROR);
			} else {
				fan.time = time();
				fan_set_state(name,FAN_STATE_WAIT_START);
			}
			break;

		// Wait for the fan to report it started
		case FAN_STATE_WAIT_START:
			if (fan.fan_state == "on") {
				fan_set_state(name,FAN_STATE_RUNNING);
			} else {
				dprintf(dlevel,"fan[%s]: time: %s, fan.time: %s\n", name, time(), fan.time);
				diff = time() - fan.time;
				dprintf(dlevel,"fan[%s]: diff: %s, start_timeout: %s\n", name, diff, fan.start_timeout);
				if (diff >= fan.start_timeout) {
					error_set("fan",name,"timeout waiting for fan "+name+" to start");
					fan_set_state(name,FAN_STATE_ERROR);
				}
			}
			break;

		// Fan is running, monitor pump to ensure it's running
		case FAN_STATE_RUNNING:
			dprintf(dlevel,"fan[%s]: pump.length: %d\n", name, fan.pump.length);
			if (fan.pump.length) {
				pump_state = pump_get_state(fan.pump);
				dprintf(dlevel,"fan[%s]: pump %s state: %s\n", name, fan.pump, pump_statestr(pump_state));
				if (pump_state != PUMP_STATE_RUNNING) {
					error_set("fan",name,sprintf("fan %s: required pump %s is not running",name,fan.pump));
					fan_set_state(name,FAN_STATE_ERROR);
				}
			}
			// Stop the fan if the water temp goes out of limits
			dprintf(dlevel,"fan[%s]: stop_wt: %s, water_temp: %s\n", name, fan.stop_wt, ac.water_temp);
			if (fan.stop_wt && ac.water_temp != INVALID_TEMP) {
				dprintf(dlevel,"mode: %s, cool_high_temp: %d, heat_low_temp: %d\n", ac_modestr(ac.mode), ac.water_temp, ac.cool_high_temp, ac.heat_low_temp);
				if ((ac.mode == AC_MODE_COOL && ac.water_temp >= ac.cool_high_temp+fan.wt_thresh) || (ac.mode == AC_MODE_HEAT && ac.water_temp <= ac.heat_low_temp-fan.wt_thresh)) {
					fan_set_mode(name,FAN_MODE_NONE)
					if (!fan.wt_warned) {
						log_warning("fan %s stopped due to water_temp out of range\n",name);
						fan.wt_warned = true;
					}
				}
			}
			break;

		// Keep running for <cooldown> seconds
		case FAN_STATE_COOLDOWN:
			dprintf(dlevel,"fan[%s]: time: %s, fan->time: %s\n", name, time(), fan.time);
			diff = time() - fan.time;
			dprintf(dlevel,"fan[%s]: diff: %s, cooldown: %s\n", name, diff, fan.cooldown);
			if (diff >= fan.cooldown) fan_off(name,fan);
			break;

		case FAN_STATE_RELEASE:
			pa_client_release(ac,"fan",name,fan.reserve);
			fan_set_state(name,FAN_STATE_STOPPED);
			fan.fan_state = fan.heat_state = fan.cool_state = "off";
			fan.mode = FAN_MODE_NONE;
			break;

		case FAN_STATE_ERROR:
			fan_force_stop(name);
			fan.enabled = false;
			fan.error = true;
			break;
		}
	}

	for(let name in pumps) {
		dprintf(dlevel,"name: %s\n", name);
		let pump = pumps[name];
//		dumpobj(pump);

		dprintf(dlevel,"pump[%s]: enabled: %d, refs: %d\n", name, pump.enabled, pump.refs);
		if ((pump.error || !pump.enabled || !pump.refs) && pump.state < PUMP_STATE_COOLDOWN) continue;

		dprintf(dlevel,"pump[%s]: state: %s\n", name, pump_statestr(pump.state));
		switch(pump.state) {
		case PUMP_STATE_STOPPED:
			dprintf(dlevel,"reserve: %d, prime: %s\n", pump.reserve, pump.primer);
			pump.temp_in = pump.last_temp_in = INVALID_TEMP
			pump.settled = false;
			if (pump.reserve) pump_set_state(name,PUMP_STATE_RESERVE);
			else if (pump.primer.length) pump_set_state(name,PUMP_STATE_START_PRIMER);
			else pump_set_state(name,PUMP_STATE_START);
			break;

		// Reserve power
		case PUMP_STATE_RESERVE:
			if (!pa_client_reserve(ac,"pump",name,pump.reserve,pump.priority))  {
				if (pump.primer.length) pump_set_state(name,PUMP_STATE_START_PRIMER);
				else pump_set_state(name,PUMP_STATE_START);
			}
			break;

		// Start the primer
		case PUMP_STATE_START_PRIMER:
			if (pump_start(pump.primer)) {
				error_set("pump",name,sprintf("pump %s: unable to start primer: %s", name, pump.primer));
				pump_set_state(name,PUMP_STATE_ERROR);
			} else {
				pump.time = time();
				pump_set_state(name,PUMP_STATE_WAIT_PRIMER);
			}

		// Wait <primer_timeout> seconds for the primer to change state to RUNNING
		case PUMP_STATE_WAIT_PRIMER:
			let primer_state = pump_get_state(pump.primer);
			dprintf(dlevel,"pump[%s]: primer_state: %s\n", name, pump_statestr(primer_state));
			if (primer_state == PUMP_STATE_RUNNING) {
				pump_set_state(name,PUMP_STATE_START);
			} else {
				dprintf(dlevel+2,"pump[%s]: time: %s, pump.time: %s\n", name, time(), pump.time);
				diff = time() - pump.time;
				dprintf(dlevel,"pump[%s]: diff: %s, primer_timeout: %s\n", name, diff, pump.primer_timeout);
				if (diff >= pump.primer_timeout) {
					error_set("pump",name,sprintf("pump %s: timeout waiting for pump %s", name, pump.primer));
					pump_set_state(name,PUMP_STATE_ERROR);
				}
			}
			break;

		// Start the pump
		case PUMP_STATE_START:
			if (pump_on(name,pump)) {
				error_set("pump",name,"unable to power on pump "+name);
				pump_set_state(name,PUMP_STATE_ERROR);
			} else {
				pump.time = time();
				pump_set_state(name,PUMP_STATE_WAIT_PUMP);
				pump.start_time = time();
			}
			break;

		// Wait <wait_time> seconds for the pump to start
		case PUMP_STATE_WAIT_PUMP:
			// just a simple timer no condition
			dprintf(dlevel,"pump[%s]: time: %s, pump->time: %s\n", name, time(), pump.time);
			diff = time() - pump.time;
			dprintf(dlevel,"pump[%s]: diff: %s, wait_time: %s\n", name, diff, pump.wait_time);
			if (diff >= pump.wait_time) {
				// Turn off primer if enabled
				if (pump.primer.length) pump_stop(pump.primer);
				if (pump.min_flow) {
					pump.time = 0;
					pump.flow_start_time = time();
					pump_set_state(name,PUMP_STATE_FLOW);
				} else if (pump.warmup) {
					pump.time = time();
					pump_set_state(name,PUMP_STATE_WARMUP);
				} else {
					pump_set_state(name,PUMP_STATE_RUNNING);
				}
			}
			break;

		// Ensure flow rate is stable for <flow_wait_time> seconds until <flow_timeout>
		case PUMP_STATE_FLOW:
			let ldlevel = 2;
			dprintf(ldlevel,"pump[%s]: min_flow: %s\n", name, pump.min_flow);
			if (pump.min_flow > 0) {
				// if flow rate is above min for flow_wait_time secs, flow is ok
				let flow_rate = get_sensor(pump.flow_sensor,false);
				dprintf(dlevel,"pump[%s]: flow_rate: %d, min_flow\n", name, flow_rate, pump.min_flow);
				if (flow_rate > pump.min_flow) {
					dprintf(ldlevel,"pump[%s]: time: %s, pump->time: %s\n", name, time(), pump.time);
					if (pump.time) {
						diff = time() - pump.time;
						dprintf(ldlevel,"pump[%s]: diff: %s, flow_wait_time: %d\n", name, diff, pump.flow_wait_time);
						if (diff >= pump.flow_wait_time) {
							if (pump.warmup) {
								pump.time = time();
								pump_set_state(name,PUMP_STATE_WARMUP);
							} else {
								pump_set_state(name,PUMP_STATE_RUNNING);
							}
						}
					} else {
						pump.time = time();
					}
				} else {
					// Reset the time
					pump.time = 0;

					// Overall timeout
					dprintf(ldlevel,"pump[%s]: time: %s, pump.flow_start_time: %s\n", name, time(), pump.flow_start_time);
					diff = time() - pump.flow_start_time;
					dprintf(ldlevel,"pump[%s]: diff: %s, flow_timeout: %d\n", name, diff, pump.flow_timeout);
					if (diff >= pump.flow_timeout) {
						error_set("pump",name,sprintf("pump %s: timeout waiting for flow", name));
						pump_set_state(name,PUMP_STATE_ERROR);
					}
				}
			} else {
				if (pump.warmup) {
					pump.time = time();
					pump_set_state(name,PUMP_STATE_WARMUP);
				} else {
					pump_set_state(name,PUMP_STATE_RUNNING);
				}
			}
			break;

		// Run for <warmup> seconds before proceeding to run state
		case PUMP_STATE_WARMUP:
			// We do not warmup or cooldown the primer
			if (pump_is_primer(name)) pump_set_state(name,PUMP_STATE_RUNNING);
			dprintf(dlevel,"pump[%s]: time: %s, pump->time: %s\n", name, time(), pump.time);
			diff = time() - pump.time;
			dprintf(dlevel,"pump[%s]: diff: %s, warmup: %s\n", name, diff, pump.warmup);
			if (diff >= pump.warmup) pump_set_state(name,PUMP_STATE_RUNNING);
			break;

		// Pump is running, ensure flow rate does not fall below <flow_min>
		case PUMP_STATE_RUNNING:
			dprintf(dlevel,"pump %s min_flow: %s\n", name, pump.min_flow);
			if (pump.min_flow > 0) {
				let flow_rate = get_sensor(pump.flow_sensor,false);
				dprintf(dlevel,"pump[%s]: flow_rate: %d, min_flow\n", name, flow_rate, pump.min_flow);
				if (flow_rate < pump.min_flow) {
					error_set("pump",name,sprintf("pump %s: flow rate is below min", name));
					pump_set_state(name,PUMP_STATE_ERROR);
				}
			}
			dprintf(dlevel,"temp_in_sensor: %s\n", pump.temp_in_sensor);
			if (pump.temp_in_sensor) {
				pump.temp_in = get_sensor(pump.temp_in_sensor,false);
				dprintf(dlevel,"pump[%s]: temp_in: %.1f\n", name, pump.temp_in);
				if (typeof(pump.temp_in) != 'undefined') {
					dprintf(dlevel,"pump[%s]: settled: %s\n", name, pump.settled);
                                        if (!pump.settled) {
                                                diff = time() - pump.start_time;
                                                dprintf(dlevel,"diff: %d\n", diff);
                                                if ((diff % 5) == 0) {
							dprintf(dlevel,"pump[%s]: temp_in: %.1f, last_temp_in: %.1f\n", name, pump.temp_in, pump.last_temp_in);
							if (pump.temp_in == pump.last_temp_in) pump.settled = true;
							else pump.last_temp_in = pump.temp_in;
                                                }
                                        }
				}
			}
			break;

		// Keep running for <cooldown> seconds
		case PUMP_STATE_COOLDOWN:
			dprintf(dlevel,"pump[%s]: time: %s, pump->time: %s\n", name, time(), pump.time);
			// We do not warmup or cooldown the primer
			if (pump_is_primer(name)) diff = pump.cooldown;
			else diff = time() - pump.time;
			dprintf(dlevel,"pump[%s]: diff: %s, cooldown: %s\n", name, diff, pump.cooldown);
			if (diff >= pump.cooldown) pump_off(name,pump);
			break;

		case PUMP_STATE_RELEASE:
			pa_client_release(ac,"pump",name,pump.reserve);
			pump_set_state(name,PUMP_STATE_STOPPED);
			break;

		case PUMP_STATE_ERROR:
			pump_force_stop(name);
			pump.enabled = false;
			pump.error = true;
			break;
		}
	}

	for(let name in units) {
		dprintf(dlevel,"name: %s\n", name);
		let unit = units[name];
//		dumpobj(unit);

		dprintf(dlevel,"unit[%s]: enabled: %d, refs: %d\n", name, unit.enabled, unit.refs);
		if ((unit.error || !unit.enabled || !unit.refs) && unit.state < UNIT_STATE_RELEASE) continue;

		dprintf(dlevel,"unit[%s]: mode: %s, state: %s\n", name, ac_modestr(unit.mode), unit_statestr(unit.state));
		switch(unit.state) {
		case UNIT_STATE_STOPPED:
			dprintf(dlevel,"unit[%s]: reserve: %d, pump: %s\n", name, unit.reserve, unit.pump);
			if (unit.reserve) unit_set_state(name,UNIT_STATE_RESERVE);
			else unit_set_state(name,UNIT_STATE_START_PUMP);
			break;

		// Reserve power
		case UNIT_STATE_RESERVE:
			dprintf(dlevel,"unit[%s]: charging: %s, charge_priority: %d, priority: %d\n", name, unit.charging, unit.charge_priority, unit.priority);
			let pri = unit.priority ? unit.priority : (unit.charging ? unit.charge_priority : 100);
			dprintf(dlevel,"pri: %d\n", pri);
			if (!pa_client_reserve(ac,"unit",name,unit.reserve,pri)) unit_set_state(name,UNIT_STATE_START_PUMP);
			break;

		// Start the pump
		case UNIT_STATE_START_PUMP:
			if (pump_start(unit.pump)) {
				error_set("unit",name,sprintf("unit %s: unable to start pump: %s", name, unit.pump));
				unit_set_state(name,UNIT_STATE_ERROR);
			} else {
				unit.time = time();
				unit_set_state(name,UNIT_STATE_WAIT_PUMP);
			}
			break;

		// Wait for pump to start
		case UNIT_STATE_WAIT_PUMP:
			pump_state = pump_get_state(unit.pump);
			dprintf(dlevel,"unit[%s]: pump_state: %s\n", name, pump_statestr(pump_state));
			if (pump_state == PUMP_STATE_RUNNING) {
				unit_set_state(name,UNIT_STATE_START);
			} else {
				dprintf(dlevel+2,"unit[%s]: time: %s, unit.time: %s\n", name, time(), unit.time);
				diff = time() - unit.time;
				dprintf(dlevel,"unit[%s]: diff: %s, pump_timeout: %s\n", name, diff, unit.pump_timeout);
				if (diff >= unit.pump_timeout) {
					error_set("unit",name,sprintf("unit %s: timeout waiting for pump %s", name, unit.pump));
					unit_set_state(name,UNIT_STATE_ERROR);

				// If we went through a few loops and pump is stuck in reserve start over
				} else if (diff > 3 && pump_state == PUMP_STATE_RESERVE) {
					unit_set_state(name,UNIT_STATE_RELEASE);
				}
			}
			break;

		case UNIT_STATE_START:
			if (unit_on(name,unit)) {
				unit_set_state(name,UNIT_STATE_ERROR);
			} else {
				unit_set_state(name,UNIT_STATE_RUNNING);
			}
			break;

		case UNIT_STATE_RUNNING:
			pump_state = pump_get_state(unit.pump);
			dprintf(dlevel,"unit[%s]: unit %s state: %s\n", name, unit.pump, pump_statestr(pump_state));
			if (pump_state != PUMP_STATE_RUNNING) {
				error_set("unit",name,sprintf("unit %s: required pump %s is not running",name,unit.pump));
				unit_set_state(name,UNIT_STATE_ERROR);
			}
			break;

		case UNIT_STATE_RELEASE:
			if (!pa_client_release(ac,"unit",name,unit.reserve)) unit_set_state(name,UNIT_STATE_STOPPED);
			break;

		case UNIT_STATE_ERROR:
			unit_force_stop(name);
			unit.enabled = false;
			unit.error = true;
			break;
		}
	}
}
