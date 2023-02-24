
#ifndef __DEBUGMEM_H
#define __DEBUGMEM_H

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG_MEM
extern int debugmem_dlevel;
void *mem_malloc(size_t size, char *file, int line);
void *mem_calloc(size_t nmemb, size_t size, char *file, int line);
void *mem_realloc(void *mem, size_t size, char *file, int line);
char *mem_strdup(const char *str, char *file, int line);
void mem_free(void *mem, char *file, int line);
size_t mem_used(void);
size_t mem_peak(void);
#ifdef malloc
#undef malloc
#endif
#define malloc(s) mem_malloc((s),__FILE__,__LINE__)
#ifdef calloc
#undef calloc
#endif
#define calloc(n,s) mem_calloc((n),(s),__FILE__,__LINE__)
#ifdef realloc
#undef realloc
#endif
#define realloc(n,s) mem_realloc((n),(s),__FILE__,__LINE__)
#ifdef strdup
#undef strdup
#endif
#define strdup(s) mem_strdup((s),__FILE__,__LINE__)
#ifdef free
#undef free
#endif
#define free(s) mem_free((s),__FILE__,__LINE__)
#endif

#endif
