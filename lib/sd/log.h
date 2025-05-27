
#ifndef __SDLOG_H
#define __SDLOG_H

/* Define the log types */
#define LOG_INFO		0x0001	/* Informational messages */
#define LOG_VERBOSE		0x0002	/* Full info messages */
#define LOG_WARNING		0x0004	/* Program warnings */
#define LOG_ERROR		0x0008	/* Program errors */
#define LOG_SYSERR		0x0010	/* System errors */
#define LOG_DEBUG		0x0020	/* Misc debug messages */
#define LOG_TYPE_MASK		0x0FFF

/* modifiers */
#define LOG_CREATE		0x1000	/* Create a new logfile */
#define LOG_TIME		0x2000	/* Prepend the time */
#define LOG_STDERR		0x4000	/* Log to stderr */
#define LOG_NEWLINE		0x8000	/* Add a newline */
#define LOG_MODIFIER_MASK	0xF000

#if 0
#define LOG_DEBUG2 		0x0200	/* func entry/exit */
#define LOG_DEBUG3		0x0400	/* inner loops! */
#define LOG_DEBUG4		0x0800	/* The dolly-lamma */
#define LOG_WX			0x8000	/* Log to wxMessage */
#define LOG_ALL			0x7FFF	/* Whoa, Nellie! */
#endif

#define LOG_DEFAULT		(LOG_INFO|LOG_WARNING|LOG_ERROR|LOG_SYSERR)

#define LOG_SYSERROR LOG_SYSERR

/* Log funcs */
#define _LOGFUNCDEC(n) int n(char *format,...) { va_list ap; va_start(ap,format); return _log_write(t,format,ap); }
int log_open(char *,char *,int);
int log_read(char *,int);
void log_close(void);

int log_write(int,char *,...);
int log_info(char *,...);
int log_verbose(char *,...);
int log_warning(char *,...);
int log_error(char *,...);
int log_syserr(char *,...);
int log_syserror(char *,...);
int log_debug(char *,...);

#define lprintf(mask, format, args...) log_write(mask,format,## args)

#ifdef JS
#include "jsapi.h"
int js_log_init(JSContext *cx, JSObject *parent, void *priv);
#endif

#endif /* __SDLOG_H */
