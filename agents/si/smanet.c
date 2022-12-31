
#ifdef SMANET
#include "si.h"

#define dlevel 4

void get_runstate(si_session_t *s, smanet_multreq_t *mr) {
	if (mr->text && strcmp(mr->text,"Run") == 0) s->data.Run = true;
	else s->data.Run = false;
	return;
}

void get_genstate(si_session_t *s, smanet_multreq_t *mr) {
	if (mr->text && strcmp(mr->text,"Run") == 0) s->data.GnOn = true;
	else s->data.GnOn = false;
	return;
}

int si_smanet_read_data(si_session_t *s) {
	config_section_t *sec;
	config_property_t *p;
	struct {
		char *smanet_name;
		char *data_name;
		double mult;
		int can;
		void (*cb)(si_session_t *, smanet_multreq_t *);
	} *pp, parminfo[] = {
		{ "ExtVtg", "ac2_voltage_l1", 1, 1 },
		{ "ExtVtgSlv1", "ac2_voltage_l2", 1, 1 },
		{ "ExtVtgSlv2", "ac2_voltage_l3", 1, 1 },
		{ "ExtFrq", "ac2_frequency", 1, 1 },
		{ "TotExtCur", "ac2_current", 1, 1 },
		{ "InvVtg", "ac1_voltage_l1", 1, 1 },
		{ "InvVtgSlv1", "ac1_voltage_l2", 1, 1 },
		{ "InvVtgSlv2", "ac1_voltage_l3", 1, 1 },
		{ "InvFrq", "ac1_frequency", 1, 1 },
		{ "TotInvCur", "ac1_current", 1, 1 },
		{ "BatSoc", "battery_soc", 1, 1 },
		{ "BatVtg", "battery_voltage", 1, 1 },
		{ "TotBatCur", "battery_current", 1, 1 },
		{ "BatTmp", "battery_temp", 1, 1 },
		{ "TotLodPwr", "TotLodPwr", 1, 1 },
		{ "Msg", "errmsg", 1, 1 },
		{ "GnStt", "GnOn", 1, 1, get_genstate },
		{ "InvOpStt", "GnOn", 1, 1, get_runstate },
		{ 0, 0, 0 }
	};
	smanet_multreq_t *mr;
	int count,mr_size,i;

	if (!s->smanet) return 1;
	if (!smanet_connected(s->smanet)) return 0;

	sec = config_get_section(s->ap->cp,"si_data");
	dprintf(dlevel,"sec: %p\n",sec);
	if (!sec) return 1;

	count = 0;
	for(pp = parminfo; pp->smanet_name; pp++) {
		dprintf(dlevel,"name: %s, can: %d, can_connected: %d\n", pp->smanet_name, pp->can, s->can_connected);
		if (pp->can && s->can_connected) continue;
		count++;
	}
	dprintf(dlevel,"input.source: %d, output.source: %d\n", s->input.source, s->output.source);
	if (s->input.source == CURRENT_SOURCE_SMANET) count++;
	if (s->output.source == CURRENT_SOURCE_SMANET) count++;
	dprintf(dlevel,"count: %d\n", count);
	if (!count) return 0;
	mr_size = count * sizeof(smanet_multreq_t);
	dprintf(dlevel,"mr_size: %d\n", mr_size);
	mr = malloc(mr_size);
	if (!mr) return 1;

	i = 0;
	for(pp = parminfo; pp->smanet_name; pp++) {
		if (pp->can && s->can_connected) continue;
		mr[i++].name = pp->smanet_name;
	}
	if (s->input.source == CURRENT_SOURCE_SMANET) mr[i++].name = s->input.name;
	if (s->output.source == CURRENT_SOURCE_SMANET) mr[i++].name = s->output.name;
//	for(i=0; i < count; i++) dprintf(dlevel,"mr[%d]: %s\n", i, mr[i].name);
	if (smanet_get_multvalues(s->smanet,mr,count)) {
		dprintf(dlevel,"smanet_get_multvalues error");
		smanet_disconnect(s->smanet);
		free(mr);
		return 1;
	}
	for(i=0; i < count; i++) {
		dprintf(dlevel,"mr[%d]: %s\n", i, mr[i].name);
		if (s->input.source == CURRENT_SOURCE_SMANET && strcmp(mr[i].name,s->input.name) == 0) {
			if (s->input.type == CURRENT_TYPE_WATTS) {
				s->data.ac2_power = mr[i].value;
				s->data.ac2_current = s->data.ac2_power / s->data.ac2_voltage_l1;
			} else {
				s->data.ac2_current = mr[i].value;
				s->data.ac2_power = s->data.ac2_current * s->data.ac2_voltage_l1;
			}
			dprintf(dlevel,"ac2_current: %.1f, ac2_power: %.1f\n", s->data.ac2_current, s->data.ac2_power);
			continue;
		}
		if (s->output.source == CURRENT_SOURCE_SMANET && strcmp(mr[i].name,s->output.name) == 0) {
			if (s->output.type == CURRENT_TYPE_WATTS) {
				s->data.ac1_power = mr[i].value;
				s->data.ac1_current = s->data.ac1_power / s->data.ac1_voltage_l1;
			} else {
				s->data.ac1_current = mr[i].value;
				s->data.ac1_power = s->data.ac1_current * s->data.ac1_voltage_l1;
			}
			dprintf(dlevel,"ac1_current: %.1f, ac1_power: %.1f\n", s->data.ac1_current, s->data.ac1_power);
			continue;
		}
		for(pp = parminfo; pp->smanet_name; pp++) {
			if (strcmp(pp->smanet_name, mr[i].name) == 0) {
				if (pp->cb) {
					pp->cb(s, &mr[i]);
				} else {
					p = config_section_get_property(sec, pp->data_name);
					if (!p) break;
					dprintf(dlevel,"mr[%d]: value: %f, text: %s\n", i, mr[i].value, mr[i].text);
					if (mr[i].text) 
						p->len = conv_type(p->type, p->dest, p->dsize, DATA_TYPE_STRING,
							mr[i].text, strlen(mr[i].text) );
					else  {
						double d = mr[i].value * pp->mult;
						p->len = conv_type(p->type, p->dest, p->dsize, DATA_TYPE_DOUBLE, &d, 0 );
					}
					dprintf(dlevel,"%s: %.1f\n", p->name, *((double *)p->dest));
				}
				break;
			}
		}
	}
	if (!s->can_connected) {

		s->data.battery_power = s->data.battery_voltage * s->data.battery_current;
		dprintf(dlevel,"Battery power: %.1f\n",s->data.battery_power);

		s->data.ac1_voltage = s->data.ac1_voltage_l1 + s->data.ac1_voltage_l2;
		dprintf(dlevel,"ac1_voltage: %.1f, ac1_frequency: %.1f\n",s->data.ac1_voltage,s->data.ac1_frequency);
		s->data.ac1_power = s->data.ac1_current * s->data.ac1_voltage_l1;
		dprintf(dlevel,"ac1_current: %.1f, ac1_power: %.1f\n",s->data.ac1_current,s->data.ac1_power);

		s->data.ac2_voltage = s->data.ac2_voltage_l1 + s->data.ac2_voltage_l2;
		dprintf(dlevel,"ac2: voltage: %.1f, frequency: %.1f\n",s->data.ac2_voltage,s->data.ac2_frequency);
		s->data.ac2_power = s->data.ac2_current * s->data.ac2_voltage_l1;
		dprintf(dlevel,"ac2_current: %.1f, ac2_power: %.1f\n",s->data.ac2_current,s->data.ac2_power);

		s->data.TotLodPwr *= 1000.0;

		/* I was not able to figure out a way to tell if the grid is "connected" using just SMANET parms */
		if ((s->data.ac2_voltage > 10 && s->data.ac2_frequency > 10) && s->data.ac2_power > 100) s->data.GdOn = true;
		else s->data.GdOn = false;
	}
	free(mr);
	dprintf(dlevel,"done\n");
	return 0;
}

