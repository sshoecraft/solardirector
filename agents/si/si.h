
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __SOLARD_SI_H
#define __SOLARD_SI_H

#include "agent.h"
#ifdef SMANET
#include "smanet.h"
#endif
#include <pthread.h>
#include "can.h"

struct si_raw_data {
	int16_t *active_grid_l1;
	int16_t *active_grid_l2;
	int16_t *active_grid_l3;

	int16_t *active_si_l1;
	int16_t *active_si_l2;
	int16_t *active_si_l3;

	int16_t *reactive_grid_l1;
	int16_t *reactive_grid_l2;
	int16_t *reactive_grid_l3;

	int16_t *reactive_si_l1;
	int16_t *reactive_si_l2;
	int16_t *reactive_si_l3;

	int16_t *ac1_voltage_l1;
	int16_t *ac1_voltage_l2;
	int16_t *ac1_voltage_l3;
	int16_t *ac1_frequency;

	int16_t *battery_voltage;
	int16_t *battery_current;
	int16_t *battery_temp;
	int16_t *battery_soc;

	int16_t *battery_soh;
	int8_t *charging_proc;
	int8_t *state;
	int16_t *errmsg;
	int16_t *battery_cvsp;

	uint16_t *relay_state;
	uint16_t *relay_bits1;
	uint16_t *relay_bits2;
	uint8_t *synch_bits;

	int16_t *TotLodPwr;

	int16_t *ac2_voltage_l1;
	int16_t *ac2_voltage_l2;
	int16_t *ac2_voltage_l3;
	int16_t *ac2_frequency;

	int16_t *PVPwrAt;
	int16_t *GdCsmpPwrAt;
	int16_t *GdFeedPwr;
};
typedef struct si_raw_data si_raw_data_t;

struct si_data {
	double active_grid_l1;
	double active_grid_l2;
	double active_grid_l3;
	double active_si_l1;
	double active_si_l2;
	double active_si_l3;
	double reactive_grid_l1;
	double reactive_grid_l2;
	double reactive_grid_l3;
	double reactive_si_l1;
	double reactive_si_l2;
	double reactive_si_l3;
	double ac1_voltage_l1;
	double ac1_voltage_l2;
	double ac1_voltage_l3;
	double ac1_voltage;
	double ac1_frequency;
	double ac1_current;
	double ac1_power;
	double battery_voltage;
	double battery_current;
	double battery_power;
	double battery_temp;
	double battery_soc;
	double battery_soh;
	double battery_cvsp;
	bool relay1;
	bool relay2;
	bool s1_relay1;
	bool s1_relay2;
	bool s2_relay1;
	bool s2_relay2;
	bool s3_relay1;
	bool s3_relay2;
	bool GnRn;
	bool s1_GnRn;
	bool s2_GnRn;
	bool s3_GnRn;
	bool AutoGn;
	bool AutoLodExt;
	bool AutoLodSoc;
	bool Tm1;
	bool Tm2;
	bool ExtPwrDer;
	bool ExtVfOk;
	bool GdOn;
	bool GnOn;
	bool Error;
	bool Run;
	bool BatFan;
	bool AcdCir;
	bool MccBatFan;
	bool MccAutoLod;
	bool Chp;
	bool ChpAdd;
	bool SiComRemote;
	bool OverLoad;
	bool ExtSrcConn;
	bool Silent;
	bool Current;
	bool FeedSelfC;
	bool Esave;
	double TotLodPwr;
	uint8_t charging_proc;
	uint8_t state;
	uint16_t errmsg;
	double ac2_voltage_l1;
	double ac2_voltage_l2;
	double ac2_voltage_l3;
	double ac2_voltage;
	double ac2_frequency;
	double ac2_current;
	double ac2_power;
	double PVPwrAt;
	double GdCsmpPwrAt;
	double GdFeedPwr;
};
typedef struct si_data si_data_t;

enum GRID_CONNECT_REASON {
	GRID_CONNECT_REASON_SOC = 501,		/* Grid request due to SOC (insufficient value) */
	GRID_CONNECT_REASON_PWR = 503,		/* Grid request due to exceeding the power limit */
	GRID_CONNECT_REASON_MAN = 505,		/* Manual grid request */
	GRID_CONNECT_REASON_FEED = 507,		/* Feed-in started */
};

enum GEN_CONNECT_REASON {
	GEN_CONNECT_REASON_AUTO = 401,		/* Automatic generator start due to set criteria */
	GEN_CONNECT_REASON_MAN = 403,		/* Manual generator start */
	GEN_CONNECT_REASON_CUR = 407,		/* Current-regulated generator operation initiated? */
};

enum CURRENT_SOURCE {
	CURRENT_SOURCE_NONE,
	CURRENT_SOURCE_CAN,
	CURRENT_SOURCE_SMANET,
	CURRENT_SOURCE_CALCULATED,
	CURRENT_SOURCE_INFLUX,
};

enum CURRENT_TYPE {
	CURRENT_TYPE_AMPS,
	CURRENT_TYPE_WATTS
};

struct si_current_source {
	char text[128];
	enum CURRENT_SOURCE source;
	enum CURRENT_TYPE type;
	union {
		struct {
			uint16_t id;
			uint8_t offset;
			uint8_t size;
		} can;
		char name[16];
		char *query;
	};
	double mult;
};
typedef struct si_current_source si_current_source_t;

struct si_session {
	solard_agent_t *ap;
	double user_soc;

