
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "si.h"
#include "transports.h"

extern char *si_version_string;
extern solard_driver_t si_driver;

static int set_readonly(void *ctx, config_property_t *p, void *old_value) {
	si_session_t *s = ctx;

	/* Also set smanet readonly */
	dprintf(dlevel,"readonly: %d, smanet: %p\n", s->readonly, s->smanet);
	if (s->smanet) smanet_set_readonly(s->smanet,s->readonly);
	if (!s->readonly) s->readonly_warn = false;
	return 0;
}

static void _getsource(si_session_t *s, si_current_source_t *spec) {
	char line[128],method[64],*p,*v;
	int i;

	dprintf(dlevel,"spec->text: %s\n", spec->text);

	/* method,interval,<method-specific> */
	i = 0;
	strncpy(method,strele(i++,",",spec->text),sizeof(method)-1);
	v = line;
	v += sprintf(v,"spec: text: %s, ", spec->text);
	if (strcasecmp(method,"none") == 0) {
		spec->source = CURRENT_SOURCE_NONE;
	} else if (strcasecmp(method,"calculated") == 0) {
		spec->source = CURRENT_SOURCE_CALCULATED;
	} else if (strcasecmp(method,"influx") == 0) {
		spec->source = CURRENT_SOURCE_INFLUX;
		/* method-specific is influx query */
		p = strele(i++,",",spec->text);
		spec->query = malloc(strlen(p)+1);
		if (!spec->query) {
			log_syserror("_getsource: malloc query");
			spec->source = CURRENT_SOURCE_NONE;
			return;
		}
		strcpy(spec->query,p);
	} else if (strcasecmp(method,"can") == 0) {
		/* can,can_id,byte offset,number of bytes */
		spec->source = CURRENT_SOURCE_CAN;
		v += sprintf(v,"source: %d, ", spec->source);
		p = strele(i++,",",spec->text);
		conv_type(DATA_TYPE_U16,&spec->can.id,0,DATA_TYPE_STRING,p,strlen(p));
		p = strele(i++,",",spec->text);
		conv_type(DATA_TYPE_U8,&spec->can.offset,0,DATA_TYPE_STRING,p,strlen(p));
		p = strele(i++,",",spec->text);
		conv_type(DATA_TYPE_U8,&spec->can.size,0,DATA_TYPE_STRING,p,strlen(p));
		v += sprintf(v,"id: %03x, offset: %d, size: %d, ", spec->can.id, spec->can.offset, spec->can.size);
	// XXX SMANET only returns unsigned data ??
	} else if (strcasecmp(method,"smanet") == 0) {
		/* smanet,<name> */
		spec->source = CURRENT_SOURCE_SMANET;
		p = strele(i++,",",spec->text);
		strncpy(spec->name,p,sizeof(spec->name)-1);
		v += sprintf(v,"source: %d, name: %s, ", spec->source, spec->name);
	} else {
		log_warning("invalid current source: %s, ignored.\n", s->input.text);
	}
	/* type, multiplier */
	p = strele(i++,",",spec->text);
	if (*p == 'W' || *p == 'w')
		spec->type = CURRENT_TYPE_WATTS;
	else
		spec->type = CURRENT_TYPE_AMPS;
	p = strele(i++,",",spec->text);
	if (strlen(p)) conv_type(DATA_TYPE_DOUBLE,&spec->mult,0,DATA_TYPE_STRING,p,strlen(p));
	else spec->mult = 1.0;
	v += sprintf(v,"type: %d, mult: %f", spec->type, spec->mult);
	dprintf(dlevel,"%s\n", line);
}

