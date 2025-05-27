
#ifndef __WPI_H
#define __WPI_H

struct bme_data {
	float c;		/* celcius */
	float h;		/* humidity */
	float f;		/* farenheight */
	float i;		/* farenheight heat index */
	float w;		/* wet bulb */
	float p;		/* barometric pressure */
	float a;		/* altitude */
};

#ifdef WPI

#include <wiringPi.h>
#include <maxdetect.h>
#include <ads1115.h>
#ifdef JS
#include <jsapi.h>
int js_wpi_init(JSContext *cx, JSObject *parent, void *priv);
#endif

/* returns 0 if successful, 1 = error */
int readbme (struct bme_data *);

#ifdef JS
int wpi_init(void *e);
#else /* !JS */
int wpi_init(void);
#endif

#else

/* taken from wiringPi.h */
#define INPUT                    0
#define OUTPUT                   1
#define PWM_OUTPUT               2
#define GPIO_CLOCK               3
#define SOFT_PWM_OUTPUT          4
#define SOFT_TONE_OUTPUT         5
#define PWM_TONE_OUTPUT          6
#define PM_OFF                   7   // to input / release line

#define LOW                      0
#define HIGH                     1

// Pull up/down/none

#define PUD_OFF                  0
#define PUD_DOWN                 1
#define PUD_UP                   2

// PWM

#define PWM_MODE_MS             0
#define PWM_MODE_BAL            1

// Interrupt levels

#define INT_EDGE_SETUP          0
#define INT_EDGE_FALLING        1
#define INT_EDGE_RISING         2
#define INT_EDGE_BOTH           3

#ifdef JS
static inline int wpi_init(void *e) { return 1; }
#else
static inline int wpi_init(void) { return 1; }
#endif
static inline int  wiringPiSetup       (void) { return 1; }
static inline int  ads1115Setup(int base,int addr) { return 1; }
static inline void pinMode             (int pin, int mode) { return; }
static inline void pullUpDnControl     (int pin, int pud) { return; }
static inline int  digitalRead         (int pin) { return 0; };
static inline void digitalWrite        (int pin, int value) { return; }
static inline void pwmWrite            (int pin, int value) { return; }
static inline int  analogRead          (int pin) { return 0; }
static inline void analogWrite         (int pin, int value) { return; }

static inline int readRHT03 (const int pin, int *temp, int *rh) { return 0; }
static inline int readbme (struct bme_data *dp) { return 1; }

#endif /* WPI */

#endif