	/* CAN */
	char can_info[SOLARD_TRANSPORT_LEN+SOLARD_TARGET_LEN+SOLARD_TOPTS_LEN];
	char can_transport[SOLARD_TRANSPORT_LEN];
	char can_target[SOLARD_TARGET_LEN];
	char can_topts[SOLARD_TOPTS_LEN];
	int can_fallback;
	solard_driver_t *can;
	void *can_handle;
	bool can_init;
	bool can_connected;
	struct can_frame frames[16];
	time_t times[16];
	pthread_t th;
	int (*can_read)(struct si_session *, uint32_t id, uint8_t *data, int len);
	si_raw_data_t raw_data;
	si_data_t data;

#ifdef SMANET
	/* SMANET */
	char smanet_info[SOLARD_TRANSPORT_LEN+SOLARD_TARGET_LEN+SOLARD_TOPTS_LEN];
	char smanet_transport[SOLARD_TRANSPORT_LEN];
	char smanet_target[SOLARD_TARGET_LEN];
	char smanet_topts[SOLARD_TOPTS_LEN];
	char smanet_channels_path[1024];
	smanet_session_t *smanet;
	char battery_type[32];
#endif

	/* Charging */
	double max_voltage;		/* Dont go above this voltage */
	double min_voltage;		/* Dont go below this voltage */
	int charge_mode;
	double charge_voltage;		/* RO|NOSAVE, charge_voltage */
	double grid_charge_amps;	
	double gen_charge_amps;
	double gen_hold_soc;		/* hold SOC when gen running */
	double solar_charge_amps;
	double charge_amps;
	double min_charge_amps;
	double max_charge_amps;
	double discharge_amps;
	double soc;
#if 0
	double last_ca;
	char last_soc[32];
#endif
	double soh;

	/* INTERNAL ONLY */
	bool zero_ca;
	bool startup_soc;
	bool running;
	int startup;
	int tozero;
	uint16_t state;
	char errmsg[128];
	bool grid_connected;
	bool gen_connected;
	int smanet_added;
	char notify_path[256];
	int readonly;
	int readonly_warn;
	si_current_source_t input;
	si_current_source_t output;
	si_current_source_t temp;
	si_current_source_t solar;
	bool force_charge_amps;
	bool have_battery_temp;
	char last_out[128];
	int disable_si_read;
	int disable_si_write;
	int sync_interval;
	struct {
		bool enabled;
		char name[256];
		char func[64];
	} eh;
#ifdef JS
	JSPropertySpec *props;
	JSFunctionSpec *funcs;
//	JSObject *si_obj;
	jsval agent_val;
	JSPropertySpec *data_props;
	jsval data_val;
//	JSObject *can_obj;
	jsval can_val;
	void *can_val_ptr;
//	JSObject *smanet_obj;
	jsval smanet_val;
//	void *smanet_val_ptr;
#endif
};
typedef struct si_session si_session_t;

#define SI_STATE_RUNNING	0x01
#define SI_STATE_CHARGING	0x02
#define SI_STATE_OPEN		0x10
#define SI_STATE_CAN_INIT	0x20
#define SI_STATE_STARTED	0x40

#define SI_VOLTAGE_MIN	41.0
#define SI_VOLTAGE_MAX	63.0

#define SI_CONFIG_FLAG_SMANET	0x1000

#define SI_STATUS_CAN		0x01
#define SI_STATUS_SMANET	0x02
#define SI_STATUS_BMS		0x04
#define SI_STATUS_CHARGING	0x08

/* driver */
extern solard_driver_t si_driver;

/* can */
int si_can_init(si_session_t *s);
int si_can_connect(si_session_t *s);
void si_can_disconnect(si_session_t *s);
int si_can_set_reader(si_session_t *s);
int si_can_get_data(si_session_t *s);
int si_can_read_relays(si_session_t *s);
int si_can_read_data(si_session_t *s, int all);
int si_can_read(si_session_t *s, uint32_t id, uint8_t *data, int rdlen);
int si_can_write(si_session_t *s, uint32_t id, uint8_t *data, int data_len);
int si_can_write_va(si_session_t *s);
int si_can_write_data(si_session_t *s);
void si_can_destroy(si_session_t *s);

/* smanet */
int si_smanet_init(si_session_t *s);
int si_smanet_connect(si_session_t *s);
int si_smanet_disconnect(si_session_t *s);
int si_smanet_destroy(si_session_t *s);
int si_smanet_load_channels(si_session_t *s);
int si_smanet_setup(si_session_t *s);
int si_smanet_read_data(si_session_t *s);

/* config */
int si_agent_init(int argc, char **argv, opt_proctab_t *si_opts, si_session_t *s);
int si_read_config(si_session_t *s);
int si_write_config(si_session_t *s);
int si_set_config(si_session_t *s, char *, char *, char *);
int si_config_add_info(si_session_t *s, json_object_t *j);
int si_control(void *handle,char *op, char *id, json_value_t *actions);
int si_config(void *,int,...);
int si_check_params(si_session_t *s);

/* info */
json_value_t *si_get_info(si_session_t *s);

#define si_isvrange(v) ((v >= SI_VOLTAGE_MIN) && (v  <= SI_VOLTAGE_MAX))

#define si_notify(session,format,args...) solard_notify(session->notify_path,format,## args)

#ifdef JS
int si_jsinit(si_session_t *);
#endif

#endif /* __SOLARD_SI_H */
