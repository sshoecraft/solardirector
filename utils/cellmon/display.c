
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

#if 0
void calc_remain(char *dest,int secs) {
	int days,hours,mins;

	dprintf(dlevel,"secs: %d\n", secs);

	days = (secs / 86400);
	dprintf(dlevel,"days: %d\n", days);
	if (days > 0) {
		secs -= (days * 86400);
		dprintf(dlevel,"new secs: %d\n", secs);
	}
	hours = (secs / 3600);
	dprintf(dlevel,"hours: %d\n", hours);
	if (hours > 0) {
		secs -= (hours * 3600.0);
		dprintf(dlevel,"new secs: %d\n", secs);
	}
	mins = (secs / 60);
	dprintf(dlevel,"mins: %d\n", mins);
	if (mins) {
		secs -= (mins * 60);
		dprintf(dlevel,"new secs: %d\n", secs);
	}
#if 0
//	if (days > 1000) secs = (-1);
	if (secs >= 0) {
		char *p = dest;
		if (days > 0) p += sprintf(p,"%d days", days);
		if (hours > 0) {
			if (strlen(dest)) p += sprintf(p,", ");
			if (hours == 1) p += sprintf(p,"1 hour");
			else p += sprintf(p,"%d hours", hours);
		}
		if (mins > 0) {
			if (strlen(dest)) p += sprintf(p,", ");
			if (mins == 1) p += sprintf(p,"1 minute");
			else p += sprintf(p,"%d minutes", mins);
		}
		if (secs > 0) {
			if (strlen(dest)) p += sprintf(p,", ");
			if (secs == 1) p += sprintf(p,"1 second");
			else p += sprintf(p,"%d seconds", secs);
		}
	}
#else
	sprintf(dest,"%d:%02d:%02d:%02d",days,hours,mins,secs);
#endif
	dprintf(dlevel,"dest: %s\n", dest);
}

static void get_remain(char *dest, float kwh, float current) {
	float amps = current;
	float secs;

	if (amps < 0) amps *= (-1);
	dprintf(dlevel,"kwh: %f, amps: %f\n", kwh, amps);
	secs = (kwh / amps);
	dprintf(dlevel,"secs: %f\n", secs);
	secs = (kwh / amps) * 3600;
	dprintf(dlevel,"secs: %f\n", secs);
	calc_remain(dest,(int)secs);
}
#endif

#define NSUMM 4
#define NBSUM 2

