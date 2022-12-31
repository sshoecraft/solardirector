
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "si.h"
#include "transports.h"

#define dlevel 1

static void *si_new(void *driver, void *driver_handle) {
	si_session_t *s;

	s = calloc(1,sizeof(*s));
	if (!s) {
		perror("si_new: calloc");
		return 0;
	}
	if (driver) {
		s->can = driver;
		s->can_handle = driver_handle;
	}
	s->running = -1;

	dprintf(dlevel,"returning: %p\n", s);
	return s;
}

static int si_destroy(void *h) {
	si_session_t *s = h;

	if (!s) return 1;

#ifdef JS
	if (s->props) free(s->props);
	if (s->funcs) free(s->funcs);
	if (s->data_props) free(s->data_props);
	if (s->ap && s->ap->js.e) {
		JSContext *cx = JS_EngineGetCX(s->ap->js.e);
		if (cx) {
			void *p = JS_GetPrivate(cx,s->can_obj);
			if (p) JS_free(cx,p);
		}
	}
#endif

        if (s->ap) agent_destroy(s->ap);

	/* Close and destroy transport */
	dprintf(dlevel,"s->can: %p, s->can_handle: %p\n", s->can, s->can_handle);
	if (s->can && s->can_handle) si_can_destroy(s);
	dprintf(dlevel,"s->smanet: %p\n", s->smanet);
        if (s->smanet) smanet_destroy(s->smanet);

	dprintf(dlevel,"input.source: %d, output.source: %d\n", s->input.source, s->output.source);
        if (s->input.source == CURRENT_SOURCE_INFLUX) free(s->input.query);
        if (s->output.source == CURRENT_SOURCE_INFLUX) free(s->output.query);

	dprintf(dlevel,"freeing session!\n");

        free(s);
	return 0;
}

static int si_open(void *h) {
	si_session_t *s = h;
	int r;

	dprintf(dlevel,"s: %p\n", s);
	if (!s->can || !s->can_handle) return 1;

	r = 0;
	if (!check_state(s,SI_STATE_OPEN)) {
		if (s->can->open(s->can_handle) == 0)
			set_state(s,SI_STATE_OPEN);
		else {
			log_error("error opening %s device %s:%s\n", s->can_transport, s->can_target, s->can_topts);
			r = 1;
		}
	}
	dprintf(dlevel,"returning: %d\n", r);
	return r;
}

static int si_close(void *handle) {
	si_session_t *s = handle;

	dprintf(dlevel,"s: %p\n", s);
	if (!s->can || !s->can_handle) return 1;

	dprintf(dlevel,"s: %p\n", s);
	if (check_state(s,SI_STATE_OPEN) && s->can->close(s->can_handle) == 0) clear_state(s,SI_STATE_OPEN);
	return 0;
}

#ifdef INFLUX
static double _get_influx_value(influx_session_t *s, char *query) {
	influx_series_t *sp;
	influx_value_t *vp;
	double value;
	list results;

	if (influx_query(s, query)) return 0;
	results = influx_get_results(s);
	dprintf(dlevel,"results: %p\n", results);
	if (!results) return 0;
	dprintf(dlevel,"results count: %d\n", list_count(results));
	list_reset(results);
	sp = list_get_next(results);
	if (!sp) return 0;
	dprintf(dlevel,"sp->values: %p\n", sp->values);
	if (!sp->values) return 0;
	vp = &sp->values[0][1];
	conv_type(DATA_TYPE_FLOAT,&value,0,vp->type,vp->data,vp->len);
	dprintf(dlevel,"value: %f\n", value);
	return value;
}
#endif

