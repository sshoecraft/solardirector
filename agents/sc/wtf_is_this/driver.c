/*
 * Service Controller Driver
 * Minimal driver implementation for SC agent
 */

#include "sc.h"

static void *sc_new(void *transport, void *transport_handle) {
    sc_session_t *s = calloc(1, sizeof(*s));
    if (!s) {
        log_syserr("sc_new: calloc");
        return 0;
    }
    return s;
}

static int sc_free(void *handle) {
    sc_session_t *s = handle;
    if (!s) return 1;

    /* Destroy agent last */
    if (s->ap) agent_destroy_agent(s->ap);
    free(s);
    return 0;
}

int sc_config(void *h, int req, ...) {
    // No special config handling needed
    return 0;
}

solard_driver_t sc_driver = {
    "sc",
    sc_new,         /* New */
    sc_free,        /* Free */
    0,              /* Open */
    0,              /* Close */
    0,              /* Read */
    0,              /* Write */
    sc_config       /* Config */
};