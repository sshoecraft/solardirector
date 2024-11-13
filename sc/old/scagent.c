
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "sc.h"
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __WIN32
#define STATFUNC _stat
#else
#define STATFUNC stat
#endif

#define AGENTINFO_PROPS(path,conf,log,info) \
	{ "path", DATA_TYPE_STRING, path, sizeof(path)-1, "", 0 }, \
	{ "conf", DATA_TYPE_STRING, conf, sizeof(conf)-1, "", 0 }, \
	{ "log", DATA_TYPE_STRING, log, sizeof(log)-1, "", 0 }, \
	{ "append_log", DATA_TYPE_BOOL, &info->append, 0, "yes", 0 }, \
	{ "managed", DATA_TYPE_BOOL, &info->managed, 0, "yes", 0 }, \
	{ "action", DATA_TYPE_INT, &info->action, 0, "0", 0 }, \
	{ "enabled", DATA_TYPE_BOOL, &info->enabled, 0, "yes", 0 }, \
	{ "monitor_topic", DATA_TYPE_BOOL, &info->monitor_topic, 0, "yes", 0 }, \
	{ "topic", DATA_TYPE_STRING, info->topic, sizeof(info->topic)-1, "", 0 }, \
	{ 0 }

static void sd_agentinfo_dump(sc_agent_t *info) {
	config_property_t props[] = { AGENTINFO_PROPS(info->path,info->conf,info->log,info) };
	config_property_t *p;
	char value[1024];

	dprintf(0,"%-30.30s: %s\n", "name", info->name);
	for(p = props; p->name; p++) {
		conv_type(DATA_TYPE_STRING,value,sizeof(value),p->type,p->dest,p->len);
		dprintf(0,"%-30.30s: %s\n", p->name, value);
	}
}

int agent_get_config(sc_session_t *sd, sc_agent_t *info) {
	char path[256],conf[256],log[256];
	config_property_t props[] = { AGENTINFO_PROPS(path,conf,log,info) };
	config_section_t *s;

	s = config_get_section(sd->ap->cp,info->name);
	dprintf(1,"s: %p\n", s);
	if (!s) return 0;

	config_section_get_properties(s, props);
	conf2path(info->path,sizeof(info->path),path);
	conf2path(info->conf,sizeof(info->conf),conf);
	conf2path(info->log,sizeof(info->log),log);
	sd_agentinfo_dump(info);
	return 0;
}

int agent_set_config(sc_session_t *sd, sc_agent_t *info) {
	char path[256],conf[256],log[256];
//	config_property_t props[] = { AGENTINFO_PROPS(path,conf,log,info) };
	config_section_t *s;

	s = config_get_section(sd->ap->cp,info->name);
	dprintf(1,"s: %p\n", s);
	if (!s) s = config_create_section(sd->ap->cp,info->name,0);
	if (!s) {
		log_error("agent_set_config(%s): unable to get/create config section",info->name);
		return 1;
	}

	path2conf(path,sizeof(path),info->path);
	path2conf(conf,sizeof(conf),info->conf);
	path2conf(log,sizeof(log),info->log);
//	config_section_set_properties(sd->ap->cp, s, props, 1, 0);
	return 0;
}

sc_agent_t *agent_find(sc_session_t *s, char *name) {
	sc_agent_t *ap;

	dprintf(5,"name: %s\n",name);
	list_reset(s->agents);
	while((ap = list_get_next(s->agents)) != 0) {
		dprintf(6,"ap->name: %s\n",ap->name);
		if (strcmp(ap->name,name)==0) return ap;
	}
	return 0;
}

int agent_start(sc_session_t *s, sc_agent_t *info) {
#if 0
	char *args[64],prog[256],configfile[256],logfile[512];
#ifdef MQTT
	char mqtt_info[256];
#endif
#if 0
	char dval[16];
#endif
	int i;

	log_info("Starting agent: %s\n", info->name);

	/* Exec the agent */
	i = 0;
	conf2path(prog,sizeof(prog),info->path);
	args[i++] = prog;
#if 0
	if (debug > 0) {
		args[i++] = "-d";
		sprintf(dval,"%d",debug);
		args[i++] = dval;
	}
#endif
	args[i++] = "-n";
	args[i++] = info->name;
	if (strlen(info->conf)) {
		conf2path(configfile,sizeof(configfile),info->conf);
		args[i++] = "-c";
		args[i++] = configfile;
	}
#ifdef MQTT
	if (!os_exists(configfile,0)) {
		/* Specify the mqtt hosts */
		if (strlen(s->ap->m->uri)) {
			snprintf(mqtt_info,sizeof(mqtt_info),"\"%s,%s,%s,%s\"",s->ap->m->uri,s->ap->m->clientid,s->ap->m->username,s->ap->m->password);
			args[i++] = "-m";
			args[i++] = mqtt_info;
		}
		if (s->ap->config_from_mqtt) args[i++] = "-M";
	}
#endif
	if (strlen(info->log)) {
		conf2path(logfile,sizeof(logfile),info->log);
	} else {
		sprintf(logfile,"%s/%s.log",SOLARD_LOGDIR,info->name);
	}
	dprintf(5,"logfile: %s\n", logfile);
	args[i++] = "-l";
	args[i++] = logfile;
	if (info->append) args[i++] = "-a";
	args[i++] = 0;
	dprintf(4,"calling solard_exec...\n");
	for(i=0; args[i]; i++) dprintf(0,"arg[%d]: %s\n", i, args[i]);
	info->pid = solard_exec(prog,args,0,0);
	dprintf(4,"pid: %d\n", info->pid);
	if (info->pid < 0) {
		log_write(LOG_SYSERR,"agent_start: exec");
		return 1;
	}
	clear_state(info,AGENTINFO_STATUS_MASK);
	time(&info->started);
#endif
	return 0;
}

