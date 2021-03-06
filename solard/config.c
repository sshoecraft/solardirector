
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "solard.h"

/* We will only read/write these config items from/to mqtt */
#define MQTT_CONFIG \
		{ "interval", DATA_TYPE_INT, &conf->interval, 0 }, \
		JSON_PROCTAB_END

int solard_read_config(solard_config_t *conf) {
	solard_agentinfo_t newinfo;
	char temp[1024], *sname, *p;
	int i,count;
	cfg_proctab_t mytab[] = {
		{ "solard", "agent_error", 0, DATA_TYPE_INT, &conf->agent_error, 0, "300" },
		{ "solard", "agent_warning", 0, DATA_TYPE_INT, &conf->agent_warning, 0, "120" },
		{ "solard", "interval", "Agent check interval", DATA_TYPE_INT, &conf->interval, 0, "15" },
		CFG_PROCTAB_END
	};
	char topic[SOLARD_TOPIC_SIZE];
	solard_message_t *msg;

	cfg_get_tab(conf->ap->cfg,mytab);
	if (debug) cfg_disp_tab(mytab,"info",0);

	/* Get agent count */
	p = cfg_get_item(conf->ap->cfg, "agents", "count");
	if (p) {
		/* Get agents */
		count = atoi(p);
		for(i=0; i < count; i++) {
			sprintf(temp,"A%02d",i);
			dprintf(1,"num temp: %s\n",temp);
			sname = cfg_get_item(conf->ap->cfg,"agents",temp);
			if (sname) {
				memset(&newinfo,0,sizeof(newinfo));
				agentinfo_getcfg(conf->ap->cfg, sname, &newinfo);
				agentinfo_add(conf,&newinfo);
			}
		}
	}
	if (conf->ap->cfg->filename) return 0;

	/* NO CONFIG FILE - READ CONFIG FROM MQTT */
	return 0;

	/* Sub to our settings topic */
	agent_mktopic(topic,sizeof(topic)-1,conf->ap,conf->ap->instance_name,SOLARD_FUNC_CONFIG,"Settings",0);
//	if (agent_sub(conf->ap,conf->ap->instance_name,SOLARD_FUNC_CONFIG,"Settings",0)) return 1;
//	sprintf(topic,"%s/%s/%s/%s/+",SOLARD_TOPIC_ROOT,SOLARD_ROLE_CONTROLLER,conf->name,SOLARD_FUNC_CONFIG);
        if (mqtt_sub(conf->ap->m,topic)) return 1;

	/* Ingest any config messages */
	dprintf(1,"ingesting...\n");
	solard_ingest(conf->ap->mq,2);
	dprintf(1,"back from ingest...\n");

	/* Process messages as config requests */
	dprintf(1,"mq count: %d\n", list_count(conf->ap->mq));
	list_reset(conf->ap->mq);
	while((msg = list_get_next(conf->ap->mq)) != 0) {
		agentinfo_get(conf,msg->data);
		list_delete(conf->ap->mq,msg);
	}
	mqtt_unsub(conf->ap->m,topic);

	dprintf(1,"done\n");
	return 0;
}

int solard_write_config(solard_config_t *conf) {
	solard_agentinfo_t *info;
	char temp[16];
	int i;

	dprintf(1,"writing config...\n");

#if 0
	if (!conf->ap->cfg) {
		char configfile[256];
		sprintf(configfile,"%s/solard.conf",SOLARD_ETCDIR);
		dprintf(1,"configfile: %s\n", configfile);
		conf->ap->cfg = cfg_create(configfile);
		if (!conf->ap->cfg) return 1;
	}
#endif


	dprintf(1,"conf->ap->cfg: %p\n", conf->ap->cfg);
	if (conf->ap->cfg) {
		sprintf(temp,"%d",list_count(conf->agents));
		dprintf(1,"count temp: %s\n",temp);
		cfg_set_item(conf->ap->cfg, "agents", "count", 0, temp);
		i = 0;
		list_reset(conf->agents);
		while((info = list_get_next(conf->agents)) != 0) {
			agentinfo_setcfg(conf->ap->cfg,info->id,info);
			sprintf(temp,"A%02d",i++);
			dprintf(1,"num temp: %s\n",temp);
			cfg_set_item(conf->ap->cfg, "agents", temp, 0, info->id);
//			agentinfo_pub(conf,info);
		}
		cfg_write(conf->ap->cfg);

	/* NO CONFIG FILE - WRITE CONFIG TO MQTT */
	} else {
		json_proctab_t params[] = { MQTT_CONFIG },*p;

		/* Publish the agent info */
		while((info = list_get_next(conf->agents)) != 0) agentinfo_pub(conf,info);

		/* Publish the param info */
		for(p=params; p->field; p++) {
			dprintf(1,"pub: %s\n", p->field);
		}
	}
	return 0;
}

