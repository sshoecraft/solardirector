#include <stdio.h>
#include <stdint.h>
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
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
#else
    #include <pthread.h>
    #include <unistd.h>
    #include <signal.h>
    #include <time.h>
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

// Cross-platform sleep function
void cross_platform_sleep(unsigned int milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

// Thread function for the alarm
#ifdef _WIN32
unsigned __stdcall alarm_thread_func(void* arg) {
#else
void* alarm_thread_func(void* arg) {
#endif
    Alarm* alarm = (Alarm*)arg;
    
    while (alarm->running) {
        cross_platform_sleep(alarm->interval_ms);
        
        if (alarm->running && alarm->callback) {
            alarm->callback(alarm->callback_arg);
        }
    }
    
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

// Initialize alarm
void alarm_init(Alarm* alarm, unsigned int interval_ms, void (*callback)(void*), void* callback_arg) {
    alarm->running = false;
    alarm->interval_ms = interval_ms;
    alarm->callback = callback;
    alarm->callback_arg = callback_arg;
    alarm->thread_handle = NULL;
}

// Start the alarm
bool alarm_start(Alarm* alarm) {
    if (alarm->running) {
        return false; // Already running
    }
    
    alarm->running = true;
    
#ifdef _WIN32
    alarm->thread_handle = (HANDLE)_beginthreadex(
        NULL,           // security
        0,              // stack size
        alarm_thread_func, // thread function
        alarm,          // argument
        0,              // creation flags
        NULL            // thread id
    );
    
    return alarm->thread_handle != NULL;
#else
    int result = pthread_create(&alarm->thread, NULL, alarm_thread_func, alarm);
    return result == 0;
#endif
}

// Stop the alarm
void alarm_stop(Alarm* alarm) {
    if (!alarm->running) {
        return;
    }
    
    alarm->running = false;
    
#ifdef _WIN32
    if (alarm->thread_handle) {
        WaitForSingleObject(alarm->thread_handle, INFINITE);
        CloseHandle(alarm->thread_handle);
        alarm->thread_handle = NULL;
    }
#else
    pthread_join(alarm->thread, NULL);
#endif
}

// Check if alarm is running
bool alarm_is_running(const Alarm* alarm) {
    return alarm->running;
}

// Change interval while running
void alarm_set_interval(Alarm* alarm, unsigned int interval_ms) {
    alarm->interval_ms = interval_ms;
}

// Set callback argument
void alarm_set_callback_arg(Alarm* alarm, void* callback_arg) {
    alarm->callback_arg = callback_arg;
}

#ifdef ALARM_MAIN_TEST
// Example callback functions
void my_alarm_callback(void* arg) {
    char* message = (char*)arg;
    static int count = 0;
    count++;
    
    // Get current time for display
    time_t now = time(NULL);
    char* time_str = ctime(&now);
    // Remove newline from time string
    if (time_str && time_str[strlen(time_str) - 1] == '\n') {
        time_str[strlen(time_str) - 1] = '\0';
    }
    
    printf("[%s] %s (Count: %d)\n", time_str ? time_str : "Unknown", 
           message ? message : "Alarm triggered!", count);
    fflush(stdout);
}

void counter_callback(void* arg) {
    int* counter = (int*)arg;
    if (counter) {
        (*counter)++;
        printf("*** Counter callback: %d ***\n", *counter);
    } else {
        printf("*** Counter callback: NULL argument ***\n");
    }
    fflush(stdout);
}

// Data structure example
typedef struct {
    char name[64];
    int value;
    double multiplier;
} AlarmData;

void data_callback(void* arg) {
    AlarmData* data = (AlarmData*)arg;
    if (data) {
        data->value++;
        printf(">>> %s: value=%d, scaled=%.2f\n", 
               data->name, data->value, data->value * data->multiplier);
    } else {
        printf(">>> Data callback: NULL argument\n");
    }
    fflush(stdout);
}

// Signal handler for graceful shutdown
static Alarm* global_alarm = NULL;

#ifdef _WIN32
BOOL WINAPI console_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        printf("\nShutting down alarm...\n");
        if (global_alarm) {
            alarm_stop(global_alarm);
        }
        return TRUE;
    }
    return FALSE;
}
#else
void signal_handler(int sig) {
    printf("\nShutting down alarm...\n");
    if (global_alarm) {
        alarm_stop(global_alarm);
    }
    exit(0);
}
#endif

int main() {
    printf("Cross-platform Alarm Timer\n");
    printf("Platform: ");
#ifdef _WIN32
    printf("Windows\n");
#elif defined(__APPLE__)
    printf("macOS\n");
#else
    printf("Linux\n");
#endif
    
    // Set up signal handling for graceful shutdown
#ifdef _WIN32
    SetConsoleCtrlHandler(console_handler, TRUE);
#else
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif
    
    // Create and configure alarm
    Alarm alarm;
    global_alarm = &alarm;
    
    // Initialize alarm to call function every 10 seconds (10000 ms)
    char* message1 = "Primary alarm triggered!";
    alarm_init(&alarm, 10000, my_alarm_callback, message1);
    
    printf("\nStarting alarm - will trigger every 10 seconds\n");
    printf("Press Ctrl+C to stop\n\n");
    
    // Start the alarm
    if (!alarm_start(&alarm)) {
        printf("Failed to start alarm!\n");
        return 1;
    }
    
    // Let it run for a while, then demonstrate changing interval
    printf("Running with 10-second interval...\n");
    cross_platform_sleep(35000); // Let it trigger 3-4 times
    
    printf("\nChanging interval to 5 seconds and message...\n");
    char* message2 = "Fast alarm triggered!";
    alarm_set_interval(&alarm, 5000);
    alarm_set_callback_arg(&alarm, message2);
    cross_platform_sleep(20000); // Let it trigger 4 times with new interval
    
    printf("\nStopping alarm...\n");
    alarm_stop(&alarm);
    
    printf("Alarm stopped. Demonstrating multiple alarms with different data types...\n");
    
    // Demonstrate multiple alarms with different argument types
    Alarm alarm1, alarm2, alarm3;
    
    // Alarm 1: String message
    char* msg = "Timer 1 event";
    alarm_init(&alarm1, 7000, my_alarm_callback, msg);
    
    // Alarm 2: Integer counter
    int counter = 0;
    alarm_init(&alarm2, 4000, counter_callback, &counter);
    
    // Alarm 3: Structured data
    AlarmData data = {"Sensor Monitor", 0, 2.5};
    alarm_init(&alarm3, 6000, data_callback, &data);
    
    printf("\nStarting three alarms with different data types...\n");
    printf("- Alarm 1: 7s interval, string message\n");
    printf("- Alarm 2: 4s interval, integer counter\n");
    printf("- Alarm 3: 6s interval, structured data\n\n");
    
    alarm_start(&alarm1);
    alarm_start(&alarm2);
    alarm_start(&alarm3);
    
    cross_platform_sleep(30000); // Run for 30 seconds
    
    printf("\nStopping all alarms...\n");
    alarm_stop(&alarm1);
    alarm_stop(&alarm2);
    alarm_stop(&alarm3);
    
    printf("Final counter value: %d\n", counter);
    printf("Final data: name=%s, value=%d, scaled=%.2f\n", 
           data.name, data.value, data.value * data.multiplier);
    
    printf("Demo complete!\n");
    return 0;
}
#endif /* ALARM_MAIN_TEST */
