
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __JK_H
#define __JK_H

#include "agent.h"
#include "battery.h"

struct jk_hwinfo {
	char manufacturer[32];		/* Maker */
	char device[64];		/* Device name */
	char model[64];			/* Model name */
	char mfgdate[9];		/* the production date, YYYYMMDD, zero terminated */
	float version;			/* the software version */
	char hwvers[34];
	char swvers[34];
};
typedef struct jk_hwinfo jk_hwinfo_t;

#define JK_MAX_TEMPS 8
#define JK_MAX_CELLS 32

#if 0
struct jk_data {
	float capacity;			/* Battery pack capacity, in AH */
	float voltage;			/* Pack voltage */
	float current;			/* Pack current */
	int ntemps;			/* Number of temps */
	float temps[JK_MAX_TEMPS];	/* Temp values */
	int ncells;			/* Number of cells */
	float cellvolt[JK_MAX_CELLS];	/* Per-cell voltages */
	float cellres[JK_MAX_CELLS];	/* Per-cell resistances */
	float cell_min;
	float cell_max;
	float cell_diff;
	float cell_avg;
	float cell_total;
	uint32_t balancebits;		/* Balance bitmask */
};
typedef struct jk_data jk_data_t;
#else
#define jk_data_t solard_battery_t
#endif

struct jk_session {
	char name[SOLARD_NAME_LEN];	/* Our instance name (from battery) */
	solard_agent_t *ap;		/* Agent config pointer */
	char tpinfo[SOLARD_TRANSPORT_LEN+SOLARD_TARGET_LEN+SOLARD_TOPTS_LEN+3];
	char transport[SOLARD_TRANSPORT_LEN];
	char target[SOLARD_TARGET_LEN];
	char topts[SOLARD_TOPTS_LEN];
	solard_driver_t *tp;		/* Our transport */
	void *tp_handle;		/* Our transport handle */
	uint16_t state;			/* Pack state */
	bool flatten;
	bool log_power;
	float last_power;
	jk_hwinfo_t hwinfo;
	solard_battery_t data;
	bool start_at_one;
	uint8_t fetstate;		/* Mosfet state */
	uint8_t balancing;		/* 0=off, 1=on, 2=only when charging */
	int errcode;			/* error indicator */
	char errmsg[256];		/* Error message if errcode !0 */
#ifdef JS
	JSPropertySpec *props;
	JSPropertySpec *data_props;
	jsval data_val;
	jsval hw_val;
	jsval agent_val;
#endif
};
typedef struct jk_session jk_session_t;

extern solard_driver_t jk_driver;

/* States */
#define JK_STATE_OPEN		0x01
#define JK_STATE_RUNNING	0x02
#if 0
#define JK_STATE_CHARGING	0x10
#define JK_STATE_DISCHARGING	0x20
#define JK_STATE_BALANCING	0x40
#else
#define JK_STATE_CHARGING	BATTERY_STATE_CHARGING
#define JK_STATE_DISCHARGING	BATTERY_STATE_DISCHARGING
#define JK_STATE_BALANCING	BATTERY_STATE_BALANCING
#endif

/* I/O */
int jk_can_get_crc(jk_session_t *s, uint32_t id, unsigned char *data, int len);
int jk_can_get(jk_session_t *s, uint32_t id, unsigned char *data, int datalen, int chk);
int jk_eeprom_start(jk_session_t *s);
int jk_eeprom_end(jk_session_t *s);
int jk_rw(jk_session_t *, uint8_t action, uint8_t reg, uint8_t *data, int datasz);
int jk_verify(uint8_t *buf, int len);
int jk_cmd(uint8_t *pkt, int pkt_size, int action, uint16_t reg, uint8_t *data, int data_len);
int jk_rw(jk_session_t *s, uint8_t action, uint8_t reg, uint8_t *data, int datasz);

/* Driver */
int jk_tp_init(jk_session_t *s);
int jk_open(void *handle);
int jk_close(void *handle);
int jk_get_hwinfo(jk_session_t *s);

/* Config */
int jk_agent_init(jk_session_t *s, int argc, char **argv);
int jk_config(void *,int,...);
int jk_config_add_params(json_object_t *);

/* Info */
json_value_t *jk_get_info(void *handle);

/* jsfuncs */
int jk_jsinit(jk_session_t *s);

#define jk_getshort(p) ((short) ((*((p)) << 8) | *((p)+1) ))
#define jk_putshort(p,v) { float tmp; *((p)) = ((int)(tmp = v) >> 8); *((p+1)) = (int)(tmp = v); }

#define JK_PKT_START		0xDD
#define JK_PKT_END		0x77
#define JK_CMD_READ		0xA5
#define JK_CMD_WRITE		0x5A

#define JK_CMD_HWINFO		0x03
#define JK_CMD_CELLINFO		0x04
#define JK_CMD_HWVER		0x05
#define JK_CMD_MOS		0xE1

#define JK_MOS_CHARGE          0x01
#define JK_MOS_DISCHARGE       0x02

#endif
