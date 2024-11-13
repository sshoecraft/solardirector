
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 1
#include "debug.h"

#include "jbd.h"
#include "jbd_regs.h"

extern char *jbd_version_string;

#define BC_STRING 1
#define NTC_STRING 1

#define JBD_CONFIG_FLAG_EEPROM 0x0100

/* internal data types */
enum JBD_PARM_DT {
	JBD_PARM_DT_UNK,
	JBD_PARM_DT_INT,		/* 16 bit number */
	JBD_PARM_DT_DOUBLE,		/* number */
	JBD_PARM_DT_STR,		/* string */
	JBD_PARM_DT_TEMP,		/* temp */
	JBD_PARM_DT_DATE,		/* date */
	JBD_PARM_DT_DR,			/* discharge rate */
	JBD_PARM_DT_FUNC,		/* function bits */
	JBD_PARM_DT_NTC,		/* ntc bits */
	JBD_PARM_DT_B0,			/* byte 0 */
	JBD_PARM_DT_B1,			/* byte 1 */
	JBD_PARM_DT_DOC,		/* Double overcurrent */
	JBD_PARM_DT_SCVAL,
	JBD_PARM_DT_SCDELAY,
	JBD_PARM_DT_DSGOC2,
	JBD_PARM_DT_DSGOC2DELAY,
	JBD_PARM_DT_HCOVPDELAY,
	JBD_PARM_DT_HCUVPDELAY,
	JBD_PARM_DT_MAX
};

static char *parmtypes[] = {
	"unknown",
	"int",
	"double",
	"string",
	"temp",
	"date",
	"discharge rate",
	"function",
	"ntc",
	"byte 0",
	"byte 1",
	"double",
	"scval",
	"scdelay",
	"DSGOC2",
	"DSGOC2DELAY",
	"HCOVPDELAY",
	"HCUVPDELAY",
};

static char *parm_typestr(int dt) {
	if (dt < 0 || dt >= JBD_PARM_DT_MAX) return "invalid";
	return parmtypes[dt];
}

/* Registers, parms, internal data types */
static struct jbd_params {
	uint8_t reg;
	char *label;
	int dt;
} params[] = {
	{ JBD_REG_DCAP,"DesignCapacity", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_CCAP,"CycleCapacity", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_FULL,"FullChargeVol", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_EMPTY,"ChargeEndVol", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_RATE,"DischargingRate", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_MFGDATE,"ManufactureDate", JBD_PARM_DT_DATE },
	{ JBD_REG_SERIAL,"SerialNumber", JBD_PARM_DT_INT },
	{ JBD_REG_CYCLE,"CycleCount", JBD_PARM_DT_INT },
	{ JBD_REG_CHGOT,"ChgOverTemp", JBD_PARM_DT_TEMP },
	{ JBD_REG_RCHGOT,"ChgOTRelease", JBD_PARM_DT_TEMP },
	{ JBD_REG_CHGUT,"ChgLowTemp", JBD_PARM_DT_TEMP },
	{ JBD_REG_RCHGUT,"ChgUTRelease", JBD_PARM_DT_TEMP },
	{ JBD_REG_DSGOT,"DisOverTemp", JBD_PARM_DT_TEMP },
	{ JBD_REG_RDSGOT,"DsgOTRelease", JBD_PARM_DT_TEMP },
	{ JBD_REG_DSGUT,"DisLowTemp", JBD_PARM_DT_TEMP },
	{ JBD_REG_RDSGUT,"DsgUTRelease", JBD_PARM_DT_TEMP },
	{ JBD_REG_POVP,"PackOverVoltage", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_RPOVP,"PackOVRelease", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_PUVP,"PackUnderVoltage", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_RPUVP,"PackUVRelease", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_COVP,"CellOverVoltage", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_RCOVP,"CellOVRelease", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_CUVP,"CellUnderVoltage", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_RCUVP,"CellUVRelease", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_CHGOC,"OverChargeCurrent", JBD_PARM_DT_INT },
	{ JBD_REG_DSGOC,"OverDisCurrent", JBD_PARM_DT_INT },
	{ JBD_REG_BALVOL,"BalanceStartVoltage", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_BALPREC,"BalanceWindow", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_CURRES,"SenseResistor", JBD_PARM_DT_DOUBLE },
#if BC_STRING
	{ JBD_REG_FUNCMASK,"BatteryConfig", JBD_PARM_DT_FUNC },
#else
	{ JBD_REG_FUNCMASK,"BatteryConfig", JBD_PARM_DT_INT },
#endif
#if NTC_STRING
	{ JBD_REG_NTCMASK,"NtcConfig", JBD_PARM_DT_NTC },
#else
	{ JBD_REG_NTCMASK,"NtcConfig", JBD_PARM_DT_INT },
#endif
	{ JBD_REG_STRINGS,"PackNum", JBD_PARM_DT_INT },
	{ JBD_REG_FETTIME,"fet_ctrl_time_set", JBD_PARM_DT_INT },
	{ JBD_REG_LEDTIME,"led_disp_time_set", JBD_PARM_DT_INT },
	{ JBD_REG_VOLCAP80,"VoltageCap80", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_VOLCAP60,"VoltageCap60", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_VOLCAP40,"VoltageCap40", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_VOLCAP20,"VoltageCap20", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_HCOVP,"HardCellOverVoltage", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_HCUVP,"HardCellUnderVoltage", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_HCOC,"DoubleOCSC", JBD_PARM_DT_DOC },
	{ JBD_REG_HCOC,"SCValue", JBD_PARM_DT_SCVAL },
	{ JBD_REG_HCOC,"SCDelay", JBD_PARM_DT_SCDELAY },
	{ JBD_REG_HCOC,"DSGOC2", JBD_PARM_DT_DSGOC2 },
	{ JBD_REG_HCOC,"DSGOC2Delay", JBD_PARM_DT_DSGOC2DELAY },
	{ JBD_REG_HTRT,"HCOVPDelay", JBD_PARM_DT_HCOVPDELAY },
	{ JBD_REG_HTRT,"HCUVPDelay",JBD_PARM_DT_HCUVPDELAY },
	{ JBD_REG_HTRT,"SCRelease",JBD_PARM_DT_B1 },
	{ JBD_REG_CHGDELAY,"ChgUTDelay", JBD_PARM_DT_B0 },
	{ JBD_REG_CHGDELAY,"ChgOTDelay", JBD_PARM_DT_B1 },
	{ JBD_REG_DSGDELAY,"DsgUTDelay", JBD_PARM_DT_B0 },
	{ JBD_REG_DSGDELAY,"DsgOTDelay", JBD_PARM_DT_B1 },
	{ JBD_REG_PVDELAY,"PackUVDelay", JBD_PARM_DT_B0 },
	{ JBD_REG_PVDELAY,"PackOVDelay", JBD_PARM_DT_B1 },
	{ JBD_REG_CVDELAY,"CellUVDelay", JBD_PARM_DT_B0 },
	{ JBD_REG_CVDELAY,"CellOVDelay", JBD_PARM_DT_B1 },
	{ JBD_REG_CHGOCDELAY,"ChgOCDelay", JBD_PARM_DT_B0 },
	{ JBD_REG_CHGOCDELAY,"ChgOCRDelay", JBD_PARM_DT_B1 },
	{ JBD_REG_DSGOCDELAY,"DsgOCDelay", JBD_PARM_DT_B0 },
	{ JBD_REG_DSGOCDELAY,"DsgOCRDelay", JBD_PARM_DT_B1 },
	{ JBD_REG_MFGNAME,"ManufacturerName", JBD_PARM_DT_STR },
	{ JBD_REG_MODEL,"DeviceName", JBD_PARM_DT_STR },
	{ JBD_REG_BARCODE,"BarCode", JBD_PARM_DT_STR },
	{ JBD_REG_GPSOFF,"GPS_VOL", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_GPSOFFTIME,"GPS_TIME", JBD_PARM_DT_INT },
	{ JBD_REG_VOLCAP90,"VoltageCap90", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_VOLCAP70,"VoltageCap70", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_VOLCAP50,"VoltageCap50", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_VOLCAP30,"VoltageCap30", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_VOLCAP10,"VoltageCap10", JBD_PARM_DT_DOUBLE },
	{ JBD_REG_VOLCAP100,"VoltageCap100", JBD_PARM_DT_DOUBLE },
	{ 0,0,0 }
};
typedef struct jbd_params jbd_params_t;