static void _addchans(si_session_t *s) {
	smanet_chaninfo_t info;
	smanet_channel_t *c;
	config_section_t *sec;
	config_property_t *newp;
	register int i;
	char temp[1024];
	int type;
	double step;
	char *scope,*values,*labels;
//	int flags = SI_CONFIG_FLAG_SMANET | CONFIG_FLAG_NOSAVE | CONFIG_FLAG_NOPUB;
	int flags = SI_CONFIG_FLAG_SMANET | CONFIG_FLAG_NOSAVE;

	dprintf(dlevel,"adding SMANET channels...\n");
	sec = config_create_section(s->ap->cp,"smanet",0);
	if (!sec) {
		log_error("_addchans: unable to create smanet section");
		return;
	}

	if (smanet_get_chaninfo(s->smanet, &info)) {
		log_error("_addchans: unable to get chaninfo");
		return;
	}

	dprintf(dlevel+1,"chancount: %d\n", info.chancount);
	for(i=0; i < info.chancount; i++) {
		c = &info.channels[i];
		dprintf(dlevel,"c->mask: %04x, CH_PARA: %04x\n", c->mask, CH_PARA);
		if ((c->mask & CH_PARA) == 0) continue;
		dprintf(dlevel,"adding chan: %s\n", c->name);
		step = 1;
		dprintf(dlevel+1,"c->format: %08x\n", c->format);
		switch(c->format & 0xf) {
		case CH_BYTE:
			type = DATA_TYPE_BYTE;
			break;
		case CH_SHORT:
			type = DATA_TYPE_SHORT;
			break;
		case CH_LONG:
			type = DATA_TYPE_LONG;
			break;
		case CH_FLOAT:
			type = DATA_TYPE_FLOAT;
			step = .1;
			break;
		case CH_DOUBLE:
			type = DATA_TYPE_DOUBLE;
			step = .1;
			break;
		case CH_ARRAY | CH_BYTE:
			type = DATA_TYPE_U8_ARRAY;
			break;
		case CH_ARRAY | CH_SHORT:
			type = DATA_TYPE_U16_ARRAY;
			break;
		case CH_ARRAY | CH_LONG:
			type = DATA_TYPE_U32_ARRAY;
			break;
		case CH_ARRAY | CH_FLOAT:
			type = DATA_TYPE_F32_ARRAY;
			step = .1;
			break;
		case CH_ARRAY | CH_DOUBLE:
			type = DATA_TYPE_F64_ARRAY;
			step = .1;
			break;
		default:
			printf("SMANET: unknown format: %04x\n", c->format & 0xf);
			break;
		}
		dprintf(dlevel+1,"c->mask: %08x\n", c->mask);
		if (c->mask & CH_PARA && c->mask & CH_ANALOG) {
			register char *p;

			scope = "range";
			p = temp;
//			sprintf(temp,"%f,%f,%f", c->gain, c->offset, step);
			if (double_isint(c->gain)) p += sprintf(p,"%d,",(int)c->gain);
			else p += sprintf(p,"%f,",c->gain);
			if (double_isint(c->offset)) p += sprintf(p,"%d,",(int)c->offset);
			else p += sprintf(p,"%f,",c->offset);
			if (double_isint(step)) p += sprintf(p,"%d",(int)step);
			else p += sprintf(p,"%f",step);
			values = temp;
			labels = 0;
		} else if (c->mask & CH_DIGITAL) {
			scope = "select";
			sprintf(temp,"%s,%s", c->txtlo, c->txthi);
			values = temp;
			labels = 0;
		} else if (c->mask & CH_STATUS && list_count(c->strings)) {
			char *str,*p;
			int i,bytes,bytes_left;

			type = DATA_TYPE_STRING;
			scope = "select";
			i = 0;
			p = temp;
			bytes_left = sizeof(temp);
			list_reset(c->strings);
			while((str = list_get_next(c->strings)) != 0) {
				bytes = snprintf(p,bytes_left,"%s%s",(i++ ? "," : ""),str);
				p += bytes;
				bytes_left -= bytes;
			}
			values = temp;
			labels = 0;
		}
		newp = config_new_property(s->ap->cp, c->name, type, 0, 0, 0, 0, flags, scope, values, labels, c->unit, 1, 1);
		config_section_add_property(s->ap->cp, sec, newp, 0);
	}

	/* Re-create info and publish it */
	if (s->ap->info) json_destroy_value(s->ap->info);
	s->ap->info = si_get_info(s);
#ifdef MQTT
	agent_pubinfo(s->ap,0);
#endif
}

