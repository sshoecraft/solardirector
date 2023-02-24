
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "common.h"
#include "transports.h"
#include "siutil.h"

#define DEBUG_STARTUP 0

#if DEBUG_STARTUP
#define _ST_DEBUG LOG_DEBUG
#else
#define _ST_DEBUG 0
#endif

int outfmt = 0;
FILE *outfp;
char sepch = ',';
char *sepstr = ",";
json_object_t *root_object;
extern double batcurmin,batcurmax;

char *siutil_version = "1.0";
char *si_version_string = "";

int si_config(void *h, int req, ...) {
	return 1;
}

int si_startstop(si_session_t *s, int op) {
	int retries,bytes;
	uint8_t data[8],b;

	dprintf(1,"op: %d\n", op);

	b = (op ? 1 : 2);
	dprintf(1,"b: %d\n", b);
	retries=10;
	while(retries--) {
		dprintf(1,"writing...\n");
		bytes = si_can_write(s,0x35c,&b,1);
		dprintf(1,"bytes: %d\n", bytes);
		if (bytes < 0) return 1;
		dprintf(1,"reading...\n");
		if (s->can_read(s,0x307,data,8) == 0) {
			if (debug >= 3) bindump("data",data,8);
			dprintf(1,"*** data[3] & 2: %d\n", data[3] & 0x0002);
			if (op) {
				if (data[3] & 0x0002) return 0;
			} else {
				if ((data[3] & 0x0002) == 0) return 0;
			}
		}
		sleep(1);
	}
	if (retries < 0) printf("start failed.\n");
	return (retries < 0 ? 1 : 0);
}

enum ACTIONS {
	ACTION_NONE=0,
	ACTION_LIST,
	ACTION_INFO,
	ACTION_SMANET,
	ACTION_DOWNLOAD,
	ACTION_GET,
	ACTION_SET,
	ACTION_FILE
};

//static solard_driver_t *transports[] = { &can_driver, &serial_driver, &rdev_driver, 0 };