struct jbd_params *_getp(char *label) {
	register struct jbd_params *pp;

	dprintf(3,"label: %s\n", label);
	for(pp = params; pp->label; pp++) {
		dprintf(3,"pp->label: %s\n", pp->label);
		if (strcmp(pp->label,label)==0) {
			return pp;
		}
	}
	return 0;
}

/* Selection values */
#define dsgoc2_vals "8,11,14,17,19,22,25,28,31,33,36,39,42,44,47,50"
#define dsgoc2delay_vals "8,20,40,80,160,320,640,1280"
#define sc_vals "22,33,44,56,67,78,89,100"
#define scdelay_vals "70,100,200,400"
#define hcovpdelay_vals "1,2,4,8"
#define hcuvpdelay_vals "1,4,8,16"

static inline void _addstr(char *str,char *newstr) {
	dprintf(4,"str: %s, newstr: %s\n", str, newstr);
	if (strlen(str)) strcat(str,",");
	strcat(str,newstr);
	dprintf(4,"str: %s\n", str);
}

#define CTEMP(v) ( (v - 2731) / 10 )

static int _pcopy(config_property_t *p, int type, void *src, int size, char *errmsg) {
	dprintf(1,"name: %s, type: %s, size: %d\n", p->name, typestr(type), size);
	dprintf(1,"dest: %p, dsize: %d\n", p->dest, p->dsize);
	if (p->dest && p->dsize != size) {
		free(p->dest);
		p->dest = 0;
	}
	if (!p->dest) {
		p->dsize = size;
		p->dest = malloc(p->dsize);
		if (!p->dest) {
			log_syserror("_pcopy: malloc(%d)",p->dsize);
			strcpy(errmsg,"memory allocation error");
			return 1;
		}
		set_bit(p->flags,CONFIG_FLAG_ALLOC);
	}
	p->len = conv_type(p->type,p->dest,p->dsize,type,src,size);
	return 0;
}

