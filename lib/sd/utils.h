
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __UTILS_H
#define __UTILS_H

#include <stdlib.h>
#include "types.h"
#include "math.h"
#include "log.h"

/* Special data types */
#ifndef WIN32
typedef char byte;
#else
#define bcopy(s,d,l) memcpy(d,s,l)
typedef unsigned char byte;
#endif
typedef long long myquad_t;

#include "cfg.h"

/* util funcs */
void bindump(char *label,void *bptr,int len);
void _bindump(long offset,void *bptr,int len);
char *trim(char *);
char *strele(int num,char *delimiter,char *string);
char *stredit(char *string, char *list);
int is_ip(char *);
int get_timestamp(char *ts, int tslen, int local);
int become_daemon(void);
int conv_type(int dtype,void *dest,int dsize,int stype,void *src,int slen);
int find_config_file(char *,char *,int);

int solard_exec(char *, char *[], char *,int);
int solard_wait(int);
int solard_checkpid(int pid, int *status);
int solard_kill(int);
void solard_notify_list(char *,list);
void solard_notify(char *,char *,...);
void path2conf(char *dest, int size, char *src);
void conf2path(char *dest, int size, char *src);
void fixpath(char *,int);
int solard_get_path(char *prog, char *dest, int dest_len);
void tmpdir(char *, int);
int gethomedir(char *dest, int dest_len);
char *os_getenv(const char *name);
int os_setenv(const char *name, const char *value, int overwrite);
#ifdef WINDOWS
int fork(void);
#endif
int os_gethostbyname(char *dest, int destsize, char *name);
int os_exists(char *path, char *name);

//bool fequal(float a, float b);
//int float_equals(float a, float b);
//int double_equals(double a, double b);
#define double_equals(a,b) ((fabs(a) - fabs(b)) < 10e-7)
int double_isint(double z);
double pround(double val, int places);

#ifndef DEBUG_MEM
size_t mem_used(void);
#endif
size_t sys_mem_used(void);

#endif