static int si_read(void *handle, uint32_t *control, void *buf, int buflen) {
	si_session_t *s = handle;

	dprintf(dlevel,"disable_si_read: %d\n", s->disable_si_read);
	if (s->disable_si_read) return 0;

	dprintf(dlevel,"can: %p, can_transport: %s, can_target: %s\n", s->can, s->can_transport, s->can_target);
	if (!s->can && strlen(s->can_transport) && strlen(s->can_target)) si_can_init(s);
	if (s->can) {
		dprintf(dlevel,"can_connected: %d\n", s->can_connected);
		if (!s->can_connected) si_can_connect(s);
		dprintf(dlevel,"can_connected: %d\n", s->can_connected);
		if (s->can_connected) {
			if (si_can_read_data(s,0)) si_can_disconnect(s);
		}
	}

#ifdef SMANET
	/* Do this after at least 1 loop if can connected */
	dprintf(dlevel,"run_count: %d, can_connected: %d\n", s->ap->run_count, s->can_connected);
	if (s->ap->run_count || !s->can_connected) {
		dprintf(dlevel,"smanet: %p, smanet_connected: %d\n", s->smanet, (s->smanet ? smanet_connected(s->smanet) : 0));
		if (!s->smanet) s->smanet = smanet_init();
		else if (!smanet_connected(s->smanet)) si_smanet_connect(s);
	}
	dprintf(dlevel,"smanet_connected: %d\n", smanet_connected(s->smanet));
	if (s->smanet && smanet_connected(s->smanet)) {
		if (si_smanet_read_data(s)) si_smanet_disconnect(s);
	}
#endif

	// Clear data if no connection
	if (!s->can_connected && !smanet_connected(s->smanet)) memset(&s->data,0,sizeof(s->data));

#ifdef INFLUX
	dprintf(dlevel,"input.source: %d\n", s->input.source);
	if (s->input.source == CURRENT_SOURCE_INFLUX) {
		double value = _get_influx_value(s->ap->i,s->input.query);
		if (s->input.type == CURRENT_TYPE_WATTS) {
			s->data.ac2_power = value;
			s->data.ac2_current = s->data.ac2_power / s->data.ac2_voltage_l1;
		} else {
			s->data.ac2_current = value;
			s->data.ac2_power = s->data.ac2_current * s->data.ac2_voltage_l1;
		}
		dprintf(dlevel,"ac2_current: %.1f, ac2_power: %.1f\n", s->data.ac2_current, s->data.ac2_power);
	}

	dprintf(dlevel,"output.source: %d\n", s->output.source);
	if (s->output.source == CURRENT_SOURCE_INFLUX) {
		double value = _get_influx_value(s->ap->i,s->output.query);
		if (s->output.type == CURRENT_TYPE_WATTS) {
			s->data.ac1_power = value;
			s->data.ac1_current = s->data.ac1_power / s->data.ac1_voltage_l1;
		} else {
			s->data.ac1_current = value;
			s->data.ac1_power = s->data.ac1_current * s->data.ac1_voltage_l1;
		}
		dprintf(dlevel,"ac1_current: %.1f, ac1_power: %.1f\n", s->data.ac1_current, s->data.ac1_power);
	}
#endif

#if 0
	/* make sure min/max are set to _something_ */
	dprintf(dlevel,"min_voltage: %.1f, max_voltage: %.1f\n", s->min_voltage, s->max_voltage);
	if (double_equals(s->min_voltage,0.0)) {
		log_warning("min_voltage is 0.0, setting to 41.0\n", 41);
		s->min_voltage = 41;
	}
	if (s->max_voltage <= s->min_voltage || double_equals(s->max_voltage,0.0)) {
		log_warning("max_voltage is 0.0, setting to 58.1\n", 41);
		s->max_voltage = 58.1;
	}
#endif

	/* Calc SoC (linear) */
	dprintf(dlevel,"user_soc: %.1f\n", s->user_soc);
	if (s->user_soc >= 0) {
		s->soc = s->user_soc;
	} else if (!double_equals(s->min_voltage,0.0) && !double_equals(s->max_voltage,0.0)) {
		dprintf(dlevel,"battery_voltage: %.1f, min_voltage: %.1f, max_voltage: %.1f\n",
			s->data.battery_voltage, s->min_voltage, s->max_voltage);
		s->soc = ( ( s->data.battery_voltage - s->min_voltage) / (s->max_voltage - s->min_voltage) ) * 100.0;
	}
	if (s->soc < 0) s->soc = 0;
	dprintf(dlevel,"SoC: %f\n", s->soc);
	s->soh = 100.0;

	// Must set before every write
	s->force_charge_amps = 0;
	return 0;
}