static int jbd_get_config(jbd_session_t *s, struct jbd_params *pp, config_property_t *p, uint8_t *data, int len, char *errmsg) {
	char temp[72];
	uint16_t val,sval;
	uint8_t bval;
	bool tval;
	double fval;
	int i,ival;

	dprintf(1,"label: %s, dt: %d (%s)\n", pp->label, pp->dt, parm_typestr(pp->dt));
	dprintf(1,"p: %p, name: %s, scale: %f\n", p, p->name, p->scale);

	temp[0] = 0;
	switch(pp->dt) {
	case JBD_PARM_DT_INT:
		ival = jbd_getshort(data);
		dprintf(1,"ival: %d\n", ival);
		dprintf(1,"scale: %f\n", p->scale);
		if (p->scale != 0.0) ival /= p->scale;
		_pcopy(p,DATA_TYPE_INT,&ival,sizeof(ival),errmsg);
		break;
	case JBD_PARM_DT_TEMP:
		val = jbd_getshort(data);
		sval = CTEMP(val);
		_pcopy(p,DATA_TYPE_U16,&sval,sizeof(sval),errmsg);
		break;
	case JBD_PARM_DT_DATE:
		{
			unsigned short mfgdate;
			int day,mon,year;

			mfgdate = jbd_getshort(data);
			dprintf(2,"mfgdate: %04x\n", mfgdate);
			day = mfgdate & 0x1f;
			mon = (mfgdate >> 5) & 0x0f;
			year = 2000 + (mfgdate >> 9);
			dprintf(2,"year: %d, mon: %d, day: %d\n", year, mon, day);
			sprintf(temp,"%04d%02d%02d",year,mon,day);
			_pcopy(p,DATA_TYPE_STRING,temp,strlen(temp)+1,errmsg);
		}
		break;
	case JBD_PARM_DT_B0:
		val = jbd_getshort(data);
		dprintf(1,"val: %04x\n", val);
		dprintf(1,"data[0]: %02x, [1]: %02x\n", data[0], data[1]);
		bval = data[0];
		_pcopy(p,DATA_TYPE_U8,&bval,sizeof(bval),errmsg);
		break;
	case JBD_PARM_DT_B1:
		val = jbd_getshort(data);
		dprintf(1,"val: %04x\n", val);
		dprintf(1,"data[0]: %02x, [1]: %02x\n", data[0], data[1]);
		bval = data[1];
		_pcopy(p,DATA_TYPE_U8,&bval,sizeof(bval),errmsg);
		break;
	case JBD_PARM_DT_FUNC:
		ival = jbd_getshort(data);
		dprintf(1,"ival: %04x\n", ival);
		*temp = 0;
#if BC_STRING
		if (ival & JBD_FUNC_SWITCH) _addstr(temp,"Switch");
		if (ival & JBD_FUNC_SCRL)  _addstr(temp,"SCRL");
		if (ival & JBD_FUNC_BALANCE_EN) _addstr(temp,"BALANCE_EN");
		if (ival & JBD_FUNC_CHG_BALANCE) _addstr(temp,"CHG_BALANCE");
		if (ival & JBD_FUNC_LED_EN) _addstr(temp,"LED_EN");
		if (ival & JBD_FUNC_LED_NUM) _addstr(temp,"LED_NUM");
		if (ival & JBD_FUNC_RTC) _addstr(temp,"RTC");
		if (ival & JBD_FUNC_EDV) _addstr(temp,"EDV");
		_pcopy(p,DATA_TYPE_STRING,temp,strlen(temp)+1,errmsg);
#else
		_pcopy(p,DATA_TYPE_INT,&ival,sizeof(ival),errmsg);
#endif
		break;
	case JBD_PARM_DT_NTC:
		ival = jbd_getshort(data);
		dprintf(1,"ival: %04x\n", ival);
#if NTC_STRING
		*temp = 0;
		if (ival & JBD_NTC1) _addstr(temp,"NTC1");
		if (ival & JBD_NTC2) _addstr(temp,"NTC2");
		if (ival & JBD_NTC3) _addstr(temp,"NTC3");
		if (ival & JBD_NTC4) _addstr(temp,"NTC4");
		if (ival & JBD_NTC5) _addstr(temp,"NTC5");
		if (ival & JBD_NTC6) _addstr(temp,"NTC6");
		if (ival & JBD_NTC7) _addstr(temp,"NTC7");
		if (ival & JBD_NTC8) _addstr(temp,"NTC8");
		_pcopy(p,DATA_TYPE_STRING,temp,strlen(temp)+1,errmsg);
#else
		_pcopy(p,DATA_TYPE_INT,&ival,sizeof(ival),errmsg);
#endif
		break;
	case JBD_PARM_DT_DOUBLE:
		fval = jbd_getshort(data);
		dprintf(1,"fval(1): %f\n", fval);
		dprintf(1,"scale: %f\n", p->scale);
		if (p->scale != 0.0) fval /= p->scale;
		dprintf(1,"fval(2): %f\n", fval);
		_pcopy(p,DATA_TYPE_DOUBLE,&fval,sizeof(fval),errmsg);
		break;
	case JBD_PARM_DT_STR:
		data[len] = 0;
		temp[0] = 0;
		strncat(temp,(char *)data,sizeof(temp)-1);
		trim(temp);
		_pcopy(p,DATA_TYPE_STRING,temp,strlen(temp)+1,errmsg);
		break;
	case JBD_PARM_DT_DOC:
		tval = ((data[0] & 0x80) != 0);
		_pcopy(p,DATA_TYPE_BOOLEAN,&tval,sizeof(tval),errmsg);
		break;
	case JBD_PARM_DT_DSGOC2:
		i = data[1] & 0x0f;
		dprintf(1,"data[1]: %02x\n", data[1]);
		dprintf(1,"i: %d\n", i);
		ival = dsgoc2_vals[i];
		_pcopy(p,DATA_TYPE_INT,&ival,sizeof(ival),errmsg);
		break;
	case JBD_PARM_DT_DSGOC2DELAY:
		i = (data[1] >> 4) & 0x07;
		dprintf(1,"data[1]: %02x\n", data[1]);
		dprintf(1,"i: %d\n", i);
		ival = dsgoc2delay_vals[i];
		_pcopy(p,DATA_TYPE_INT,&ival,sizeof(ival),errmsg);
		break;
	case JBD_PARM_DT_SCVAL:
		i = data[0] & 0x07;
		dprintf(1,"data[0]: %02x\n", data[0]);
		dprintf(1,"i: %d\n", i);
		ival = sc_vals[i];
		_pcopy(p,DATA_TYPE_INT,&ival,sizeof(ival),errmsg);
		break;
	case JBD_PARM_DT_SCDELAY:
		i = (data[0] >> 3) & 0x03;
		dprintf(1,"data[0]: %02x\n", data[0]);
		dprintf(1,"i: %d\n", i);
		ival = scdelay_vals[i];
		_pcopy(p,DATA_TYPE_INT,&ival,sizeof(ival),errmsg);
		break;
	case JBD_PARM_DT_HCOVPDELAY:
		i = (data[0] >> 4) & 0x03;
		dprintf(1,"data[0]: %02x\n", data[0]);
		dprintf(1,"i: %d\n", i);
		ival = hcovpdelay_vals[i];
		_pcopy(p,DATA_TYPE_INT,&ival,sizeof(ival),errmsg);
		break;
	case JBD_PARM_DT_HCUVPDELAY:
		i = (data[0] >> 6) & 0x03;
		dprintf(1,"data[0]: %02x\n", data[0]);
		dprintf(1,"i: %d\n", i);
		ival = hcuvpdelay_vals[i];
		_pcopy(p,DATA_TYPE_INT,&ival,sizeof(ival),errmsg);
		break;
	default:
		sprintf(temp,"unhandled switch for: %d",pp->dt);
		break;
	}
	return 0;
}

static int jbd_get_value(void *ctx, list args, char *errmsg, json_object_t *results) {
	jbd_session_t *s = ctx;
	config_arg_t *arg;
	char *name;
	config_property_t *p;
	json_value_t *v;
	int r,we_opened;

	dprintf(dlevel,"args count: %d\n", list_count(args));
	r = 0;
	list_reset(args);
	while((arg = list_get_next(args)) != 0) {
		name = arg->argv[0];
		dprintf(dlevel,"name: %s\n", name);
		p = config_find_property(s->ap->cp, name);
		dprintf(dlevel,"p: %p\n", p);
		if (!p) {
			sprintf(errmsg,"property %s not found",name);
			return 1;
		}
		dprintf(dlevel,"flags: %04x\n", p->flags);
		if (p->flags & JBD_CONFIG_FLAG_EEPROM) {
			struct jbd_params *pp;
			uint8_t data[256];
			int bytes;

			/* Get the internal parm info */
			pp = _getp(p->name);
			dprintf(1,"pp: %p\n", pp);
			if (!pp) return 1;

			/* Make sure we're open */
			we_opened = 0;
			if (!check_state(s,JBD_STATE_OPEN)) {
				if (jbd_driver.open(s)) {
					strcpy(errmsg,"unable to open BMS");
					return 1;
				}
				we_opened = 1;
			}
			r = 1;

			/* Open the eeprom */
			if (!check_state(s,JBD_STATE_EEPROM_OPEN)) {
				if (jbd_eeprom_open(s) < 0) {
					strcpy(errmsg,"unable to open eeprom");
					goto jbd_get_value_error;
				}
			}

			/* Read the register */
			bytes = jbd_rw(s, JBD_CMD_READ, pp->reg, data, sizeof(data));
			dprintf(3,"bytes: %d\n", bytes);
			if (bytes < 0) {
				sprintf(errmsg, "error reading register %d for param %s", pp->reg, p->name);
				goto jbd_get_value_error;
			}

			/* process the info */
			r = jbd_get_config(s, pp, p, data, bytes, errmsg);
			if (r) break;
		}
		v = json_from_type(p->type,p->dest,p->len);
		if (v) json_object_set_value(results,p->name,v);
	}
	strcpy(errmsg,"success");

jbd_get_value_error:
	if (check_state(s,JBD_STATE_EEPROM_OPEN)) jbd_eeprom_close(s);
	if (we_opened) jbd_driver.close(s);
	return r;
}