static int si_get_value(void *ctx, list args, char *errmsg, json_object_t *results) {
	si_session_t *s = ctx;
	config_arg_t *arg;
	char *key;
//	char *prefix;
	char prefix[66];
	char sname[64], name[128];
	config_property_t *p;
	json_value_t *v;

	dprintf(dlevel,"args count: %d\n", list_count(args));
	list_reset(args);
	while((arg = list_get_next(args)) != 0) {
		key = arg->argv[0];
		dprintf(dlevel,"key: %s\n", key);

		/* handle special cases of info/config */
		if (strcmp(key,"info") == 0) {
			cf_agent_getinfo(s->ap,args,errmsg,results);
			continue;
		} else if (strcmp(key,"config") == 0) {
			cf_agent_getconfig(s->ap,args,errmsg,results);
			continue;
		}

		*prefix = 0;
		*sname = *name = 0;
		if (strchr(key,'.')) {
			strncpy(sname,strele(0,".",key),sizeof(sname)-1);
			snprintf(prefix,sizeof(prefix)-1,"%s.",sname);
			strncpy(name,strele(1,".",key),sizeof(name)-1);
		}
		if (!strlen(sname)) strncpy(sname,s->ap->section_name,sizeof(sname)-1);
		if (!strlen(name)) strncpy(name,key,sizeof(name)-1);
		dprintf(dlevel,"sname: %s, name: %s\n", sname, name);
		p = config_get_property(s->ap->cp, sname, name);
		dprintf(dlevel,"p: %p\n", p);
		if (!p) {
			sprintf(errmsg,"property %s not found",name);
			return 1;
		}

#ifdef SMANET
		dprintf(dlevel,"name: %s, flags: %04x, SI_CONFIG_FLAG_SMANET: %x, val: %d\n", p->name,
			p->flags, SI_CONFIG_FLAG_SMANET, check_bit(p->flags,SI_CONFIG_FLAG_SMANET));
		/* If someone put it in the config file and its SMANET */
		if (!check_bit(p->flags,SI_CONFIG_FLAG_SMANET)) {
			smanet_channel_t *c;

			dprintf(dlevel,"checking if SMANET...\n");
			c = smanet_get_channel(s->smanet,p->name);
			if (c) {
				clear_bit(p->flags,CONFIG_FLAG_NOPUB);
				set_bit(p->flags,SI_CONFIG_FLAG_SMANET);
			}
		}
		dprintf(dlevel,"name: %s, flags: %04x, SI_CONFIG_FLAG_SMANET: %x, val: %d\n", p->name,
			p->flags, SI_CONFIG_FLAG_SMANET, check_bit(p->flags,SI_CONFIG_FLAG_SMANET));
		if (check_bit(p->flags,SI_CONFIG_FLAG_SMANET)) {
			char *text;
			double d;

			if (!s->smanet || !smanet_connected(s->smanet)) return 1;
			if (smanet_get_value(s->smanet, p->name, &d, &text)) return 1;
			dprintf(dlevel,"p->type: %d(%s), p->flags: %x\n", p->type, typestr(p->type), p->flags);
			if (text) config_property_set_value(p,DATA_TYPE_STRING,text,strlen(text),true,true);
			else config_property_set_value(p,DATA_TYPE_DOUBLE,&d,sizeof(d),true,true);
		}
#endif
		v = json_from_type(p->type,p->dest,p->len);
		dprintf(dlevel,"v: %p\n", v);
		if (v) {
			char rname[128];

			if (strcmp(sname,s->ap->section_name) == 0)
				snprintf(rname,sizeof(rname)-1,"%s",p->name);
			else
				snprintf(rname,sizeof(rname)-1,"%s.%s",sname,p->name);
			dprintf(dlevel,"rname: %s\n", rname);
			json_object_set_value(results,rname,v);
		}
	}
	dprintf(dlevel,"returning: 0\n");
	return 0;
}

static int si_set_value(void *ctx, list args, char *errmsg, json_object_t *results) {
	si_session_t *s = ctx;
	config_arg_t *arg;
	char *name, *value;
	config_property_t *p;

	dprintf(dlevel,"args: %p\n", args);
	dprintf(dlevel,"args count: %d\n", list_count(args));
	list_reset(args);
	while((arg = list_get_next(args)) != 0) {
		name = arg->argv[0];
		value = arg->argv[1];
		dprintf(dlevel,"name: %s, value: %s\n", name, value);
		p = config_find_property(s->ap->cp, name);
		dprintf(dlevel,"p: %p\n", p);
		if (!p) {
			sprintf(errmsg,"property %s not found",name);
			return 1;
		}
		if (strcasecmp(value,"default")==0) {
			if (!p->def) {
				sprintf(errmsg,"property %s has no default",name);
				return 1;
			}
			value = p->def;
		}
#ifdef SMANET
		if (!check_bit(p->flags,SI_CONFIG_FLAG_SMANET)) {
			smanet_channel_t *c;

			c = smanet_get_channel(s->smanet,p->name);
			dprintf(dlevel,"c: %p\n", c);
			if (c) {
				clear_bit(p->flags,CONFIG_FLAG_NOPUB);
				set_bit(p->flags,SI_CONFIG_FLAG_SMANET);
			}
		}
		if (check_bit(p->flags,SI_CONFIG_FLAG_SMANET)) {
			if (!s->smanet || !smanet_connected(s->smanet)) return 1;
			dprintf(dlevel,"p->type: %d(%s)\n", p->type, typestr(p->type));
			if (p->type == DATA_TYPE_STRING) {
				if (smanet_set_value(s->smanet, p->name, 0, value)) return 1;
			} else {
				double d;

				conv_type(DATA_TYPE_DOUBLE,&d,sizeof(d),DATA_TYPE_STRING,value,strlen(value));
				if (smanet_set_value(s->smanet, p->name, d, 0)) return 1;
			}
		}
#endif
		log_info("siconfig: Setting %s.%s to %s\n", (p->sp ? p->sp->name : ""), p->name, value);
		if (config_property_set_value(p,DATA_TYPE_STRING,value,strlen(value)+1,true,true)) return 1;
		config_write(s->ap->cp);
		agent_pubconfig(s->ap);
	}
	return 0;
}

