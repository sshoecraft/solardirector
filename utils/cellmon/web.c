
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#include "cellmon.h"

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

#define NSUMM 4
#define NBSUM 2

void termstart(char *format) { sprintf(format,"%%-%d.%ds ",COLUMN_WIDTH,COLUMN_WIDTH); }
void termtstart(void) { printf("\n"); }
void termthitem(char *format, char *p) { printf(format,p); }
void termtitem(char *format, char *p) { printf(format,p); }
void termtspace(void) { printf("\n"); }
void termtend(void) { printf("\n"); }
//	printf("%s","<table style=\"width: 960px; height: 81px;\" border=\"0\" cellpadding=\"0\" cellspacing=\"0\" width:100%\">");
//	printf("%s","<table style=\"width: 1000px%; height: 81px;\" border=\"0\" cellpadding=\"0\" cellspacing=\"0\">");
//	printf("%s","<table style=\"width: 1000px%;\" border=\"0\" cellpadding=\"0\" cellspacing=\"0\">");
void webstart(void) { printf("%s","<table style=\"width: 1000px%;\" border=\"0\" cellpadding=\"0\" cellspacing=\"0\">"); }
void webtstart(void) { printf("<tr>"); }
void webthitem(char *format, char *p) { printf("<th>%s</th>",p); }
void webtitem(char *p) { printf("<td>%s</td>",p); }
void webtspace(void) { printf("<tr></tr>"); }
void webtend(void) { printf("</tr>"); }

typedef void (*dfuncnp_t)(void);
typedef void (*dfunc_t)(char *);