int agent_stop(sc_session_t *s, sc_agent_t *info) {
	int status,r;

	/* If running, kill it */
	r = 1;
	if (info->pid > 0) {
		log_info("Killing agent: %s\n", info->name);
		solard_kill(info->pid);
		sleep(1);
		if (solard_checkpid(info->pid, &status)) {
			dprintf(1,"status: %d\n", status);
			r = 0;
		}
	}
	return r;
}

#if 0
void agent_warning(sc_session_t *s, sc_agent_t *info, int num) {
	set_state(info,AGENTINFO_STATUS_WARNED);
	log_write(LOG_WARNING,"Agent %s has not reported in %d seconds\n",info->name,num);
}

void agent_error(sc_session_t *s, sc_agent_t *info, int secs) {
	set_state(info,AGENTINFO_STATUS_ERROR);
	if (!info->managed || info->pid < 1) {
		log_write(LOG_ERROR,"%s has not reported in %d seconds, considered lost\n", info->name,secs);
		return;
	}
	log_write(LOG_ERROR,"%s has not reported in %d seconds, killing\n",info->name,secs);
	solard_kill(info->pid);
}

static time_t get_updated(sc_session_t *s, sc_agent_t *info) {
	time_t last_update;

	dprintf(5,"name: %s\n", ap->name);

	last_update = 0;
	if (strcmp(info->role,SOLARD_ROLE_BATTERY)==0) {
		solard_battery_t *bp;

		list_reset(s->batteries);
		while((bp = list_get_next(s->batteries)) != 0) {
			dprintf(7,"bp->name: %s\n", bp->name);
			if (strcmp(bp->name,ap->name) == 0) {
				dprintf(7,"found!\n");
				last_update = bp->last_update;
				break;
			}
		}
	} else if (strcmp(info->role,SOLARD_ROLE_INVERTER)==0) {
		solard_inverter_t *inv;

		list_reset(s->inverters);
		while((inv = list_get_next(s->inverters)) != 0) {
			dprintf(7,"inv->name: %s\n", inv->name);
			if (strcmp(inv->name,info->name) == 0) {
				dprintf(7,"found!\n");
				last_update = inv->last_update;
				break;
			}
		}
	}
	dprintf(5,"returning: %ld\n",last_update);
	return last_update;
}
#endif

#if 0
void agent_check(sc_session_t *s, sc_agent_t *info) {
	time_t cur,diff,last;

	time(&cur);
	dprintf(2,"name: %s, managed: %d\n",info->name,info->managed);
	if (info->managed) {
		/* Give it a little bit to start up */
		if ((cur - info->started) < 31) return;
		if (info->pid < 1) {
			if (agent_start(s,info)) log_write(LOG_ERROR,"unable to start agent!");
			return;
		} else {
			int status;

			/* Check if the process exited */
			dprintf(5,"pid: %d\n", info->pid);
			if (solard_checkpid(info->pid, &status)) {
				dprintf(5,"status: %d\n", status);

				/* XXX if process exits normally do we restart?? */
				/* XXX Number of restart attempts in a specific time */

				/* Re-start agent */
				if (agent_start(s,info)) log_write(LOG_ERROR,"unable to re-start agent!");
				return;
			}
		}
	}
	dprintf(2,"monitor_topic: %d, topic: %s\n", info->monitor_topic, info->topic);
	if (info->monitor_topic && strlen(info->topic)) {
//		last = get_updated(s,info);
		last = 0;
		dprintf(5,"last: %d\n", (int)last);
		if (last < 0) return;
		diff = cur - last;
		dprintf(5,"diff: %d\n", (int)diff);
//		dprintf(1,"agent_error: %d, agent_warning: %d\n", s->agent_error, s->agent_warning);
//		dprintf(1,"AGENTINFO_STATUS_ERROR: %d\n", check_state(info,AGENTINFO_STATUS_ERROR));
//		dprintf(1,"AGENTINFO_STATUS_WARNED: %d\n", check_state(info,AGENTINFO_STATUS_WARNED));
//		dprintf(1,"last_action: %d\n",cur - info->last_action);
		if (s->agent_error && diff >= s->agent_error && !check_state(info,AGENTINFO_STATUS_ERROR))
			agent_error(s,info,diff);
		else if (s->agent_warning && diff >= s->agent_warning && !check_state(info,AGENTINFO_STATUS_WARNED))
			agent_warning(s,info,diff);
	}
}
#endif

