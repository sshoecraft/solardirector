
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 2
#include "debug.h"

#include "template.h"
#include "transports.h"

#if defined(__WIN32) || defined(__WIN64)
solard_driver_t *template_transports[] = { &ip_driver, &serial_driver, 0 };
#else
#ifdef BLUETOOTH
solard_driver_t *template_transports[] = { &ip_driver, &bt_driver, &serial_driver, &can_driver, 0 };
#else
solard_driver_t *template_transports[] = { &ip_driver, &serial_driver, &can_driver, 0 };
#endif
#endif

static int template_close(void *handle) {
	template_session_t *s = handle;
	int r;

	dprintf(3,"s: %p\n", s);
	r = 0;
	if (check_state(s,TEMPLATE_STATE_OPEN)) {
		if (s->tp && s->tp->close(s->tp_handle) == 0)
			clear_state(s,TEMPLATE_STATE_OPEN);
		else
			r = 1;
	}
	dprintf(3,"returning: %d\n", r);
	return r;
}

#if 0
/* init the transport */
int template_tp_init(template_session_t *s) {
	solard_driver_t *old_driver;
	void *old_handle;

	/* If it's open, close it */
	if (check_state(s,TEMPLATE_STATE_OPEN)) template_close(s);

	/* Save current driver & handle */
	dprintf(1,"s->tp: %p, s->tp_handle: %p\n", s->tp, s->tp_handle);
	if (s->tp && s->tp_handle) {
		old_driver = s->tp;
		old_handle = s->tp_handle;
	} else {
		old_driver = 0;
		old_handle = 0;
	}

	/* Find the driver */
	dprintf(1,"transport: %s\n", s->transport);
	if (strlen(s->transport)) s->tp = find_driver(template_transports,s->transport);
	dprintf(1,"s->tp: %p\n", s->tp);
	if (!s->tp) {
		/* Restore old driver & handle, open it and return */
		dprintf(1,"old_driver: %p, old_handle: %p\n", old_driver, old_handle);
		if (old_driver && old_handle) {
			dprintf(1,"restoring...\n");
			s->tp = old_driver;
			s->tp_handle = old_handle;
			return 1;
		} else if (strlen(s->transport)) {
			sprintf(s->errmsg,"unable to find driver for transport: %s",s->transport);
			return 1;
		} else {
			sprintf(s->errmsg,"transport is empty!\n");
			return 2;
		}
	}

	/* Open new one */
	dprintf(1,"using driver: %s\n", s->tp->name);
	s->tp_handle = s->tp->new(s->target, s->topts);
	dprintf(1,"tp_handle: %p\n", s->tp_handle);
	if (!s->tp_handle) {
		dprintf(1,"old_driver: %p, old_handle: %p\n", old_driver, old_handle);
		/* Restore old driver */
		if (old_driver && old_handle) {
			dprintf(1,"restoring...\n");
			s->tp = old_driver;
			s->tp_handle = old_handle;
			/* Make sure we dont close it below */
			old_driver = 0;
		} else {
			sprintf(s->errmsg,"unable to create new instance %s driver",s->tp->name);
			return 1;
		}
	}

	/* Warn if using the null driver */
	if (strcmp(s->tp->name,"null") == 0) log_warning("using null driver for I/O\n");

	/* Open the new driver */
//	template_open(s);

	/* Close and free the old handle */
	dprintf(1,"old_driver: %p, old_handle: %p\n", old_driver, old_handle);
	if (old_driver && old_handle) {
		old_driver->close(old_handle);
		old_driver->destroy(old_handle);
	}
	return 0;
}
#endif

static void *template_new(void *transport, void *transport_handle) {
	template_session_t *s;

	s = calloc(1,sizeof(*s));
	if (!s) {
		log_syserr("template_new: calloc");
		return 0;
	}
	if (transport && transport_handle) {
		s->tp = transport;
		s->tp_handle = transport_handle;
	}

	return s;
}

static int template_free(void *handle) {
	template_session_t *s = handle;

        if (!s) return 1;

#ifdef JS
        if (s->props) free(s->props);
        if (s->funcs) free(s->funcs);
#endif

        /* Close and destroy transport */
	if (check_state(s,TEMPLATE_STATE_OPEN)) template_close(s);
	if (s->tp->destroy) s->tp->destroy(s->tp_handle);

        /* must be last */
        dprintf(dlevel,"destroying agent...\n");
        if (s->ap) agent_destroy_agent(s->ap);

	free(s);
	return 0;
}

static int template_open(void *handle) {
	template_session_t *s = handle;
	int r;

	dprintf(3,"s: %p\n", s);

	r = 0;
	dprintf(3,"state: %d\n", check_state(s,TEMPLATE_STATE_OPEN));
	if (!check_state(s,TEMPLATE_STATE_OPEN)) {
		if (s->tp && s->tp->open(s->tp_handle) == 0)
			set_state(s,TEMPLATE_STATE_OPEN);
		else
			r = 1;
	}
	dprintf(3,"returning: %d\n", r);
	return r;
}

int template_read(void *handle, uint32_t *what, void *buf, int buflen) {
	template_session_t *s = handle;

	if (!check_state(s,TEMPLATE_STATE_OPEN) && template_open(s)) return 1;

	/* Do some reading... */

#if 0
#ifdef JS
	/* If JS and read script exists, we'll use that */
	if (agent_script_exists(s->ap, s->ap->js.read_script)) return 0;
#endif


#ifdef MQTT
	if (mqtt_connected(s->ap->m)) {
		json_value_t *v = (s->flatten ? battery_to_flat_json(bp) : battery_to_json(bp));
		dprintf(2,"v: %p\n", v);
		if (v) {
			agent_pubdata(s->ap, v);
			json_destroy_value(v);
		}	
	}
#endif
#ifdef INFLUX
	if (influx_connected(s->ap->i)) {
		json_value_t *v = battery_to_flat_json(bp);
		dprintf(2,"v: %p\n", v);
		if (v) {
			char *j;

			influx_write_json(s->ap->i, "battery", v);
			j = json_dumps(v,0);
			if (j) {
				log_info("%s\n",j);
				free(j);
			}
			json_destroy_value(v);
		}	
	}
#endif
#endif
	return 0;
}

int template_write(void *handle, uint32_t *what, void *buf, int buflen) {
	return 0;
}

solard_driver_t template_driver = {
	AGENT_NAME,
	template_new,			/* New */
	template_free,			/* Free */
	template_open,			/* Open */
	template_close,			/* Close */
	template_read,			/* Read */
	template_write,			/* Write */
	template_config			/* Config */
};