static int si_set_can_info(void *ctx, config_property_t *p, void *old_value) {
	si_session_t *s = ctx;
	char *infop = p->dest;

	dprintf(dlevel,"p: %p\n", p);

	dprintf(dlevel,"can_info: %s\n", infop);
	if (s->can) si_can_disconnect(s);
	*s->can_transport = *s->can_target = *s->can_topts = 0;
	strncat(s->can_transport,strele(0,",",infop),sizeof(s->can_transport)-1);
	strncat(s->can_target,strele(1,",",infop),sizeof(s->can_target)-1);
	strncat(s->can_topts,strele(2,",",infop),sizeof(s->can_topts)-1);
	dprintf(dlevel,"can: transport: %s, target: %s, topts: %s\n", s->can_transport, s->can_target, s->can_topts);
	return 0;
}

static int si_set_smanet_info(void *ctx, config_property_t *p, void *old_value) {
	si_session_t *s = ctx;
	char *infop = p->dest;

	dprintf(dlevel,"p: %p\n", p);

	dprintf(dlevel,"smanet_info: %s\n", infop);
	*s->smanet_transport = *s->smanet_target = *s->smanet_topts = 0;
	strncat(s->smanet_transport,strele(0,",",infop),sizeof(s->smanet_transport)-1);
	strncat(s->smanet_target,strele(1,",",infop),sizeof(s->smanet_target)-1);
	strncat(s->smanet_topts,strele(2,",",infop),sizeof(s->smanet_topts)-1);
	dprintf(dlevel,"smanet: transport: %s, target: %s, topts: %s\n", s->smanet_transport, s->smanet_target, s->smanet_topts);
	if (s->smanet && (!strlen(s->smanet_transport) || !strlen(s->smanet_target))) smanet_disconnect(s->smanet);
	return 0;
}

static int si_charge(void *ctx, list args, char *errmsg, json_object_t *results) {
#ifdef JS
	si_session_t *s = ctx;
	jsval jstrue = BOOLEAN_TO_JSVAL(JS_TRUE);
#endif
	config_arg_t *arg;
	char *action;

	/* We take 1 arg: start/stop */
	arg = list_get_first(args);
	action = arg->argv[0];
//	dprintf(dlevel,"action: %s\n", action);
	*s->errmsg = 0;
#ifdef JS
	// XXX using jsexec works and works well - except it wont reload the script file if its modified
	if (strcmp(action,"start") == 0 || strcasecmp(action,"on") == 0) {
//		agent_jsexec(s->ap, "charge_start(true);");
		agent_start_jsfunc(s->ap, "charge.js", "charge_start", 1, &jstrue);
	} else if (strcmp(action,"startcv") == 0 || strcasecmp(action,"cv") == 0) {
//		agent_jsexec(s->ap, "charge_start_cv(true)");
		agent_start_jsfunc(s->ap, "charge.js", "charge_start_cv", 1, &jstrue);
	} else if (strcmp(action,"stop") == 0 || strcasecmp(action,"off") == 0) {
//		agent_jsexec(s->ap, "charge_stop(true)");
		agent_start_jsfunc(s->ap, "charge.js", "charge_stop", 1, &jstrue);
	} else if (strcmp(action,"end") == 0) {
//		agent_jsexec(s->ap, "charge_end()");
		agent_start_jsfunc(s->ap, "charge.js", "charge_end", 1, &jstrue);
	} else
#endif
	{
		sprintf(s->errmsg,"invalid charge mode: %s", action);
	}
	strcpy(errmsg,s->errmsg);
	return (*errmsg != 0);
}

