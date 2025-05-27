
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

//#include "cellmon.h"
#include "client.h"
#include "battery.h"

#define TESTING 0

struct _agent_info {
	char name[SOLARD_NAME_LEN];
	char role[SOLARD_ROLE_LEN];
	solard_battery_t data;
	bool have_data;
};
typedef struct _agent_info agent_info_t;

int sort_agents(void *i1, void *i2) {
	agent_info_t *p1 = (agent_info_t *)i1;
	agent_info_t *p2 = (agent_info_t *)i2;

	dprintf(7,"p1: %s, p2: %s\n", p1->name, p2->name);
	return strcmp(p1->name,p2->name);
}

int process_messages(solard_client_t *c, list agents) {
	solard_message_t *msg;
	agent_info_t *ap;
	int count,found;

	int ldlevel = dlevel;

	count = 0;
	list_reset(c->mq);
	dprintf(ldlevel,"mq.length: %d\n", list_count(c->mq));
	while((msg = list_get_next(c->mq)) != 0) {
//		solard_message_dump(msg,-1);
		found = 0;
		list_reset(agents);
		while((ap = list_get_next(agents)) != 0) {
			if (strcmp(ap->name,msg->name) == 0) {
				found=1;
				break;
			}
		}
		if (!found  && strcmp(msg->func,SOLARD_FUNC_INFO) == 0) {
			agent_info_t newagent;
			json_value_t *v;
			char *role;

			memset(&newagent,0,sizeof(newagent));
			strncpy(newagent.name,msg->name,sizeof(newagent.name));
			v = json_parse(msg->data);
			if (!v) continue;
			role = json_object_dotget_string(json_value_object(v),"agent_role");
			if (!role) continue;
			strncpy(newagent.role,role,sizeof(newagent.role));
			ap = list_add(agents,&newagent,sizeof(newagent));
			list_sort(agents,sort_agents,0);
			dprintf(ldlevel,"added agent: %s\n", ap->name);
		} else if (found && strcmp(ap->role,SOLARD_ROLE_BATTERY) == 0 && strcmp(msg->func,SOLARD_FUNC_DATA) == 0) {
			printf("getting data...\n");
			battery_from_json(&ap->data,msg->data);
			time(&ap->data.last_update);
			ap->have_data = true;
		}
	}
	list_purge(c->mq);
        return count;
}

void display(list packs);
int main(int argc,char **argv) {
	solard_client_t *c;
	char configfile[256];
	bool no_clear;
	int refresh;
	opt_proctab_t opts[] = {
		/* Spec, dest, type len, reqd, default val, have */
		{ "-N|dont clear screen",&no_clear,DATA_TYPE_BOOL,0,0,"no" },
		{ "-r:#|refresh rate",&refresh,DATA_TYPE_INT,0,0,"1" },
		OPTS_END
	};
	bool average,minmax;
	config_property_t props[] = {
		/* name, type, dest, dsize, def, flags, scope, values, labels, units, scale, precision, trigger, ctx */
		{ "average", DATA_TYPE_BOOL, &average, 0, "yes", 0, },
		{ "minmax", DATA_TYPE_BOOL, &minmax, 0, "yes", 0, },
		{ 0 }
	};
	list agents;
	time_t t;

	find_config_file("cellmon.conf",configfile,sizeof(configfile)-1);
	dprintf(1,"configfile: %s\n", configfile);

	average = minmax = false;
	c = client_init(argc,argv,"cellmon","1.0",opts,CLIENT_FLAG_NOINFLUX|CLIENT_FLAG_NOJS|CLIENT_FLAG_NOEVENT,props);
	if (!c) return 1;
	c->addmq = true;
	agents = list_create();
	mqtt_sub(c->m,SOLARD_TOPIC_ROOT"/"SOLARD_TOPIC_AGENTS"/#");

	/* main loop */
	while(1) {
		process_messages(c,agents);
		if (!no_clear) {
			char dt[32];
			time(&t);
			strcpy(dt,asctime(localtime(&t)));
			if (strlen(dt)) dt[strlen(dt)-1] = 0;
			clear_screen();
			printf("**** %s *****\n", dt);
		}
		display(agents);
		sleep(refresh);
//		dprintf(0,"count: %d, used: %ld\n", list_count(packs), mem_used());
	}
	return 0;
}