#if 0
#if defined(MQTT) || defined(INFLUX)
static void si_pub(si_session_t *s) {
	char data[4096],out[192],status[128];
	double charge_power;
	json_proctab_t tab[] = {
		{ "name",DATA_TYPE_STRING,s->ap->instance_name,sizeof(s->ap->instance_name)-1,0 },
		{ "battery_current",DATA_TYPE_FLOAT,&s->data.battery_current,0,0 }, 
		{ "battery_level",DATA_TYPE_FLOAT,&s->data.battery_soc,0,0 }, 
		{ "battery_power",DATA_TYPE_FLOAT,&s->data.battery_power,0,0 }, 
		{ "battery_temp",DATA_TYPE_FLOAT,&s->data.battery_temp,0,0 }, 
		{ "battery_voltage",DATA_TYPE_FLOAT,&s->data.battery_voltage,0,0 }, 
		{ "charge_mode",DATA_TYPE_INT,&s->charge_mode,0,0 }, 
		{ "charge_current",DATA_TYPE_FLOAT,&s->charge_amps,0,0 }, 
		{ "charge_power",DATA_TYPE_FLOAT,&charge_power,0,0 }, 
		{ "charge_voltage",DATA_TYPE_FLOAT,&s->charge_voltage,0,0 }, 
		{ "input_current",DATA_TYPE_FLOAT,&s->data.ac2_current,0,0 }, 
		{ "input_frequency",DATA_TYPE_FLOAT,&s->data.ac2_frequency,0,0 }, 
		{ "input_power",DATA_TYPE_FLOAT,&s->data.ac2_power,0,0 }, 
		{ "input_voltage",DATA_TYPE_FLOAT,&s->data.ac2_voltage,0,0 }, 
		{ "load_power",DATA_TYPE_FLOAT,&s->data.TotLodPwr,0,0 }, 
		{ "output_current",DATA_TYPE_FLOAT,&s->data.ac1_current,0,0 }, 
		{ "output_frequency",DATA_TYPE_FLOAT,&s->data.ac1_frequency,0,0 }, 
		{ "output_power",DATA_TYPE_FLOAT,&s->data.ac1_power,0,0 }, 
		{ "output_voltage",DATA_TYPE_FLOAT,&s->data.ac1_voltage,0,0 }, 
		{ "status",DATA_TYPE_STRING,&status,sizeof(status),0 }, 
		{ 0 }
	};
	json_value_t *v;
	char *p;
	char topic[SOLARD_TOPIC_SIZE];
#ifdef JS
	bool feed_enabled,dynfeed;
	JSContext *cx;
	JSBool ok;
	jsval val;

	cx = JS_EngineGetCX(s->ap->js.e);
	if (cx) {
//		if (js_get_property_value(cx,s->si_obj,"feed_enabled",DATA_TYPE_BOOL,&feed_enabled,0)) feed_enabled = false;
		ok = JS_GetProperty(cx, s->si_obj, "feed_enabled", &val);
		dprintf(dlevel,"ok: %d\n", ok);
		if (ok) jsval_to_type(DATA_TYPE_BOOL,&feed_enabled,0,cx,val);
		else feed_enabled = false;
		dprintf(dlevel,"feed_enabled: %d\n", feed_enabled);

		ok = JS_GetProperty(cx, s->si_obj, "dynfeed", &val);
		dprintf(dlevel,"ok: %d\n", ok);
		if (ok) jsval_to_type(DATA_TYPE_BOOL,&dynfeed,0,cx,val);
		else dynfeed = false;
		dprintf(dlevel,"dynfeed: %d\n", dynfeed);
	}
#endif

	p = status;
	*status = 0;
	#define addstat(stat,cond,str) if (cond) p += sprintf(p,"[%s]",str)
	addstat(status,s->can_connected,"can");
	addstat(status,s->readonly,"readonly");
	addstat(status,s->smanet_connected,"smanet");
	addstat(status,s->data.GdOn,"grid");
	addstat(status,s->data.GnOn,"gen");
	addstat(status,s->charge_mode,"charge");
        addstat(status,s->charge_mode == 1,"CC");
        addstat(status,s->charge_mode == 2,"CV");
#ifdef JS
        addstat(status,feed_enabled,"feed");
        addstat(status,(s->input.source != CURRENT_SOURCE_NONE && feed_enabled && s->charge_mode && dynfeed && s->data.GdOn),"dynfeed");
#endif

	dprintf(dlevel,"status(%d): %s\n", strlen(status), status);

	p = out;
	p += sprintf(p,"%s Battery: voltage: %.1f",status,s->data.battery_voltage);
//	if (check_state(s,SI_HAVE_TEMP)) p += sprintf(p,", temp: %.1f", s->data.battery_temp);
	p += sprintf(p,", level: %.1f", s->data.battery_soc);
	if (strcmp(out,s->last_out) != 0) {
		log_info("%s\n",out);
		strcpy(s->last_out,out);
	}

	charge_power = (s->charge_mode ? s->charge_voltage * s->charge_amps : 0.0);
	v = json_from_tab(tab);
#ifdef MQTT
//	if (json_string_from_tab(data,sizeof(data)-1,tab,1) > 0) {
	if (!json_tostring(v,data,sizeof(data),1)) {
		dprintf(dlevel,"data: %s\n", data);
		agent_mktopic(topic,sizeof(topic)-1,s->ap->instance_name,SOLARD_FUNC_DATA);
		dprintf(dlevel,"topic: %s\n", topic);
		if (mqtt_pub(s->ap->m, topic, data, 1, 0)) log_error("si_pub: error sending mqtt message!\n");
	}
#endif
#ifdef INFLUX
	if (influx_connected(s->ap->i)) influx_write_json(s->ap->i,"inverter",v);
#endif
	json_destroy_value(v);
	return;
}
#endif
#endif

static int si_write(void *handle, uint32_t *control, void *buffer, int len) {
	si_session_t *s = handle;

	dprintf(dlevel,"disable_si_write: %d\n", s->disable_si_write);
	if (s->disable_si_write) return 0;

	dprintf(dlevel,"can_connected: %d\n", s->can_connected);
	if (s->can_connected && si_can_write_data(s)) {
		si_can_disconnect(s);
		return 1;
	}

#ifdef JS
//	if (agent_script_exists(s->ap,"pub.js")) agent_start_script(s->ap,"pub.js");
	agent_start_script(s->ap,"pub.js");
//	else
#endif
#if defined(MQTT) || defined(INFLUX)
//	si_pub(s);
#endif
	return 0;
}

solard_driver_t si_driver = {
	"si",
	si_new,		/* New */
	si_destroy,	/* Free */
	si_open,	/* Open */
	si_close,	/* Close */
	si_read,	/* Read */
	si_write,	/* Write */
	si_config,	/* Config */
};