static int si_grid(void *ctx, list args, char *errmsg, json_object_t *results) {
#ifdef JS
	si_session_t *s = ctx;
	jsval jstrue = BOOLEAN_TO_JSVAL(JS_TRUE);
#endif
	config_arg_t *arg;
	char *action;

	/* We take 1 arg: start/stop */
	arg = list_get_first(args);
	action = arg->argv[0];
	dprintf(2,"action: %s\n", action);
	*s->errmsg = 0;
#ifdef JS
	if (strcasecmp(action,"start") == 0 || strcasecmp(action,"on") == 0) {
//		agent_jsexec(s->ap, "grid_start(true)");
		agent_start_jsfunc(s->ap, "grid.js", "grid_start", 1, &jstrue);
	} else if (strcasecmp(action,"stop") == 0 || strcasecmp(action,"off") == 0) {
//		agent_jsexec(s->ap, "grid_stop(true)");
		agent_start_jsfunc(s->ap, "grid.js", "grid_stop", 1, &jstrue);
	} else
#endif
	{
		sprintf(s->errmsg,"invalid grid mode: %s", action);
	}
	strcpy(errmsg,s->errmsg);
	return (*errmsg != 0);
}

static int si_feed(void *ctx, list args, char *errmsg, json_object_t *results) {
#ifdef JS
	si_session_t *s = ctx;
	jsval jstrue = BOOLEAN_TO_JSVAL(JS_TRUE);
#endif
	config_arg_t *arg;
	char *action;

	/* We take 1 arg: start/stop */
	arg = list_get_first(args);
	action = arg->argv[0];
	dprintf(2,"action: %s\n", action);
	*s->errmsg = 0;
#ifdef JS
	if (strcasecmp(action,"start") == 0 || strcasecmp(action,"on") == 0) {
//		agent_jsexec(s->ap, "feed_start(true)");
		agent_start_jsfunc(s->ap, "feed.js", "feed_start", 1, &jstrue);
	} else if (strcasecmp(action,"stop") == 0 || strcasecmp(action,"off") == 0) {
//		agent_jsexec(s->ap, "feed_stop(true)");
		agent_start_jsfunc(s->ap, "feed.js", "feed_stop", 1, &jstrue);
	} else
#endif
	{
		sprintf(s->errmsg,"invalid feed mode: %s", action);
	}
	strcpy(errmsg,s->errmsg);
	return (*errmsg != 0);
}

/* smanet_channels_path trigger */
static int smanet_chanpath_set(void *ctx, config_property_t *p, void *old_value) {
	si_session_t *s = ctx;

	return si_smanet_load_channels(s);
}

static int set_input_current_source(void *ctx, config_property_t *p, void *old_value) {
	si_session_t *s = ctx;

	dprintf(dlevel,"input->text: %s\n", s->input.text);

	memset(&s->input.name,0,sizeof(s->input.name));
	if (strlen(s->input.text)) _getsource(s,&s->input);
	return 0;
}

static int set_output_current_source(void *ctx, config_property_t *p, void *old_value) {
	si_session_t *s = ctx;

	dprintf(dlevel,"output->text: %s\n", s->output.text);

	memset(&s->output.name,0,sizeof(s->output.name));
	if (strlen(s->output.text)) _getsource(s,&s->output);
	return 0;
}

static int set_battery_temp_source(void *ctx, config_property_t *p, void *old_value) {
	si_session_t *s = ctx;

	dprintf(dlevel,"temp->text: %s\n", s->temp.text);

	memset(&s->temp.name,0,sizeof(s->temp.name));
	if (strlen(s->temp.text)) _getsource(s,&s->temp);
	return 0;
}

static int set_solar_output_source(void *ctx, config_property_t *p, void *old_value) {
	si_session_t *s = ctx;

	dprintf(dlevel,"solar->text: %s\n", s->solar.text);

	memset(&s->solar.name,0,sizeof(s->solar.name));
	if (strlen(s->solar.text)) _getsource(s,&s->solar);
	return 0;
}