void web_display(cellmon_config_t *conf, int web_flag) {
	float temps[BATTERY_MAX_TEMPS][BATTERY_MAX_PACKS];
	float values[BATTERY_MAX_CELLS][BATTERY_MAX_PACKS];
	float summ[NSUMM][BATTERY_MAX_PACKS];
	float bsum[NBSUM][BATTERY_MAX_PACKS];
	char lupd[BATTERY_MAX_PACKS][32];
	float cell_total, cell_min, cell_max, cell_diff, cell_avg, min_cap;
	int x,y,npacks,cells,max_temps,pack_reported;
	char str[32];
	char *slabels[NSUMM] = { "Min","Max","Avg","Diff" };
	char *tlabels[NBSUM] = { "Current","Voltage" };
	solard_battery_t *pp;
	char format[16];
	time_t curr_time;
	dfuncnp_t tstart,tspace,tend;
	dfunc_t start,thitem,titem;

	npacks = list_count(conf->packs);

	cells = 0;
	max_temps = 0;
	list_reset(conf->packs);
	while((pp = list_get_next(conf->packs)) != 0) {
		if (!check_state(pp,BATTERY_STATE_UPDATED)) continue;
		if (pp->ntemps > max_temps) max_temps = pp->ntemps;
		if (!cells) cells = pp->ncells;
	}
	dprintf(1,"cells: %d\n",cells);
	if (!cells) {
		dprintf(1,"error: no cells to display!\n");
		return;
	}
	memset(temps,0,sizeof(temps));
	memset(values,0,sizeof(values));
	memset(summ,0,sizeof(summ));
	memset(bsum,0,sizeof(bsum));
	x = 0;
	pack_reported = 0;
	list_reset(conf->packs);
	while((pp = list_get_next(conf->packs)) != 0) {
		if (!check_state(pp,BATTERY_STATE_UPDATED)) {
			cell_min = cell_max = cell_avg = cell_total = 0;
		} else {
			cell_min = 9999999999.0;
			cell_max = 0.0;
			cell_total = 0;
			for(y=0; y < pp->ncells; y++) {
				if (pp->cellvolt[y] < cell_min) cell_min = pp->cellvolt[y];
				if (pp->cellvolt[y] > cell_max) cell_max = pp->cellvolt[y];
				values[y][x] = pp->cellvolt[y];
//				dprintf(1,"values[%d][%d]: %.3f\n", y, x, values[y][x]);
				cell_total += pp->cellvolt[y];
			}
			dprintf(2,"min_cap: %.3f, pp->capacity: %.3f\n", min_cap, pp->capacity);
			if (pp->capacity < min_cap) min_cap = pp->capacity;
			pack_reported++;
			for(y=0; y < pp->ntemps; y++) temps[y][x] = pp->temps[y];
			cell_avg = cell_total / pp->ncells;
		}
		cell_diff = cell_max - cell_min;
		dprintf(1,"conf->cells: %d\n", conf->cells);
		dprintf(1,"cells: total: %.3f, min: %.3f, max: %.3f, diff: %.3f, avg: %.3f\n",
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

	if (web_flag) {
		start = webstart;
		tstart = webtstart;
		thitem = webthitem;
		titem = webtitem;
		tspace = webtspace;
		tend = webtend;
	}

	tstart();

	/* Header */
//	printf("<tr>");
//	printf(format,"");
	printf("<th></th>");
	thitem("");
	x = 1;
	list_reset(conf->packs);
	while((pp = list_get_next(conf->packs)) != 0) {
		if (strlen(pp->name) > COLUMN_WIDTH) {
			char temp[32];
			int i;

			i = strlen(pp->name) - COLUMN_WIDTH;
			strcpy(temp,&pp->name[i]);
//			printf(format,temp);
//			printf("<th>%s</th>",temp);
			thitem(temp);
		} else {
//			printf(format,pp->name);
//			printf("<th>%s</th>",pp->name);
			thitem(pp->name);
		}
	}
//	printf("\n");
	printf("</tr>");
#if 0
	/* Lines */
	printf(format,"");
	list_reset(conf->packs);
	while((pp = list_get_next(conf->packs)) != 0) printf(format,"----------------------");
	printf("\n");
#endif

	/* Charge/Discharge/Balance */
//	printf(format,"CDB");
	printf("<tr>");
	printf("<td>CDB</td>");
	list_reset(conf->packs);
	while((pp = list_get_next(conf->packs)) != 0) {
		if (check_state(pp,BATTERY_STATE_UPDATED)) {
			str[0] = check_state(pp,BATTERY_STATE_CHARGING) ? '*' : ' ';
			str[1] = check_state(pp,BATTERY_STATE_DISCHARGING) ? '*' : ' ';
			str[2] = check_state(pp,BATTERY_STATE_BALANCING) ? '*' : ' ';
		} else {
			str[0] = str[1] = str[2] = ' ';
		}
		str[3] = 0;
//		printf(format,str);
		printf("<td>%s</td>",str);
	}
//	printf("\n");
	printf("</tr>");

#define FTEMP(v) ( ( ( (float)(v) * 9.0 ) / 5.0) + 32.0)
	/* Temps */
	for(y=0; y < max_temps; y++) {
		sprintf(str,"Temp%d",y+1);
//		printf(format,str);
		printf("<tr>");
		printf("<td>%s</td>",str);
		for(x=0; x < npacks; x++) {
			sprintf(str,"%2.1f",temps[y][x]);
//			printf(format,str);
			printf("<td>%s</td>",str);
		}
//		printf("\n");
//		tend();
		printf("</tr>");
	}
//	if (max_temps) printf("\n");

	/* Cell values */
	for(y=0; y < cells; y++) {
		sprintf(str,"Cell%02d",y+1);
//		printf(format,str);
//		titem(str);
		printf("<tr>");
		printf("<td>%s</td>",str);
		for(x=0; x < npacks; x++) {
			sprintf(str,"%.3f",values[y][x]);
//			printf(format,str);
//			titem(str);
			printf("<td>%s</td>",str);
		}
//		printf("\n");
//		tend():
		printf("</tr>");
	}
//	printf("\n");
	printf("<tr></tr>");

	/* Summ values */
	for(y=0; y < NSUMM; y++) {
//		sprintf(str,format,slabels[y]);
		printf("<tr>");
		printf("<td>%s</td>",slabels[y]);
		for(x=0; x < npacks; x++) {
			sprintf(str,"%.3f",summ[y][x]);
//			printf(format,str);
			printf("<td>%s</td>",str);
		}
//		printf("\n");
		printf("</tr>");
	}
//	printf("\n");
	printf("<tr></tr>");

	/* Current/Total */
	for(y=0; y < NBSUM; y++) {
//		sprintf(str,format,tlabels[y]);
		printf("<tr>");
		printf("<td>%s</td>",tlabels[y]);
		for(x=0; x < npacks; x++) {
			sprintf(str,"%2.3f",bsum[y][x]);
//			printf(format,str);
			printf("<td>%s</td>",str);
		}
//		printf("\n");
		printf("</tr>");
	}

	/* Last update */
//	printf("\n");

	printf("<tr>");
//	printf(format,"updated");
	printf("<td>updated</td>");
//	for(x=0; x < npacks; x++) printf(format,lupd[x]);
	for(x=0; x < npacks; x++) printf("<td>%s</td>",lupd[x]);
//	printf("\n");
	printf("</tr>");

	if (web_flag) printf("</table>");
}
