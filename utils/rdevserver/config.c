
#include "rdevserver.h"
#include "transports.h"

#define dlevel 2

#if defined(__WIN32) || defined(__WIN64)
static solard_driver_t *transports[] = { &serial_driver, 0 };
#else
#ifdef BLUETOOTH
static solard_driver_t *transports[] = { &bt_driver, &can_driver, &serial_driver, 0 };
#else
static solard_driver_t *transports[] = { &can_driver, &serial_driver, 0 };
#endif
#endif

int rdev_add_device(rdev_config_t *conf, char *name) {
	char transport[SOLARD_TRANSPORT_LEN],target[SOLARD_TARGET_LEN],topts[SOLARD_TOPTS_LEN];
	bool shared;
	cfg_proctab_t tab[] = {
		{ name,"transport","Device transport",DATA_TYPE_STRING,&transport,sizeof(transport)-1, "" },
		{ name,"target","Device target",DATA_TYPE_STRING,&target,sizeof(target)-1, "" },
		{ name,"topts","Device transport options",DATA_TYPE_STRING,&topts,sizeof(topts)-1, "" },
		{ name,"shared","Can device be shared",DATA_TYPE_BOOL,&shared,0, "N" },
		CFG_PROCTAB_END
	};
	rdev_device_t dev;
	solard_driver_t *mp;
//	void *mp_handle;

	dprintf(dlevel,"reading...\n");
	cfg_get_tab(conf->cfg,tab);
	if (debug) cfg_disp_tab(tab,name,0);

	dprintf(dlevel,"transport: %s, target: %s, topts: %s\n", transport, target, topts);
	if (!strlen(transport) || !strlen(target)) {
		log_write(LOG_ERROR,"[%s] must have both transport and target\n",name);
		return 1;
	}

	/* Find the transport in the list of transports */
	mp = find_driver(transports,transport);
	dprintf(dlevel,"mp: %p\n", mp);
	if (!mp) goto add_device_error;

#if 0
	/* Test the driver */
	mp_handle = mp->new(target,topts);
	if (!mp_handle) goto add_device_error;
	if (mp->open(mp_handle)) goto add_device_error;
	mp->close(mp_handle);
	mp->destroy(mp_handle);
#endif

	memset(&dev,0,sizeof(dev));
	strncpy(dev.name,name,sizeof(dev.name)-1);
	strcpy(dev.target,target);
	strcpy(dev.topts,topts);
	dev.driver = mp;
	dev.shared = shared;
	pthread_mutex_init(&dev.lock,0);

	dprintf(dlevel,"adding device: %s\n", name);
	conf->devices[conf->device_count++] = dev;
	return 0;

add_device_error:
	dprintf(dlevel,"error adding device %s!\n", name);
	return 1;
}

int rdev_get_config(rdev_config_t *conf, char *configfile) {
	list devices = list_create();
	cfg_proctab_t tab[] = {
		{ "rdev", "port", "Network port", DATA_TYPE_INT,&conf->port, 0, "3900" },
		{ "rdev","devices","Device list",DATA_TYPE_STRING_LIST,devices,0,0 },
		CFG_PROCTAB_END
	};
	char *p;

	if (!strlen(configfile)) {
		log_write(LOG_ERROR,"configfile required.\n");
		return 1;
	}

	dprintf(dlevel,"reading...\n");
	conf->cfg = cfg_read(configfile);
	if (!conf->cfg) {
		log_write(LOG_SYSERROR,"error reading configfile '%s'", configfile);
		goto init_error;
	}
	cfg_get_tab(conf->cfg,tab);
	if (debug) cfg_disp_tab(tab,"rdev",0);

	dprintf(dlevel,"devices count: %d\n", list_count(devices));
	list_reset(devices);
	while((p = list_get_next(devices)) != 0) {
		if (rdev_add_device(conf,p)) return 1;
	}

	return 0;
init_error:
	free(conf);
	return 1;
}