int si_agent_init(int argc, char **argv, opt_proctab_t *si_opts, si_session_t *s) {
	config_property_t si_props[] = {
		/* name, type, dest, dsize, def, flags, scope, values, labels, units, scale, precision, trigger, ctx */
		{ "readonly", DATA_TYPE_BOOL, &s->readonly, 0, "yes", 0, 0, 0, 0, 0, 0, 1, set_readonly, s },
		{ "charge_mode", DATA_TYPE_INT, &s->charge_mode, 0, "0", 0, "select", "0, 1, 2", "Off,CC,CV", 0, 0, 0 },
		{ "charge_voltage", DATA_TYPE_DOUBLE, &s->charge_voltage, 0, 0, CONFIG_FLAG_PRIVATE },
		{ "charge_amps", DATA_TYPE_DOUBLE, &s->charge_amps, 0, 0, CONFIG_FLAG_PRIVATE, },
		{ "zero_ca", DATA_TYPE_BOOL, &s->zero_ca, 0, "yes", 0, 0, 0, 0, 0, 0, 1 },
		{ "startup_soc", DATA_TYPE_BOOL, &s->startup_soc, 0, "no", 0, 0, 0, 0, 0, 0, 1 },
		{ "soc", DATA_TYPE_DOUBLE, &s->soc, 0, 0, CONFIG_FLAG_PRIVATE },
		{ "grid_charge_amps", DATA_TYPE_DOUBLE, &s->grid_charge_amps, 0, "560", 0,
			"range", "0, 5000, .1",  "Charge amps when grid is connected", "A", 1, 0 },
		{ "gen_charge_amps", DATA_TYPE_DOUBLE, &s->gen_charge_amps, 0, "560", 0,
			"range", "0, 5000, .1",  "Charge amps when gen is connected", "A", 1, 0 },
		{ "solar_charge_amps", DATA_TYPE_DOUBLE, &s->solar_charge_amps, 0, "560", 0,
			"range", "0, 5000, .1",  "Charge amps when neither grid or gen connected", "A", 1, 0 },
		{ "min_voltage", DATA_TYPE_DOUBLE, &s->min_voltage, 0, 0, 0,
			"range", STRINGIFY(SI_VOLTAGE_MIN)STRINGIFY(SI_VOLTAGE_MAX)".1", "Min Battery Voltage", "V" },
		{ "max_voltage", DATA_TYPE_DOUBLE, &s->max_voltage, 0, 0, 0,
			"range", STRINGIFY(SI_VOLTAGE_MIN)STRINGIFY(SI_VOLTAGE_MAX)".1", "Max Battery Voltage", "V" },
		{ "min_charge_amps", DATA_TYPE_DOUBLE, &s->min_charge_amps, 0, 0, 0,
			"range", "0, 5000, .1",  "Minimum charge amps", "A", 1, 0 },
		{ "max_charge_amps", DATA_TYPE_DOUBLE, &s->max_charge_amps, 0, "560", 0,
			"range", "0, 5000, .1",  "Maximum charge amps", "A", 1, 0 },
		{ "can_info", DATA_TYPE_STRING, s->can_info, sizeof(s->can_info)-1, 0, 0,
			0, 0, 0, 0, 0, 1, si_set_can_info, s },
		{ "can_transport", DATA_TYPE_STRING, s->can_transport, sizeof(s->can_transport)-1, 0, CONFIG_FLAG_READONLY },
		{ "can_target", DATA_TYPE_STRING, s->can_target, sizeof(s->can_target)-1, 0, CONFIG_FLAG_READONLY, },
		{ "can_topts", DATA_TYPE_STRING, s->can_topts, sizeof(s->can_topts)-1, 0, CONFIG_FLAG_READONLY, },
		{ "can_connected", DATA_TYPE_BOOL, &s->can_connected, 0, "N", CONFIG_FLAG_PRIVATE },
#ifdef SMANET
		{ "smanet_transport", DATA_TYPE_STRING, &s->smanet_transport, sizeof(s->smanet_transport)-1, 0, CONFIG_FLAG_READONLY, },
		{ "smanet_target", DATA_TYPE_STRING, &s->smanet_target, sizeof(s->smanet_target)-1, 0, CONFIG_FLAG_READONLY, },
		{ "smanet_topts", DATA_TYPE_STRING, &s->smanet_topts, sizeof(s->smanet_topts)-1, 0, CONFIG_FLAG_READONLY, },
		{ "smanet_info", DATA_TYPE_STRING, s->smanet_info, sizeof(s->smanet_info)-1, 0, 0,
			0, 0, 0, 0, 0, 1, si_set_smanet_info, s },
		{ "smanet_channels_path", DATA_TYPE_STRING, &s->smanet_channels_path, sizeof(s->smanet_channels_path)-1, 0, 0, 0, 0, 0, 0, 0, 1, smanet_chanpath_set, s },
		{ "battery_type", DATA_TYPE_STRING, &s->battery_type, sizeof(s->battery_type)-1, 0, 0, },
#endif
		{ "input_current_source", DATA_TYPE_STRING, &s->input.text, sizeof(s->input.text)-1, "calculated", 0,
			0, 0, 0, 0, 0, 1, set_input_current_source, s },
		{ "output_current_source", DATA_TYPE_STRING, &s->output.text, sizeof(s->output.text)-1, "calculated", 0,
			0, 0, 0, 0, 0, 1, set_output_current_source, s },
		{ "battery_temp_source", DATA_TYPE_STRING, &s->temp.text, sizeof(s->temp.text)-1, 0, 0,
			0, 0, 0, 0, 0, 1, set_battery_temp_source, s },
		{ "solar_output_source", DATA_TYPE_STRING, &s->solar.text, sizeof(s->solar.text)-1, 0, 0,
			0, 0, 0, 0, 0, 1, set_solar_output_source, s },
		{ "input_source", DATA_TYPE_INT, &s->input.source, 0, 0, CONFIG_FLAG_PRIVATE },
		{ "output_source", DATA_TYPE_INT, &s->output.source, 0, 0, CONFIG_FLAG_PRIVATE },
		{ "temp_source", DATA_TYPE_INT, &s->temp.source, 0, 0, CONFIG_FLAG_PRIVATE },
		{ "solar_source", DATA_TYPE_INT, &s->solar.source, 0, 0, CONFIG_FLAG_PRIVATE },
		{ "notify", DATA_TYPE_STRING, &s->notify_path, sizeof(s->notify_path)-1, 0, 0, },
		{ "disable_si_read", DATA_TYPE_BOOL, &s->disable_si_read, 0, 0, 0 },
		{ "disable_si_write", DATA_TYPE_BOOL, &s->disable_si_write, 0, 0, 0 },
		{ "force_charge_amps", DATA_TYPE_BOOL, &s->force_charge_amps, 0, "N", CONFIG_FLAG_PRIVATE },
		{ "discharge_amps", DATA_TYPE_DOUBLE, &s->discharge_amps, 0, "1120", 0, },
		{ "user_soc", DATA_TYPE_DOUBLE, &s->user_soc, 0, "-1", CONFIG_FLAG_NOSAVE, },
		{ "gen_hold_soc", DATA_TYPE_DOUBLE, &s->gen_hold_soc, 0, "-1", 0, },
		{ "errmsg", DATA_TYPE_STRING, s->errmsg, sizeof(s->errmsg)-1, 0, CONFIG_FLAG_PRIVATE },
		{ "have_battery_temp", DATA_TYPE_BOOL, &s->have_battery_temp, 0, "no", CONFIG_FLAG_PRIVATE },
		{ 0 }
	};
	config_function_t si_funcs[] = {
		{ "get", si_get_value, s, 1 },
		{ "set", si_set_value, s, 2 },
		{ "charge", si_charge, s, 1 },
		{ "grid", si_grid, s, 1 },
		{ "feed", si_feed, s, 1 },
		{0}
	};

	s->ap = agent_init(argc,argv,si_version_string,si_opts,&si_driver,s,0,si_props,si_funcs);
	dprintf(dlevel,"ap: %p\n",s->ap);
	if (!s->ap) return 1;

	if (s->min_voltage < SI_VOLTAGE_MIN) {
		dprintf(dlevel,"min voltage < si minimum, setting to: %.1f\n", SI_VOLTAGE_MIN);
		s->min_voltage = SI_VOLTAGE_MIN;
	}
	if (s->max_voltage <= s->min_voltage || s->max_voltage > SI_VOLTAGE_MAX) {
		dprintf(dlevel,"max voltage > si maximum, setting to: %.1f\n", SI_VOLTAGE_MAX);
		s->max_voltage = SI_VOLTAGE_MAX;
	}

	si_smanet_config(s);

	if (!strlen(s->notify_path)) sprintf(s->notify_path,"%s/notify",SOLARD_BINDIR);
	return 0;
}

