

/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "siutil.h"

static char temp[256];

void dint(char *label, char *format, int val) {
	dprintf(3,"label: %s, val: %d\n", label, val);
	switch(outfmt) {
	case 2:
		json_object_set_number(root_object, label, val);
		break;
	case 1:
		sprintf(temp,"%%s,%s\n",format);
		dprintf(3,"temp: %s\n", temp);
		fprintf(outfp,temp,label,val);
		break;
	default:
		sprintf(temp,"%%-25s %s\n",format);
		dprintf(3,"temp: %s\n", temp);
		fprintf(outfp,temp,label,val);
		break;
	}
}
#define _dint(l,v) dint(l,"%d",v)

void ddouble(char *label, char *format, double val) {
	dprintf(3,"label: %s, val: %f\n", label, val);
	switch(outfmt) {
	case 2:
		json_object_set_number(root_object, label, val);
		break;
	case 1:
		sprintf(temp,"%%s,%s\n",format);
		dprintf(3,"temp: %s\n", temp);
		fprintf(outfp,temp,label,val);
		break;
	default:
		sprintf(temp,"%%-25s %s\n",format);
		dprintf(3,"temp: %s\n", temp);
		fprintf(outfp,temp,label,val);
		break;
	}
}
#define _ddouble(l,v) ddouble(l,"%f",v)

void dstr(char *label, char *format, char *val) {
	dprintf(3,"label: %s, val: %s\n", label, val);
	switch(outfmt) {
	case 2:
		json_object_set_string(root_object, label, val);
		break;
	case 1:
		sprintf(temp,"%%s,%s\n",format);
		dprintf(3,"temp: %s\n", temp);
		fprintf(outfp,temp,label,val);
		break;
	default:
		sprintf(temp,"%%-25s %s\n",format);
		dprintf(3,"temp: %s\n", temp);
		fprintf(outfp,temp,label,val);
		break;
	}
}
#define _dstr(l,v) dstr(l,"%s",v)

#define dbool(l,v) _dstr(l,v ? "true" : "false")

static inline void _addstr(char *str,char *newstr) {
	dprintf(4,"str: %s, newstr: %s\n", str, newstr);
	if (strlen(str)) strcat(str,sepstr);
	dprintf(4,"str: %s\n", str);
	if (outfmt == 2) strcat(str,"\"");
	strcat(str,newstr);
	if (outfmt == 2) strcat(str,"\"");
	dprintf(4,"str: %s\n", str);
}

void _outpower(char *label, double l1, double l2, double l3) {
	char str[128];

	str[0] = 0;
	sprintf(temp,"%3.2f",l1);
	_addstr(str,temp);
	sprintf(temp,"%3.2f",l2);
	_addstr(str,temp);
	sprintf(temp,"%3.2f",l3);
	_addstr(str,temp);
	switch(outfmt) {
	case 2:
		sprintf(temp,"[ %s ]",str);
		dprintf(1,"temp: %s\n", temp);
		json_object_dotset_value(root_object, label, json_parse(temp));
		break;
	case 1:
		fprintf(outfp,"%s,%s\n",label,str);
		break;
	default:
		fprintf(outfp,"%-25s %s\n",label,str);
		break;
	}
}

void _outrelay(char *label, int r1, int r2) {
	char str[32];

	str[0] = 0;
	_addstr(str,r1 ? "1" : "0");
	_addstr(str,r2 ? "1" : "0");
	switch(outfmt) {
	case 2:
		sprintf(temp,"[ %s ]",str);
		dprintf(1,"temp: %s\n", temp);
		json_object_dotset_value(root_object, label, json_parse(temp));
		break;
	case 1:
		fprintf(outfp,"%s,%s\n",label,str);
		break;
	default:
		fprintf(outfp,"%-25s %s\n",label,str);
		break;
	}
}

double batcurmin, batcurmax;

void dbat(si_session_t *s) {
	if (outfmt) ddouble("Battery Current","%3.2f",s->data.battery_current);
	else {
		double avg;
		if (s->data.battery_current < batcurmin) batcurmin = s->data.battery_current;
		else if (s->data.battery_current > batcurmax) batcurmax = s->data.battery_current;
		avg = (batcurmin + batcurmax) / 2;
		fprintf(outfp,"%-25s %3.2f (min: %3.2f, max: %3.2f, avg: %3.2f)\n","Battery Current",s->data.battery_current,
			batcurmin, batcurmax, avg);
	}
}

#if 0
#define _getshort(v) (short)( ((v)[1]) << 8 | ((v)[0]) )
#define _getlong(v) (long)( ((v)[3]) << 24 | ((v)[2]) << 16 | ((v)[1]) << 8 | ((v)[0]) )

