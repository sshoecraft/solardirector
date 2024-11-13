
#ifndef __AH_H
#define __AH_H

#include "agent.h"
#include <wiringPi.h>
#include <ads1115.h>

struct ah_session {
	int count;
	int refres;
	int reftemp;
	int beta;
	int divres;
	int air_in_ch;
	float air_in;
	int air_out_ch;
	float air_out;
	int water_in_ch;
	float water_in;
	int water_out_ch;
	float water_out;
	solard_agent_t *ap;
};
typedef struct ah_session ah_session_t;

extern solard_driver_t ah_driver;

int ah_agent_init(int argc,char **argv,opt_proctab_t *opts,ah_session_t *s);
json_value_t *ah_get_info(ah_session_t *s);
int ah_config(void *,int,...);

#define AD_BASE 120

#endif
