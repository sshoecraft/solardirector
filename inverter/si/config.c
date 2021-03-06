
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "si.h"

static json_descriptor_t charge_params[] = {
	{ "charge", DATA_TYPE_INT, "select", 3, (int []){ 0, 1, 2 }, 1, (char *[]){ "Off","On","CV" }, 0, 1, 0 },
	{ "max_voltage", DATA_TYPE_FLOAT, "range", 3, (float []){ SI_VOLTAGE_MIN, SI_VOLTAGE_MAX, .1 }, 1, (char *[]){ "Max Battery Voltage" }, "V", 1, "%2.1f" },
	{ "min_voltage", DATA_TYPE_FLOAT, "range", 3, (float []){ SI_VOLTAGE_MIN, SI_VOLTAGE_MAX, .1 }, 1, (char *[]){ "Min Battery Voltage" }, "V", 1, "%2.1f" },
	{ "charge_start_voltage", DATA_TYPE_FLOAT, "range", 3, (float []){ SI_VOLTAGE_MIN, SI_VOLTAGE_MAX, .1 }, 1, (char *[]){ "Charge start voltage" }, "V", 1, "%2.1f" },
	{ "charge_end_voltage", DATA_TYPE_FLOAT, "range", 3, (float []){ SI_VOLTAGE_MIN, SI_VOLTAGE_MAX, .1 }, 1, (char *[]){ "Charge end voltage" }, "V", 1, "%2.1f" },
	{ "charge_amps", DATA_TYPE_FLOAT, "range", 3, (float []){ 0, 5000, .1 }, 1, (char *[]){ "Charge Amps" }, "A", 1, "%.1f" },
	{ "discharge_amps", DATA_TYPE_FLOAT, "range", 3, (float []){ 0, 5000, .1 }, 1, (char *[]){ "Discharge Amps" }, "A", 1, "%.1f" },
	{ "charge_min_amps", DATA_TYPE_FLOAT, "range", 3, (float []){ 0, 5000, .1 }, 1, (char *[]){ "Minimum charge amps" }, "A", 1, "%.1f" },
	{ "charge_at_max", DATA_TYPE_BOOL, 0, 0, 0, 1, (char *[]){ "Charge at max_voltage" }, 0, 0 },
	{ "charge_creep", DATA_TYPE_BOOL, 0, 0, 0, 1, (char *[]){ "Increase voltage to maintain charge amps" }, 0, 0 },
	{ "sim_step", DATA_TYPE_FLOAT, "range", 3, (float []){ 0, 5000, .1 }, 1, (char *[]){ "SIM Voltage Step" }, "V", 1, "%.1f" },
	{ "interval", DATA_TYPE_INT, "range", 3, (int []){ 0, 10000, 1 }, 0, 0, 0, 1, 0 },
	{ "can_transport", DATA_TYPE_STRING, 0, 0, 0, 0, 0, 0, 1, 0 },
	{ "can_target", DATA_TYPE_STRING, 0, 0, 0, 0, 0, 0, 1, 0 },
	{ "can_topts", DATA_TYPE_STRING, 0, 0, 0, 0, 0, 0, 1, 0 },
	{ "smanet_transport", DATA_TYPE_STRING, 0, 0, 0, 0, 0, 0, 1, 0 },
	{ "smanet_target", DATA_TYPE_STRING, 0, 0, 0, 0, 0, 0, 1, 0 },
	{ "smanet_topts", DATA_TYPE_STRING, 0, 0, 0, 0, 0, 0, 1, 0 },
	{ "smanet_channels_path", DATA_TYPE_STRING, 0, 0, 0, 0, 0, 0, 1, 0 },
	{ "run", DATA_TYPE_BOOL, 0, 0, 0, 1, (char *[]){ "run state" }, 0, 0 },
};
#define NCHG (sizeof(charge_params)/sizeof(json_descriptor_t))

