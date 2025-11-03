
#ifndef __AH_H
#define __AH_H

#include "agent.h"
#include "wpi.h"

struct ah_session {
	int fan_control_pin;
	int fan_state_pin;
	bool fan_state;
	bool prev_fan_state;
	int manual_fan_on;
	int cool_state_pin;
	bool cool_state;
	int heat_state_pin;
	bool heat_state;
	int count;
	int refres;
	int reftemp;
	int beta;
	int divres;
	int air_in_ch;
	float air_in;
	int air_out_ch;
	float air_out;
#ifdef WATER
	int water_in_ch;
	float water_in;
	int water_out_ch;
	float water_out;
#endif
	solard_agent_t *ap;
	char last_out[128];
	bool sim_enable;
	int sim_fan;
	int sim_cool;
	int sim_heat;
};
typedef struct ah_session ah_session_t;

extern solard_driver_t ah_driver;

#define AH_MODE_NONE	0
#define AH_MODE_COOL	1
#define AH_MODE_HEAT	2
#define AH_MODE_EHEAT	3

#define AD_BASE 120

char *get_modestr(int mode);

#endif
