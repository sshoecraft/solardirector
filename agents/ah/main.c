
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include <fcntl.h>
#include "ah.h"
#include "__sd_build.h"

#define TESTING 0
#define TESTLVL 2

char *ah_version_string = STRINGIFY(__SD_BUILD);
bool ignore_wpi;

char *get_modestr(int mode) {
	char *str;

	switch(mode) {
	case AH_MODE_NONE:
		str = "None";
		break;
	case AH_MODE_COOL:
		str = "Cool";
		break;
	case AH_MODE_HEAT:
		str = "Heat";
		break;
	case AH_MODE_EHEAT:
		str = "Emergency Heat";
		break;
	default:
		str = "Unknown";
		break;
	}
	return str;
}

static void fan_on(ah_session_t *s) {
	dprintf(-1,"fan_control_pin: %d, fan_state: %d\n", s->fan_control_pin, s->fan_state);
	if (s->fan_control_pin >= 0) digitalWrite(s->fan_control_pin,HIGH);
	else if (s->sim_enable) s->sim_fan = 1;
}

static void fan_off(ah_session_t *s) {
	dprintf(-1,"fan_control_pin: %d, fan_state: %d\n", s->fan_control_pin, s->fan_state);
	if (s->fan_control_pin >= 0) digitalWrite(s->fan_control_pin,LOW);
	else if (s->sim_enable) s->sim_fan = 0;
}

static int ah_cb(void *ctx) {
	ah_session_t *s = ctx;

	s->fan_state = (s->fan_state_pin >= 0 ? digitalRead(s->fan_state_pin) : 0);
	s->cool_state = (s->cool_state_pin >= 0 ? digitalRead(s->cool_state_pin) : 0);
	s->heat_state = (s->heat_state_pin >= 0 ? digitalRead(s->heat_state_pin) : 0);

	if (s->sim_fan) s->fan_state = 1;
	if (s->sim_cool) s->cool_state = 1;
	if (s->sim_heat) s->heat_state = 1;
        return 0;
}

static int ah_start_fan(void *handle, list args, char *errmsg, json_object_t *results) {
	ah_session_t *s = handle;

	s->manual_fan_on = 1;
	fan_on((ah_session_t *)handle);
	return 0;
}

static int ah_stop_fan(void *handle, list args, char *errmsg, json_object_t *results) {
	ah_session_t *s = handle;

	s->manual_fan_on = 0;
	fan_off((ah_session_t *)handle);
	return 0;
}