int si_smanet_load_channels(si_session_t *s) {

	/* dont do anything before SMANET initialized */
	if (!s->smanet) return 0;

	dprintf(dlevel,"smanet_channels_path: %s\n", s->smanet_channels_path);

	/* if path empty, set default */
	if (!strlen(s->smanet_channels_path)) {
        	smanet_info_t info;

		/* Get the model # */
	        if (smanet_get_info(s->smanet,&info)) {
			log_error("si_smanet_setup: unable to get model: %s\n", smanet_get_errmsg(s->smanet));
			return 1;
		}
		sprintf(s->smanet_channels_path,"%s/%s_channels.json",s->ap->agent_libdir,info.type);
	}

	log_info("loading SMANET channels from: %s\n", s->smanet_channels_path);
	if (smanet_load_channels(s->smanet,s->smanet_channels_path)) log_error("%s\n",smanet_get_errmsg(s->smanet));

	_addchans(s);
	return 0;
}

/* Setup SMANET - runs 1 time per connection */
int si_smanet_setup(si_session_t *s) {
#if 0
	smanet_info_t info;

	if (smanet_get_info(s->smanet,&info)) {
		log_error("si_smanet_setup: unable to get type: %s\n", smanet_get_errmsg(s->smanet));
		return 1;
	}

	/* Load channels */
	if (!strlen(s->smanet_channels_path)) sprintf(s->smanet_channels_path,"%s/%s_channels.json",s->ap->libdir,info.type);
	fixpath(s->smanet_channels_path, sizeof(s->smanet_channels_path)-1);
	dprintf(dlevel,"smanet_channels_path: %s\n", s->smanet_channels_path);
	if (smanet_load_channels(s->smanet, s->smanet_channels_path)) {
		log_error("%s\n", smanet_get_errmsg(s->smanet));
		return 1;
	}
#endif
	if (si_smanet_load_channels(s)) return 1;

	/* Add channels to config */
	_addchans(s);

#ifdef JS
	if (s->ap && s->ap->js.e) smanet_jsinit(s->ap->js.e);
#endif

	s->smanet_added = 1;
	return 0;
}

int si_smanet_connect(si_session_t *s) {

	dprintf(dlevel,"smanet: %p\n", s->smanet);
	if (!s->smanet) return 1;

	dprintf(dlevel,"smanet_transport: %s, smanet_target: %s, smanet_topts: %s\n",
		s->smanet_transport, s->smanet_target, s->smanet_topts);
	if (!strlen(s->smanet_transport) || !strlen(s->smanet_target)) return 0;
	smanet_connect(s->smanet, s->smanet_transport, s->smanet_target, s->smanet_topts);
	if (smanet_connected(s->smanet)) {
		if (!s->smanet_added) si_smanet_setup(s);
		if (s->ap) agent_event(s->ap,"SMANET","Connected");
	}
	return 0;
}

int si_smanet_disconnect(si_session_t *s) {

	dprintf(dlevel,"smanet: %p\n", s->smanet);
	if (!s->smanet) return 1;

	smanet_disconnect(s->smanet);
	dprintf(dlevel,"connected: %d\n", smanet_connected(s->smanet));
	if (!smanet_connected(s->smanet) && s->ap) agent_event(s->ap,"SMANET","Disconnected");
	return 0;
}

#endif