#if 0
static json_descriptor_t state_params[] = {
	{ "SenseResistor", DATA_TYPE_FLOAT, 0, 0, 0, 1, (char *[]){ "Current Res" }, "mR", 10, "%.1f" },
	{ "PackNum", DATA_TYPE_INT, "select",
		28, (int []){ 3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30 },
		1, (char *[]){ "mR PackNum" }, 0, 0 },
	{ "CycleCount", DATA_TYPE_INT, 0, 0, 0, 0, 0, 0, 0 },
	{ "SerialNumber", DATA_TYPE_INT, 0, 0, 0, 0, 0, 0, 0 },
	{ "ManufacturerName", DATA_TYPE_STRING, 0, 0, 0, 0, 0, 0, 0 },
	{ "DeviceName", DATA_TYPE_STRING, 0, 0, 0, 0, 0, 0, 0 },
	{ "ManufactureDate", DATA_TYPE_DATE, 0, 0, 0, 0, 0, 0, 0 },
	{ "BarCode", DATA_TYPE_STRING, 0, 0, 0, 0, 0, 0, 0 },
};
#define NOTHER (sizeof(other_params)/sizeof(json_descriptor_t))

static json_descriptor_t basic_params[] = {
	{ "CellOverVoltage", DATA_TYPE_FLOAT, "range", 3, (int []){ 0, 10000, 1 }, 1, (char *[]){ "COVP" }, "mV", 1000, "%.3f" },
	{ "CellOVRelease", DATA_TYPE_FLOAT, "range", 3, (int []){ 0, 10000, 1 }, 1, (char *[]){ "COVP Release" }, "mV", 1000, "%.3f" },
	{ "CellOVDelay", DATA_TYPE_INT, "range", 3, (int []){ 0, 999, 1 }, 1, (char *[]){ "COVP Delay" }, "S", 0 },
	{ "CellUnderVoltage", DATA_TYPE_FLOAT, "range", 3, (int []){ 0, 10000, 1 }, 1, (char *[]){ "CUVP" }, "mV", 1000, "%.3f" },
	{ "CellUVRelease", DATA_TYPE_FLOAT, "range", 3, (int []){ 0, 10000, 1 }, 1, (char *[]){ "CUVP Release" }, "mV", 1000, "%.3f" },
	{ "CellUVDelay", DATA_TYPE_INT, "range", 3, (int []){ 0, 999, 1 }, 1, (char *[]){ "CUVP Delay" }, "S", 0 },
	{ "PackOverVoltage", DATA_TYPE_FLOAT, "range", 3, (int []){ 0, 99999, 1 }, 1, (char *[]){ "POVP" }, "mV", 100, "%.1f" },
	{ "PackOVRelease", DATA_TYPE_FLOAT, "range", 3, (int []){ 0, 99999, 1 }, 1, (char *[]){ "POVP Release" }, "mV", 100, "%.1f" },
	{ "PackOVDelay", DATA_TYPE_INT, "range", 3, (int []){ 0, 999, 1 }, 1, (char *[]){ "POVP Delay" }, "S", 0 },
};
#endif

static struct parmdir {
	char *name;
	json_descriptor_t *parms;
	int count;
} allparms[] = {
	{ "Charge parameters", charge_params, NCHG },
#if 0
	{ "Function Configuration", func_params, NFUNC },
	{ "NTC Configuration", ntc_params, NNTC },
	{ "Balance Configuration", balance_params, NBAL },
	{ "GPS Configuration", gps_params, NBAL },
	{ "Other Configuration", other_params, NOTHER },
	{ "Basic Parameters", basic_params, NBASIC },
	{ "Hard Parameters", hard_params, NHARD },
	{ "Misc Parameters", misc_params, NMISC },
#endif
};
#define NALL (sizeof(allparms)/sizeof(struct parmdir))

