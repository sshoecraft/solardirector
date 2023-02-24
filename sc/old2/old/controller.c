
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "controller.h"

struct controller_session {
	char name[SOLARD_NAME_LEN];
	solard_agent_t *ap;
	solard_driver_t *driver;
	void *handle;
	solard_controller_t info;
};
typedef struct controller_session controller_session_t;

static int controller_get_config(controller_session_t *s) {
	solard_controller_t *cp = &s->info;
	char *section_name = s->ap->section_name;

	dprintf(1,"instance_name: %s\n", s->ap->instance_name);
	struct cfg_proctab packtab[] = {
		{ section_name, "name", 0, DATA_TYPE_STRING,&cp->name,sizeof(cp->name)-1, s->ap->instance_name },
		CFG_PROCTAB_END
	};

	cfg_get_tab(s->ap->cfg,packtab);
	if (debug >= 3) cfg_disp_tab(packtab,s->info.name,1);
	return 0;
}

static void *controller_new(void *handle, void *driver, void *driver_handle) {
	solard_agent_t *conf = handle;
	controller_session_t *s;

	s = calloc(1,sizeof(*s));
	if (!s) {
		log_write(LOG_SYSERR,"controller_new: calloc");
		return 0;
	}
	s->ap = conf;
	s->driver = driver;
	s->handle = driver_handle;

	/* Get our specific config */
	controller_get_config(s);

	/* Update the role_data in agent conf */
	conf->role_data = &s->info;

	/* Save a copy of the name */
	strncpy(s->name,s->info.name,sizeof(s->name)-1);

	return s;
}

static int controller_open(void *handle) {
	return 0;
}

static int controller_close(void *handle) {
	return 0;
}

#if 0
static void _get_arr(char *name,void *dest, int len,json_value_t *v) {
//	solard_controller_t *cp = dest;
	json_array_t *a;
//	int i;

	dprintf(6,"type: %d(%s)\n", v->type, json_typestr(v->type));
	if (v->type != JSONArray) return;
 
	a = v->value.array;
	if (!a->count) return;
	return;
}

static void _set_arr(char *name,void *dest, int len,json_value_t *v) {
//	solard_controller_t *cp = dest;
	json_value_t *a;
//	int i;

	a = json_create_array();
	if (!a) return;
	dprintf(1,"len: %d\n", len);
	json_add_value(v,name,a);
	return;
}
#endif

static struct controller_states {
	int mask;
	char *label;
} states[] = {
#if 0
	{ CONTROLLER_STATE_SOMETHING, "Something" },
#endif
};
#define NSTATES (sizeof(states)/sizeof(struct controller_states))

static void _get_state(char *name, void *dest, int len, json_value_t *v) {
	solard_controller_t *cp = dest;
	list lp;
	char *p;
	int i;

	dprintf(4,"value: %s\n", v->value.string.chars);
	conv_type(DATA_TYPE_LIST,&lp,0,DATA_TYPE_STRING,v->value.string.chars,v->value.string.length);

	cp->state = 0;
	list_reset(lp);
	while((p = list_get_next(lp)) != 0) {
		dprintf(6,"p: %s\n", p);
		for(i=0; i < NSTATES; i++) {
			if (strcmp(p,states[i].label) == 0) {
				cp->state |= states[i].mask;
				break;
			}
		}
	}
	dprintf(4,"state: %x\n", cp->state);
	list_destroy(lp);
}

static void _set_state(char *name, void *dest, int len, json_value_t *v) {
	solard_controller_t *cp = dest;
	char temp[128],*p;
	int i,j;

	dprintf(1,"state: %x\n", cp->state);

	/* States */
	temp[0] = 0;
	p = temp;
	for(i=j=0; i < NSTATES; i++) {
		if (check_state(cp,states[i].mask)) {
			if (j) p += sprintf(p,",");
			p += sprintf(p,states[i].label);
			j++;
		}
	}
	dprintf(1,"temp: %s\n", temp);
	json_add_string(v, name, temp);
}

#if CONTROLLER_CELLRES
		{ "cellres",0,cp,NBAT,ACTION }, 
#endif
#define CONTROLLER_TAB(NTEMP,NBAT,ACTION,STATE) \
		{ "name",DATA_TYPE_STRING,&cp->name,sizeof(cp->name)-1,0 }, \
		{ "errcode",DATA_TYPE_INT,&cp->errcode,0,0 }, \
		{ "errmsg",DATA_TYPE_STRING,&cp->errmsg,sizeof(cp->errmsg)-1,0 }, \
		{ "state",0,cp,0,STATE }, \
		JSON_PROCTAB_END

#if 0
static void _dump_arr(char *name,void *dest, int flen,json_value_t *v) {
//	solard_controller_t *cp = dest;
//	char format[16];
	int *dlevel = (int *) v;
//	int i;

	if (debug >= *dlevel) {
		dprintf(1,"flen: %d\n", flen);
	}
	return;
}
#endif

static void _dump_state(char *name, void *dest, int flen, json_value_t *v) {
	solard_controller_t *cp = dest;
	char format[16];
	int *dlevel = (int *) v;

	sprintf(format,"%%%ds: %%d\n",flen);
	if (debug >= *dlevel) printf(format,name,cp->state);
}

