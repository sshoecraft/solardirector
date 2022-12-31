
#include "transports.h"

struct null_session {
	char target[SOLARD_TARGET_LEN];
	char topts[SOLARD_TOPTS_LEN];
};
typedef struct null_session null_session_t;

static void *null_new(void *target, void *topts) {
	null_session_t *s;

	s = calloc(sizeof(*s),1);
	if (!s) return 0;
	if (target) strncpy(s->target,target,sizeof(s->target)-1);
	if (topts) strncpy(s->topts,topts,sizeof(s->topts)-1);

	return s;
}

static int null_openclose(void *handle) {
	return 0;
}

static int null_readwrite(void *handle, uint32_t *control, void *buf, int buflen) {
	/* Cant read/write to buf - can expects id in buflen */
	return 0;
}

static int null_config(void *h, int func, ...) {
	va_list ap;
	int r;

	r = 1;
	va_start(ap,func);
	switch(func) {
	default:
//		dprintf(1,"error: unhandled func: %d\n", func);
		break;
	}
	return r;
}

static int null_destroy(void *handle) {
	null_session_t *s = handle;

        free(s);
        return 0;
}

solard_driver_t null_driver = {
	"null",
	null_new,		/* New */
	null_destroy,		/* Destroy */
	null_openclose,		/* Open */
	null_openclose,		/* Close */
	null_readwrite,		/* Read */
	null_readwrite,		/* Write */
	null_config,		/* Config */
};
