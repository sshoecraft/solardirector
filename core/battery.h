
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __BATTERY_H
#define __BATTERY_H

#include "common.h"

/* For storage class drivers */

/*******************************
*
** Batteries
*
*******************************/

enum CELL_STATUS {
	CELL_STATUS_OK,
	CELL_STATUS_WARNING,
	CELL_STATUS_ERROR,
	CELL_STATUS_UNDERVOLT,
	CELL_STATUS_OVERVOLT
};

enum BATTERY_STATUS {
	BATTERY_STATUS_OK,
	BATTERY_STATUS_UNDERVOLT,
	BATTERY_STATUS_OVERVOLT,
	BATTERY_STATUS_UNDERTEMP,
	BATTERY_STATUS_OVERTEMP,
	BATTERY_STATUS_OVERCURRENT,
};

struct battery_cell {
	char *label;
	enum CELL_STATUS status;
	double voltage;
	double resistance;
};
typedef struct battery_cell battery_cell_t;

#define BATTERY_ID_LEN 38
#define BATTERY_NAME_LEN 32
#define BATTERY_MAX_TEMPS 8
#define BATTERY_MAX_CELLS 32

struct solard_battery {
	char name[BATTERY_NAME_LEN];
	double capacity;			/* Battery pack capacity, in AH */
	double voltage;
	double current;
	double power;
	bool one;			/* start @ 1 vs 0 when flattening */
	int ntemps;			/* Number of temps */
	double temps[BATTERY_MAX_TEMPS];	/* Temp values */
	int ncells;
//	battery_cell_t cells[BATTERY_MAX_CELLS];		/* Cell info */
	double cellvolt[BATTERY_MAX_CELLS];
	double cellres[BATTERY_MAX_CELLS];
	double cell_min;
	double cell_max;
	double cell_diff;
	double cell_avg;
	double cell_total;
	uint32_t balancebits;		/* Balance bitmask */
	int errcode;			/* Battery status, updated by agent */
	char errmsg[256];		/* Error message if status !0, updated by agent */
	uint16_t state;			/* Battery state */
	time_t last_update;		/* Set by client */
};
typedef struct solard_battery solard_battery_t;

/* Battery states */
#define BATTERY_STATE_UPDATED		0x01
#define BATTERY_STATE_HASRES		0x02
#define BATTERY_STATE_CHARGING		0x10
#define BATTERY_STATE_DISCHARGING	0x20
#define BATTERY_STATE_BALANCING		0x40

void battery_dump(solard_battery_t *,int);
#if 1
int battery_from_json(solard_battery_t *bp, char *str);
#else
int battery_from_json(solard_battery_t *bp, json_value_t *json);
#endif
json_value_t *battery_to_json(solard_battery_t *bp);
json_value_t *battery_to_flat_json(solard_battery_t *bp);

#ifdef JS
#include "jsapi.h"
JSObject *js_InitBatteryClass(JSContext *cx, JSObject *parent);
extern JSObject *js_battery_new(JSContext *, JSObject *, solard_battery_t *);
#endif

#endif /* __BATTERY_H */
