
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "jk.h"

extern char *jk_version_string;

int jk_agent_init(jk_session_t *s, int argc, char **argv) {
	opt_proctab_t jk_opts[] = {
		/* Spec, dest, type len, reqd, default val, have */
		{ "-t::|transport,target,opts",&s->tpinfo,DATA_TYPE_STRING,sizeof(s->tpinfo)-1,0,"" },
		{ "-r|retry transport connection",&s->retry_tp,DATA_TYPE_BOOL,0,0,"N" },
		{ "-w|retry wait time",&s->wait_time,DATA_TYPE_INT,0,0,"30" },
		{ "-F|flatten arrays",&s->flatten,DATA_TYPE_BOOLEAN,0,0,"N" },
		OPTS_END
	};
	config_property_t jk_props[] = {
		/* name, type, dest, dsize, def, flags */
		{ "transport", DATA_TYPE_STRING, s->transport, sizeof(s->transport)-1, "", 0 },
		{ "target", DATA_TYPE_STRING, s->target, sizeof(s->target)-1, "", 0 },
		{ "topts", DATA_TYPE_STRING, s->topts, sizeof(s->topts)-1, "", 0 },
		{ "flatten", DATA_TYPE_BOOLEAN, &s->flatten, 0, "no", 0 },
		{ "log_power", DATA_TYPE_BOOLEAN, &s->log_power, 0, "no", 0 },
		{ "start_at_one", DATA_TYPE_BOOLEAN, &s->start_at_one, 0, "yes", 0 },
		{ "state", DATA_TYPE_INT, &s->state, 0, 0, CONFIG_FLAG_READONLY | CONFIG_FLAG_PRIVATE },
		{0}
	};

	s->ap = agent_init(argc,argv,jk_version_string,jk_opts,&jk_driver,s,0,jk_props,0);
	if (!s->ap) return 1;
	return 0;
}

int jk_config(void *h, int req, ...) {
	jk_session_t *s = h;
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
		/* XXX hax */
		if (!strlen(s->topts)) strcpy(s->topts,"ffe1");

		/* Init our transport */
		r = jk_tp_init(s);
		dprintf(1,"r: %d, retry_tp: %d\n", r, s->retry_tp);
		if (!r && s->retry_tp) {
			do {
				dprintf(1,"calling open...\n");
				r = jk_open(s);
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
//		jk_config_add_parms(s);

		/* add data props */
//		jk_config_add_jk_data(s);

#ifdef JS
		/* Init JS */
		r = jk_jsinit(s);
#else
		r = 0;
#endif
	    }
	    break;
	case SOLARD_CONFIG_GET_INFO:
		{
			json_value_t **vpp = va_arg(va,json_value_t **);
			dprintf(1,"vpp: %p\n", vpp);
			if (vpp) {
				*vpp = jk_get_info(s);
				if (*vpp) r = 0;
			}
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