#if 0
static json_descriptor_t charge_parms[] = {
	{ "batteries", DATA_TYPE_LIST, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ "min_voltage", DATA_TYPE_FLOAT, "range", 3, (float []){ SI_VOLTAGE_MIN, SI_VOLTAGE_MAX, .1 }, 1, (char *[]){ "Min Battery Voltage" }, "V", 1, "%2.1f" },
	{ "charge_start_voltage", DATA_TYPE_FLOAT, "range", 3, (float []){ SI_VOLTAGE_MIN, SI_VOLTAGE_MAX, .1 }, 1, (char *[]){ "Charge start voltage" }, "V", 1, "%2.1f" },
	{ "charge_end_voltage", DATA_TYPE_FLOAT, "range", 3, (float []){ SI_VOLTAGE_MIN, SI_VOLTAGE_MAX, .1 }, 1, (char *[]){ "Charge end voltage" }, "V", 1, "%2.1f" },
	{ "charge_amps", DATA_TYPE_FLOAT, "range", 3, (float []){ 0, 5000, .1 }, 1, (char *[]){ "Charge Amps" }, "A", 1, "%.1f" },
	{ "charge_mode", DATA_TYPE_INT, "select", 3, (int []){ 0, 1, 2 }, 1, (char *[]){ "Off","CC","CV" }, 0, 1, 0 },
	{ "discharge_amps", DATA_TYPE_FLOAT, "range", 3, (float []){ 0, 5000, .1 }, 1, (char *[]){ "Discharge Amps" }, "A", 1, "%.1f" },
	{ "charge_min_amps", DATA_TYPE_FLOAT, "range", 3, (float []){ 0, 5000, .1 }, 1, (char *[]){ "Minimum charge amps" }, "A", 1, "%.1f" },
	{ "charge_focus_amps", DATA_TYPE_BOOL, 0, 0, 0, 1, (char *[]){ "Increase voltage to maintain charge amps" }, 0, 0 },
	{ "sim_step", DATA_TYPE_FLOAT, "range", 3, (float []){ 0, 5000, .1 }, 1, (char *[]){ "SIM Voltage Step" }, "V", 1, "%.1f" },
	{ "interval", DATA_TYPE_INT, "range", 3, (int []){ 0, 10000, 1 }, 0, 0, 0, 1, 0 },
};
#define NCHG (sizeof(charge_params)/sizeof(json_descriptor_t))