int main(int argc, char **argv) {
	si_session_t *s;
	char configfile[256],cantpinfo[256],smatpinfo[256],chanpath[480],filename[256],list_type[16];
	char outfile[256];
	int list_flag,start_flag,stop_flag,ver_flag,quiet_flag,force_flag,mon_flag,mon_interval;
#ifdef SMANET
	int smanet_flag;
	smanet_chaninfo_t chaninfo;
	char downpath[320];
#endif
	int json_flag, pretty_flag, comma_flag, mult_flag;
//	int clear_flag;
	opt_proctab_t opts[] = {
                /* Spec, dest, type len, reqd, default val, have */
		{ "-b",0,0,0,0,0 },
#ifdef SMANET
		{ "-u::|SMANET <transport,target,topts>",&smatpinfo,DATA_TYPE_STRING,sizeof(smatpinfo)-1,0,"" },
		{ "-l|list SMANET channels",&list_flag,DATA_TYPE_BOOL,0,0,"N" },
		{ "-a|display all SMANET values",&smanet_flag,DATA_TYPE_BOOL,0,0,"N" },
		{ "-x::|specify SMANET channel type (para,spot,mean,test,all)",&list_type,DATA_TYPE_STRING,sizeof(list_type)-1,0,"all" },
		{ "-g|get multiple chans",&mult_flag,DATA_TYPE_BOOL,0,0,"N" },
		{ "-p::|channels path",&chanpath,DATA_TYPE_STRING,sizeof(chanpath)-1,0,"" },
		{ "-D::|download channels to path",&downpath,DATA_TYPE_STRING,sizeof(downpath)-1,0,"" },
		{ "-f:%|get/set vals in file",&filename,DATA_TYPE_STRING,sizeof(filename)-1,0,"" },
		{ "-q|dont list changes",&quiet_flag,DATA_TYPE_BOOL,0,0,"N" },
		{ "-w|write anyway (force)",&force_flag,DATA_TYPE_BOOL,0,0,"N" },
		{ "-v|verify settings in file",&ver_flag,DATA_TYPE_BOOL,0,0,"N" },
#endif
		{ "-j|json output",&json_flag,DATA_TYPE_BOOL,0,0,"N" },
		{ "-J|json output pretty print",&pretty_flag,DATA_TYPE_BOOL,0,0,"N" },
		{ "-C|comma delimited output",&comma_flag,DATA_TYPE_BOOL,0,0,"N" },
		{ "-o::|output file",&outfile,DATA_TYPE_STRING,sizeof(outfile)-1,0,"" },
		{ "-R|Start Sunny Island",&start_flag,DATA_TYPE_BOOL,0,0,"N" },
		{ "-S|Stop Sunny Island",&stop_flag,DATA_TYPE_BOOL,0,0,"N" },
		{ "-t::|CAN <transport,target,topts>",&cantpinfo,DATA_TYPE_STRING,sizeof(cantpinfo)-1,0,"" },
		{ "-c::|configfile",&configfile,DATA_TYPE_STRING,sizeof(configfile)-1,0,"" },
#if 0
		/* XXX doesnt work */
		{ "-x|clear value (set default)",&clear_flag,DATA_TYPE_BOOL,0,0,"N" },
#endif
		{ "-m|monitor info",&mon_flag,DATA_TYPE_BOOL,0,0,"N" },
		{ "-n:d|monitor interval (default: 3s)",&mon_interval,DATA_TYPE_INT,0,0,"1" },
		OPTS_END
	};
	int logopts = LOG_INFO|LOG_WARNING|LOG_ERROR|LOG_SYSERR|_ST_DEBUG;
	int action,source;
	cfg_info_t *cfg;
#ifdef SMANET
	smanet_channel_t *c;
	int list_mask;
#endif
	json_array_t *a;

	log_open("siutil",0,logopts);

	*configfile = *cantpinfo = *smatpinfo = 0;
	if (solard_common_init(argc,argv,siutil_version,opts,logopts)) return 1;
	if (debug >= 0) logopts |= LOG_DEBUG|LOG_DEBUG2;
	log_open("siutil",0,logopts);

	argc -= optind;
	argv += optind;

	{ int i; for(i=0; i < argc; i++) dprintf(1,"arg[%d]: %s\n",i,argv[i]); }

	dprintf(1,"argc: %d\n", argc);
	source = 1;
	if (list_flag) {
		action = ACTION_LIST;
#ifdef SMANET
	} else if (smanet_flag) {
		action = ACTION_SMANET;
	} else if (strlen(downpath)) {
		action = ACTION_DOWNLOAD;
#endif
	} else if (mult_flag) {
		action = ACTION_GET;
	} else if (argc == 0) {
		if (strlen(filename))
			action = ACTION_FILE;
		else {
			action = ACTION_INFO;
			source = 2;
		}
	} else if (argc > 1) {
		if (strcmp(argv[1],"-l") == 0)
			action = ACTION_LIST;
		else
			action = ACTION_SET;
	} else if (argc > 0) {
		action = ACTION_GET;
	}
	if (start_flag || stop_flag) source = 2;
	dprintf(1,"source: %d, action: %d\n", source, action);

#ifdef SMANET
	if (strcmp(list_type,"all") == 0) list_mask = CH_PARA | CH_SPOT | CH_MEAN | CH_TEST;
	else if (strcmp(list_type,"para") == 0) list_mask = CH_PARA;
	else if (strcmp(list_type,"spot") == 0) list_mask = CH_SPOT;
	else if (strcmp(list_type,"mean") == 0) list_mask = CH_MEAN;
	else if (strcmp(list_type,"test") == 0) list_mask = CH_TEST;
	else {
		printf("invalid list type: %s (must be one of para,spot,mean,test,all)\n",list_type);
		return 1;
	}
#endif

	s = si_driver.new(0,0);
	if (!s) {
		log_syserror("si_driver.new");
		return 1;
	}

	/* Read config */
	cfg = 0;
	dprintf(1,"configfile: %s\n", configfile);
	if (!strlen(configfile)) find_config_file("siutil.conf",configfile,sizeof(configfile)-1);
	dprintf(1,"configfile: %s\n", configfile);
	if (strlen(configfile)) {
		cfg = cfg_read(configfile);
		dprintf(1,"cfg: %p\n", cfg);
		if (!cfg) {
			log_write(LOG_SYSERROR,"error reading configfile '%s'", configfile);
			return 1;
		}
	}

	/* -t takes precedence over config */
	dprintf(1,"cantpinfo(%d): %s, cfg: %p\n", strlen(cantpinfo), cantpinfo, cfg);
	if (strlen(cantpinfo)) {
		strncat(s->can_transport,strele(0,",",cantpinfo),sizeof(s->can_transport)-1);
		strncat(s->can_target,strele(1,",",cantpinfo),sizeof(s->can_target)-1);
		strncat(s->can_topts,strele(2,",",cantpinfo),sizeof(s->can_topts)-1);
	} else if (cfg) {
		cfg_proctab_t tab[] = {
			{ "siutil","can_transport","CAN transport",DATA_TYPE_STRING,s->can_transport,SOLARD_TRANSPORT_LEN-1, "" },
			{ "siutil","can_target","CAN target",DATA_TYPE_STRING,s->can_target,SOLARD_TARGET_LEN-1, "" },
			{ "siutil","can_topts","CAN transport options",DATA_TYPE_STRING,s->can_topts,SOLARD_TOPTS_LEN-1, "" },
			CFG_PROCTAB_END
		};
		cfg_get_tab(cfg,tab);
		if (debug > 0) cfg_disp_tab(tab,"can",0);
	}
        if (!strlen(s->can_transport) || !strlen(s->can_target)) {
#if defined(WINDOWS)
                strcpy(s->can_transport,"serial");
                strcpy(s->can_target,"COM1");
                strcpy(s->can_topts,"9600");
#else
		strcpy(s->can_transport,"can");
		strcpy(s->can_target,"can0");
		strcpy(s->can_topts,"500000");
#endif
	}

	if (source == 2) {
		/* Load the can driver */
		dprintf(1,"s->can_transport: %s, s->can_target: %s, s->can_topts: %s\n", s->can_transport, s->can_target, s->can_topts);
		if (si_can_init(s)) {
			log_error(s->errmsg);
			return 1;
		}
		si_can_connect(s);
		if (!s->can_connected) return 1;
	}

	/* Init the SI driver */
	/* Only an error if we need can */
	if (!s && source == 2) return 1;

#ifdef SMANET
	*s->smanet_transport = *s->smanet_target = *s->smanet_topts = 0;
	dprintf(1,"cmatpinfo: %s\n", smatpinfo);
	if (strlen(smatpinfo)) {
		strncat(s->smanet_transport,strele(0,",",smatpinfo),sizeof(s->smanet_transport)-1);
		strncat(s->smanet_target,strele(1,",",smatpinfo),sizeof(s->smanet_target)-1);
		strncat(s->smanet_topts,strele(2,",",smatpinfo),sizeof(s->smanet_topts)-1);
	} else if (cfg) {
		cfg_proctab_t tab[] = {
			{ "siutil","smanet_transport","SMANET transport",DATA_TYPE_STRING,s->smanet_transport,SOLARD_TRANSPORT_LEN-1, "" },
			{ "siutil","smanet_target","SMANET target",DATA_TYPE_STRING,s->smanet_target,SOLARD_TARGET_LEN-1, "" },
			{ "siutil","smanet_topts","SMANET transport options",DATA_TYPE_STRING,s->smanet_topts,SOLARD_TOPTS_LEN-1, "" },
			{ "siutil","smanet_channels_path","SMANET transport options",DATA_TYPE_STRING,s->smanet_channels_path,sizeof(s->smanet_channels_path)-1, "" },
			CFG_PROCTAB_END
		};
		cfg_get_tab(cfg,tab);
		if (debug > 0) cfg_disp_tab(tab,"smanet",0);
	}
        if (!strlen(s->smanet_transport) || !strlen(s->smanet_target)) {
#if defined(WINDOWS)
                strcpy(s->smanet_transport,"serial");
                strcpy(s->smanet_target,"COM2");
                strcpy(s->smanet_topts,"9600");
#else
		strcpy(s->smanet_transport,"serial");
		strcpy(s->smanet_target,"/dev/ttyS0");
		strcpy(s->smanet_topts,"19200");
#endif
        }

	if (source == 1) {
		/* Load the smanet driver */
		dprintf(1,"s->smanet_transport: %s, s->smanet_target: %s, s->smanet_topts: %s\n",
			s->smanet_transport, s->smanet_target, s->smanet_topts);
		s->smanet = smanet_init();
		if (smanet_connect(s->smanet,s->smanet_transport, s->smanet_target, s->smanet_topts)) {
			log_error("unable to connect to SMANET: %s\n", smanet_get_errmsg(s->smanet));
			return 1;
		}
		if (!smanet_connected(s->smanet)) {
			log_error("unable to connect to SMANET: %s\n", smanet_get_errmsg(s->smanet));
			return 1;
		}
//		si_smanet_setup(s);
	}
#endif

	dprintf(1,"source: %d\n", source);
	if (start_flag || stop_flag) {
		si_driver.open(s);
		if (start_flag) return si_startstop(s,1);
		else if (stop_flag) return si_startstop(s,0);
	}

#ifdef SMANET
	if (s->smanet) {
		smanet_info_t info;

		/* if downpath specified, download chans */
		dprintf(1,"downpath: %s\n", downpath);
		if (strlen(downpath)) {
			log_write(LOG_INFO,"Downloading channels...\n");
			if (smanet_read_channels(s->smanet)) log_error("%s\n",smanet_get_errmsg(s->smanet));
			smanet_save_channels(s->smanet,downpath);
			smanet_get_chaninfo(s->smanet,&chaninfo);
		} else {
			smanet_get_info(s->smanet,&info);
			dprintf(1,"chanpath: %s, smanet_channels_path: %s\n", chanpath, s->smanet_channels_path);
			if (!strlen(chanpath)) {
				if (strlen(s->smanet_channels_path))
					strncpy(chanpath,s->smanet_channels_path,sizeof(chanpath)-1);
				else
					sprintf(chanpath,"%s/agents/si/%s_channels.json",SOLARD_LIBDIR,info.type);
			}
			fixpath(chanpath,sizeof(chanpath)-1);
			dprintf(1,"chanpath: %s\n", chanpath);
			if (smanet_load_channels(s->smanet,chanpath) != 0) {
				log_write(LOG_INFO,"Downloading channels...\n");
				smanet_read_channels(s->smanet);
				smanet_save_channels(s->smanet,chanpath);
			}
			smanet_get_chaninfo(s->smanet,&chaninfo);
		}
	}
#endif

	if (*outfile) {
		dprintf(1,"outfile: %s\n", outfile);
		outfp = fopen(outfile,"w+");
		if (!outfp) {
			perror("fopen outfile");
			return 1;
		}
	} else {
		outfp = fdopen(1,"w");
	}
	dprintf(1,"outfp: %p\n", outfp);

	if (pretty_flag) json_flag = 1;
	if (json_flag) {
		outfmt = 2;
		if (action == ACTION_LIST) {
			a = json_create_array();
		} else {
			root_object = json_create_object();
		}
	}
	else if (comma_flag) outfmt = 1;
	else outfmt = 0;


	dprintf(1,"action: %d\n", action);
	switch(action) {
	case ACTION_INFO:
		if (si_driver.open(s)) {
			log_error("unable to open si driver");
			return 1;
		}
		/* XXX Important */
		s->readonly = 1;
		dprintf(1,"mon_flag: %d\n", mon_flag);
		batcurmin = 1000; batcurmax = -1000;
		if (mon_flag) {
			monitor(s,mon_interval);
		} else {
			if (si_can_read_data(s,1)) return 1;
			display_data(s,0);
		}
		break;
#ifdef SMANET
	case ACTION_FILE:
		{
			FILE *fp;
			char line[256],label[64],value[128];
			double d,dv;
			char *text;


			fp = fopen(filename,"r");
			if (!fp) {
				log_syserror("fopen(%s,r)",filename);
				return 1;
			}
			while(fgets(line,sizeof(line)-1,fp)) {
				dprintf(1,"line: %s\n", line);
				if (*line == '#') continue;
				*label = *value = 0;
				strncpy(label,strele(0," ",line),sizeof(label)-1);
				strncpy(value,strele(1," ",line),sizeof(value)-1);
				dprintf(1,"label: %s, value: %s\n", label, value);
				if (smanet_get_value(s->smanet,label,&d,&text)) {
					log_write(LOG_ERROR,"%s: error getting value\n",label);
					return 1;
				}
				if (strlen(value)) {
					if (!text) {
						dv = strtod(value,0);
						dprintf(1,"dv: %f, d: %f\n", dv, d);
						if (!double_equals(dv,d) || force_flag) {
							if (ver_flag && !force_flag) printf("%s %f != %f\n",label,d,dv);
							else if (smanet_set_value(s->smanet,label,dv,0))
								printf("%s: error setting value: %f\n", label, dv);
							else if (!quiet_flag) printf("%s %f -> %f\n",label,d,dv);
						}
					} else {
						dprintf(1,"value: %s, text: %s\n", value, text);
						if (strcmp(text,"---") == 0) text = "Auto";
						if (strcmp(value,text) != 0 || force_flag) {
							if (ver_flag && !force_flag) printf("%s %s != %s\n",label,text,value);
							else if (smanet_set_value(s->smanet,label,0,value))
								printf("%s: error setting value: %f\n", label, dv);
							else if (!quiet_flag) printf("%s %s -> %s\n",label,text,value);
						}
					}
				} else {
					if (text) printf("%s %s\n",label,text);
					else if ((int)d == d) printf("%s %d\n",label,(int)d);
					else printf("%s %f\n",label,d);
				}
			}
			fclose(fp);
		}
		break;
	case ACTION_LIST:
		if (argc > 0) {
			char *p;
			register int i;

			dprintf(1,"argv[0]: %s\n", argv[0]);
			c = smanet_get_channel(s->smanet,argv[0]);
			if (!c) {
				log_error("%s: channel not found\n",argv[0]);
				return 1;
			}
			if ((c->mask & CH_STATUS) == 0) {
				log_error("%s: channel has no options\n",argv[0]);
				return 1;
			}
			i=0;
			list_reset(c->strings);
			while((p = list_get_next(c->strings)) != 0) {
				if (strcmp(p,"---")==0) continue;
				switch(outfmt) {
				case 2:
					json_array_add_string(a,p);
					break;
				case 1:
					if (i) fprintf(outfp,",");
					fprintf(outfp,"%s",p);
					break;
				default:
					fprintf(outfp,"%s\n",p);
					break;
				}
				i++;
			}
			if (outfmt == 1) fprintf(outfp,"\n");
		} else {
			char *p;
			register int i;

			dprintf(1,"channel_count: %d\n", chaninfo.chancount);
			for(i=0; i < chaninfo.chancount; i++) {
				if ((chaninfo.channels[i].mask & list_mask) == 0) continue;
				p = chaninfo.channels[i].name;
				switch(outfmt) {
				case 2:
					json_array_add_string(a,p);
					break;
				case 1:
					if (i) fprintf(outfp,",");
					fprintf(outfp,"%s",p);
					break;
				default:
					fprintf(outfp,"%s\n",p);
					break;
				}
			}
			if (outfmt == 1) fprintf(outfp,"\n");
		}
		break;
	case ACTION_SMANET:
		{
			double d;
			char *text;
			smanet_channel_t *c;
			register int i;

			for(i=0; i < chaninfo.chancount; i++) {
				c = &chaninfo.channels[i];
				dprintf(5,"c->mask: %04x, list_mask: %04x\n", c->mask, list_mask);
				if ((c->mask & list_mask) == 0) continue;
				if (smanet_get_chanvalue(s->smanet,c,&d,&text)) {
					log_write(LOG_ERROR,"%s: error getting value\n",c->name);
					return 1;
				}
				if (text) dstr(c->name,"%s",text);
				else if ((int)d == d) dint(c->name,"%d",(int)d);
				else ddouble(c->name,"%f",d);
			}
		}
		break;
	case ACTION_GET:
		{
			double d;
			char *text;

			if (mult_flag) {
				smanet_multreq_t *mr;
				int mr_size,i;

				mr_size = argc * sizeof(smanet_multreq_t);
				dprintf(1,"mr_size: %d\n", mr_size);
				mr = malloc(mr_size);
				if (!mr) return 1;

				for(i=0; i < argc; i++) mr[i].name = argv[i];
				for(i=0; i < argc; i++) dprintf(1,"mr[%d]: %s\n", i, mr[i].name);
				if (smanet_get_multvalues(s->smanet,mr,argc)) {
					log_error("error getting multi values: %s\n", smanet_get_errmsg(s->smanet));
					free(mr);
					return 1;
				}
				for(i=0; i < argc; i++) {
					dprintf(1,"mr[%d]: value: %f, text: %s\n", i, mr[i].value, mr[i].text);
					dprintf(1,"text: %p\n", text);
					if (mr[i].text) dstr(mr[i].name,"%s",mr[i].text);
					else if (double_isint(mr[i].value)) dint(mr[i].name,"%d",(int)mr[i].value);
					else ddouble(mr[i].name,"%f",mr[i].value);
				}

			} else {
				if (smanet_get_value(s->smanet,argv[0],&d,&text)) {
					log_write(LOG_ERROR,"%s: error getting value\n",argv[0]);
					return 1;
				}
				if (text) dstr(argv[0],"%s",text);
				else if ((int)d == d) dint(argv[0],"%d",(int)d);
				else ddouble(argv[0],"%f",d);
			}
		}
		break;
	case ACTION_SET:
		smanet_set_value(s->smanet,argv[0],atof(argv[1]),argv[1]);
		break;
#endif
	}

	if (outfmt == 2) {
		json_value_t *v;
		char *p;

		v = (action == ACTION_LIST ? json_array_get_value(a) : json_object_value(root_object));
		p = json_dumps(v,pretty_flag);
		if (p) {
			fprintf(outfp,"%s",p);
			free(p);
		}
	}

	if (source == 1) smanet_disconnect(s->smanet);
	dprintf(1,"done!\n");
	return 0;
}
