
#include "common.h"
#include "socket.h"

static void *si_console(void *ctx) {
	si_session_t *s = ctx;

	return (void *)0;
}

int si_console_init(si_session_t *s) {
	return 1;
}
