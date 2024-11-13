
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define DEBUG_THIS 0
#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG DEBUG_THIS

#define dlevel 6
#include "debug.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef __WIN32
#include <windows.h>
#endif
#include <math.h>
#include "utils.h"

void _bindump(long offset,void *bptr,int len) {
	unsigned char *buf = bptr;
	char line[128];
	int end;
	register char *ptr;
	register int x,y;

//	dprintf(dlevel,"buf: %p, len: %d\n", buf, len);
#ifdef __WIN32__
	if (buf == (void *)0xBAADF00D) return;
#endif

	for(x=y=0; x < len; x += 16) {
		sprintf(line,"%04lX: ",offset);
		ptr = line + strlen(line);
		end=(x+16 >= len ? len : x+16);
		for(y=x; y < end; y++) {
			sprintf(ptr,"%02X ",buf[y]);
			ptr += 3;
		}
		for(y=end; y < x+17; y++) {
			sprintf(ptr,"   ");
			ptr += 3;
		}
		for(y=x; y < end; y++) {
			if (buf[y] > 31 && buf[y] < 127)
				*ptr++ = buf[y];
			else
				*ptr++ = '.';
		}
		for(y=end; y < x+16; y++) *ptr++ = ' ';
		*ptr = 0;
		log_info("%s\n", line);
		offset += 16;
	}
}

void bindump(char *label,void *bptr,int len) {
	log_info("%s(%d):\n",label,len);
	_bindump(0,bptr,len);
}

char *trim(char *string) {
	register char *p;
	char *end;

	/* If string is empty, just return it */
	if (*string == '\0') return string;

//	bindump("string",string,strlen(string)+1);

	/* Trim the front */
	p = string;
	end = string + strlen(string);
	while(p < end && isspace(*p)) p++;
//	printf("p: %p, string: %p, end: %p\n", p, string, end);
	if (p != string) {
		strcpy(string,p);
		end = string + strlen(string);
	}
	p = end;
//	printf("p: %c(%d)\n", *p,*p);
//	printf("p: %p, string: %p, end: %p\n", p, string, end);
	while(p > string && isspace(*(p-1))) p--;
//	printf("p: %c(%d)\n", *p,*p);
//	printf("p: %p, string: %p, end: %p\n", p, string, end);
	if (*p != 0) *p = 0;

//	printf("final string: >%s<\n", string);
	return string;
}

char *strele(int num,char *delimiter,char *string) {
	static char return_info[1024];
	register char *src,*dptr,*eptr,*cptr;
	register char *dest, qc;
	register int count;

	dprintf(dlevel,"Element: %d, delimiter: %s, string: %s\n",num,delimiter,string);

	eptr = string;
	dptr = delimiter;
	dest = return_info;
	count = 0;
	qc = 0;
	for(src = string; *src != '\0'; src++) {
		dprintf(dlevel,"src: %d, qc: %d\n", *src, qc);
		if (qc) {
			if (*src == qc) qc = 0;
			continue;
		} else {
			if (*src == '\"' || *src == '\'')  {
				qc = *src;
				cptr++;
			}
		}
		if (isspace(*src)) *src = 32;
#ifdef DEBUG_STRELE
		if (*src)
			dprintf(dlevel,"src: %c == ",*src);
		else
			dprintf(dlevel,"src: (null) == ");
		if (*dptr != 32)
			dprintf(dlevel,"dptr: %c\n",*dptr);
		else if (*dptr == 32)
			dprintf(dlevel,"dptr: (space)\n");
		else
			dprintf(dlevel,"dptr: (null)\n");
#endif
		if (*src == *dptr) {
			cptr = src+1;
			dptr++;
#ifdef DEBUG_STRELE
			if (*cptr != '\0')
				dprintf(dlevel,"cptr: %c == ",*cptr);
			else
				dprintf(dlevel,"cptr: (null) == ");
			if (*dptr != '\0')
				dprintf(dlevel,"dptr: %c\n",*dptr);
			else
				dprintf(dlevel,"dptr: (null)\n");
#endif
			while(*cptr == *dptr && *cptr != '\0' && *dptr != '\0') {
				cptr++;
				dptr++;
#ifdef DEBUG_STRELE
				if (*cptr != '\0')
					dprintf(dlevel,"cptr: %c == ",*cptr);
				else
					dprintf(dlevel,"cptr: (null) == ");
				if (*dptr != '\0')
					dprintf(dlevel,"dptr: %c\n",*dptr);
				else
					dprintf(dlevel,"dptr: (null)\n");
#endif
				if (*dptr == '\0' || *cptr == '\0') {
					dprintf(dlevel,"Breaking...\n");
					break;
				}
/*
				dptr++;
				if (*dptr == '\0') break;
				cptr++;
				if (*cptr == '\0') break;
*/
			}
#ifdef DEBUG_STRELE
			if (*cptr != '\0')
				dprintf(dlevel,"cptr: %c == ",*cptr);
			else
				dprintf(dlevel,"cptr: (null) == ");
			if (*dptr != '\0')
				dprintf(dlevel,"dptr: %c\n",*dptr);
			else
				dprintf(dlevel,"dptr: (null)\n");
#endif
			if (*dptr == '\0') {
				dprintf(dlevel,"Count: %d, num: %d\n",count,num);
				if (count == num) break;
				if (cptr > src+1) src = cptr-1;
				eptr = src+1;
				count++;
//				dprintf(dlevel,"eptr[0]: %c\n", eptr[0]);
				if (*eptr == '\"' || *eptr == '\'') eptr++;
				dprintf(dlevel,"eptr: %s, src: %s\n",eptr,src+1);
			}
			dptr = delimiter;
		}
	}
	dprintf(dlevel,"Count: %d, num: %d\n",count,num);
	if (count == num) {
		dprintf(dlevel,"eptr: %s\n",eptr);
		dprintf(dlevel,"src: %s\n",src);
		while(eptr < src) {
			if (*eptr == '\"' || *eptr == '\'') break;
			*dest++ = *eptr++;
		}
	}
	*dest = '\0';
	dprintf(dlevel,"Returning: %s\n",return_info);
	return(return_info);
}