json_descriptor_t *_getd(si_session_t *s,char *name) {
	json_descriptor_t *dp;
	register int i,j;

	dprintf(1,"label: %s\n", name);

	for(i=0; i < NALL; i++) {
		dprintf(1,"section: %s\n", allparms[i].name);
		dp = allparms[i].parms;
		for(j=0; j < allparms[i].count; j++) {
			dprintf(1,"dp->name: %s\n", dp[j].name);
			if (strcmp(dp[j].name,name)==0)
				return &dp[j];
		}
	}

	/* See if it's in our desc list */
	dprintf(1,"looking in desc list...\n");
	list_reset(s->desc);
	while((dp = list_get_next(s->desc)) != 0) {
		dprintf(1,"dp->name: %s\n", dp->name);
		if (strcmp(dp->name,name)==0)
			return dp;
	}

	return 0;
}

static void _addchans(si_session_t *s, json_value_t *ca) {
	smanet_session_t *ss = s->smanet;
	smanet_channel_t *c;
	json_descriptor_t newd;
	float step;

	list_reset(ss->channels);
	while((c = list_get_next(ss->channels)) != 0) {
		dprintf(1,"adding chan: %s\n", c->name);
		memset(&newd,0,sizeof(newd));
		newd.name = c->name;
		step = 1;
		switch(c->format & 0xf) {
		case CH_BYTE:
			newd.type = DATA_TYPE_BYTE;
			break;
		case CH_SHORT:
			newd.type = DATA_TYPE_SHORT;
			break;
		case CH_LONG:
			newd.type = DATA_TYPE_LONG;
			break;
		case CH_FLOAT:
			newd.type = DATA_TYPE_FLOAT;
			step = .1;
			break;
		case CH_DOUBLE:
			newd.type = DATA_TYPE_DOUBLE;
			step = .1;
			break;
		default:
			dprintf(1,"unknown type: %d\n", c->format & 0xf);
			break;
		}
		if (c->mask & CH_PARA && c->mask & CH_ANALOG) {
			float *values;

			newd.scope = "range";
			values = malloc(3*sizeof(float));
			values[0] = c->gain;
			values[1] = c->offset;
			values[2] = step;
			newd.nvalues = 3;
			newd.values = values;
			dprintf(1,"adding range: 0: %f, 1: %f, 2: %f\n", values[0], values[1], values[2]);
		} else if (c->mask & CH_DIGITAL) {
			char **labels;

			newd.nlabels = 2;
 			labels = malloc(newd.nlabels*sizeof(char *));
			labels[0] = c->txtlo;
			labels[1] = c->txthi;
			newd.labels = labels;
		} else if (c->mask & CH_STATUS && list_count(c->strings)) {
			char **labels,*p;
			int i;

			newd.scope = "select";
			newd.nlabels = list_count(c->strings);
 			labels = malloc(newd.nlabels*sizeof(char *));
			i = 0;
			list_reset(c->strings);
			while((p = list_get_next(c->strings)) != 0) labels[i++] = p;
			newd.labels = labels;
		}
		newd.units = c->unit;
		newd.scale = 1.0;
		json_array_add_descriptor(ca,newd);
		/* Also add to our desc list */
		list_add(s->desc,&newd,sizeof(newd));
	}
}

int si_config_add_info(si_session_t *s, json_value_t *j) {
	int x,y;
	json_value_t *ca,*o,*a;
	struct parmdir *dp;

	/* Configuration array */
	ca = json_create_array();

	/* Do we have a saved rs485 config? */
	dprintf(1,"smanet: %p\n", s->smanet);
	if (s->smanet) {
		dprintf(1,"channels_path: %s\n", s->channels_path);
		if (!strlen(s->channels_path) && s->ap->cfg) {
			char *p;

			p = cfg_get_item(s->ap->cfg,"config","channels_path");
			if (p) strncat(s->channels_path,p,sizeof(s->channels_path)-1);
		}
		if (!strlen(s->channels_path)) sprintf(s->channels_path,"%s/%s.dat",SOLARD_LIBDIR,s->smanet->type);
		fixpath(s->channels_path, sizeof(s->channels_path)-1);
		dprintf(1,"channels_path: %s\n", s->channels_path);
		if (smanet_load_channels(s->smanet,s->channels_path) == 0) _addchans(s,ca);
	}

	/* Add the inverter config */
	for(x=0; x < NALL; x++) {
		o = json_create_object();
		dp = &allparms[x];
		if (dp->count > 1) {
			a = json_create_array();
			for(y=0; y < dp->count; y++) json_array_add_descriptor(a,dp->parms[y]);
			json_add_value(o,dp->name,a);
		} else if (dp->count == 1) {
			json_add_descriptor(o,dp->name,dp->parms[0]);
		}
		json_array_add_value(ca,o);
	}

	json_add_value(j,"configuration",ca);
	return 0;
}