void controller_dump(solard_controller_t *cp, int dlevel) {
	json_proctab_t tab[] = { CONTROLLER_TAB(cp->ntemps,cp->ncells,_dump_arr,_dump_state) }, *p;
	char format[16],temp[1024];
	int flen;

	flen = 0;
	for(p=tab; p->field; p++) {
		if (strlen(p->field) > flen)
			flen = strlen(p->field);
	}
	flen++;
	sprintf(format,"%%%ds: %%s\n",flen);
	dprintf(dlevel,"controller:\n");
	for(p=tab; p->field; p++) {
		if (p->cb) p->cb(p->field,p->ptr,flen,(void *)&dlevel);
		else {
			conv_type(DATA_TYPE_STRING,&temp,sizeof(temp)-1,p->type,p->ptr,p->len);
			if (debug >= dlevel) printf(format,p->field,temp);
		}
	}
}

json_value_t *controller_to_json(solard_controller_t *cp) {
	json_proctab_t controller_tab[] = { CONTROLLER_TAB(cp->ntemps,cp->ncells,_set_arr,_set_state) };

	return json_from_tab(controller_tab);
}

int controller_from_json(solard_controller_t *cp, char *str) {
	json_proctab_t controller_tab[] = { CONTROLLER_TAB(CONTROLLER_MAX_TEMPS,CONTROLLER_MAX_CELLS,_get_arr,_get_state) };
	json_value_t *v;

	v = json_parse(str);
	memset(cp,0,sizeof(*cp));
	json_to_tab(controller_tab,v);
//	controller_dump(cp,3);
	json_destroy(v);
	return 0;
};

int controller_send_mqtt(controller_session_t *s) {
	solard_controller_t *pp = &s->info;
	char temp[256],*p;
	json_value_t *v;

//	strcpy(pp->id,s->id);
//	strcpy(pp->name,s->name);
	v = controller_to_json(pp);
	if (!v) return 1;
	p = json_dumps(v,0);
	sprintf(temp,"%s/%s/%s/%s",SOLARD_TOPIC_ROOT,SOLARD_ROLE_CONTROLLER,s->name,SOLARD_FUNC_DATA);
	dprintf(2,"sending mqtt data...\n");
	if (mqtt_pub(s->ap->m, temp, p, 1, 0)) {
		log_write(LOG_ERROR,"unable to publish to mqtt, reconnecting");
		mqtt_disconnect(s->ap->m,5);
		mqtt_connect(s->ap->m,20);
	}
	free(p);
	json_destroy(v);
	return 0;
}

static int controller_read(void *handle,void *buf,int buflen) {
	controller_session_t *s = handle;
	int r;

	if (!s->driver->read) return 0;

	if (s->driver->open) {
		dprintf(5,"%s: opening...\n", s->name);
		if (s->driver->open(s->handle)) {
			dprintf(1,"%s: open error\n",s->name);
			return 1;
		}
	}
	dprintf(5,"%s: reading...\n", s->name);
	r = s->driver->read(s->handle,&s->info,sizeof(s->info));
	if (s->driver->close) {
		dprintf(5,"%s: closing\n", s->name);
		s->driver->close(s->handle);
	}
	dprintf(5,"%s: returning: %d\n", s->name, r);
	return r;
}

static int controller_write(void *handle, void *buf, int buflen) {
	controller_session_t *s = handle;
	int r;

	if (!s->driver->write) return controller_send_mqtt(s);

	if (s->driver->open) {
		dprintf(5,"%s: opening...\n", s->name);
		if (s->driver->open(s->handle)) {
			dprintf(1,"%s: open error\n",s->name);
			return 1;
		}
	}
	dprintf(5,"%s: writing...\n", s->name);
	r = s->driver->write(s->handle,&s->info,sizeof(s->info));
	if (s->driver->close) {
		dprintf(5,"%s: closing\n", s->name);
		s->driver->close(s->handle);
	}
	dprintf(5,"%s: returning: %d\n", s->name, r);
	if (!r) return controller_send_mqtt(s);
	else return r;
}

#if 0
static int controller_write(void *handle, void *buf, int buflen) {
	controller_session_t *s = handle;
	int r;

	dprintf(4,"%s: opening...\n", s->name);
	if (s->driver->open(s->handle)) {
		dprintf(1,"%s: open error\n",s->name);
		return 1;
	}
	dprintf(4,"%s: writing...\n", s->name);
	r = (s->driver->write ? s->driver->write(s->handle,&s->info,sizeof(s->info)) : 1);
	dprintf(4,"%s: closing\n", s->name);
	s->driver->close(s->handle);
	dprintf(4,"%s: returning: %d\n", s->name, r);
	return 1;
}
#endif

static int controller_config(void *h, int req, ...) {
	controller_session_t *s = h;
	va_list ap;
	int r;

	dprintf(1,"**** CONTROLLER CONFIG ****\n");

	r = 1;
	va_start(ap,req);
	dprintf(1,"req: %d\n", req);
	switch(req) {
	case SOLARD_CONFIG_GET_HANDLE:
		{
			void **hp;

			dprintf(1,"getting handle...\n");
			hp = va_arg(ap,void **);
			if (hp) {
				*hp = s->handle;
				r = 0;
			}
		}
		break;
	}
	dprintf(1,"returning: %d\n", r);
	return r;
}

solard_driver_t controller_driver = {
	SOLARD_DRIVER_ROLE,
	"Controller",
	controller_new,
	controller_open,
	controller_close,
	controller_read,
	controller_write,
	controller_config,
};