void si_config_add_si_data(si_session_t *s) {
	/* Only used by JS funcs */
	int flags = CONFIG_FLAG_PRIVATE;
	config_property_t si_data_props[] = {
		{ "ac1_voltage_l1", DATA_TYPE_DOUBLE, &s->data.ac1_voltage_l1, 0, 0, flags },
		{ "ac1_voltage_l2", DATA_TYPE_DOUBLE, &s->data.ac1_voltage_l2, 0, 0, flags },
		{ "ac1_voltage", DATA_TYPE_DOUBLE, &s->data.ac1_voltage, 0, 0, flags },
		{ "ac1_frequency", DATA_TYPE_DOUBLE, &s->data.ac1_frequency, 0, 0, flags },
		{ "ac1_current", DATA_TYPE_DOUBLE, &s->data.ac1_current, 0, 0, flags },
		{ "ac1_power", DATA_TYPE_DOUBLE, &s->data.ac1_power, 0, 0, flags },
		{ "battery_voltage", DATA_TYPE_DOUBLE, &s->data.battery_voltage, 0, 0, flags },
		{ "battery_current", DATA_TYPE_DOUBLE, &s->data.battery_current, 0, 0, flags },
		{ "battery_power", DATA_TYPE_DOUBLE, &s->data.battery_power, 0, 0, flags },
		{ "battery_temp", DATA_TYPE_DOUBLE, &s->data.battery_temp, 0, 0, flags },
		{ "battery_soc", DATA_TYPE_DOUBLE, &s->data.battery_soc, 0, 0, flags },
		{ "battery_soh", DATA_TYPE_DOUBLE, &s->data.battery_soh, 0, 0, flags },
		{ "battery_cvsp", DATA_TYPE_DOUBLE, &s->data.battery_cvsp, 0, 0, flags },
		{ "relay1", DATA_TYPE_BOOL, &s->data.relay1, 0, 0, flags },
		{ "relay2", DATA_TYPE_BOOL, &s->data.relay2, 0, 0, flags },
		{ "s1_relay1", DATA_TYPE_BOOL, &s->data.s1_relay1, 0, 0, flags },
		{ "s1_relay2", DATA_TYPE_BOOL, &s->data.s1_relay2, 0, 0, flags },
		{ "s2_relay1", DATA_TYPE_BOOL, &s->data.s2_relay1, 0, 0, flags },
		{ "s2_relay2", DATA_TYPE_BOOL, &s->data.s2_relay2, 0, 0, flags },
		{ "s3_relay1", DATA_TYPE_BOOL, &s->data.s3_relay1, 0, 0, flags },
		{ "s3_relay2", DATA_TYPE_BOOL, &s->data.s3_relay2, 0, 0, flags },
		{ "GnRn", DATA_TYPE_BOOL, &s->data.GnRn, 0, 0, flags },
		{ "s1_GnRn", DATA_TYPE_BOOL, &s->data.s1_GnRn, 0, 0, flags },
		{ "s2_GnRn", DATA_TYPE_BOOL, &s->data.s2_GnRn, 0, 0, flags },
		{ "s3_GnRn", DATA_TYPE_BOOL, &s->data.s3_GnRn, 0, 0, flags },
		{ "AutoGn", DATA_TYPE_BOOL, &s->data.AutoGn, 0, 0, flags },
		{ "AutoLodExt", DATA_TYPE_BOOL, &s->data.AutoLodExt, 0, 0, flags },
		{ "AutoLodSoc", DATA_TYPE_BOOL, &s->data.AutoLodSoc, 0, 0, flags },
		{ "Tm1", DATA_TYPE_BOOL, &s->data.Tm1, 0, 0, flags },
		{ "Tm2", DATA_TYPE_BOOL, &s->data.Tm2, 0, 0, flags },
		{ "ExtPwrDer", DATA_TYPE_BOOL, &s->data.ExtPwrDer, 0, 0, flags },
		{ "ExtVfOk", DATA_TYPE_BOOL, &s->data.ExtVfOk, 0, 0, flags },
		{ "GdOn", DATA_TYPE_BOOL, &s->data.GdOn, 0, 0, flags },
		{ "GnOn", DATA_TYPE_BOOL, &s->data.GnOn, 0, 0, flags },
		{ "Error", DATA_TYPE_BOOL, &s->data.Error, 0, 0, flags },
		{ "Run", DATA_TYPE_BOOL, &s->data.Run, 0, 0, flags },
		{ "BatFan", DATA_TYPE_BOOL, &s->data.BatFan, 0, 0, flags },
		{ "AcdCir", DATA_TYPE_BOOL, &s->data.AcdCir, 0, 0, flags },
		{ "MccBatFan", DATA_TYPE_BOOL, &s->data.MccBatFan, 0, 0, flags },
		{ "MccAutoLod", DATA_TYPE_BOOL, &s->data.MccAutoLod, 0, 0, flags },
		{ "Chp", DATA_TYPE_BOOL, &s->data.Chp, 0, 0, flags },
		{ "ChpAdd", DATA_TYPE_BOOL, &s->data.ChpAdd, 0, 0, flags },
		{ "SiComRemote", DATA_TYPE_BOOL, &s->data.SiComRemote, 0, 0, flags },
		{ "OverLoad", DATA_TYPE_BOOL, &s->data.OverLoad, 0, 0, flags },
		{ "ExtSrcConn", DATA_TYPE_BOOL, &s->data.ExtSrcConn, 0, 0, flags },
		{ "Silent", DATA_TYPE_BOOL, &s->data.Silent, 0, 0, flags },
		{ "Current", DATA_TYPE_BOOL, &s->data.Current, 0, 0, flags },
		{ "FeedSelfC", DATA_TYPE_BOOL, &s->data.FeedSelfC, 0, 0, flags },
		{ "Esave", DATA_TYPE_BOOL, &s->data.Esave, 0, 0, flags },
		{ "TotLodPwr", DATA_TYPE_DOUBLE, &s->data.TotLodPwr, 0, 0, flags },
		{ "charging_proc", DATA_TYPE_BYTE, &s->data.charging_proc, 0, 0, flags },
		{ "state", DATA_TYPE_BYTE, &s->data.state, 0, 0, flags },
		{ "errmsg", DATA_TYPE_SHORT, &s->data.errmsg, 0, 0, flags },
		{ "ac2_voltage_l1", DATA_TYPE_DOUBLE, &s->data.ac2_voltage_l1, 0, 0, flags },
		{ "ac2_voltage_l2", DATA_TYPE_DOUBLE, &s->data.ac2_voltage_l2, 0, 0, flags },
		{ "ac2_voltage", DATA_TYPE_DOUBLE, &s->data.ac2_voltage, 0, 0, flags },
		{ "ac2_frequency", DATA_TYPE_DOUBLE, &s->data.ac2_frequency, 0, 0, flags },
		{ "ac2_current", DATA_TYPE_DOUBLE, &s->data.ac2_current, 0, 0, flags },
		{ "ac2_power", DATA_TYPE_DOUBLE, &s->data.ac2_power, 0, 0, flags },
		{ "PVPwrAt", DATA_TYPE_DOUBLE, &s->data.PVPwrAt, 0, 0, flags },
		{ "GdCsmpPwrAt", DATA_TYPE_DOUBLE, &s->data.GdCsmpPwrAt, 0, 0, flags },
		{ "GdFeedPwr", DATA_TYPE_DOUBLE, &s->data.GdFeedPwr, 0, 0, flags },
		{0}
	};

	/* Add info_props to config */
	config_add_props(s->ap->cp, "si_data", si_data_props, flags);
}