/* TODO: check value for limits in desc */
static int jbd_set_config(jbd_session_t *s, uint8_t *data, struct jbd_params *pp, config_property_t *dp, char *value, char *errmsg) {
	char *p;
	int bytes,len,i,ival;
	uint16_t val;
	double fval;

	dprintf(1,"dp: name: %s, unit: %s, scale: %f\n", dp->name, dp->units, dp->scale);

	/* Len = 2 (16 bit size) for most */
	len = 2;

	dprintf(1,"pp->dt: %d, DATA_TYPE_STRING: %d\n", pp->dt, DATA_TYPE_STRING);
	switch(pp->dt) {
	case JBD_PARM_DT_INT:
		conv_type(DATA_TYPE_INT,&ival,0,DATA_TYPE_STRING,value,0);
		dprintf(1,"scale: %f\n", dp->scale);
		if (dp->scale != 0.0) ival *= dp->scale;
		val = ival;
		dprintf(1,"val: %d\n", val);
		jbd_putshort(data,val);
		break;
	case JBD_PARM_DT_TEMP:
		conv_type(DATA_TYPE_SHORT,&val,0,DATA_TYPE_STRING,value,0);
		dprintf(1,"val: %d\n", val);
		val = ((val * 10) + 2731);
		dprintf(1,"val: %d\n", val);
		jbd_putshort(data,val);
		break;
	case JBD_PARM_DT_DATE:
		break;
	case JBD_PARM_DT_B0:
       		bytes = jbd_rw(s, JBD_CMD_READ, pp->reg, data, 2);
		if (bytes < 0) return -1;
		conv_type(DATA_TYPE_SHORT,&val,0,DATA_TYPE_STRING,value,0);
		data[0] = val;
		break;
	case JBD_PARM_DT_B1:
       		bytes = jbd_rw(s, JBD_CMD_READ, pp->reg, data, 2);
		if (bytes < 0) return -1;
		conv_type(DATA_TYPE_SHORT,&val,0,DATA_TYPE_STRING,value,0);
		data[1] = val;
		break;
	case JBD_PARM_DT_FUNC:
		val = 0;
		dprintf(1,"value: %s\n", value);
		for(i=0; i < 99; i++) {
			p = strele(i,",",value);
			if (!strlen(p)) break;
			dprintf(1,"p: %s\n", p);
			trim(p);
			if (strcasecmp(p,"Switch")==0)
				val |= JBD_FUNC_SWITCH;
			else if (strcasecmp(p,"SCRL")==0)
				val |= JBD_FUNC_SCRL;
			else if (strcasecmp(p,"BALANCE_EN")==0)
				val |= JBD_FUNC_BALANCE_EN;
			else if (strcasecmp(p,"CHG_BALANCE")==0)
				val |= JBD_FUNC_CHG_BALANCE;
			else if (strcasecmp(p,"LED_EN")==0)
				val |= JBD_FUNC_LED_EN;
			else if (strcasecmp(p,"LED_NUM")==0)
				val |= JBD_FUNC_LED_NUM;
		}
		if (val & JBD_FUNC_CHG_BALANCE)
			s->balancing = 2;
		else if (val & JBD_FUNC_BALANCE_EN)
			s->balancing = 1;
		else
			s->balancing = 0;
		dprintf(1,"val: %d\n", val);
		jbd_putshort(data,val);
		break;
	case JBD_PARM_DT_NTC:
		val = 0;
		for(i=0; i < 99; i++) {
			p = strele(i,",",value);
			if (!strlen(p)) break;
			dprintf(1,"p: %s\n", p);
			trim(p);
			if (strcasecmp(p,"NTC1")==0)
				val |= JBD_NTC1;
			else if (strcasecmp(p,"NTC2")==0)
				val |= JBD_NTC2;
			else if (strcasecmp(p,"NTC3")==0)
				val |= JBD_NTC3;
			else if (strcasecmp(p,"NTC4")==0)
				val |= JBD_NTC4;
			else if (strcasecmp(p,"NTC5")==0)
				val |= JBD_NTC5;
			else if (strcasecmp(p,"NTC6")==0)
				val |= JBD_NTC6;
			else if (strcasecmp(p,"NTC7")==0)
				val |= JBD_NTC7;
			else if (strcasecmp(p,"NTC8")==0)
				val |= JBD_NTC8;
		}
		dprintf(1,"val: %d\n", val);
		jbd_putshort(data,val);
		break;
	case JBD_PARM_DT_DOUBLE:
		/* The COVP and CUVP release values must be 100mV diff  */
		conv_type(DATA_TYPE_DOUBLE,&fval,0,DATA_TYPE_STRING,value,0);
		dprintf(1,"fval: %f\n", fval);
		dprintf(1,"scale: %f\n", dp->scale);
		if (dp->scale != 0.0) fval *= dp->scale;
		val = fval;
		dprintf(1,"val: %d\n", val);
		jbd_putshort(data,val);
		break;
	case JBD_PARM_DT_STR:
		data[0] = 0;
		/* XXX */
		strncat((char *)data,value,255);
		trim((char *)data);
		len = strlen((char *)data);
		break;
	case JBD_PARM_DT_DOC:
		dprintf(1,"sval: %s\n", value);
		conv_type(DATA_TYPE_BOOL,&ival,0,DATA_TYPE_STRING,value,0);
		dprintf(1,"ival: %d\n", ival);
       		bytes = jbd_rw(s, JBD_CMD_READ, pp->reg, data, 2);
		if (bytes < 0) return -1;
		dprintf(1,"data[0]: %x\n", data[0]);
		if (ival) data[0] |= 0x80;
		else data[0] &= 0x7F;
		dprintf(1,"data[0]: %x\n", data[0]);
		break;
	case JBD_PARM_DT_DSGOC2:
       		bytes = jbd_rw(s, JBD_CMD_READ, pp->reg, data, 2);
		if (bytes < 0) return -1;
		conv_type(DATA_TYPE_INT,&ival,0,DATA_TYPE_STRING,value,0);
		dprintf(1,"ival: %d\n", ival);
		dprintf(1,"data[1]: %02x\n", data[1]);
#if 0
		for(i=0; i < ndsgoc2_vals; i++) {
			if (dsgoc2_vals[i] == ival) {
				dprintf(1,"i: %d\n", i);
				data[1] &= ~0x0f;
				data[1] |= i;
				break;
			}
		}
		if (i >= ndsgoc2_vals) return 0;
#endif
		dprintf(1,"data[1]: %02x\n", data[1]);
		break;
	case JBD_PARM_DT_DSGOC2DELAY:
       		bytes = jbd_rw(s, JBD_CMD_READ, pp->reg, data, 2);
		if (bytes < 0) return -1;
		conv_type(DATA_TYPE_INT,&ival,0,DATA_TYPE_STRING,value,0);
		dprintf(1,"ival: %d\n", ival);
		dprintf(1,"data[1]: %02x\n", data[1]);
#if 0
		for(i=0; i < ndsgoc2delay_vals; i++) {
			if (dsgoc2delay_vals[i] == ival) {
				dprintf(1,"i: %d\n", i);
				data[1] &= 0xF0;
				data[1] |= (i << 4);
				break;
			}
		}
		if (i >= ndsgoc2delay_vals) return 0;
#endif
		dprintf(1,"data[1]: %02x\n", data[1]);
		break;
	case JBD_PARM_DT_SCVAL:
       		bytes = jbd_rw(s, JBD_CMD_READ, pp->reg, data, 2);
		if (bytes < 0) return -1;
		conv_type(DATA_TYPE_INT,&ival,0,DATA_TYPE_STRING,value,0);
		dprintf(1,"ival: %d\n", ival);
		dprintf(1,"data[0]: %02x\n", data[0]);
#if 0
		for(i=0; i < nsc_vals; i++) {
			if (sc_vals[i] == ival) {
				dprintf(1,"i: %d\n", i);
				data[0] &= ~0x07;
				data[0] |= i;
				break;
			}
		}
		if (i >= nsc_vals) return 0;
#endif
		dprintf(1,"data[0]: %02x\n", data[0]);
		break;
	case JBD_PARM_DT_SCDELAY:
       		bytes = jbd_rw(s, JBD_CMD_READ, pp->reg, data, 2);
		if (bytes < 0) return -1;
		conv_type(DATA_TYPE_INT,&ival,0,DATA_TYPE_STRING,value,0);
		dprintf(1,"ival: %d\n", ival);
		dprintf(1,"data[0]: %02x\n", data[0]);
#if 0
		for(i=0; i < nscdelay_vals; i++) {
			if (scdelay_vals[i] == ival) {
				dprintf(1,"i: %d\n", i);
				data[0] &= ~0x18;
				data[0] |= (i << 3);
				break;
			}
		}
		if (i >= nscdelay_vals) return 0;
#endif
		dprintf(1,"data[0]: %02x\n", data[0]);
		break;
	case JBD_PARM_DT_HCOVPDELAY:
       		bytes = jbd_rw(s, JBD_CMD_READ, pp->reg, data, 2);
		if (bytes < 0) return -1;
		conv_type(DATA_TYPE_INT,&ival,0,DATA_TYPE_STRING,value,0);
		dprintf(1,"ival: %d\n", ival);
		dprintf(1,"data[0]: %02x\n", data[0]);
#if 0
		for(i=0; i < nhcovpdelay_vals; i++) {
			if (hcovpdelay_vals[i] == ival) {
				dprintf(1,"i: %d\n", i);
				data[0] &= 0xCF;
				data[0] |= (i << 4);
				break;
			}
		}
		if (i >= nhcovpdelay_vals) return 0;
#endif
		dprintf(1,"data[0]: %02x\n", data[0]);
                break;
	case JBD_PARM_DT_HCUVPDELAY:
       		bytes = jbd_rw(s, JBD_CMD_READ, pp->reg, data, 2);
		if (bytes < 0) return -1;
		conv_type(DATA_TYPE_INT,&ival,0,DATA_TYPE_STRING,value,0);
		dprintf(1,"ival: %d\n", ival);
		dprintf(1,"data[0]: %02x\n", data[0]);
#if 0
		for(i=0; i < nhcuvpdelay_vals; i++) {
			if (hcuvpdelay_vals[i] == ival) {
				dprintf(1,"i: %d\n", i);
				data[0] &= 0x3F;
				data[0] |= (i << 6);
				break;
			}
		}
		if (i >= nhcuvpdelay_vals) return 0;
#endif
		dprintf(1,"data[0]: %02x\n", data[0]);
		break;
	default:
		dprintf(1,"unhandled case: %d\n", pp->dt);
		return 0;
		break;
	}
	jbd_get_config(s,pp,dp,data,len,errmsg);
	return len;
}