void display(cellmon_config_t *conf, bool do_average, bool do_minmax) {
	float temps[BATTERY_MAX_TEMPS][BATTERY_MAX_PACKS];
	float values[BATTERY_MAX_CELLS][BATTERY_MAX_PACKS];
	float summ[NSUMM][BATTERY_MAX_PACKS];
	float bsum[NBSUM][BATTERY_MAX_PACKS];
	char lupd[BATTERY_MAX_PACKS][32];
	float v, cell_total, cell_min, cell_max, cell_diff, cell_avg, cap, kwh;
	float total_current,true_min,true_max;
	int x,y,npacks,cells,max_temps,pack_reported;
	char str[32];
//	char *slabels[NSUMM] = { "Min","Max","Avg","Diff" };
	char *slabels[NSUMM] = { "Min","Max","Diff","Avg" };
	char *tlabels[NBSUM] = { "Current","Voltage" };
	solard_battery_t *pp,avgpack,*app;
	char format[16];
	time_t curr_time;
	list packs;

	packs = list_dup(conf->packs);

	cells = 0;
	max_temps = 0;
	list_reset(packs);
	while((pp = list_get_next(packs)) != 0) {
		dprintf(1,"updated: %d\n", check_state(pp,BATTERY_STATE_UPDATED));
		if (!check_state(pp,BATTERY_STATE_UPDATED)) continue;
		if (pp->ntemps > max_temps) max_temps = pp->ntemps;
		if (!cells) cells = pp->ncells;
	}
	dprintf(1,"cells: %d\n",cells);
	if (!cells) return;

	if (do_average) {
		/* Setup the "average" pack */
		memset(&avgpack,0,sizeof(avgpack));
		strcpy(avgpack.name,"average");
		avgpack.ncells = cells;
		avgpack.ntemps = max_temps;
		set_state(&avgpack,BATTERY_STATE_UPDATED);
		app = list_add(packs,&avgpack,sizeof(avgpack));
	}

	npacks = list_count(packs);
	dprintf(1,"npacks: %d\n", npacks);

	memset(temps,0,sizeof(temps));
	memset(values,0,sizeof(values));
	memset(summ,0,sizeof(summ));
	memset(bsum,0,sizeof(bsum));
	x = 0;
	cap = 0.0;
	pack_reported = 0;
	total_current = 0;
	true_min = 9999999999.0;
	true_max = 0.0;
	list_reset(packs);
	while((pp = list_get_next(packs)) != 0) {
		if (strcmp(pp->name,"average") == 0) continue;
		if (!check_state(pp,BATTERY_STATE_UPDATED)) {
			cell_min = cell_max = cell_avg = cell_total = 0;
		} else {
			cell_min = 9999999999.0;
			cell_max = 0.0;
			cell_total = 0;
			for(y=0; y < pp->ncells; y++) {
				v = pp->cellvolt[y];
				if (v < cell_min) cell_min = v;
				if (v > cell_max) cell_max = v;
				cell_total += v;
				values[y][x] = v;
//				dprintf(1,"values[%d][%d]: %.3f\n", y, x, values[y][x]);
				if (do_average) {
					app->cellvolt[y] += pp->cellvolt[y];
//					dprintf(1,"new cellvolt[%d]: %f\n", y, app->cellvolt[y]);
				}
			}
			cap += pp->capacity;
			pack_reported++;
			for(y=0; y < pp->ntemps; y++) {
				temps[y][x] = pp->temps[y];
				if (do_average) {
					app->temps[y] += pp->temps[y];
//					dprintf(1,"new temps[%d]: %f\n", y, app->temps[y]);
				}
			}
			cell_avg = cell_total / pp->ncells;
		}
		cell_diff = cell_max - cell_min;
//		dprintf(1,"conf->cells: %d\n", conf->cells);
		dprintf(1,"cells: total: %.3f, min: %.3f, max: %.3f, diff: %.3f, avg: %.3f\n",
			cell_total, cell_min, cell_max, cell_diff, cell_avg);
		summ[0][x] = cell_min;
		summ[1][x] = cell_max;
		summ[2][x] = cell_diff;
		summ[3][x] = cell_avg;
		bsum[0][x] = pp->current;
		if (do_average) app->current += pp->current;
		total_current += pp->current;
		bsum[1][x] = cell_total;
		time(&curr_time);
		strcpy(lupd[x],gettd(pp->last_update,curr_time));
		if (cell_min < true_min) true_min = cell_min;
		if (cell_max > true_max) true_max = cell_max;
		x++;
	}
	if (do_average) {
		x=0;
		list_reset(packs);
		while((pp = list_get_next(packs)) != 0) {
			if (strcmp(pp->name,"average") == 0) {
				int d = npacks - 1;

				dprintf(1,"d: %d\n", d);
				dprintf(1,"pp: %p, app: %p\n", pp, app);
				if (!do_minmax) {
					cell_min = 9999999999.0;
					cell_max = 0.0;
					cell_total = 0;
				}
				for(y=0; y < pp->ncells; y++) {
					dprintf(1,"cellvolt[%d]: %f\n", y, pp->cellvolt[y]);
					v = pp->cellvolt[y] / d;
					if (!do_minmax) {
						dprintf(1,"v: %f, cell_min: %f, cell_max: %f\n", v, cell_min, cell_max);
						if (v < cell_min) cell_min = v;
						if (v > cell_max) cell_max = v;
						cell_total += v;
					}
					values[y][x] = v;
					dprintf(1,"new values[%d][%d]: %f\n", y, x, values[y][x]);
				}
				for(y=0; y < pp->ntemps; y++) {
					temps[y][x] = app->temps[y] / d;
					dprintf(1,"temps[%d][%d]: %f\n", y, x, temps[y][x]);
				}
				dprintf(1,"cell_total: %f\n", cell_total);
				if (do_minmax) {
					summ[0][x] = true_min;
					summ[1][x] = true_max;
					summ[2][x] = true_max - true_min;
					summ[3][x] = (cell_min + cell_max) / 2;
				} else {
					summ[0][x] = cell_min;
					summ[1][x] = cell_max;
					summ[2][x] = cell_max - cell_min;
					summ[3][x] = cell_total / pp->ncells;
				}
				bsum[0][x] = pp->current / d;
				bsum[1][x] = cell_total;
				strcpy(lupd[x],"");
			}
			x++;
		}
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
	list_reset(packs);
	while((pp = list_get_next(packs)) != 0) {
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
	list_reset(packs);
	while((pp = list_get_next(packs)) != 0) printf(format,"----------------------");
	printf("\n");

	/* Charge/Discharge/Balance */
	printf(format,"CDB");
	list_reset(packs);
	while((pp = list_get_next(packs)) != 0) {
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
	printf("Total current: %2.1f\n", total_current);
	printf("Total packs: %d\n", pack_reported);
	kwh = (cap * 48.0) / 1000.0;
//	printf("Capacity: %2.1f\n", cap);
	printf("kWh: %2.1f (80%%: %2.1f, 60%%: %2.1f)\n", kwh, (kwh * .8), (kwh * .6));

#if 0
	if (total_current < 0) {
		char remain[32], remain_80[32], remain_60[32];

		get_remain(remain, cap, total_current);
		get_remain(remain_80, cap * .8, total_current);
		get_remain(remain_60, cap * .6, total_current);
		printf("remain: %s (80%%: %s, 60%%: %s)\n", remain, remain_80, remain_60);
	}
#endif

	list_destroy(packs);
}