#define COLUMN_WIDTH 7

static char *gettd(time_t start, time_t end) {
	static char ts[32];
	int diff,mins;

	diff = (int)difftime(end,start);
	dprintf(1,"start: %ld, end: %ld, diff: %d\n", start, end, diff);
	if (diff > 0) {
		mins = diff / 60;
		if (mins) diff -= (mins * 60);
		dprintf(1,"mins: %d, diff: %d\n", mins, diff);
	} else {
		mins = diff = 0;
	}
	if (mins >= 5) {
		sprintf(ts,"--:--");
	} else {
		sprintf(ts,"%02d:%02d",mins,diff);
	}
	dprintf(1,"ts: %s\n", ts);
	return ts;
}

#define BATTERY_MAX_PACKS 32
#define BATTERY_MAX_TEMPS 8
#define NSUMM 4
#define NBSUM 2

void display(list agents) {
	float temps[BATTERY_MAX_TEMPS][BATTERY_MAX_PACKS];
	float values[BATTERY_MAX_CELLS][BATTERY_MAX_PACKS];
	float summ[NSUMM][BATTERY_MAX_PACKS];
	float bsum[NBSUM][BATTERY_MAX_PACKS];
	char lupd[BATTERY_MAX_PACKS][32];
	agent_info_t *ap;
	float v, cell_total, cell_min, cell_max, cell_diff, cell_avg, cap, kwh;
	int x,y,npacks,cells,max_temps,pack_reported;
	char str[32];
	char *slabels[NSUMM] = { "Min","Max","Avg","Diff" };
	char *tlabels[NBSUM] = { "Current","Voltage" };
	solard_battery_t *pp;
	char format[16];
	time_t curr_time;

	int ldlevel = dlevel;

	npacks = 0;
	cells = 0;
	max_temps = 0;
	list_reset(agents);
	while((ap = list_get_next(agents)) != 0) {
		if (!ap->have_data) continue;
		pp = &ap->data;
		if (pp->ntemps > max_temps) max_temps = pp->ntemps;
		if (!cells) cells = pp->ncells;
		npacks++;
	}
	dprintf(ldlevel,"npacks: %d\n", npacks);
	if (!npacks) return;
	dprintf(ldlevel,"cells: %d\n",cells);
	if (!cells) return;
	memset(temps,0,sizeof(temps));
	memset(values,0,sizeof(values));
	memset(summ,0,sizeof(summ));
	memset(bsum,0,sizeof(bsum));
	x = 0;
	cap = 0.0;
	pack_reported = 0;
	list_reset(agents);
	while((ap = list_get_next(agents)) != 0) {
		if (!ap->have_data) continue;
		pp = &ap->data;
		cell_min = 9999999999.0;
		cell_max = 0.0;
		cell_total = 0;
		for(y=0; y < pp->ncells; y++) {
			v = pp->cellvolt[y];
			if (v < cell_min) cell_min = v;
			if (v > cell_max) cell_max = v;
			cell_total += v;
			values[y][x] = v;
			dprintf(ldlevel,"values[%d][%d]: %.3f\n", y, x, values[y][x]);
		}
		cap += pp->capacity;
		pack_reported++;
		for(y=0; y < pp->ntemps; y++) temps[y][x] = pp->temps[y];
		cell_avg = cell_total / pp->ncells;
		cell_diff = cell_max - cell_min;
		dprintf(ldlevel,"cells: total: %.3f, min: %.3f, max: %.3f, diff: %.3f, avg: %.3f\n",
			cell_total, cell_min, cell_max, cell_diff, cell_avg);
		summ[0][x] = cell_min;
		summ[1][x] = cell_max;
		summ[2][x] = cell_avg;
		summ[3][x] = cell_diff;
		bsum[0][x] = pp->current;
		bsum[1][x] = cell_total;
		time(&curr_time);
		strcpy(lupd[x],gettd(pp->last_update,curr_time));
		x++;
	}

	sprintf(format,"%%-%d.%ds ",COLUMN_WIDTH,COLUMN_WIDTH);
	/* Header */
	printf(format,"");
	x = 1;
	list_reset(agents);
	while((ap = list_get_next(agents)) != 0) {
		if (!ap->have_data) continue;
		pp = &ap->data;
		if (strlen(pp->name) > COLUMN_WIDTH) {
			char temp[32];
			int i;

			i = strlen(pp->name) - COLUMN_WIDTH;
			strcpy(temp,&pp->name[i]);
			printf(format,temp);
		} else {
			printf(format,pp->name);
		}
	}
	printf("\n");
	/* Lines */
	printf(format,"");
	list_reset(agents);
	while((ap = list_get_next(agents)) != 0) {
		if (!ap->have_data) continue;
		printf(format,"----------------------");
	}
	printf("\n");

	/* Charge/Discharge/Balance */
	printf(format,"CDB");
	list_reset(agents);
	while((ap = list_get_next(agents)) != 0) {
		if (!ap->have_data) continue;
		pp = &ap->data;
		str[0] = check_state(pp,BATTERY_STATE_CHARGING) ? '*' : ' ';
		str[1] = check_state(pp,BATTERY_STATE_DISCHARGING) ? '*' : ' ';
		str[2] = check_state(pp,BATTERY_STATE_BALANCING) ? '*' : ' ';
//			str[0] = str[1] = str[2] = ' ';
		str[3] = 0;
		printf(format,str);
	}
	printf("\n");

#define FTEMP(v) ( ( ( (float)(v) * 9.0 ) / 5.0) + 32.0)
	/* Temps */
	for(y=0; y < max_temps; y++) {
		sprintf(str,"Temp%d",y+1);
		printf(format,str);
		for(x=0; x < npacks; x++) {
			sprintf(str,"%2.1f",FTEMP(temps[y][x]));
//			sprintf(str,"%2.1f",temps[y][x]);
			printf(format,str);
		}
		printf("\n");
	}
	if (max_temps) printf("\n");

	/* Cell values */
	for(y=0; y < cells; y++) {
		sprintf(str,"Cell%02d",y+1);
		printf(format,str);
		for(x=0; x < npacks; x++) {
			sprintf(str,"%.3f",values[y][x]);
			printf(format,str);
		}
		printf("\n");
	}
	printf("\n");

	/* Summ values */
	for(y=0; y < NSUMM; y++) {
		printf(format,slabels[y]);
		for(x=0; x < npacks; x++) {
			sprintf(str,"%.3f",summ[y][x]);
			printf(format,str);
		}
		printf("\n");
	}
	printf("\n");

	/* Current/Total */
	for(y=0; y < NBSUM; y++) {
		printf(format,tlabels[y]);
		for(x=0; x < npacks; x++) {
			sprintf(str,"%2.3f",bsum[y][x]);
			printf(format,str);
		}
		printf("\n");
	}

	/* Last update */
	printf("\n");
	printf(format,"updated");
	for(x=0; x < npacks; x++) printf(format,lupd[x]);
	printf("\n");

	printf("\n");
	printf("Total packs: %d\n", pack_reported);
	kwh = (cap * 48.0) / 1000.0;
//	printf("Capacity: %2.1f\n", cap);
	printf("kWh: %2.1f (80%%: %2.1f, 60%%: %2.1f)\n", kwh, (kwh * .8), (kwh * .6));
}