static int jbd_set_value(void *ctx, list args, char *errmsg, json_object_t *results) {
	jbd_session_t *s = ctx;
	config_arg_t *arg;
	char *name, *value;
	config_property_t *p;
	int r,we_opened;

	dprintf(dlevel,"args: %p\n", args);
	dprintf(dlevel,"args count: %d\n", list_count(args));
	list_reset(args);
	r = 0;
	strcpy(errmsg,"unknown");
	while((arg = list_get_next(args)) != 0) {
		name = arg->argv[0];
		value = arg->argv[1];
		dprintf(dlevel,"name: %s, value: %s\n", name, value);
		p = config_find_property(s->ap->cp, name);
		dprintf(dlevel,"p: %p\n", p);
		if (!p) {
			sprintf(errmsg,"property %s not found",name);
			return 1;
		}
		if (strcasecmp(value,"default")==0) {
			if (p->def) value = p->def;
			else value = "";
		}
		if (p->flags & JBD_CONFIG_FLAG_EEPROM) {
			struct jbd_params *pp;
			uint8_t data[256];
			int len,bytes;

			/* Get the internal parm info */
			pp = _getp(p->name);
			dprintf(1,"pp: %p\n", pp);
			if (!pp) return 1;

			/* Make sure we're open */
			we_opened = 0;
			if (!check_state(s,JBD_STATE_OPEN)) {
				if (jbd_driver.open(s)) {
					strcpy(errmsg,"unable to open BMS");
					return 1;
				}
				we_opened = 1;
			}
			r = 1;

			/* Open the eeprom */
			if (!check_state(s,JBD_STATE_EEPROM_OPEN)) {
				if (jbd_eeprom_open(s) < 0) {
					strcpy(errmsg,"unable to open eeprom");
					goto jbd_set_value_error;
				}
			}

			/* Set the value */
			len = jbd_set_config(s,data,pp,p,value,errmsg);
			if (len < 0) {
				sprintf(errmsg,"I/O error");
				goto jbd_set_value_error;
			} else if (len == 0) {
				sprintf(errmsg,"bad data");
				goto jbd_set_value_error;
			}
			bytes = jbd_rw(s, JBD_CMD_WRITE, pp->reg, data, len);
			if (bytes < 0) {
				sprintf(errmsg,"write error");
				goto jbd_set_value_error;
			}
			r = 0;

		} else {
			dprintf(1,"updating property...\n");
			p->len = conv_type(p->type,p->dest,p->dsize,DATA_TYPE_STRING,value,strlen(value));
			p->dirty = 1;
			dprintf(1,"writing config...\n");
			config_write(s->ap->cp);
		}
	}

	strcpy(errmsg,"success");

jbd_set_value_error:
	if (check_state(s,JBD_STATE_EEPROM_OPEN)) jbd_eeprom_close(s);
	if (we_opened) jbd_driver.close(s);
	return r;
}

int xor_bits(unsigned x) {
    return __builtin_parity(x);
}

