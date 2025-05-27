
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

void clear_screen(void) {
#ifdef WINDOWS
	system("cls");
#else
	system("clear");
#endif
}

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
	register char *src,*dest;

	if (*string) return 0;
	if (!*string) return string;
	src = dest = string;
	while(isspace((int)*src) && *src != '\t' && *src) src++;
	while(*src != '\0') *dest++ = *src++;
	*dest-- = '\0';
	while((dest >= string) && isspace((int)*dest)) dest--;
	*(dest+1) = '\0';
	return string;
#if 0
	register char *p;
	char *end;

	/* If string is empty, just return it */
//	if (*string == '\0') return string;
	if (!*string) return string;

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
#endif
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

float heat_index(float T, float RH) {
#if 0
from: https://www.wpc.ncep.noaa.gov/html/heatindex_equation.shtml
The computation of the heat index is a refinement of a result obtained by multiple regression analysis carried out by Lans P. Rothfusz and described in a 1990 National Weather Service (NWS) Technical Attachment (SR 90-23).  The regression equation of Rothfusz is
HI = -42.379 + 2.04901523*T + 10.14333127*RH - .22475541*T*RH - .00683783*T*T - .05481717*RH*RH + .00122874*T*T*RH + .00085282*T*RH*RH - .00000199*T*T*RH*RH
where T is temperature in degrees F and RH is relative humidity in percent.  HI is the heat index expressed as an apparent temperature in degrees F.  If the RH is less than 13% and the temperature is between 80 and 112 degrees F, then the following adjustment is subtracted from HI:
ADJUSTMENT = [(13-RH)/4]*SQRT{[17-ABS(T-95.)]/17}
where ABS and SQRT are the absolute value and square root functions, respectively.  On the other hand, if the RH is greater than 85% and the temperature is between 80 and 87 degrees F, then the following adjustment is added to HI:
ADJUSTMENT = [(RH-85)/10] * [(87-T)/5]
The Rothfusz regression is not appropriate when conditions of temperature and humidity warrant a heat index value below about 80 degrees F. In those cases, a simpler formula is applied to calculate values consistent with Steadmans results:
HI = 0.5 * {T + 61.0 + [(T-68.0)*1.2] + (RH*0.094)}
In practice, the simple formula is computed first and the result averaged with the temperature. If this heat index value is 80 degrees F or higher, the full regression equation along with any adjustment as described above is applied.
The Rothfusz regression is not valid for extreme temperature and relative humidity conditions beyond the range of data considered by Steadman.
#endif
	float HI;

//	printf("T: %f, RH: %f\n", T, RH);
	if (T < 80.0) {
		float adj;
		adj = 0.5 * (T + 61.0 + ((T-68.0)*1.2) + (RH*0.094));
		HI = (T + adj)/2;
	} else {
		HI = -42.379 + 2.04901523*T + 10.14333127*RH - .22475541*T*RH - .00683783*T*T - .05481717*RH*RH + .00122874*T*T*RH + .00085282*T*RH*RH - .00000199*T*T*RH*RH;
		if (RH <= 13.0 && (T >= 80.0 && T <= 112.0)) {
			float adj;
			adj = ((13.0-RH)/4) * sqrt((17.0-fabs(T-95.0) )/17.0);
//			printf("adj: %f\n", adj);
			HI -= adj;
		} else if (RH >= 85.0 && (T >= 80.0 && T <= 87.0)) {
			float adj;
			adj = ((RH-85.0)/10) * ((87.0-T)/5);
//			printf("adj: %f\n", adj);
			HI += adj;
		}
			
	}
	return HI;
}

float wet_bulb(float T, float RH) {
	float Tw;

	Tw = (0.151977 * sqrt(RH + 8.313659)) + atan(T + RH) - atan(RH - 1.676331) + (0.00391838 * pow(RH, 1.5) * atan(0.023101 * RH)) - 4.686035;
	return Tw;
}

float wind_chill(float T, float WS) {
	return 13.12 + 0.6215 * T - 11.37 * pow(WS, 0.16) + 0.3965 * T * pow(WS, 0.16);
}

#ifdef JS
#include <jsapi.h>

static JSBool js_clear_screen(JSContext *cx, uintN argc, jsval *vp) {
	clear_screen();
	return JS_TRUE;
}

static JSBool js_heat_index(JSContext *cx, uintN argc, jsval *vp) {
	double temp,rh,hi;

	if (argc != 2) {
		JS_ReportError(cx,"heat_index requires 2 arguments (temp: number, rh: number\n");
		return JS_FALSE;
	}

	/* Get the args*/
	if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "d d", &temp, &rh)) return JS_FALSE;
	dprintf(dlevel,"temp: %f, rh: %f\n", temp, rh);

	hi = heat_index(temp,rh);
	dprintf(dlevel,"hi: %f\n", hi);
	JS_NewDoubleValue(cx,hi,vp);
	return JS_TRUE;
}

static JSBool js_wet_bulb(JSContext *cx, uintN argc, jsval *vp) {
	double temp,rh,wb;

	if (argc != 2) {
		JS_ReportError(cx,"wet_bulb requires 2 arguments (temp: number, rh: number\n");
		return JS_FALSE;
	}

	/* Get the args*/
	if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "d d", &temp, &rh)) return JS_FALSE;
	dprintf(dlevel,"temp: %f, rh: %f\n", temp, rh);

	wb = heat_index(temp,rh);
	dprintf(dlevel,"wb: %f\n", wb);
	JS_NewDoubleValue(cx,wb,vp);
	return JS_TRUE;
}

static JSBool js_wind_chill(JSContext *cx, uintN argc, jsval *vp) {
	double temp,ws,wc;

	if (argc != 2) {
		JS_ReportError(cx,"heat_index requires 2 arguments (temp: number, wind speed: number\n");
		return JS_FALSE;
	}

	/* Get the args*/
	if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx,vp), "d d", &temp, &ws)) return JS_FALSE;
	dprintf(dlevel,"temp: %f, ws: %f\n", temp, ws);

	wc = wind_chill(temp,ws);
	dprintf(dlevel,"wc: %f\n", wc);
	JS_NewDoubleValue(cx,wc,vp);
	return JS_TRUE;
}

int js_utils_init(JSContext *cx, JSObject *parent, void *priv) {
	JSFunctionSpec funcs[] = {
		JS_FN("clear_screen",js_clear_screen,0,0,0),
		JS_FN("heat_index",js_heat_index,2,2,0),
		JS_FN("wet_bulb",js_wet_bulb,2,2,0),
		JS_FN("wind_chill",js_wind_chill,2,2,0),
		{ 0 }
	};
	JSConstantSpec constants[] = {
		{ 0 }
	};

	dprintf(dlevel,"Defining utils funcs...\n");
	if(!JS_DefineFunctions(cx, parent, funcs)) {
		return 1;
	}

	dprintf(dlevel,"Defining utils constants...\n");
	if(!JS_DefineConstants(cx, parent, constants)) {
		return 1;
	}

	return 0;
}
#endif /* JS */