static json_proctab_t *_getinv(si_session_t *s, char *name) {
	solard_inverter_t *inv = s->ap->role_data;
	json_proctab_t params[] = {
                { "charge",DATA_TYPE_INT,&s->charge_mode,0,0 },
                { "max_voltage",DATA_TYPE_FLOAT,&inv->max_voltage,0,0 },
                { "min_voltage",DATA_TYPE_FLOAT,&inv->min_voltage,0,0 },
                { "charge_start_voltage",DATA_TYPE_FLOAT,&inv->charge_start_voltage,0,0 },
                { "charge_end_voltage",DATA_TYPE_FLOAT,&inv->charge_end_voltage,0,0 },
                { "charge_amps",DATA_TYPE_FLOAT,&inv->charge_amps,0,0 },
                { "discharge_amps",DATA_TYPE_FLOAT,&inv->discharge_amps,0,0 },
		{ "charge_min_amps", DATA_TYPE_FLOAT, &s->charge_min_amps,0,0 },
		{ "charge_at_max", DATA_TYPE_BOOL, &inv->charge_at_max, 0,0 },
		{ "charge_creep", DATA_TYPE_BOOL, &s->charge_creep, 0,0 },
		{ "sim_step", DATA_TYPE_FLOAT, &s->sim_step, 0,0 },
		{ "interval", DATA_TYPE_INT, &s->interval, 0,0 },
                { "can_transport", DATA_TYPE_STRING, &s->can_transport, sizeof(s->can_transport)-1,0 },
                { "can_target", DATA_TYPE_STRING, &s->can_target, sizeof(s->can_target)-1,0 },
                { "can_topts", DATA_TYPE_STRING, &s->can_topts, sizeof(s->can_topts)-1,0 },
                { "smanet_transport", DATA_TYPE_STRING, &s->smanet_transport, sizeof(s->smanet_transport)-1,0 },
                { "smanet_target", DATA_TYPE_STRING, &s->smanet_target, sizeof(s->smanet_target)-1,0 },
                { "smanet_topts", DATA_TYPE_STRING, &s->smanet_topts, sizeof(s->smanet_topts)-1,0 },
                { "smanet_channels_path", DATA_TYPE_STRING, &s->channels_path, sizeof(s->channels_path)-1,0 },
		{ "run", DATA_TYPE_BOOL, &s->run_state, 0,0 },
		JSON_PROCTAB_END
	};
	json_proctab_t *invp;

	dprintf(1,"name: %s\n", name);

	for(invp=params; invp->field; invp++) {
		dprintf(1,"invp->field: %s\n", invp->field);
		if (strcmp(invp->field,name)==0) {
			dprintf(1,"found!\n");
			s->idata = *invp;
			return &s->idata;
		}
	}
	dprintf(1,"NOT found\n");
	return 0;
}

