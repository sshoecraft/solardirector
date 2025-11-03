
#ifndef __SIUTIL_H
#define __SIUTIL_H

#include "si.h"

extern int outfmt;
extern FILE *outfp;
extern char sepch;
extern char *sepstr;
extern json_object_t *root_object;

void dint(char *label, char *format, int val);
void ddouble(char *label, char *format, double val);
void dstr(char *label, char *format, char *val);
void display_data(si_session_t *,int);
int monitor(si_session_t *,int);

#endif