void dodump(si_session_t *s, int id) {
	uint8_t data[8];
	double x,y;

	memset(data,0,sizeof(data));
	s->can_read(s,id,data,8);
	sprintf(temp,"%03x",id);
	bindump(temp,data,8);
	x = _getlong(&data[0]);
	y = _getlong(&data[4]);
	printf("%s 1: %d %f %x, 2: %d %f %x\n", temp, (int)x, x, (unsigned int)x, (int)y, y, (unsigned int)y);
}
#endif

void display_data(si_session_t *s,int mon_flag) {
	ddouble("TotLodPwr","%3.2f",s->data.TotLodPwr);
	_outpower("Active Grid",s->data.active_grid_l1,s->data.active_grid_l2,s->data.active_grid_l3);
	_outpower("Active SI",s->data.active_si_l1,s->data.active_si_l2,s->data.active_si_l3);
	_outpower("Reactive Grid",s->data.reactive_grid_l1,s->data.reactive_grid_l2,s->data.reactive_grid_l3);
	_outpower("Reactive SI",s->data.reactive_si_l1,s->data.reactive_si_l2,s->data.reactive_si_l3);
	_outpower("AC1 Voltage",s->data.ac1_voltage_l1,s->data.ac1_voltage_l2,s->data.ac1_voltage_l3);
	ddouble("AC1 Frequency","%2.2f",s->data.ac1_frequency);
	ddouble("AC1 Current","%2.2f",s->data.ac1_current);
	_outpower("AC2 Voltage",s->data.ac2_voltage_l1,s->data.ac2_voltage_l2,s->data.ac2_voltage_l3);
	ddouble("AC2 Frequency","%2.2f",s->data.ac2_frequency);
	ddouble("AC2 Current","%2.2f",s->data.ac2_current);
	ddouble("Battery Voltage","%3.2f",s->data.battery_voltage);
	if (mon_flag) dbat(s);
	else ddouble("Battery Current","%3.2f",s->data.battery_current);
	ddouble("Battery Temp","%3.2f",s->data.battery_temp);
	ddouble("Battery SoC","%3.2f",s->data.battery_soc);
	ddouble("Battery CVSP","%3.2f",s->data.battery_cvsp);
	ddouble("Battery SOH","%3.2f",s->data.battery_soh);
	_outrelay("Master Relay",s->data.relay1,s->data.relay2);
	_outrelay("Slave1 Relays",s->data.s1_relay1,s->data.s1_relay2);
	_outrelay("Slave2 Relays",s->data.s2_relay1,s->data.s2_relay2);
	_outrelay("Slave3 Relays",s->data.s3_relay1,s->data.s3_relay2);
//	dbool("Gen On",s->data.GnOn);
#if 0
61633   33921.509294   0.550920    0x181       8    BC D4 01 00 F9 D8 01 00  TPDO1      0x01
62740   33921.291691   0.539855    0x182       8    B4 D4 01 00 55 D9 01 00  TPDO1      0x02
65042   33921.470177   0.520279    0x183       8    77 D4 01 00 23 D8 01 00  TPDO1      0x03
16622   33920.516405   2.040832    0x191       8    D6 97 00 00 3A 70 00 00  TPDO1      0x11
16606   33920.570715   2.041846    0x192       8    B8 A3 00 00 B6 82 00 00  TPDO1      0x12
16784   33920.950843   2.022806    0x193       8    5C A6 00 00 6E 7D 00 00  TPDO1      0x13
61629   33921.289509   0.551052    0x1B1       8    00 00 00 00 00 00 00 00  TPDO1      0x31
62737   33921.412395   0.540647    0x1B2       8    00 00 00 00 00 00 00 00  TPDO1      0x32
65034   33921.209947   0.519584    0x1B3       8    00 00 00 00 00 00 00 00  TPDO1      0x33
61621   33921.344444   0.550891    0x1C1       8    5F EA 00 00 28 11 00 00  TPDO1      0x41
62736   33921.111759   0.540801    0x1C2       8    5F EA 00 00 49 21 00 00  TPDO1      0x42
65036   33921.404965   0.519631    0x1C3       8    5F EA 00 00 BE 0E 00 00  TPDO1      0x43
32373   33920.848495   1.046000    0x1E1       8    00 00 00 00 00 00 00 00  TPDO1      0x61
33211   33921.051932   1.022798    0x1E2       8    00 00 00 00 00 00 00 00  TPDO1      0x62
32518   33920.624061   1.041640    0x1E3       8    00 00 00 00 00 00 00 00  TPDO1      0x63
32375   33920.793816   1.047211    0x1F1       8    0B 02 00 00 64 F7 3F 00  TPDO1      0x71
33212   33920.690937   1.021827    0x1F2       8    FF 03 00 00 B0 C0 1B 00  TPDO1      0x72
32518   33920.755241   1.042886    0x1F3       8    C1 01 00 00 44 B9 1B 00  TPDO1      0x73
32374   33921.013550   1.046934    0x201       8    B0 01 00 00 D4 FE FF FF  RPDO1      0x01
33212   33920.872045   1.023024    0x202       8    FB 03 00 00 A9 FF FF FF  RPDO1      0x02
32517   33921.015102   1.042736    0x203       8    A6 01 00 00 6F FF FF FF  RPDO1      0x03
61647   33921.344801   0.549366    0x221       8    2C 01 00 00 03 00 01 00  RPDO1      0x21
62733   33921.412786   0.540282    0x222       8    66 01 00 00 03 00 01 00  RPDO1      0x22
65032   33921.340181   0.520025    0x223       8    A3 00 00 00 03 00 01 00  RPDO1      0x23
#endif

#if 0
	{
		int ids[] = { 0x181, 0x191, 0x1B1, 0x1C1, 0x1E1, 0x1F1, 0x201, 0x221 };
#define NIDS (sizeof(ids)/sizeof(int))
		int i;

		for(i = 0; i < NIDS; i++) {
			dodump(s,ids[i]);
			dodump(s,ids[i]+1);
			dodump(s,ids[i]+2);
		}
	}
#else
	_dstr("Generator Running",s->data.GnRn ? "true" : "false");
	_dstr("Generator Autostart",s->data.AutoGn ? "true" : "false");
	_dstr("AutoLodExt",s->data.AutoLodExt ? "true" : "false");
	_dstr("AutoLodSoc",s->data.AutoLodSoc ? "true" : "false");
	_dstr("Tm1",s->data.Tm1 ? "true" : "false");
	_dstr("Tm2",s->data.Tm2 ? "true" : "false");
	_dstr("ExtPwrDer",s->data.ExtPwrDer ? "true" : "false");
	_dstr("ExtVfOk",s->data.ExtVfOk ? "true" : "false");
	dbool("Grid On",s->data.GdOn);
	_dstr("Error",s->data.Error ? "true" : "false");
	_dstr("Run",s->data.Run ? "true" : "false");
	_dstr("Battery Fan On",s->data.BatFan ? "true" : "false");
	_dstr("Acid Circulation On",s->data.AcdCir ? "true" : "false");
	_dstr("MccBatFan",s->data.MccBatFan ? "true" : "false");
	_dstr("MccAutoLod",s->data.MccAutoLod ? "true" : "false");
	_dstr("CHP #1 On",s->data.Chp ? "true" : "false");
	_dstr("CHP #2 On",s->data.ChpAdd ? "true" : "false");
	_dstr("SiComRemote",s->data.SiComRemote ? "true" : "false");
	_dstr("Overload",s->data.OverLoad ? "true" : "false");
	_dstr("ExtSrcConn",s->data.ExtSrcConn ? "true" : "false");
	_dstr("Silent",s->data.Silent ? "true" : "false");
	_dstr("Current",s->data.Current ? "true" : "false");
	_dstr("FeedSelfC",s->data.FeedSelfC ? "true" : "false");
	_dint("Charging procedure",s->data.charging_proc);
	_dint("State",s->data.state);
	_dint("Error Message",s->data.errmsg);
#if 0
	ddouble("PVPwrAt","%3.2f",s->data.PVPwrAt);
	ddouble("GdCsmpPwrAt","%3.2f",s->data.GdCsmpPwrAt);
	ddouble("GdFeedPwr","%3.2f",s->data.GdFeedPwr);
#endif
#endif
}

int monitor(si_session_t *s, int interval) {
//	struct tm *tm;
//	time_t t;


	while(1) {
//		t = time(NULL);
//		tm = localtime(&t);
//		if (si_can_get_data(s)) {
		if (si_can_read_data(s,1)) {
			log_error("unable to read data");
			return 1;
		}
//		sleep(3);
//		continue;
#ifdef __WIN32
		system("cls");
//		system("time /t");
#else
//		system("clear; echo \"**** $(date) ****\"; echo \"\"");
		system("clear");
#endif
//		fprintf(outfp,"%s\n", asctime(tm));
		display_data(s,1);
		fflush(outfp);
		if (interval <= 0) usleep(35000);
		else sleep(interval);
	}
	return 0;
}
