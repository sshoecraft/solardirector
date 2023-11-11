
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

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

void display(cellmon_config_t *conf) {
	float temps[BATTERY_MAX_TEMPS][BATTERY_MAX_PACKS];
	float values[BATTERY_MAX_CELLS][BATTERY_MAX_PACKS];
	float summ[NSUMM][BATTERY_MAX_PACKS];
	float bsum[NBSUM][BATTERY_MAX_PACKS];
	char lupd[BATTERY_MAX_PACKS][32];
	float v, cell_total, cell_min, cell_max, cell_diff, cell_avg, cap, kwh;
	int x,y,npacks,cells,max_temps,pack_reported;
	char str[32];
	char *slabels[NSUMM] = { "Min","Max","Avg","Diff" };
	char *tlabels[NBSUM] = { "Current","Voltage" };
	solard_battery_t *pp;
	char format[16];
	time_t curr_time;

	npacks = list_count(conf->packs);
	dprintf(1,"npacks: %d\n", npacks);

	cells = 0;
	max_temps = 0;
	list_reset(conf->packs);
	while((pp = list_get_next(conf->packs)) != 0) {
		dprintf(1,"updated: %d\n", check_state(pp,BATTERY_STATE_UPDATED));
		if (!check_state(pp,BATTERY_STATE_UPDATED)) continue;
		if (pp->ntemps > max_temps) max_temps = pp->ntemps;
		if (!cells) cells = pp->ncells;
	}
	dprintf(1,"cells: %d\n",cells);
	if (!cells) return;
	memset(temps,0,sizeof(temps));
	memset(values,0,sizeof(values));
	memset(summ,0,sizeof(summ));
	memset(bsum,0,sizeof(bsum));
	x = 0;
	cap = 0.0;
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
#if 0
				v = pp->cells[y].voltage;
				if (pp->cells[y].voltage < cell_min) cell_min = pp->cells[y].voltage;
				if (pp->cells[y].voltage > cell_max) cell_max = pp->cells[y].voltage;
				values[y][x] = pp->cells[y].voltage;
				cell_total += pp->cells[y].voltage;
#endif
				v = pp->cellvolt[y];
				if (v < cell_min) cell_min = v;
				if (v > cell_max) cell_max = v;
				cell_total += v;
				values[y][x] = v;
//				dprintf(1,"values[%d][%d]: %.3f\n", y, x, values[y][x]);
			}
			cap += pp->capacity;
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

//	if (!debug) {
#ifdef WINDOWS
		system("cls");
		system("time /t");
#else
//		printf("*** running clear ***\n");
		system("clear; echo \"**** $(date) ****\"");
#endif
//	}

	sprintf(format,"%%-%d.%ds ",COLUMN_WIDTH,COLUMN_WIDTH);
	/* Header */
	printf(format,"");
	x = 1;
	list_reset(conf->packs);
	while((pp = list_get_next(conf->packs)) != 0) {
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
	list_reset(conf->packs);
	while((pp = list_get_next(conf->packs)) != 0) printf(format,"----------------------");
	printf("\n");

	/* Charge/Discharge/Balance */
	printf(format,"CDB");
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