si_param_t *_getp(si_session_t *s, char *name, int invonly) {
	json_proctab_t *invp;
	si_param_t *pinfo;

	dprintf(1,"name: %s, invonly: %d\n", name, invonly);

	pinfo = &s->pdata;
	memset(pinfo,0,sizeof(*pinfo));

	/* Is it from the inverter struct? */
	invp = _getinv(s,name);
	if (invp) {
		uint8_t *bptr;
		short *wptr;
		int *iptr;
		long *lptr;
		float *fptr;
		double *dptr;

		dprintf(1,"inv: name: %s, type: %d, ptr: %p, len: %d\n", invp->field, invp->type, invp->ptr, invp->len);
		dprintf(1,"invp->type: %d\n", invp->type);

		strncat(pinfo->name,invp->field,sizeof(pinfo->name)-1);
		pinfo->type = invp->type;
		pinfo->source = 1;
		switch(invp->type) {
		case DATA_TYPE_BOOL:
			iptr = invp->ptr;
			pinfo->bval = *iptr;
			break;
		case DATA_TYPE_BYTE:
			bptr = invp->ptr;
			pinfo->bval = *bptr;
			break;
		case DATA_TYPE_SHORT:
			wptr = invp->ptr;
			pinfo->wval = *wptr;
			break;
		case DATA_TYPE_LONG:
			lptr = invp->ptr;
			pinfo->lval = *lptr;
			break;
		case DATA_TYPE_FLOAT:
			fptr = invp->ptr;
			pinfo->fval = *fptr;
			break;
		case DATA_TYPE_DOUBLE:
			dptr = invp->ptr;
			pinfo->dval = *dptr;
			break;
		case DATA_TYPE_STRING:
			pinfo->sval = invp->ptr;
			break;
		default: break;
		}
		dprintf(1,"found\n");
		return pinfo;
	}
	if (invonly) goto _getp_done;

	if (s->smanet) {
		json_descriptor_t *dp;
		double value;
		char *text;

		dprintf(1,"NOT found, checking smanet\n");

		dp = _getd(s,name);
		if (!dp) {
			dprintf(1,"_getd returned null!\n");
			goto _getp_done;
		}
		if (smanet_get_value(s->smanet, name, &value, &text)) goto _getp_done;
		dprintf(1,"val: %f, sval: %s\n", value, text);
		strncat(pinfo->name,name,sizeof(pinfo->name)-1);
		pinfo->source = 2;
		if (text) {
			pinfo->sval = text;
			pinfo->type = DATA_TYPE_STRING;
		} else {
			conv_type(dp->type,&pinfo->bval,0,DATA_TYPE_DOUBLE,&value,0);
			pinfo->type = dp->type;
		}
		return pinfo;
	}

_getp_done:
	dprintf(1,"NOT found\n");
	return 0;
}

static int si_get_config(si_session_t *s, si_param_t *pp, json_descriptor_t *dp) {
	char topic[200];
	char temp[72];
	unsigned char bval;
	short wval;
	long lval;
	float fval;
	double dval;

	dprintf(1,"dp: name: %s, unit: %s, scale: %f\n", dp->name, dp->units, dp->scale);

	temp[0] = 0;
	dprintf(1,"pp->type: %d (%s)\n", pp->type, typestr(pp->type));
	switch(pp->type) {
	case DATA_TYPE_BOOL:
		dprintf(1,"bval: %d\n", pp->bval);
		sprintf(temp,"%s",pp->bval ? "true" : "false");
		break;
	case DATA_TYPE_BYTE:
		bval = pp->bval;
		dprintf(1,"bval: %d\n", bval);
		dprintf(1,"scale: %f\n", dp->scale);
		if (dp->scale != 0.0) bval /= dp->scale;
		dprintf(1,"format: %s\n", dp->format);
		if (dp->format) sprintf(temp,dp->format,bval);
		else sprintf(temp,"%d",bval);
		break;
	case DATA_TYPE_SHORT:
		wval = pp->wval;
		dprintf(1,"wval: %d\n", wval);
		dprintf(1,"scale: %f\n", dp->scale);
		if (dp->scale != 0.0) wval /= dp->scale;
		dprintf(1,"format: %s\n", dp->format);
		if (dp->format) sprintf(temp,dp->format,wval);
		else sprintf(temp,"%d",wval);
		break;
	case DATA_TYPE_LONG:
		lval = pp->lval;
		dprintf(1,"ival: %d\n", lval);
		dprintf(1,"scale: %f\n", dp->scale);
		if (dp->scale != 0.0) lval /= dp->scale;
		dprintf(1,"format: %s\n", dp->format);
		if (dp->format) sprintf(temp,dp->format,lval);
		else sprintf(temp,"%ld",lval);
		break;
	case DATA_TYPE_FLOAT:
		fval = pp->fval;
		dprintf(1,"fval: %f\n", fval);
		dprintf(1,"scale: %f\n", dp->scale);
		if (dp->scale != 0.0) fval /= dp->scale;
		if (dp->format) sprintf(temp,dp->format,fval);
		else sprintf(temp,"%f",fval);
		break;
	case DATA_TYPE_DOUBLE:
		dval = pp->dval;
		dprintf(1,"dval: %lf\n", dval);
		dprintf(1,"scale: %f\n", dp->scale);
		if (dp->scale != 0.0) dval /= dp->scale;
		if (dp->format) sprintf(temp,dp->format,dval);
		else sprintf(temp,"%f",dval);
		break;
	case DATA_TYPE_STRING:
		temp[0] = 0;
		strncat(temp,pp->sval,sizeof(temp)-1);
		trim(temp);
		break;
	default:
		sprintf(temp,"unhandled switch for: %d",dp->type);
		break;
	}
	sprintf(topic,"%s/%s/%s/%s/%s",SOLARD_TOPIC_ROOT,s->ap->role->name,s->ap->name,SOLARD_FUNC_CONFIG,pp->name);
	dprintf(1,"topic: %s, temp: %s\n", topic, temp);
	return mqtt_pub(s->ap->m,topic,temp,1);
}