int is_ip(char *string) {
	register char *ptr;
	int dots,digits;

	dprintf(7,"string: %s\n", string);

	digits = dots = 0;
	for(ptr=string; *ptr; ptr++) {
		if (*ptr == '.') {
			if (!digits) goto is_ip_error;
			if (++dots > 3) goto is_ip_error;
			digits = 0;
		} else if (isdigit((int)*ptr)) {
			if (++digits > 3) goto is_ip_error;
		} else {
			goto is_ip_error;
		}
	}
	dprintf(7,"dots: %d\n", dots);

	return (dots == 3 ? 1 : 0);
is_ip_error:
	return 0;
}


/* XXX cant log_write here */
int get_timestamp(char *ts, int tslen, int local) {
	struct tm *tptr;
	time_t t;

	if (!ts || !tslen) return 1;

	/* Fill the tm struct */
	t = time(NULL);
	tptr = 0;
	if (local) tptr = localtime(&t);
	else tptr = gmtime(&t);
	if (!tptr) {
		dprintf(2,"unable to get %s!\n",local ? "localtime" : "gmtime");
		return 1;
	}

	/* Set month to 1 if month is out of range */
	if (tptr->tm_mon < 0 || tptr->tm_mon > 11) tptr->tm_mon = 0;

	/* Fill string with yyyymmddhhmmss */
	snprintf(ts,tslen,"%04d-%02d-%02d %02d:%02d:%02d",
		1900+tptr->tm_year,tptr->tm_mon+1,tptr->tm_mday,
		tptr->tm_hour,tptr->tm_min,tptr->tm_sec);

	return 0;
}

char *os_getenv(const char *name) {
#ifdef __WIN32
	{
		static char value[4096];
		DWORD len = GetEnvironmentVariable(name, value, sizeof(value)-1);
		if (!len) return 0;
		if (len > sizeof(value)) return 0;
		return value;
	}
#else
	return getenv(name);
#endif
}

int os_setenv(const char *name, const char *value, int overwrite) {
#ifdef __WIN32
	return (SetEnvironmentVariable(name,value) ? 0 : 1);
#else
	return setenv(name,value,overwrite);
#endif
}

int os_exists(char *path, char *name) {

	if (name) {
		char temp[1024];
		snprintf(temp,sizeof(temp)-1,"%s/%s",path,name);
		return (access(temp,0) == 0);
	} else {
		return (access(path,0) == 0);
	}
}

