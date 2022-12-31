#ifdef DEBUG_MEM
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

/* To disable the tracking but keep the functions, set this to 0 */
#define MEM_ENABLE 1

/* Track file and line #s */
#define MEM_FILE 1

/* output debug info? */
#define DEBUG_OUTPUT 1
int debugmem_dlevel = 9;

#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG DEBUG_OUTPUT
#include "debug.h"

/* must remove for circ calls */
#undef malloc
#undef calloc
#undef realloc
#undef strdup
#undef free

//#undef dprintf
//#define dprintf(l,fmt,...) printf(fmt,##__VA_ARGS__)
#include <stdlib.h>
#include <string.h>

struct __attribute__((__packed__, aligned(4))) _meminfo {
	uint32_t sig;
	uint32_t size;
#if MEM_FILE
	uint32_t line;
	char file[12];
#endif
};
typedef struct _meminfo meminfo_t;

#define SIG 0xD5AA

static size_t used = 0, peak = 0;

size_t mem_used(void) { return(used); }
size_t mem_peak(void) { return(peak); }

void *mem_malloc(size_t size, char *file, int line) {
	void *mem;

//	printf("meminfo_t size: %ld\n", sizeof(meminfo_t));
	mem = malloc(size+sizeof(meminfo_t));
#if MEM_ENABLE
	if (mem) {
		((meminfo_t *)mem)->sig = SIG;
		((meminfo_t *)mem)->size = size;
#if MEM_FILE
		((meminfo_t *)mem)->line = line;
		strncpy(((meminfo_t *)mem)->file,file,sizeof(((meminfo_t *)mem)->file));
#endif
		used += size;
		if (used > peak) peak = used;
		mem += sizeof(meminfo_t);
	}
	dprintf(debugmem_dlevel,"(%s:%d) mem: %p, size: %ld, used: %ld\n", file, line, mem, size, used);
#else
	dprintf(debugmem_dlevel,"mem: %p, size: %ld\n",mem, size);
#endif
	return(mem);
}

void *mem_calloc(size_t nmemb, size_t msize, char *file, int line) {
	void *mem;
	size_t size;

	size = nmemb * msize;
	mem = calloc(size+sizeof(meminfo_t),1);
#if MEM_ENABLE
	if (mem) {
		((meminfo_t *)mem)->sig = SIG;
		((meminfo_t *)mem)->size = size;
#if MEM_FILE
		((meminfo_t *)mem)->line = line;
		strncpy(((meminfo_t *)mem)->file,file,sizeof(((meminfo_t *)mem)->file));
#endif
		used += size;
		if (used > peak) peak = used;
		mem += sizeof(meminfo_t);
	}
	dprintf(debugmem_dlevel,"(%s:%d) mem: %p, size: %ld, used: %ld\n", file, line, mem, size, used);
#else
	dprintf(debugmem_dlevel,"mem: %p, size: %ld\n",mem, size);
#endif
	return(mem);
}

void *mem_realloc(void *mem, size_t size, char *file, int line) {
	void *newmem;

#if MEM_ENABLE
	if (mem) {
		if (((meminfo_t *)(mem-sizeof(meminfo_t)))->sig != SIG) {
			log_error("mem_free: not our mem, aborting\n");
			return 0;
		}
		mem -= sizeof(meminfo_t);
	}
#endif
	newmem = realloc(mem,size+sizeof(meminfo_t));
#if MEM_ENABLE
	if (newmem) {
		((meminfo_t *)newmem)->sig = SIG;
		((meminfo_t *)newmem)->size = size;
#if MEM_FILE
		((meminfo_t *)newmem)->line = line;
		strncpy(((meminfo_t *)newmem)->file,file,sizeof(((meminfo_t *)newmem)->file)-1);
#endif
		used += size;
		if (used > peak) peak = used;
		newmem += sizeof(meminfo_t);
	}
	dprintf(debugmem_dlevel,"(%s:%d) newmem: %p, size: %ld, used: %ld\n", file, line, newmem, size, used);
#else
	dprintf(debugmem_dlevel,"newmem: %p, size: %ld\n",newmem,size);
#endif
	return newmem;
}

char *mem_strdup(const char *str, char *file, int line) {
	void *mem;
	size_t size;

	if (!str) return 0;
	size = strlen(str) + 1;
	mem = malloc(sizeof(meminfo_t)+size);
#if MEM_ENABLE
	if (mem) {
		((meminfo_t *)mem)->sig = SIG;
		((meminfo_t *)mem)->size = size;
#if MEM_FILE
		((meminfo_t *)mem)->line = line;
		strncpy(((meminfo_t *)mem)->file,file,sizeof(((meminfo_t *)mem)->file));
#endif
		used += size;
		if (used > peak) peak = used;
		mem += sizeof(meminfo_t);
		strcpy((char *)mem,str);
	}
	dprintf(debugmem_dlevel,"(%s:%d) mem: %p, size: %ld, used: %ld\n", file, line, mem, size, used);
#else
	dprintf(debugmem_dlevel,"mem: %p, size: %ld\n",mem, size);
#endif
	return(mem);
}

void mem_free(void *mem, char *file, int line) {
#if MEM_ENABLE
	meminfo_t *info;
#endif

	if (!mem) return;
#if MEM_ENABLE
	info = mem - sizeof(meminfo_t);
	if (info->sig != SIG) {
		log_error("mem_free(%s:%d): not our mem, aborting\n",file,line);
		free(mem);
		return;
	}
	used -= info->size;
#if MEM_FILE
	dprintf(debugmem_dlevel,"(%s:%d) mem: %p (%s:%d), size: %d, used: %ld\n", file, line, mem, info->file, info->line, info->size, used);
#else
	dprintf(debugmem_dlevel,"(%s:%d) mem: %p, size: %d, used: %ld\n", file, line, mem, info->size, used);
#endif
	free(info);
#else
	dprintf(debugmem_dlevel,"mem: %p\n", mem);
	free(mem);
#endif
}

#endif /* DEBUG_MEM */
