
#ifndef __SDLOG_H
#define __SDLOG_H

/* Define the log options */
#define LOG_CREATE		0x0001	/* Create a new logfile */
#define LOG_TIME		0x0002	/* Prepend the time */
#define LOG_STDERR		0x0004	/* Log to stderr */
#define LOG_INFO		0x0008	/* Informational messages */
#define LOG_VERBOSE		0x0010	/* Full info messages */
#define LOG_WARNING		0x0020	/* Program warnings */
#define LOG_ERROR		0x0040	/* Program errors */
#define LOG_SYSERR		0x0080	/* System errors */
#define LOG_DEBUG		0x0100	/* Misc debug messages */
#define LOG_DEBUG2 		0x0200	/* func entry/exit */
#define LOG_DEBUG3		0x0400	/* inner loops! */
#define LOG_DEBUG4		0x0800	/* The dolly-lamma */
#define LOG_WX			0x8000	/* Log to wxMessage */
#define LOG_ALL			0x7FFF	/* Whoa, Nellie */
#define LOG_DEFAULT		(LOG_INFO|LOG_WARNING|LOG_ERROR|LOG_SYSERR)

#define LOG_SYSERROR LOG_SYSERR

/* Log funcs */
int log_open(char *,char *,int);
int log_read(char *,int);
int log_write(int,char *,...);
int log_info(char *,...);
int log_warning(char *,...);
int log_error(char *,...);
int log_syserr(char *,...);
int log_syserror(char *,...);
int log_debug(char *,...);
void log_close(void);
void log_writeopts(void);
char *log_nextname(void);

#define lprintf(mask, format, args...) log_write(mask,format,## args)

#ifdef JS
#include "jsapi.h"
int js_log_init(JSContext *cx, JSObject *parent, void *priv);
#endif

#endif /* __SDLOG_H */
