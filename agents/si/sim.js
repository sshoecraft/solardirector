
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

function sim_init() {
	var flags = CONFIG_FLAG_NOPUB|CONFIG_FLAG_NOSAVE;
	var config = si.agent.config;
	var sim = new Class("sim",config,flags);
	config.add("sim","enable",DATA_TYPE_BOOL,"false",flags);
	config.add("sim","step_up",DATA_TYPE_FLOAT,"2.4",flags);
	config.add("sim","step_down",DATA_TYPE_FLOAT,"0.8",flags);
	config.add("sim","ba_start",DATA_TYPE_FLOAT,"-100.0",flags);
	config.add("sim","ba_step",DATA_TYPE_FLOAT,"10.0",flags);
	config.add("sim","cv_time_step",DATA_TYPE_INT,"1800",flags);
	sim.volts = si.charge_start_voltage;
	sim.amps = -50;
	sim.grid_connected = false;
	sim.gen_connected = false;
//	si.agent.read_interval = si.agent.write_interval = 5;
}

function sim_main() {

//	printf("==> sim_main\n");

	if (sim.ba_amps >= 0) sim.ba_amps = -100;
	if (si.charge_mode == 0) {
		dprintf(0,"battery_voltage: %.1f, sim.volts: %.1f, step_down: %.1f\n",
			si.data.battery_voltage, sim.volts, sim.step_down);
		si.data.battery_voltage = (sim.volts -= sim.step_down);
	} else if (si.charge_mode == 1) {
		dprintf(0,"battery_voltage: %.1f, sim.volts: %.1f, step_up: %.1f\n",
			si.data.battery_voltage, sim.volts, sim.step_up);
		si.data.battery_voltage = (sim.volts += sim.step_up);
	} else if (si.charge_mode == 2) {
		if (sim.previous_mode != si.charge_mode) sim.ba_amps = sim.ba_start;
		dprintf(0,"cv_method: %d\n", si.cv_method);
		if (si.cv_method == 0) {
			dprintf(0,"cv_start_time: %d, cv_step: %d\n", si.cv_start_time, sim.cv_step);
			si.cv_start_time -= sim.cv_step;
		} else if (si.cv_method == 1) {
 			dprintf(0,"ba_amps: %.1f, ba_step: %.1f\n", sim.ba_amps, sim.ba_step);
			si.data.battery_current = (sim.ba_amps += sim.ba_step);
			if (si.data.battery_current >= 0) si.data.battery_current = 0 - sim.ba_step;
		}
	}
	if (si.data.battery_voltage > si.max_voltage) si.data.battery_voltage = si.max_voltage;
	sim.previous_mode = si.charge_mode;

	data.GdOn = sim.grid_connected;
	data.GnOn = sim.gen_connected;
}
