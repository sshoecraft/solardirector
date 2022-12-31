
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __SOLARD_PVINVERTER_H
#define __SOLARD_PVINVERTER_H

#include "agent.h"

#define PVINVERTER_NAME_LEN 32

struct solard_pvinverter {
	char name[PVINVERTER_NAME_LEN];
	double input_voltage;
	double input_current;
	double input_power;
	double output_voltage;
	double output_frequency;
	double output_current;
	double output_power;
	double total_yield;
	double daily_yield;
	int errcode;			/* Inverter Error code, 0 if none */
	char errmsg[256];		/* Inverter Error message */
	uint16_t state;			/* Inverter State */
	long last_update;
};
typedef struct solard_pvinverter solard_pvinverter_t;

void pvinverter_dump(solard_pvinverter_t *,int);
int pvinverter_from_json(solard_pvinverter_t *, char *);
json_value_t *pvinverter_to_json(solard_pvinverter_t *);

#endif /* __SOLARD_PVINVERTER_H */