const char *getBuild() { //Get current architecture, detectx nearly every architecture. Coded by Freak
        #if defined(__x86_64__) || defined(_M_X64)
        return "x86_64";
        #elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
        return "x86_32";
        #elif defined(__ARM_ARCH_2__)
        return "ARM2";
        #elif defined(__ARM_ARCH_3__) || defined(__ARM_ARCH_3M__)
        return "ARM3";
        #elif defined(__ARM_ARCH_4T__) || defined(__TARGET_ARM_4T)
        return "ARM4T";
        #elif defined(__ARM_ARCH_5_) || defined(__ARM_ARCH_5E_)
        return "ARM5"
        #elif defined(__ARM_ARCH_6T2_) || defined(__ARM_ARCH_6T2_)
        return "ARM6T2";
        #elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__)
        return "ARM6";
        #elif defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
        return "ARM7";
        #elif defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
        return "ARM7A";
        #elif defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
        return "ARM7R";
        #elif defined(__ARM_ARCH_7M__)
        return "ARM7M";
        #elif defined(__ARM_ARCH_7S__)
        return "ARM7S";
        #elif defined(__aarch64__) || defined(_M_ARM64)
        return "ARM64";
        #elif defined(mips) || defined(__mips__) || defined(__mips)
        return "MIPS";
        #elif defined(__sh__)
        return "SUPERH";
        #elif defined(__powerpc) || defined(__powerpc__) || defined(__powerpc64__) || defined(__POWERPC__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
        return "POWERPC";
        #elif defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)
        return "POWERPC64";
        #elif defined(__sparc__) || defined(__sparc)
        return "SPARC";
        #elif defined(__m68k__)
        return "M68K";
        #else
        return "UNKNOWN";
        #endif
    }

/* no debugmem after this */
#ifdef DEBUG_MEM
#undef malloc
#undef calloc
#undef realloc
#undef strdup
#undef free
#endif
#include <malloc.h>

#ifdef __WINDOWS__
#include <windows.h>
#include <psapi.h>
size_t sys_mem_used(void) {
	MEMORYSTATUSEX buf;
	size_t used;

	GlobalMemoryStatusEx(&buf);
	used = buf.ullTotalVirtual - buf.ullAvailVirtual;
	return used;
#if 0
	HANDLE hProcess;
	PROCESS_MEMORY_COUNTERS pmc;
	DWORD processID;
	size_t used;

	// Print the process identifier.
	processID = getpid();
	hProcess = OpenProcess(  PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID );
	if (NULL == hProcess) return 0;

	if ( GetProcessMemoryInfo( hProcess, &pmc, sizeof(pmc)) ) {
		used = pmc.WorkingSetSize;
#if 0
	printf( "\tPageFaultCount: 0x%08X\n", pmc.PageFaultCount );
	printf( "\tPeakWorkingSetSize: 0x%08X\n", 
		  pmc.PeakWorkingSetSize );
	printf( "\tWorkingSetSize: 0x%08X\n", pmc.WorkingSetSize );
	printf( "\tQuotaPeakPagedPoolUsage: 0x%08X\n", 
		  pmc.QuotaPeakPagedPoolUsage );
	printf( "\tQuotaPagedPoolUsage: 0x%08X\n", 
		  pmc.QuotaPagedPoolUsage );
	printf( "\tQuotaPeakNonPagedPoolUsage: 0x%08X\n", 
		  pmc.QuotaPeakNonPagedPoolUsage );
	printf( "\tQuotaNonPagedPoolUsage: 0x%08X\n", 
		  pmc.QuotaNonPagedPoolUsage );
	printf( "\tPagefileUsage: 0x%08X\n", pmc.PagefileUsage ); 
	printf( "\tPeakPagefileUsage: 0x%08X\n", 
		  pmc.PeakPagefileUsage );
#endif
	} else {
		used = 0;
	}

	CloseHandle( hProcess );
	return used;
#endif
}
#else 
#include <malloc.h>
size_t sys_mem_used(void) {
        struct mallinfo mi = mallinfo();
	return mi.uordblks;
}
#endif /* !__WINDOWS */

/* define mem_used when DEBUG_MEM not set */
#ifndef DEBUG_MEM
size_t mem_used(void) { return sys_mem_used(); }
#endif /* !DEBUG_MEM */

double pround(double val, int places) {
	char value[64], format[32];
	double d;

	if (places < 0) places *= (-1);
	if (places > 15) places = 15;
//	printf("val: %lf, places: %d\n", val, places);
	snprintf(format,sizeof(format),"%%.%df",places);
//	printf("format: %s\n", format);
	sprintf(value,format,val);
//	printf("value: %s\n", value);
	d = strtod(value,0);
//	printf("d: %lf\n", d);
	return d;
}

int double_equals(double a, double b) {
	double fa,fb,fd;

	fa = fabs(a);
	fb = fabs(b);
	fd = (fa > fb ? fa - fb : fb - fa);
//	dprintf(0,"fa: %f, fb: %f, fd: %f\n", fa, fb, fd);
	return fd < 10e-7 ? true : false;
}

int double_isint(double z) {
	return (fabs(round(z) - z) <= 10e-7 ? 1 : 0);
}
