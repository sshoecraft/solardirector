
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __JBD_H
#define __JBD_H

#include "agent.h"
#include "battery.h"
#include "jbd_regs.h"
#include "can.h"

#define JBD_NAME_LEN 32
#define JBD_MAX_TEMPS 8
#define JBD_MAX_CELLS 32

#if 0
struct jbd_data {
	float capacity;			/* Battery pack capacity, in AH */
	float voltage;			/* Pack voltage */
	float current;			/* Pack current */
	int ntemps;			/* Number of temps */
	float temps[JBD_MAX_TEMPS];	/* Temp values */
	int ncells;			/* Number of cells, updated by BMS */
	float cellvolt[JBD_MAX_CELLS]; /* Per-cell voltages, updated by BMS */
	float cell_min;
	float cell_max;
	float cell_diff;
	float cell_avg;
	float cell_total;
	uint32_t balancebits;		/* Balance bitmask */
};
typedef struct jbd_data jbd_data_t;
#else
#define jbd_data_t solard_battery_t
#endif

struct jbd_hwinfo {
	char manufacturer[32];		/* Manufacturer name */
	char model[64];			/* Model name */
	char mfgdate[24];		/* the production date, YYYYMMDD, zero terminated */
	char version[16];		/* the software version */
};
typedef struct jbd_hwinfo jbd_hwinfo_t;

struct jbd_session {
	solard_agent_t *ap;
	char tpinfo[SOLARD_TRANSPORT_LEN+SOLARD_TARGET_LEN+SOLARD_TOPTS_LEN+3];
	char transport[SOLARD_TRANSPORT_LEN];
	char target[SOLARD_TARGET_LEN];
	char topts[SOLARD_TOPTS_LEN];
	solard_driver_t *tp;		/* Our transport */
	void *tp_handle;		/* Our transport handle */
	int (*reader)(struct jbd_session *); /* Read can format or std (serial/bt) */
	jbd_hwinfo_t hwinfo;
	uint16_t state;			/* Pack state */
	jbd_data_t data;
	bool start_at_one;
	uint8_t fetstate;		/* Mosfet state */
	uint8_t balancing;		/* 0=off, 1=on, 2=only when charging */
	int errcode;			/* error indicator */
	char errmsg[256];		/* Error message if errcode !0 */
//	config_property_t *data_props;
	bool flatten;
	bool log_power;
	float last_power;
#ifdef JS
	JSPropertySpec *propspec;
	JSPropertySpec *data_propspec;
	jsval data_val;
	jsval hw_val;
	jsval agent_val;
#endif
};
typedef struct jbd_session jbd_session_t;

/* States */
#define JBD_STATE_OPEN		0x01
#define JBD_STATE_RUNNING	0x02
#define JBD_STATE_EEPROM_OPEN	0x80
#define JBD_STATE_CHARGING	BATTERY_STATE_CHARGING
#define JBD_STATE_DISCHARGING	BATTERY_STATE_DISCHARGING
#define JBD_STATE_BALANCING	BATTERY_STATE_BALANCING

struct jbd_protect {
	unsigned sover: 1;		/* Single overvoltage protection */
	unsigned sunder: 1;		/* Single undervoltage protection */
	unsigned gover: 1;		/* Whole group overvoltage protection */
	unsigned gunder: 1;		/* Whole group undervoltage protection */
	unsigned chitemp: 1;		/* Charge over temperature protection */
	unsigned clowtemp: 1;		/* Charge low temperature protection */
	unsigned dhitemp: 1;		/* Discharge over temperature protection */
	unsigned dlowtemp: 1;		/* Discharge low temperature protection */
	unsigned cover: 1;		/* Charge overcurrent protection */
	unsigned cunder: 1;		/* Discharge overcurrent protection */
	unsigned shorted: 1;		/* Short circuit protection */
	unsigned ic: 1;			/* Front detection IC error */
	unsigned mos: 1;		/* Software lock MOS */
};

#define JBD_PKT_START		0xDD
#define JBD_PKT_END		0x77
#define JBD_CMD_READ		0xA5
#define JBD_CMD_WRITE		0x5A

#define JBD_CMD_HWINFO		0x03
#define JBD_CMD_CELLINFO	0x04
#define JBD_CMD_HWVER		0x05
#define JBD_CMD_MOS		0xE1

#define JBD_MOS_CHARGE          0x01
#define JBD_MOS_DISCHARGE       0x02

#define JBD_FUNC_SWITCH 	0x01
#define JBD_FUNC_SCRL		0x02
#define JBD_FUNC_BALANCE_EN	0x04
#define JBD_FUNC_CHG_BALANCE	0x08
#define JBD_FUNC_LED_EN		0x10
#define JBD_FUNC_LED_NUM	0x20
#define JBD_FUNC_RTC		0x40
#define JBD_FUNC_EDV		0x80

#define JBD_NTC1		0x01
#define JBD_NTC2		0x02
#define JBD_NTC3		0x04
#define JBD_NTC4		0x08
#define JBD_NTC5		0x10
#define JBD_NTC6		0x20
#define JBD_NTC7		0x40
#define JBD_NTC8		0x80

extern solard_driver_t *jbd_transports[];

/* jbd.c */
extern solard_driver_t jbd_driver;
int jbd_tp_init(jbd_session_t *s);
int jbd_verify(uint8_t *buf, int len);
int jbd_cmd(uint8_t *pkt, int pkt_size, int action, uint16_t reg, uint8_t *data, int data_len);
int jbd_can_get(jbd_session_t *s, uint32_t id, unsigned char *data, int datalen, int chk);
int jbd_can_get_crc(jbd_session_t *s, int id, unsigned char *data, int len);
int jbd_rw(jbd_session_t *s, uint8_t action, uint8_t reg, uint8_t *data, int datasz);
int jbd_eeprom_open(jbd_session_t *s);
int jbd_eeprom_close(jbd_session_t *s);
int jbd_set_mosfet(jbd_session_t *s, int val);
int jbd_get_balance(jbd_session_t *s);
int jbd_set_balance(jbd_session_t *s, int newbits);
int jbd_reset(jbd_session_t *s);
int jbd_can_get_fetstate(struct jbd_session *s);
int jbd_can_read(struct jbd_session *s);
int jbd_std_read(jbd_session_t *s);
int jbd_get_fetstate(jbd_session_t *s);
int jbd_read(void *handle, uint32_t *, void *buf, int buflen);
int jbd_open(void *handle);
int jbd_close(void *handle);
int jbd_free(void *handle);

/* info */
int jbd_get_hwinfo(jbd_session_t *s);
json_value_t *jbd_get_info(jbd_session_t *s);

/* config.c */
int jbd_agent_init(jbd_session_t *s, int argc, char **argv);
int jbd_config_init(jbd_session_t *s);
int jbd_config_add_params(json_value_t *j);
int jbd_get(void *h, char *name, char *value, char *errmsg);
int jbd_config(void *h, int req, ...);

/* jsfuncs.c */
int jbd_jsinit(jbd_session_t *s);

//#define jbd_getshort(p) ((short) ((*((p)) << 8) | *((p)+1) ))
//#define jbd_putshort(p,v) { float tmp; *((p)) = ((int)(tmp = v) >> 8); *((p+1)) = (int)(tmp = v); }

#define jbd_getshort _gets16
#define jbd_putshort _puts16

#endif