int ah_agent_init(int argc, char **argv, opt_proctab_t *opts, ah_session_t *s) {
	config_property_t ah_props[] = {
		/* name, type, dest, dsize, def, flags, scope, values, labels, units, scale, precision, trigger, ctx */
		{ "fan_control_pin", DATA_TYPE_INT, &s->fan_control_pin, 0, "-1", 0, "range", "1,31,1", "fan control GPIO pin", 0, 0, 0 },
		{ "fan_state_pin", DATA_TYPE_INT, &s->fan_state_pin, 0, "-1", 0, "range", "1,31,1", "fan state GPIO pin", 0, 0, 0 },
		{ "cool_state_pin", DATA_TYPE_INT, &s->cool_state_pin, 0, "-1", 0, "range", "1,29,1", "cool state GPIO pin", 0, 0, 0 },
		{ "heat_state_pin", DATA_TYPE_INT, &s->heat_state_pin, 0, "-1", 0, "range", "1,29,1", "heat state GPIO pin", 0, 0, 0 },
		{ "count", DATA_TYPE_INT, &s->count, 0, "5", 0, "range", "1,100,1", "number of readings to average", 0, 0, 0 },
		{ "refres", DATA_TYPE_INT, &s->refres, 0, "10", 0, "range", "1,999999,1", "reference resistance", 0, 0, 0 },
		{ "reftemp", DATA_TYPE_INT, &s->reftemp, 0, "25", 0, "range", "1,999999,1", "reference temperature in C", 0, 0, 0 },
		{ "beta", DATA_TYPE_INT, &s->beta, 0, "3950", 0, "range", "1,999999,1", "beta value", 0, 0, 0 },
		{ "divres", DATA_TYPE_INT, &s->divres, 0, "10", 0, "range", "1,999999,1", "divider resistance", 0, 0, 0 },
		{ "air_in_ch", DATA_TYPE_INT, &s->air_in_ch, 0, "-1", 0, "select", "0,1,2,3", "air_in channel", 0, 0, 0 },
		{ "air_in", DATA_TYPE_FLOAT, &s->air_in, 0, 0, CONFIG_FLAG_READONLY },
		{ "air_out_ch", DATA_TYPE_INT, &s->air_out_ch, 0, "-1", 0, "select", "0,1,2,3", "air_out channel", 0, 0, 0 },
		{ "air_out", DATA_TYPE_FLOAT, &s->air_out, 0, 0, CONFIG_FLAG_READONLY },
		{ "water_in_ch", DATA_TYPE_INT, &s->water_in_ch, 0, "-1", 0, "select", "0,1,2,3", "water_in channel", 0, 0, 0 },
		{ "water_in", DATA_TYPE_FLOAT, &s->water_in, 0, 0, CONFIG_FLAG_READONLY },
		{ "water_out_ch", DATA_TYPE_INT, &s->water_out_ch, 0, "-1", 0, "select", "0,1,2,3", "water_out channel", 0, 0, 0 },
		{ "water_out", DATA_TYPE_FLOAT, &s->water_out, 0, 0, CONFIG_FLAG_READONLY },
		{ "sim_enable", DATA_TYPE_BOOL, &s->sim_enable, 0, "0", 0 },
		{ "sim_fan", DATA_TYPE_BOOL, &s->sim_fan, 0, "0", CONFIG_FLAG_NOSAVE },
		{ "sim_cool", DATA_TYPE_INT, &s->sim_cool, 0, "0", CONFIG_FLAG_NOSAVE },
		{ "sim_heat", DATA_TYPE_INT, &s->sim_heat, 0, "0", CONFIG_FLAG_NOSAVE },
		{ 0 }
	};
	config_function_t ah_funcs[] = {
		{ "fan_on", ah_start_fan, s, 0 },
		{ "fan_off", ah_stop_fan, s, 0 },
		{ 0 }
	};

	s->ap = agent_init(argc,argv,ah_version_string,opts,&ah_driver,s,0,ah_props,ah_funcs);
	if (!s->ap) return 1;

	/* Set pins to IN/OUT and get current value */
	s->manual_fan_on = 0;
	if (s->fan_control_pin >= 0) pinMode(s->fan_control_pin, OUTPUT);
	if (s->fan_state_pin >= 0) {
		pinMode(s->fan_state_pin, INPUT);
		s->fan_state = digitalRead(s->fan_state_pin);
	}
        if (s->cool_state_pin >= 0) {
		pinMode(s->cool_state_pin, INPUT);
		s->cool_state = digitalRead(s->cool_state_pin);
	}
        if (s->heat_state_pin >= 0) {
		pinMode(s->heat_state_pin, INPUT);
		s->heat_state = digitalRead(s->heat_state_pin);
	}
	if (s->fan_state_pin < 0 && s->fan_control_pin >= 0) {
		dprintf(dlevel,"NEW fan_state_pin: %d\n", s->fan_state_pin);
		s->fan_state_pin = s->fan_control_pin;
		s->fan_state = digitalRead(s->fan_state_pin);
	}

	dprintf(dlevel,"air_in_ch: %d\n", s->air_in_ch);
	dprintf(dlevel,"air_out_ch: %d\n", s->air_out_ch);
	dprintf(dlevel,"water_in_ch: %d\n", s->water_in_ch);
	dprintf(dlevel,"water_out_ch: %d\n", s->water_out_ch);
	return 0;
}

int main(int argc, char **argv) {
	ah_session_t *s;
#if TESTING
        char *args[] = { "template", "-d", STRINGIFY(TESTLVL), "-c", "test.json" };
        argc = (sizeof(args)/sizeof(char *));
        argv = args;
#endif
	bool norun;
	opt_proctab_t opts[] = {
		{ "-1|dont enter run loop",&norun,DATA_TYPE_BOOL,0,0,"no" },
		{ "-i|ignore wpi_init errors",&ignore_wpi,DATA_TYPE_BOOL,0,0,"no" },
		OPTS_END
	};

	s = ah_driver.new(0,0);
	if (!s) {
		perror("driver.new");
		return 1;
	}
	if (ah_agent_init(argc,argv,opts,s)) return 1;
	agent_set_callback(s->ap, ah_cb, s);

        /* Set inteval to 10s */
        if (s->ap && s->ap->interval != 10) {
                config_property_t *p = config_get_property(s->ap->cp,ah_driver.name,"interval");
                dprintf(dlevel,"p: %p\n", p);
                if (p) {
                        config_property_set_value(p,DATA_TYPE_STRING,"10",2,true,true);
//                      config_dump_property(p,0);
                        agent_pubconfig(s->ap);
                }
        }

	if (norun) ah_driver.read(s,0,0,0);
	else agent_run(s->ap);

	ah_driver.destroy(s);
	agent_shutdown();
#ifdef DEBUG_MEM
	log_info("final mem used: %ld\n", mem_used());
#endif

	return 0;
}
