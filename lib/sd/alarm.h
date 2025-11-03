#ifndef __ALARM_H
#define __ALARM_H

#ifndef JS
#include <stdbool.h>
#else
/* When building with JS engine, bool might be defined differently */
#ifndef bool
typedef int bool;
#define true 1
#define false 0
#endif
#endif

#ifdef _WIN32
    #include <windows.h>
#else
    #include <pthread.h>
#endif

// Alarm structure
typedef struct {
    bool running;
    unsigned int interval_ms;
    void (*callback)(void*);
    void* callback_arg;
    void* thread_handle;
#ifndef _WIN32
    pthread_t thread;
#endif
} Alarm;

// Function prototypes
void alarm_init(Alarm* alarm, unsigned int interval_ms, void (*callback)(void*), void* callback_arg);
bool alarm_start(Alarm* alarm);
void alarm_stop(Alarm* alarm);
bool alarm_is_running(const Alarm* alarm);
void alarm_set_interval(Alarm* alarm, unsigned int interval_ms);
void alarm_set_callback_arg(Alarm* alarm, void* callback_arg);

#endif /* __ALARM_H */