static int si_set_config(si_session_t *s, si_param_t *pp, json_descriptor_t *dp, solard_confreq_t *req, int pub) {
	char temp[72];

	dprintf(1,"dp: name: %s, unit: %s, scale: %f\n", dp->name, dp->units, dp->scale);

	/* We can use any field in the union as a source or dest */
	if (pp->source == SI_PARAM_SOURCE_INV) {
		solard_inverter_t *inv = s->ap->role_data;
		json_proctab_t *invp;
		int oldval;

		invp = _getinv(s,pp->name);

		/* Save old vals */
		if (strcmp(invp->field,"run")==0) oldval = s->run_state;
		else if (strcmp(invp->field,"charge")==0) oldval = s->charge_mode;
		else if (strcmp(invp->field,"charge_at_max")==0) oldval = inv->charge_at_max;
		else oldval = -1;

		/* Update the inv struct directly */
		conv_type(invp->type,invp->ptr,0,req->type,&req->sval,0);
		log_info("%s set to %s\n", pp->name, req->sval);

		/* Update our local copy of charge amps too */
		if (strcmp(invp->field,"charge_amps")==0) s->charge_amps = *((float *)invp->ptr);

		if (strcmp(invp->field,"run")==0) {
			int r;

			printf("oldval: %d, state: %d\n", oldval, s->run_state);
			if (!oldval && s->run_state) {
				r = si_startstop(s,1);
				if (!r) log_write(LOG_INFO,"Started Sunny Island\n");
				else {
					log_error("start failed\n");
					s->run_state = 0;
				}
			}
			if (oldval && !s->run_state) {
				r = si_startstop(s,0);
				if (!r) {
					log_write(LOG_INFO,"Stopped Sunny Island\n");
					charge_stop(s,1);
				} else {
					log_error("stop failed\n");
					s->run_state = 0;
				}
			}
		} else if (strcmp(invp->field,"charge")==0) {
                        switch(s->charge_mode) {
                        case 0:
                                charge_stop(s,1);
                                break;
                        case 1:
                                charge_start(s,1);
                                break;
                        case 2:
                                charge_start_cv(s,1);
                                break;
                        }
		} else if (strcmp(invp->field,"charge_at_max")==0) {
			printf("at_max: %d, oldval: %d\n", inv->charge_at_max, oldval);
			if (inv->charge_at_max && !oldval) charge_max_start(s);
			if (!inv->charge_at_max && oldval) charge_max_stop(s);
		}


		/* ALSO update this value in our config file */
		conv_type(DATA_TYPE_STRING,temp,sizeof(temp)-1,req->type,&req->sval,0);
		cfg_set_item(s->ap->cfg,s->ap->section_name,invp->field,"",temp);
		cfg_write(s->ap->cfg);
	} else { 
		double value;
		char *text;

		text = 0;
		dprintf(1,"pp->type: %s\n", typestr(pp->type));
		if (pp->type == DATA_TYPE_STRING) text = req->sval;
		else conv_type(DATA_TYPE_DOUBLE,&value,0,DATA_TYPE_STRING,req->sval,strlen(req->sval));
		if (smanet_set_value(s->smanet,dp->name,value,text)) return 1;
	}
	/* Re-get the param to update internal vars and publish */
	return (pub ? si_get_config(s,_getp(s,dp->name,0),dp) : 0);
}

