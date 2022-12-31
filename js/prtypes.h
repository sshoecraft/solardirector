
#ifndef __PRTYPES_H
#define __PRTYPES_H

#define PRLock pthread_mutex_t
#define PRCondVar pthread_cond_t
typedef int PRIntervalTime;
#define PR_INTERVAL_NO_TIMEOUT 0xffffffffUL
extern int _PR_AtomicIncrement_num;
typedef int PRInt32;
typedef unsigned int PRUintn;
typedef enum { PR_FAILURE = -1, PR_SUCCESS = 0 } PRStatus;

#endif