static int jbd_charge(void *ctx, list args, char *errmsg, json_object_t *results) {
	jbd_session_t *s = ctx;
	config_arg_t *arg;
	char *action;
	int r,bits;

	/* We take 1 arg: start/stop */
	arg = list_get_first(args);
	action = arg->argv[0];
	dprintf(dlevel,"action: %s\n", action);
	if (jbd_get_fetstate(s)) {
		strcpy(errmsg,"unable to get fetstate");
		return 1;
	}
	dprintf(dlevel,"fetstate: %x\n", s->fetstate);
	bits = (s->fetstate & JBD_MOS_DISCHARGE ? 0 : JBD_MOS_DISCHARGE);
	if (strcmp(action,"start") == 0 || strcasecmp(action,"on") == 0) {
		dprintf(0,"bits: %x\n", bits);
		r = jbd_set_mosfet(s,bits);
		if (r) strcpy(errmsg,"unable to set mosfet");
	} else if (strcmp(action,"stop") == 0 || strcasecmp(action,"off") == 0) {
		bits |= JBD_MOS_CHARGE;
		dprintf(0,"bits: %x\n", bits);
		r = jbd_set_mosfet(s,bits);
		if (r) strcpy(errmsg,"unable to set mosfet");
	} else {
		strcpy(errmsg,"invalid charge mode");
		r = 1;
	}
	return r;
}

static int jbd_discharge(void *ctx, list args, char *errmsg, json_object_t *results) {
	jbd_session_t *s = ctx;
	config_arg_t *arg;
	char *action;
	int r,bits;

	/* We take 1 arg: start/stop */
	arg = list_get_first(args);
	action = arg->argv[0];
	dprintf(dlevel,"action: %s\n", action);
	if (jbd_get_fetstate(s)) {
		strcpy(errmsg,"unable to get fetstate");
		return 1;
	}
	dprintf(0,"fetstate: %x\n", s->fetstate);
	bits = (s->fetstate & JBD_MOS_CHARGE ? 0 : JBD_MOS_CHARGE);
	if (strcmp(action,"start") == 0 || strcasecmp(action,"on") == 0) {
		bits &= ~JBD_MOS_DISCHARGE;
		dprintf(0,"bits: %x\n", bits);
		r = jbd_set_mosfet(s,bits);
		if (r) strcpy(errmsg,"unable to set mosfet");
	} else if (strcmp(action,"stop") == 0 || strcasecmp(action,"off") == 0) {
		bits |= JBD_MOS_DISCHARGE;
		dprintf(0,"bits: %x\n", bits);
		r = jbd_set_mosfet(s,bits);
		if (r) strcpy(errmsg,"unable to set mosfet");
	} else {
		strcpy(errmsg,"invalid discharge mode");
		r = 1;
	}
	return r;
}

static int cf_jbd_balance(void *ctx, list args, char *errmsg, json_object_t *results) {
	jbd_session_t *s = ctx;
	config_arg_t *arg;
	char *action;
	int r;

	/* We take 1 arg: start/stop */
	arg = list_get_first(args);
	action = arg->argv[0];
	dprintf(dlevel,"action: %s\n", action);
	if (strcmp(action,"start") == 0 || strcasecmp(action,"on") == 0) {
		r = jbd_set_balance(s,JBD_FUNC_BALANCE_EN);
		if (r) strcpy(errmsg,"unable to set balance");
		else s->balancing = 1;
	} else if (strcmp(action,"stop") == 0 || strcasecmp(action,"off") == 0) {
		r = jbd_set_balance(s,0);
		if (r) strcpy(errmsg,"unable to set balance");
		else s->balancing = 0;
	} else if (strcasecmp(action,"charge") == 0) {
		r = jbd_set_balance(s,JBD_FUNC_BALANCE_EN | JBD_FUNC_CHG_BALANCE);
		if (r) strcpy(errmsg,"unable to set balance");
		else s->balancing = 2;
	} else {
		strcpy(errmsg,"invalid balance mode");
		r = 1;
	}
	dprintf(0,"r: %d, balancing: %d\n", r, s->balancing);
	return r;
}

static int cf_jbd_reset(void *ctx, list args, char *errmsg, json_object_t *results) {
	jbd_session_t *s = ctx;

	if (jbd_reset(s)) {
		strcpy(errmsg,"unable to reset");
		return 1;
	}
	return 0;
}

int jbd_agent_init(jbd_session_t *s, int argc, char **argv) {
	opt_proctab_t jbd_opts[] = {
		/* Spec, dest, type len, reqd, default val, have */
		{ "-t::|transport,target,opts",s->tpinfo,DATA_TYPE_STRING,sizeof(s->tpinfo)-1,0,"" },
		{ "-r|retry transport connection",&s->retry_tp,DATA_TYPE_BOOL,0,0,"N" },
		{ "-w|retry wait time",&s->wait_time,DATA_TYPE_INT,0,0,"30" },
		OPTS_END
	};
	config_property_t jbd_props[] = {
		/* name, type, dest, dsize, def, flags */
		{ "transport", DATA_TYPE_STRING, s->transport, sizeof(s->transport)-1, 0, 0 },
		{ "target", DATA_TYPE_STRING, s->target, sizeof(s->target)-1, 0, 0 },
		{ "topts", DATA_TYPE_STRING, s->topts, sizeof(s->topts)-1, 0, 0 },
		{ "flatten", DATA_TYPE_BOOLEAN, &s->flatten, 0, "no", 0 },
		{ "log_power", DATA_TYPE_BOOLEAN, &s->log_power, 0, "no", 0 },
		{ "start_at_one", DATA_TYPE_BOOLEAN, &s->start_at_one, 0, "yes", 0 },
		{ "state", DATA_TYPE_INT, &s->state, 0, 0, CONFIG_FLAG_READONLY | CONFIG_FLAG_PRIVATE },
		{0}
	};
	config_function_t jbd_funcs[] = {
		{ "get", jbd_get_value, s, 1 },
		{ "set", jbd_set_value, s, 2 },
		{ "charge", jbd_charge, s, 1 },
		{ "discharge", jbd_discharge, s, 1 },
		{ "balance", cf_jbd_balance, s, 1 },
		{ "reset", cf_jbd_reset, s, 0 },
		{0}
	};

	s->ap = agent_init(argc,argv,jbd_version_string,jbd_opts,&jbd_driver,s,0,jbd_props,jbd_funcs);
	dprintf(1,"ap: %p\n",s->ap);
	if (!s->ap) return 1;
	return 0;
}

