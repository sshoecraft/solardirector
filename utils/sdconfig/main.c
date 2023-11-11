
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "sdconfig.h"

#define TESTING 0
#define TESTLVL 4

#define AGENT_CALLED		0x1000
#define STATUS_CHECKED		0x2000

struct sdconfig_agent_private {
	time_t last_call;
}; 
typedef struct sdconfig_agent_private sdconfig_agent_private_t;

int main(int argc,char **argv) {
	char value[16384],temp[20000];
	char save_file[256];
	char target[SOLARD_NAME_LEN];
	char func[SOLARD_FUNC_LEN];
	solard_client_t *c;
	client_agentinfo_t *ap;
	bool alias_match;
	int timeout,list_flag,ping_flag;
//	read_flag
	int func_flag,agent_flag;
	int retry_time;
//		{ "-r|ignore persistant config",&read_flag,DATA_TYPE_BOOL,0,0,"0" },
	opt_proctab_t opts[] = {
                /* Spec, dest, type len, reqd, default val, have */
		{ "-a|match aliases as well",&alias_match,DATA_TYPE_BOOL,0,0,"no" },
		{ "-t:#|wait time",&timeout,DATA_TYPE_INT,0,0,"30" },
		{ "-r:#|retry time",&retry_time,DATA_TYPE_INT,0,0,0 },
		{ "-l|list parameters",&list_flag,DATA_TYPE_BOOL,0,0,"0" },
		{ "-f|list functions",&func_flag,DATA_TYPE_BOOL,0,0,"0" },
		{ "-g|list agents",&agent_flag,DATA_TYPE_BOOL,0,0,"0" },
		{ "-p|ping target(s)",&ping_flag,DATA_TYPE_BOOL,0,0,"0" },
		{ "-W:|save config",&save_file,DATA_TYPE_STRING,sizeof(save_file)-1,0,"" },
		{ ":target|agent name",&target,DATA_TYPE_STRING,sizeof(target)-1,1,0 },
		{ ":func|agent func",&func,DATA_TYPE_STRING,sizeof(func)-1,0,0 },
		OPTS_END
	};
	bool cfg_exact;
	config_property_t props[] = {
		/* name, type, dest, dsize, def, flags, scope, values, labels, units, scale, precision, trigger, ctx */
		{ "exact", DATA_TYPE_BOOL, &cfg_exact, 0, "yes", 0, 0, 0, 0, 0, 0, 1, 0, 0 },
		{ 0 }
	};
	bool exact;
	time_t start,now,diff;
	config_function_t *f;
	solard_message_t *msg;
	int called,have_all,i,r,found;
	list output,funcs;
	sdconfig_agent_private_t *priv;
	json_object_t *results;
	json_value_t *v;
#if TESTING
	char *args[] = { "sdconfig", "-d", STRINGIFY(TESTLVL), "-l", "Battery/jbd" };
	argc = (sizeof(args)/sizeof(char *));
	argv = args;
#endif

	c = client_init(argc,argv,"1.0",opts,"sdconfig",CLIENT_FLAG_NOJS,props,0,0,0);
	if (!c) return 1;
//	c->addmq = true;
	exact = cfg_exact;
	if (alias_match) exact = false;
	dprintf(dlevel,"exact: %d, alias_match: %d, cfg_exact: %d\n", exact, alias_match, cfg_exact);
	retry_time = timeout / 3;
	dprintf(dlevel,"timeout: %d, retry_time: %d\n", timeout, retry_time);
	if (retry_time < 1) retry_time = 1;

	if (ping_flag) strcpy(func,"ping");

	dprintf(dlevel,"target: %s, func: %s\n", target, func);
	if (!strlen(func) && !list_flag && !func_flag && !agent_flag && !strlen(save_file)) {
		log_error("a flag or func must be specified\n");
		return 1;
	}

        argc -= optind;
        argv += optind;
        optind = 0;

	dprintf(dlevel,"my clientid: %s\n", c->m->clientid);
	sprintf(temp,"%s/%s/%s/#",SOLARD_TOPIC_ROOT,SOLARD_TOPIC_CLIENTS,c->m->clientid);
	dprintf(dlevel,"temp: %s\n", temp);
	mqtt_sub(c->m,temp);

	/* Sleep 1 second to ingest persistant configs */
	sleep(1);
	if (!list_count(c->agents)) {
		sleep(1);
		if (!list_count(c->agents)) {
			log_error("no agents responded, aborting\n");
			return 1;
		}
	}

	if (list_flag || strcmp(func,"-l") == 0) {
		list_reset(c->agents);
		while((ap = list_get_next(c->agents)) != 0) {
			if (!client_matchagent(ap,target,exact)) continue;
			do_list(ap);
		}
		return 0;
	}

	dprintf(dlevel,"save_file(%d): %s\n", strlen(save_file), save_file);
	if (strlen(save_file)) {
		list_reset(c->agents);
		while((ap = list_get_next(c->agents)) != 0) {
			if (!client_matchagent(ap,target,exact)) continue;
			if (!ap->cp) {
				printf("%s: no config\n",ap->name);
				continue;
			}
			debug = 6;
			config_set_filename(ap->cp,save_file,CONFIG_FILE_FORMAT_AUTO);
			config_write(ap->cp);
			break;
		}
		return 0;
	}

	if (func_flag) {
		list_reset(c->agents);
		while((ap = list_get_next(c->agents)) != 0) {
			if (!client_matchagent(ap,target,exact)) continue;
			funcs = config_get_funcs(ap->cp);
			if (!list_count(funcs)) {
				printf("%s has no functions\n",target);
				return 0;
			}
			printf("%s functions:\n",ap->name);
			printf("  %-25.25s %s\n", "Name", "# args");
			list_reset(funcs);
			while((f = list_get_next(funcs)) != 0) printf("  %-25.25s %d\n", f->name, f->nargs);
		}
		return 0;
	}

	if (agent_flag) {
		list_reset(c->agents);
		while((ap = list_get_next(c->agents)) != 0) {
			printf("%s\n", ap->name);
		}
		return 0;
	}

	list_reset(c->agents);
	while((ap = list_get_next(c->agents)) != 0) {
		if (!client_matchagent(ap,target,exact)) continue;
		clear_bit(ap->state,AGENT_CALLED);
	}

	/* For each agent ... call the func */
	found = called = 0;
	dprintf(dlevel,"agent count: %d\n", list_count(c->agents));
	list_reset(c->agents);
	while((ap = list_get_next(c->agents)) != 0) {
		if (!client_matchagent(ap,target,exact)) continue;
		found++;
		if (check_bit(ap->state,AGENT_CALLED)) continue;
		if ((f = client_getagentfunc(ap,func)) == 0) {
			printf("%s: function %s not found\n", ap->name, func);
			continue;
		}

		dprintf(dlevel,"%s(%s): calling: %s\n", ap->name, ap->id, f->name);
		if (client_callagentfunc(ap,f,argc,argv)) {
			printf("%s\n", ap->errmsg);
			continue;
		}
		set_bit(ap->state,AGENT_CALLED);
		if (!ap->private) {
			ap->private = malloc(sizeof(*priv));
			if (!ap->private) {
				log_syserror("malloc private");
				return 1;
			}
		}
		priv = ap->private;
		time(&priv->last_call);
		called++;
	}
	dprintf(dlevel,"called: %d\n", called);
	if (!found) {
		printf("%s: agent not found\n", target);
		return 1;
	}

	/* Reset status for all matches */
	list_reset(c->agents);
	while((ap = list_get_next(c->agents)) != 0) {
		if (!client_matchagent(ap,target,exact)) continue;
		clear_bit(ap->state,CLIENT_AGENTINFO_STATUS);
		clear_bit(ap->state,CLIENT_AGENTINFO_CHECKED);
		ap->addmq = true;
	}

	/* get the replies... */
	time(&start);
	output = list_create();
	r = 0;
	while(1) {
		have_all = 1;
		list_reset(c->agents);
		while((ap = list_get_next(c->agents)) != 0) {
			if (!check_bit(ap->state,AGENT_CALLED)) continue;
			priv = ap->private;
			dprintf(dlevel,"%s...count: %d\n", ap->name, list_count(ap->mq));
			results = 0;
			list_reset(ap->mq);
			if ((msg = list_get_next(ap->mq)) != 0) {
				if (msg->type == SOLARD_MESSAGE_TYPE_CLIENT) {
//					solard_message_dump(msg,0);
					v = json_parse(msg->data);
					dprintf(dlevel,"v: %p\n", v);
					if (v && json_value_get_type(v) == JSON_TYPE_OBJECT) {
						json_value_t *sv;
						json_object_t *o;

						o = json_value_object(v);
						sv = json_object_get_value(o,"status");
						dprintf(dlevel,"sv: %p\n", sv);
						if (sv && json_value_get_type(sv) == JSON_TYPE_NUMBER) {
							char *str;

							ap->status = json_value_get_number(sv);
							dprintf(dlevel,"NEW status: %d\n", ap->status);
						 	str = json_object_get_string(o,"message");
							dprintf(dlevel,"str: %s\n", str);
							/* XXX is errmsg a "must have?" */
							if (str) strncpy(ap->errmsg,str,sizeof(ap->errmsg)-1);
							else ap->errmsg[0] = 0;
							set_bit(ap->state,CLIENT_AGENTINFO_STATUS);
							results = json_object_get_object(o,"results");
							dprintf(dlevel,"results: %p\n", results);
						}
					}
				}
				list_delete(ap->mq,msg);
			}
			dprintf(dlevel,"%s: status: %d\n", ap->name, check_bit(ap->state,CLIENT_AGENTINFO_STATUS));
			if (check_bit(ap->state,CLIENT_AGENTINFO_STATUS)) {
				dprintf(dlevel,"got reply: status: %d, errmsg: %s\n", ap->status, ap->errmsg);
				dprintf(dlevel,"%s: checked: %d\n", ap->name, (ap->state & CLIENT_AGENTINFO_CHECKED) != 0);
				if (check_bit(ap->state,CLIENT_AGENTINFO_CHECKED)) continue;
				set_bit(ap->state,CLIENT_AGENTINFO_CHECKED);
				/* If this was a "get" request, get the value from the agent */
				dprintf(dlevel,"status: %d, isget: %d\n", ap->status, (strcasecmp(func,"get") == 0));
				if (ap->status == 0 && strcasecmp(func,"get") == 0 && results) {
					json_value_t *ov;
					int type;

					if (!ap->cp) {
						log_error("%s: no config data\n",ap->name);
						continue;
					}
					dprintf(dlevel,"results: %p\n", results);
					for(i=0; i < argc; i++) {
						dprintf(dlevel,"count: %d\n", results->count);
						for(int j=0; j < results->count; j++) {
							dprintf(dlevel,"result[%d]: name: %s, type: %s\n", j, results->names[j],
								json_typestr(json_value_get_type(results->values[j])));
							dprintf(dlevel,"argv[%d]: %s\n", i, argv[i]);
							if (strcmp(results->names[j],argv[i]) != 0) continue;
							ov = results->values[j];
#if 0
						}
						ov = json_object_dotget_value(results,argv[i]);
						dprintf(dlevel,"ov: %p\n", ov);
						printf("ov: %p\n", ov);
						if (ov) {
#endif
							type = json_value_get_type(ov);
							dprintf(dlevel,"type: %s\n", json_typestr(type));
							if (type == JSON_TYPE_OBJECT || type == JSON_TYPE_ARRAY) {
								json_dumps_r(ov, value, sizeof(value)-1);
							} else {
								json_to_type(DATA_TYPE_STRING,value,sizeof(value)-1,ov);
							}
							sprintf(temp,"%s: %s=%s", ap->name, argv[i], value);
							list_add(output,temp,strlen(temp)+1);
						}
					}
				} else {
					sprintf(temp,"%s: %s", ap->name, ap->errmsg);
					list_add(output,temp,strlen(temp)+1);
				}
			} else {
				have_all = 0;
			}
			list_purge(ap->mq);
		}
		dprintf(dlevel,"have_all: %d\n", have_all);
		if (have_all) {
			break;
		} else {
			list_reset(c->agents);
			while((ap = list_get_next(c->agents)) != 0) {
				if (!client_matchagent(ap,target,exact)) continue;
				if (check_bit(ap->state,CLIENT_AGENTINFO_STATUS)) continue;
				priv = ap->private;
				if (!priv) {
					printf("priv is null for agent %s\n", ap->name);
					continue;
				}
				/* try again after retry_time seconds */
				time(&now);
				diff = now - priv->last_call;
				dprintf(dlevel,"%s: diff: %d, retry_time: %d\n", ap->name, diff, retry_time);
				if (diff > retry_time)  {
					dprintf(dlevel,"%s: calling %s again...\n", ap->name, func);
					f = client_getagentfunc(ap,func);
					client_callagentfunc(ap,f,argc,argv);
					time(&priv->last_call);
				}
			}
		}
		time(&now);
		dprintf(dlevel,"now - start: %d, timeout: %d\n", (now - start), timeout-1);
		if ((now - start) >= timeout-1)  {
			list_reset(c->agents);
			while((ap = list_get_next(c->agents)) != 0) {
				if (!client_matchagent(ap,target,exact)) continue;
				if (check_bit(ap->state,CLIENT_AGENTINFO_STATUS)) continue;
				sprintf(temp,"%s: timeout", ap->name);
				list_add(output,temp,strlen(temp)+1);
				r++;
			}
			break;
		}
		sleep(1);
	}
	dprintf(dlevel,"count: %d\n", list_count(output));
	if (list_count(output)) {
		char *p;

		list_sort(output,0,0);
		list_reset(output);
		while((p = list_get_next(output)) != 0) printf("%s\n",p);
	}
	return (r > 0);
}
