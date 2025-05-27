
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __LIST_H
#define __LIST_H

#include <time.h>

struct _llist;
typedef struct _llist * list;

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*list_item_free_t)(void *);
list list_create( void );
void list_set_item_free(list_item_free_t);
list list_create( void );
int list_destroy( list );
list list_dup( list );
void *list_add( list, void *, int );
int list_add_list( list, list );
int list_delete( list, void * );
int list_purge(list);
void *list_get_first( list );
void *list_get_next( list );
void *list_pop( list );
#define list_next list_get_next
int list_reset( list );
int list_count( list );
int list_is_next( list );
//typedef int (*list_compare)(list_item, list_item);
typedef int (*list_compare)(void *, void *);
int list_sort( list, list_compare, int);
time_t list_updated(list);
void list_save_next(list);
void list_restore_next(list);

#ifdef __cplusplus
}
#endif

#ifdef JS
#include "jsapi.h"
JSObject *NewListArray(JSContext *cx, JSObject *obj, list l);
#endif

#endif /* __LIST_H */