int jbd_config_add_parms(jbd_session_t *s) {
	config_property_t capacity_params[] = {
		/* Name, Type, dest, len, def, flags, scope, nvalues, nlabels, labels, units, scale, precision */
		{ "DesignCapacity", DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", "0, 10000, 1", "Design Capacity", "mAH", 100, 3 },
		{ "CycleCapacity",  DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", "0, 10000, 1", "Cycle Capacity", "mAH", 100, 1 },
		{ "FullChargeVol",  DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", "0, 10000, 1", "Full Charge Voltage", "mV", 1000, 3 },
		{ "ChargeEndVol",   DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", "0, 10000, 1", "Full Discharge Voltage", "mV", 1000, 3 },
		{ "DischargingRate",DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", "0, 100, .1", "DischargingRate", "%", 10, 1 },
		{ "VoltageCap100",  DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", "0, 10000, 1", "Voltage at 100% Capacity", "mV", 1000, 3 },
		{ "VoltageCap90",   DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", "0, 10000, 1", "Voltage at 90% capacity", "mV", 1000, 3 },
		{ "VoltageCap80",   DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", "0, 10000, 1", "Voltage at 80% capacity", "mV", 1000, 3 },
		{ "VoltageCap70",   DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", "0, 10000, 1", "Voltage at 70% capacity", "mV", 1000, 3 },
		{ "VoltageCap60",   DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", "0, 10000, 1", "Voltage at 60% capacity", "mV", 1000, 3 },
		{ "VoltageCap50",   DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", "0, 10000, 1", "Voltage at 50% capacity", "mV", 1000, 3 },
		{ "VoltageCap40",   DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", "0, 10000, 1", "Voltage at 40% capacity", "mV", 1000, 3 },
		{ "VoltageCap30",   DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", "0, 10000, 1", "Voltage at 30% capacity", "mV", 1000, 3 },
		{ "VoltageCap20",   DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", "0, 10000, 1", "Voltage at 20% capacity", "mV", 1000, 3 },
		{ "VoltageCap10",   DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", "0, 10000, 1", "Voltage at 10% capacity", "mV", 1000, 3 },
		{ "fet_ctrl_time_set", DATA_TYPE_INT, 0, 0, 0, 0, "range", "0, 999, 1", "Fet Control", "S", 0, 0 },
		{ "led_disp_time_set", DATA_TYPE_INT, 0, 0, 0, 0, "range", "0, 999, 1", "LED Timer", "S", 0, 0 },
		{0}
	};

	config_property_t func_params[] = {
		{ "BatteryConfig", DATA_TYPE_STRING, 0, 0, 0, 0, "mselect", "Switch,SCRL,BALANCE_EN,CHG_BALANCE,LED_EN,LED_NUM,RTC,EDV" },
		{0}
	};

	config_property_t ntc_params[] = {
		{ "NtcConfig", DATA_TYPE_STRING, 0, 0, 0, 0, "mselect", "NTC1,NTC2,NTC3,NTC4,NTC5,NTC6,NTC7,NTC8" },
		{0}
	};

	config_property_t balance_params[] = {
		{ "BalanceStartVoltage", DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", "0, 10000, 1", "Start Voltage", "mV", 1000, 3 },
		{ "BalanceWindow", DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", "0, 10000, 1", "BalancPrecision", "mV", 1000, 3 },
		{0}
	};

	config_property_t gps_params[] = {
		{ "GPS_VOL", DATA_TYPE_INT, 0, 0, 0, 0, "range", "0, 10000, 1", "GPS Off", "mV", 1000, 3 },
		{ "GPS_TIME", DATA_TYPE_INT, 0, 0, 0, 0, "range", "0, 999, 1", "GPS Time", "S", 0, 0 },
		{0}
	};

	config_property_t other_params[] = {
		/* name, type, dest, dsize, def, flags, scope, values, labels, units, scale, precision */
		{ "SenseResistor", DATA_TYPE_DOUBLE, 0, 0, 0, 0, },
		{ "PackNum", DATA_TYPE_INT, 0, 0, 0, 0, "select",
			"3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30",
			"number of cells in pack", 0, 0 },
		{ "CycleCount", DATA_TYPE_INT, 0, 0, 0, 0, 0, },
		{ "SerialNumber", DATA_TYPE_INT, 0, 0, 0, 0, 0, },
		{ "ManufacturerName", DATA_TYPE_STRING, 0, 0, 0, 0, },
		{ "DeviceName", DATA_TYPE_STRING, 0, 0, 0, 0, 0, },
		{ "ManufactureDate", DATA_TYPE_STRING, 0, 0, 0, 0, },
		{ "BarCode", DATA_TYPE_STRING, 0, 0, 0, 0, },
		{0}
	};

	config_property_t basic_params[] = {
#define CELL_RANGE "0, 10000, 1"
#define TIME_RANGE "0, 999, 1"
#define PACK_RANGE "0, 99999, 1"
#define TEMP_RANGE "-500, 500, 1"
		{ "CellOverVoltage", DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", CELL_RANGE, "COVP", "mV", 1000, 3 },
		{ "CellOVRelease", DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", CELL_RANGE, "COVP Release", "mV", 1000, 3 },
		{ "CellOVDelay", DATA_TYPE_INT, 0, 0, 0, 0, "range", TIME_RANGE, "COVP Delay", "S", 0 },
		{ "CellUnderVoltage", DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", CELL_RANGE, "CUVP", "mV", 1000, 3 },
		{ "CellUVRelease", DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", CELL_RANGE,"CUVP Release", "mV", 1000, 3 },
		{ "CellUVDelay", DATA_TYPE_INT, 0, 0, 0, 0, "range", TIME_RANGE, "CUVP Delay", "S", 0 },
		{ "PackOverVoltage", DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", PACK_RANGE, "POVP", "mV", 100, 1 },
		{ "PackOVRelease", DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", PACK_RANGE, "POVP Release", "mV", 100, 1 },
		{ "PackOVDelay", DATA_TYPE_INT, 0, 0, 0, 0, "range", TIME_RANGE, "POVP Delay", "S", 0 },
		{ "PackUnderVoltage", DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", PACK_RANGE, "PUVP", "mV", 100, 1 },
		{ "PackUVRelease", DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", PACK_RANGE, "PUVP Release", "mV", 100, 1 },
		{ "PackUVDelay", DATA_TYPE_INT, 0, 0, 0, 0, "range", TIME_RANGE, "PUVP Delay", "S", 0 },
		{ "ChgOverTemp", DATA_TYPE_INT, 0, 0, 0, 0, "range", TEMP_RANGE, "CHGOT", "C", 0 },
		{ "ChgOTRelease", DATA_TYPE_INT, 0, 0, 0, 0, "range", TEMP_RANGE, "CHGOT Release", "C", 0 },
		{ "ChgOTDelay", DATA_TYPE_INT, 0, 0, 0, 0, "range", TIME_RANGE, "CHGOT Delay", "S", 0 },
		{ "ChgLowTemp", DATA_TYPE_INT, 0, 0, 0, 0, "range", TEMP_RANGE, "CHGUT", "C", 0 },
		{ "ChgUTRelease", DATA_TYPE_INT, 0, 0, 0, 0, "range", TEMP_RANGE, "CHGUT Release", "C", 0 },
		{ "ChgUTDelay", DATA_TYPE_INT, 0, 0, 0, 0, "range", TIME_RANGE, "CHGUT Delay", "S", 0 },
		{ "DisOverTemp", DATA_TYPE_INT, 0, 0, 0, 0, "range", TEMP_RANGE, "DSGOT", "C", 0 },
		{ "DsgOTRelease", DATA_TYPE_INT, 0, 0, 0, 0, "range", TEMP_RANGE, "DSGOT Release", "C", 0 },
		{ "DsgOTDelay", DATA_TYPE_INT, 0, 0, 0, 0, "range", TIME_RANGE, "DSGOT Delay", "S", 0 },
		{ "DisLowTemp", DATA_TYPE_INT, 0, 0, 0, 0, "range", TEMP_RANGE, "DSGUT", "C", 0 },
		{ "DsgUTRelease", DATA_TYPE_INT, 0, 0, 0, 0, "range", TEMP_RANGE, "DSGUT Release", "C", 0 },
		{ "DsgUTDelay", DATA_TYPE_INT, 0, 0, 0, 0, "range", TIME_RANGE, "DSGUT Delay", "S", 0 },
		{ "OverChargeCurrent", DATA_TYPE_INT, 0, 0, 0, 0, "range", "0, 99999, 1", "CHGOC", "mA", .1 },
		{ "ChgOCRDelay", DATA_TYPE_INT, 0, 0, 0, 0, "range", TIME_RANGE, "CHGOC Release", "S", 0 },
		{ "ChgOCDelay", DATA_TYPE_INT, 0, 0, 0, 0, "range", TIME_RANGE, "CHGOC Delay", "S", 0 },
		{ "OverDisCurrent", DATA_TYPE_INT, 0, 0, 0, 0, "range", "0, 99999, 1", "DSGOC", "mA", .1 },
		{ "DsgOCRDelay", DATA_TYPE_INT, 0, 0, 0, 0, "range", TIME_RANGE, "DSGOC Release", "S", 0 },
		{ "DsgOCDelay", DATA_TYPE_INT, 0, 0, 0, 0, "range", TIME_RANGE, "DSGOC Delay", "S", 0 },
		{0}
	};

	config_property_t hard_params[] = {
		{ "DoubleOCSC", DATA_TYPE_BOOL, 0, 0, 0, 0, 0, "Doubled Overcurrent and short-circuit value", 0, 0 },
		{ "DSGOC2", DATA_TYPE_INT, 0, 0, 0, 0, "select", dsgoc2_vals, 0, "mV", 0 },
		{ "DSGOC2Delay", DATA_TYPE_INT, 0, 0, 0, 0, "select", dsgoc2delay_vals, 0, "mS", 0 },
		{ "SCValue", DATA_TYPE_INT, 0, 0, 0, 0, "select", sc_vals, 0, "mV", 0 },
		{ "SCDelay", DATA_TYPE_INT, 0, 0, 0, 0, "select", scdelay_vals, 0, "uS", 0 },
		{ "HardCellOverVoltage", DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", CELL_RANGE, "High COVP", "mV", 1000, 3 },
		{ "HCOVPDelay", DATA_TYPE_INT, 0, 0, 0, 0, "select", hcovpdelay_vals, 0, "S", 0 },
		{ "HardCellUnderVoltage", DATA_TYPE_DOUBLE, 0, 0, 0, 0, "range", CELL_RANGE, "High COVP", "mV", 1000, 3 },
		{ "HCUVPDelay", DATA_TYPE_INT, 0, 0, 0, 0, "select", hcuvpdelay_vals, 0, "S", 0 },
		{ "SCRelease", DATA_TYPE_INT, 0, 0, 0, 0, "range", TIME_RANGE, "SC Release Time", "S", 0 },
		{0}
	};

	struct parmdir {
		char *name;
		config_property_t *parms;
	} *pd, allparms[] = {
		{ "Capacity Config", capacity_params },
		{ "Function Configuration", func_params },
		{ "NTC Configuration", ntc_params },
		{ "Balance Configuration", balance_params },
		{ "GPS Configuration", gps_params },
		{ "Other Configuration", other_params },
		{ "Basic Parameters", basic_params },
		{ "Hard Parameters", hard_params },
		{ 0 }
	};
	config_property_t *p;

	for(pd=allparms; pd->name; pd++) {
		dprintf(1,"adding params: %s\n", pd->name);
		for(p=pd->parms; p->name; p++) p->flags |= JBD_CONFIG_FLAG_EEPROM;
		config_add_props(s->ap->cp, pd->name, pd->parms, CONFIG_FLAG_NOSAVE | CONFIG_FLAG_NOPUB);
	}

	return 0;
}

int jbd_config(void *h, int req, ...) {
	jbd_session_t *s = h;
	va_list va;
	void **vp;
	int r;

	r = 1;
	va_start(va,req);
	dprintf(1,"req: %d\n", req);
	switch(req) {
	case SOLARD_CONFIG_INIT:
	    {
		dprintf(1,"**** CONFIG INIT *******\n");

		/* 1st arg is AP */
		s->ap = va_arg(va,solard_agent_t *);
		dprintf(1,"ap: %p\n", s->ap);

		/* -t takes precedence over config */
		dprintf(1,"tpinfo: %s\n", s->tpinfo);
		if (strlen(s->tpinfo)) {
			*s->transport = *s->target = *s->topts = 0;
			strncat(s->transport,strele(0,",",s->tpinfo),sizeof(s->transport)-1);
			strncat(s->target,strele(1,",",s->tpinfo),sizeof(s->target)-1);
			strncat(s->topts,strele(2,",",s->tpinfo),sizeof(s->topts)-1);
		}

		/* Init our transport */
		r = jbd_tp_init(s);
		dprintf(1,"r: %d, retry_tp: %d\n", r, s->retry_tp);
		if (!r && s->retry_tp) {
			do {
				dprintf(1,"calling open...\n");
				r = jbd_open(s);
				dprintf(1,"r: %d\n", r);
				if (r) {
					dprintf(1,"open failed, sleeping %d seconds...\n",s->wait_time);
					sleep(s->wait_time);
					dprintf(1,"retrying...\n");
					if (s->tp && s->tp_handle) s->tp->close(s->tp_handle);
				}
			dprintf(1,"r: %d\n", r);
			} while(r != 0);
		}

		/* Add our internal params to the config */
		jbd_config_add_parms(s);

		/* Add the data props */
//		jbd_config_add_jbd_data(s);

#ifdef JS
		/* Init JS */
		r = jbd_jsinit(s);
#else
		r = 0;
#endif
		r = 0;
	    }
	    break;
	case SOLARD_CONFIG_GET_INFO:
		dprintf(1,"**** GET_INFO *******\n");
		{
			json_value_t **vpp = va_arg(va,json_value_t **);
			dprintf(1,"vpp: %p\n", vpp);
			if (vpp) {
				*vpp = jbd_get_info(s);
				if (*vpp) r = 0;
			}
			r = 0;
		}
		break;
	case SOLARD_CONFIG_GET_DRIVER:
		dprintf(1,"GET_DRIVER called!\n");
		vp = va_arg(va,void **);
		if (vp) {
			*vp = s->tp;
			r = 0;
		}
		break;
	case SOLARD_CONFIG_GET_HANDLE:
		dprintf(1,"GET_HANDLE called!\n");
		vp = va_arg(va,void **);
		if (vp) {
			*vp = s->tp_handle;
			r = 0;
		}
		break;
	}
	dprintf(1,"returning: %d\n", r);
	va_end(va);
	return r;
}