#if 0
static json_descriptor_t other_params[] = {
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
#endif

static struct parmdir {
	char *name;
	json_descriptor_t *parms;
	int count;
} allparms[] = {
#if 0
	{ "Charge parameters", charge_params, NCHG },
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

json_descriptor_t *_getd(solard_config_t *s,char *name) {
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

	return 0;
}

int solard_config_add_info(solard_config_t *s, json_value_t *j) {
	int x,y;
	json_value_t *ca,*o,*a;
	struct parmdir *dp;

	/* Configuration array */
	ca = json_create_array();

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

#if 0
static int solard_get_config(solard_config_t *s, json_value_t *v, char *message) {
#if 0
	char topic[200];
	char temp[72];
	unsigned char bval;
	short wval;
	long lval;
	float fval;
	double dval;

	dprintf(1,"dp: name: %s, unit: %s, scale: %f\n", dp->name, dp->units, dp->scale);

	temp[0] = 0;
	dprintf(1,"pp->type: %d(%s)\n", pp->type,typestr(pp->type));
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
		else sprintf(temp,"%lf",dval);
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
	sprintf(topic,"%s/%s/%s/%s/%s",SOLARD_TOPIC_ROOT,s->ap->role->name,s->ap->instance_name,SOLARD_FUNC_CONFIG,pp->name);
	dprintf(1,"topic: %s, temp: %s\n", topic, temp);
	return mqtt_pub(s->ap->m,topic,temp,1);
#endif
	return 1;
}
#endif

static solard_agentinfo_t *agent_infobyname(solard_config_t *s, char *name) {
	register solard_agentinfo_t *info;

	list_reset(s->agents);
	while((info = list_get_next(s->agents)) != 0) {
		if (strcmp(info->name,name)==0)
			return info;
	}
	return 0;
}

static int _parseinfo(solard_config_t *s, json_value_t *v, char *message, char *name, int namesz, char *dvalue, int valuesz) {
	json_value_t *ov;
	json_object_t *o;
	char *p,*str;

	dprintf(1,"type: %d (%s)\n", v->type, json_typestr(v->type));

	/* Must be an object with 1 value and that value must be a string */
	if (v->type != JSONObject) {
		strcpy(message,"must be a json object");
		return 1;
	}
	o = v->value.object;
	dprintf(1,"count: %d\n", o->count);
	if (o->count != 1) {
		strcpy(message,"json object must have 1 value");
		return 1;
	}
	ov = o->values[0];
	dprintf(1,"name: %s, type: %d (%s)\n", o->names[0], ov->type, json_typestr(ov->type));
	if (ov->type != JSONString) {
		strcpy(message,"value must be a string");
		return 1;
	}
	str = (char *)json_string(ov);
	dprintf(1,"str: %s\n", str);
	p = strchr(str,':');
	if (!p) {
		strcpy(message,"invalid format");
		return 1;
	}
	*p = 0;
	strncpy(name,str,namesz);
	strncpy(dvalue,p+1,valuesz);
	return 0;
}

static int solard_set_config(solard_config_t *conf, json_value_t *v, char *message) {
	char name[64],value[128],*p,*str;
	solard_agentinfo_t *info;
	int name_changed;
	list lp;

	if (_parseinfo(conf,v,message,name,sizeof(name),value,sizeof(value))) return 1;
	dprintf(1,"name: %s, value: %s\n", name, value);
	info = agent_infobyname(conf,name);
	dprintf(1,"info: %p\n", info);
	if (!info) {
		strcpy(message,"agent not found");
		return 1;
	}
	conv_type(DATA_TYPE_LIST,&lp,0,DATA_TYPE_STRING,value,strlen(value));
	dprintf(1,"lp: %p\n", lp);
	if (!lp) {
		strcpy(message,"key/value pairs must be comma seperated");
		return 1;
	}
	dprintf(1,"count: %d\n", list_count(lp));
	name_changed = 0;
	list_reset(lp);
	while((str = list_get_next(lp)) != 0) {
		strncpy(name,strele(0,"=",str),sizeof(name)-1);
		p = strele(1,"=",str);
		dprintf(1,"name: %s, value: %s\n", name, p);
		if (!strlen(name) || !strlen(p)) {
			strcpy(message,"format is keyword=value");
			return 1;
		}
		cfg_set_item(conf->ap->cfg,info->id,name,0,p);
		if (strcmp(name,"name")==0) name_changed = 1;
	}
	dprintf(1,"name_changed: %d\n", name_changed);
	if (name_changed) {
		/* if we dont manage it, then cant change the name */
		if (!info->managed) {
			strcpy(message,"agent is not managed, unable to change name");
			return 1;
		}
		if (info->pid > 0) {
			log_info("Killing %s/%s due to name change\n", info->role, info->name);
			solard_kill(info->pid);
		}
	}
	if (cfg_is_dirty(conf->ap->cfg)) {
		/* getcfg writes sname into id, get a local copy */
		strncpy(name,info->id,sizeof(name)-1);
		agentinfo_getcfg(conf->ap->cfg,name,info);
		solard_write_config(conf);
	}
	return 0;
}

static int solard_add_config(solard_config_t *conf, json_value_t *v, char *message) {
	solard_agentinfo_t newinfo,*info = &newinfo;
	char name[64],value[128],key[64],*p,*str;
	list lp;
	int have_managed;

	strcpy(message,"unable to add agent");

	if (_parseinfo(conf,v,message,name,sizeof(name),value,sizeof(value))) return 1;
	dprintf(1,"name: %s, value: %s\n", name, value);

	conv_type(DATA_TYPE_LIST,&lp,0,DATA_TYPE_STRING,value,strlen(value));
	dprintf(1,"lp: %p\n", lp);
	if (!lp) return 1;
	dprintf(1,"count: %d\n", list_count(lp));
	memset(&newinfo,0,sizeof(newinfo));
	strncpy(newinfo.agent,name,sizeof(newinfo.agent)-1);
	have_managed = 0;
	list_reset(lp);
	while((str = list_get_next(lp)) != 0) {
		p = strchr(str,'=');
		if (!p) continue;
		*p++ = 0;
		strncpy(key,str,sizeof(key)-1);
//		dprintf(1,"key: %s, value: %s\n", key, p);
		if (strcmp(key,"managed")==0) have_managed = 1;
		agentinfo_set(&newinfo,key,p);
	}
	list_destroy(lp);
	if (!strlen(newinfo.name)) strncpy(newinfo.name,name,sizeof(newinfo.name)-1);
	/* Default to managed if not specified */
	if (!have_managed) info->managed = 1;
	agentinfo_newid(&newinfo);
	if (agentinfo_add(conf,&newinfo)) return 1;
	solard_write_config(conf);
	return 0;
}

static int _doget(solard_config_t *conf, char *name, char *message) {
	solard_agentinfo_t *info;

	*message = 0;

	dprintf(1,"name: %s\n", name);

	info = agent_infobyname(conf,name);
	if (!info) {
		strcpy(message,"agent not found");
		return 1;
	}
	agentinfo_pub(conf,info);
	return 0;
}

static int _dodel(solard_config_t *conf, char *name, char *message) {
	solard_agentinfo_t *info;

	dprintf(1,"name: %s\n", name);

	info = agent_infobyname(conf,name);
	if (!info) {
		strcpy(message,"agent not found");
		return 1;
	}
	return 1;
}

typedef int (gdfunc_t)(solard_config_t *, char *, char *);

static int solard_gd_config(solard_config_t *conf, json_value_t *v, char *message, gdfunc_t *func) {
	json_object_t *o;
	json_array_t *a;
	int i;
	char *str;

	dprintf(1,"type: %d (%s)\n", v->type, json_typestr(v->type));

	switch(v->type) {
	case JSONObject:
		o = v->value.object;
		dprintf(1,"count: %d\n", o->count);
		for(i=0; i < o->count; i++) if (solard_gd_config(conf,o->values[i],message,func)) return 1;
		break;
	case JSONArray:
		a = v->value.array;
		dprintf(1,"count: %d\n", a->count);
		for(i=0; i < a->count; i++) if (solard_gd_config(conf,a->items[i],message,func)) return 1;
		break;
	case JSONString:
		str = (char *)json_string(v);
		if (func(conf,str,message)) return 1;
		break;
	default:
		dprintf(1,"unhandled type!");
		strcpy(message,"unhandled type");
		return 1;
	}
	return 0;
}

int solard_send_status(solard_config_t *conf, char *func, char *action, char *id, int status, char *message) {
	char msg[4096];
	char topic[SOLARD_TOPIC_LEN];
	json_value_t *o;

	dprintf(1,"func: %s, action: %s, id: %s, status: %d, message: %s\n", func, action, id, status, message);

	o = json_create_object();
	json_add_number(o,"status",status);
	json_add_string(o,"message",message);
	json_tostring(o,msg,sizeof(msg)-1,0);
	json_destroy(o);
	/* /root/role/name/func/op/Status/id */
	sprintf(topic,"%s/%s/%s/%s/%s/Status/%s",SOLARD_TOPIC_ROOT,SOLARD_ROLE_CONTROLLER,conf->name,func,action,id);
	dprintf(1,"topic: %s\n", topic);
	return mqtt_pub(conf->ap->m,topic,msg,0);
}

int solard_config(solard_config_t *conf, solard_message_t *msg) {
	json_value_t *v;
	char message[128];
	int status;
	long start;

	solard_message_dump(msg,1);

	start = mem_used();
	status = 1;
	v = json_parse(msg->data);
	if (!v) return 1;
	/* CRUD */
	if (strcmp(msg->action,"Add") == 0 && solard_add_config(conf,v,message)) goto solard_config_error;
	else if (strcmp(msg->action,"Get") == 0 && solard_gd_config(conf,v,message,_doget)) goto solard_config_error;
	else if (strcmp(msg->action,"Set") == 0 && solard_set_config(conf,v,message)) goto solard_config_error;
	else if (strcmp(msg->action,"Del") == 0 && solard_gd_config(conf,v,message,_dodel)) goto solard_config_error;
	json_destroy(v);

	status = 0;
	strcpy(message,"success");
solard_config_error:
	/* If the agent is solard_control, dont send status */
	if (strcmp(msg->id,"solard_control")!=0) solard_send_status(conf, "Config", msg->action, msg->id, status, message);
	dprintf(1,"used: %ld\n", mem_used() - start);
	return status;
	return 1;
}