#if 0
int agent_get_role(sc_session_t *s, sc_agent_t *info) {
	char temp[128],configfile[256],logfile[256],*args[32], *output, *p;
	cfg_info_t *cfg;
	json_value_t *v;
	int i,r,status;
	struct STATFUNC sb;
	FILE *fp;
//	cfg_section_t *section;

	/* 1st get the path to the agent if we dont have one */
	dprintf(1,"path: %s\n", info->path);
	if (!strlen(info->path) && solard_get_path(info->agent, info->path, sizeof(info->path)-1)) return 1;

	/* Get the tmpdir */
	tmpdir(temp,sizeof(temp)-1);

	/* Create a temp configfile */
	sprintf(configfile,"%s/%s.conf",temp,info->id);
	dprintf(1,"configfile: %s\n", configfile);
	fixpath(configfile,sizeof(configfile)-1);
	cfg = cfg_create(configfile);
	if (!cfg) {
		log_write(LOG_SYSERR,"agent_get_role: cfg_create(%s)",configfile);
		return 1;
	}
//	agentinfo_setcfg(cfg,info->name,info);
//	dprintf(1,"writing config...\n");
//	cfg_write(cfg);

	/* Use a temp logfile */
	sprintf(logfile,"%s/%s.log",temp,info->id);
	dprintf(1,"logfile: %s\n", logfile);
	fixpath(logfile,sizeof(logfile)-1);

	/* Exec the agent with the -I option and capture the output */
	i = 0;
	args[i++] = info->path;
#if 0
	/* Turn off debug output */
//	if (debug > 0) {
		args[i++] = "-d";
		args[i++] = "-1";
//		sprintf(temp,"%d",debug);
//		args[i++] = temp;
//	}
#endif
	args[i++] = "-c";
	args[i++] = configfile;
	args[i++] = "-n";
	args[i++] = info->name;
	args[i++] = "-l";
#ifdef WINDOWS
	args[i++] = "NUL:";
#else
	args[i++] = "/dev/null";
#endif
	args[i++] = "-I";
	args[i++] = 0;

	r = 1;
	output = 0;
	status = solard_exec(info->path,args,logfile,1);
	dprintf(1,"status: %d\n", status);
	if (status != 0) {
//		sprintf(configfile,"type %s",logfile);
//		system(configfile);
		goto agent_get_role_error;
	}

	/* Get the logfile size */
	if (STATFUNC(logfile,&sb) < 0) {
		log_write(LOG_SYSERR,"agent_get_role: stat(%s)",logfile);
		goto agent_get_role_error;
	}
	dprintf(1,"sb.st_size: %d\n", sb.st_size);

	/* Alloc buffer and read log into */
	output = malloc(sb.st_size+1);
	if (!output) {
		log_write(LOG_SYSERR,"agent_get_role: malloc(%d)",sb.st_size);
		goto agent_get_role_error;
	}
	dprintf(1,"output: %p\n", output);

	fp = fopen(logfile,"r");
	if (!fp) {
		log_write(LOG_SYSERR,"agent_get_role: fopen(%s)",logfile);
		goto agent_get_role_error;
	}
	fread(output,1,sb.st_size,fp);
	output[sb.st_size] = 0;
	fclose(fp);

	/* Find the start of the json data */
	dprintf(1,"output: %s\n", output);
	p = strchr(output, '{');
	if (!p) {
		log_write(LOG_ERROR,"agent_get_role: invalid json data");
		goto agent_get_role_error;
	}
	v = json_parse(p);
	if (!v) {
		log_write(LOG_ERROR,"agent_get_role: invalid json data");
		goto agent_get_role_error;
	}
	p = 0;
	if (json_value_get_type(v) == JSON_TYPE_OBJECT) p = json_object_get_string(json_value_object(v), "agent_role");
	if (!p) {
		log_write(LOG_ERROR,"agent_get_role: agent_role not found in json data");
		goto agent_get_role_error;
	}
	dprintf(1,"p: %s\n", p);
	*info->role = 0;
	strncat(info->role,p,sizeof(info->role)-1);

	/* If we have this in our conf already, update it */
//	section = cfg_get_section(s->ap->cfg,info->id);
//	if (section) agentinfo_setcfg(s->ap->cfg,info->id,info);

	r = 0;
agent_get_role_error:
	unlink(configfile);
	unlink(logfile);
	if (output) free(output);
	return r;
}
#endif