static void si_pubconfig(si_session_t *s) {
	json_descriptor_t *dp;
	si_param_t *pp;
	int i,j;

	/* Pub local */
	for(i=0; i < NALL; i++) {
		dprintf(1,"section: %s\n", allparms[i].name);
		dp = allparms[i].parms;
		for(j=0; j < allparms[i].count; j++) {
			dprintf(1,"dp->name: %s\n", dp[j].name);
			pp = _getp(s,dp[j].name,1);
			if (pp) si_get_config(s,pp,&dp[j]);
		}
	}
	/* XXX do not pub smanet values */
	return;

	/* Pub smanet */
	list_reset(s->desc);
	while((dp = list_get_next(s->desc)) != 0) {
		dprintf(1,"dp->name: %s\n", dp->name);
	}
}

int si_config(void *handle, char *op, char *id, list lp) {
	si_session_t *s = handle;
	solard_confreq_t *req;
	si_param_t *pp;
	json_descriptor_t *dp;
	char message[128],*p;
	int status,pub;
	long start;

	status = 1;
	pub = (strcmp(id,"si_control") == 0 || strcmp(id,"mqtt") == 0) ? 0 : 1;

	dprintf(1,"op: %s, pub: %d\n", op, pub);

	start = mem_used();
	list_reset(lp);
	/* Publish our config */
	if (strcmp(op,"Pub") == 0) {
		si_pubconfig(s);
	} else if (strcmp(op,"Get") == 0) {
		while((p = list_get_next(lp)) != 0) {
			sprintf(message,"%s: not found",p);
			dprintf(1,"p: %s\n", p);
			pp = _getp(s,p,0);
			if (!pp) goto si_config_error;
			dprintf(1,"pp: name: %s, type: %d\n", pp->name, pp->type);
			dp = _getd(s,p);
			if (!dp) goto si_config_error;
			dprintf(1,"dp: name: %s, unit: %s, scale: %f\n", dp->name, dp->units, dp->scale);
			if (si_get_config(s,pp,dp)) goto si_config_error;
		}
	} else {
		while((req = list_get_next(lp)) != 0) {
			dprintf(1,"req: name: %s, type: %d\n", req->name, req->type);
			dprintf(1,"req value: %s\n", req->sval);
			sprintf(message,"%s: not found",req->name);
			pp = _getp(s,req->name,0);
			if (!pp) goto si_config_error;
			dp = _getd(s,req->name);
			if (!dp) goto si_config_error;
			if (si_set_config(s,pp,dp,req,pub)) goto si_config_error;
		}
	}
	{
		solard_inverter_t *inv = s->ap->role_data;
		dprintf(1,"charge_amps: %f\n", inv->charge_amps);
	}

	status = 0;
	strcpy(message,"success");
si_config_error:
	/* If the agent is si_control, dont send status */
	if (pub) agent_send_status(s->ap, s->ap->name, "Config", op, id, status, message);
	dprintf(1,"used: %ld\n", mem_used() - start);
	return status;
}
