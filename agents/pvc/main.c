
#include "pvc.h"
#include "__sd_build.h"

#define TESTING 0
#define TESTLVL 4

char *pvc_agent_version_string = "1.0-" STRINGIFY(__SD_BUILD);

int pvc_agent_init(int argc, char **argv, opt_proctab_t *pvc_opts, pvc_session_t *s) {
	config_property_t pvc_props[] = {
		{ "log_power", DATA_TYPE_BOOL, &s->log_power, 0, "0", 0,
			"select", "0, 1", "log output_power from each read", 0, 1, 0 },
		{0}
	};

	agent_init(argc,argv,pvc_agent_version_string,pvc_opts,&pvc_driver,s,0,pvc_props,0);
	if (!s->ap) return 1;

	return 0;
}

int main(int argc,char **argv) {
	pvc_session_t *s;
	bool norun_flag;
	opt_proctab_t pvc_opts[] = {
		/* Spec, dest, type len, reqd, default val, have */
		{ "-N|dont enter run loop",&norun_flag,DATA_TYPE_BOOL,0,0,"no" },
		OPTS_END
	};

	s = pvc_driver.new(0,0);
	if (!s) {
		perror("new");
		return 1;
	}
	if (pvc_agent_init(argc,argv,pvc_opts,s)) return 1;
	if (!norun_flag) agent_run(s->ap);
	else pvc_driver.read(s,0,0,0);
	return 0;
}