// Just for information
static void si_event(void *ctx, char *name, char *module, char *action) {
	dprintf(dlevel,"name: %s, module: %s, action: %s\n", name, module, action);
	if (!name && !module && action && strcmp(action,"__destroy__") == 0) return;
	log_info("EVENT: module: %s, action: %s\n", module, action);
}

int si_config(void *h, int req, ...) {
	si_session_t *s = h;
	va_list va;
	void **vp;
	int r;

	r = 1;
	va_start(va,req);
	dprintf(dlevel,"req: %d\n", req);
	switch(req) {
	case SOLARD_CONFIG_INIT:
		dprintf(dlevel,"**** CONFIG INIT *******\n");
		/* 1st arg is AP */
		s->ap = va_arg(va,solard_agent_t *);

		/* Add si_data to config */
		si_config_add_si_data(s);

		/* Do not let JS errors stop us from writing (causing BMSTimeout) */
		s->ap->js.ignore_js_errors = true;

		// display events
		agent_event_handler(s->ap,si_event,s,s->ap->instance_name,0,0);

#ifdef SMANET
		/* XXX smanet needs to be created here (before jsinit) */
		s->smanet = smanet_init(s->readonly);
		if (!s->smanet) {
			log_error("error initializing SMANET");
			return 1;
		}
#endif

#ifdef JS
		/* Init JS */
		si_jsinit(s);
#ifdef SMANET
		smanet_jsinit(s->ap->js.e);
#endif
#endif
		r = 0;
		break;
	case SOLARD_CONFIG_GET_INFO:
		{
			json_value_t **vp = va_arg(va,json_value_t **);
			dprintf(dlevel,"vp: %p\n", vp);
			if (vp) {
				*vp = si_get_info(s);
				r = 0;
			}
		}
		break;
	case SOLARD_CONFIG_GET_DRIVER:
		dprintf(dlevel,"GET_DRIVER called!\n");
		vp = va_arg(va,void **);
		if (vp) {
			*vp = s->can;
			r = 0;
		}
		break;
	case SOLARD_CONFIG_GET_HANDLE:
		dprintf(dlevel,"GET_HANDLE called!\n");
		vp = va_arg(va,void **);
		if (vp) {
			*vp = s->can_handle;
			r = 0;
		}
		break;
	}
	dprintf(dlevel,"returning: %d\n", r);
	va_end(va);
	return r;
}
