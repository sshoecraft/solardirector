
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define DEBUG_THIS 0

#ifdef DEBUG
#undef DEBUG
#define DEBUG DEBUG_THIS
#endif

#define dlevel 8
#include "debug.h"

//#undef dprintf
//#define dprintf(l,f,...) printf(f,##__VA_ARGS__)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#if THREAD_SAFE
#include <pthread.h>
#endif
#include "list.h"

/* Define the list item */
struct _list_item {
	void *item;
	int size;
	struct _list_item *prev;
	struct _list_item *next;
};
typedef struct _list_item * list_item;
#define LIST_ITEM_SIZE sizeof(struct _list_item)

/* Define the list */
struct _llist {
	int type;			/* Data type in list */
	list_item_free_t item_free;
	list_item first;		/* First item in list */
	list_item last;			/* Last item in list */
	list_item next;			/* Next item in list */
	list_item saved_next;		/* Saved next item */
#if THREAD_SAFE
	pthread_mutex_t mutex;
#endif
	time_t last_update;
};
typedef struct _llist * list;
#define LIST_SIZE sizeof(struct _llist)

time_t list_updated(list lp) {
	time_t upd;

#if THREAD_SAFE
	pthread_mutex_lock(&lp->mutex);
#endif
	upd = lp->last_update;
#if THREAD_SAFE
	pthread_mutex_unlock(&lp->mutex);
#endif
	return upd;
}

#define _dump(i) dprintf(dlevel,"%s: item: %p, prev: %p, next: %p\n", #i, i, i->prev, i->next);
void list_checkitem(list_item ip) {
	dprintf(dlevel,"ip: %p\n", ip);
	_dump(ip);
	dprintf(dlevel,"ip->prev: %p\n", ip->prev);
	assert(ip->prev != ip);
	dprintf(dlevel,"ip->next: %p\n", ip->next);
	assert(ip->next != ip);
	if (ip->prev) {
		_dump(ip->prev);
		assert(ip->prev->prev != ip);
		assert(ip->prev->next == ip);
	}
	if (ip->next) {
		_dump(ip->next);
		assert(ip->next->prev == ip);
		assert(ip->next->next != ip);
	}
}

static list_item _newitem(void *item, int size) {
	list_item new_item;

	dprintf(dlevel,"_newitem: item: %p, size: %d\n", item, size);
	new_item = (list_item) calloc(1,LIST_ITEM_SIZE);
	if (!new_item) return 0;

	if (size) {
		new_item->item = (void *) malloc((size_t)size);
		if (!new_item->item) {
			free(new_item);
			dprintf(dlevel,"_newitem: returning: 0");
			return 0;
		}
		memcpy(new_item->item,item,size);
		new_item->size = size;
	} else {
		new_item->item = item;
	}

	dprintf(dlevel,"_newitem: returning: %p\n", new_item);
	return new_item;
}

void *list_add(list lp, void *item, int size) {
	list_item ip, new_item;

	dprintf(dlevel,"list_add: lp: %p, item: %p, size: %d\n", lp, item, size);
	if (!lp) return 0;

	/* Create a new item */
	new_item = _newitem(item,size);
	if (!new_item) return 0;

#if THREAD_SAFE
	pthread_mutex_lock(&lp->mutex);
#endif
	/* Add it to the list */
	dprintf(dlevel,"lp->first: %p\n", lp->first);
	if (!lp->first)
		lp->first = lp->last = lp->next = new_item;
	else {
		dprintf(dlevel,"lp->last: %p\n", lp->last);
		ip = lp->last;                  /* Get last item */
		ip->next = new_item;            /* Add this one */
		new_item->prev = ip;		/* Point to it */
		lp->last = new_item;            /* Make this last */
	}
	time(&lp->last_update);
#if THREAD_SAFE
	pthread_mutex_unlock(&lp->mutex);
#endif
	return new_item->item;
}

int list_add_list(list lp,list lp2) {
	list_item ip;

	if (!lp) return 1;

#if THREAD_SAFE
	pthread_mutex_lock(&lp->mutex);
	pthread_mutex_lock(&lp2->mutex);
#endif

	/* Get each item from the old list and copy it to the new one */
	ip = lp2->first;
	while(ip) {
		list_add(lp,ip->item,ip->size);
		ip = ip->next;
	}
	time(&lp->last_update);

#if THREAD_SAFE
	pthread_mutex_unlock(&lp->mutex);
	pthread_mutex_unlock(&lp2->mutex);
#endif

	return 0;
}

int list_count(list lp) {
	list_item ip;
	register int count;

	dprintf(dlevel,"lp: %p\n", lp);
	if (!lp) return -1;

#if THREAD_SAFE
	dprintf(dlevel,"locking...\n");
	pthread_mutex_lock(&lp->mutex);
#endif

	dprintf(dlevel,"counting...\n");
	count = 0;
	for(ip = lp->first; ip; ip = ip->next) count++;

#if THREAD_SAFE
	dprintf(dlevel,"unlocking...\n");
	pthread_mutex_unlock(&lp->mutex);
#endif

	dprintf(dlevel,"returning count: %d\n", count);
	return count;
}

list list_create(void) {
	list lp;

	lp = (list) malloc(LIST_SIZE);
	dprintf(dlevel,"list_create: lp: %p\n", lp);
	if (!lp) return 0;
	lp->item_free = free;

	lp->first = lp->last = lp->next = (list_item) 0;

#if THREAD_SAFE
	dprintf(dlevel,"initializing lock...\n");
	pthread_mutex_init(&lp->mutex,NULL);
#endif

	dprintf(dlevel,"list_create: returning: %p\n", lp);
	time(&lp->last_update);
	return lp;
}

static void _delete_item(list lp,list_item ip) {
	list_item prev = ip->prev;        /* Get the pointers */
	list_item next = ip->next;

	/* Fixup links in other first */
	if (prev) prev->next = next;
	if (next) next->prev = prev;

	/* Was this the 1st item? */
	if (ip == lp->first) lp->first = next;

	/* Was this the last item? */
	if (ip == lp->last) lp->last = prev;

	/* Was this the next item? */
	if (ip == lp->next) lp->next = next;

	if (ip->size) lp->item_free(ip->item); /* Free the item */
	free(ip);		/* Free the ptr */
	time(&lp->last_update);
}

int list_delete(list lp,void *item) {
	list_item ip;
	int found;

	if (!lp) return -1;

#if THREAD_SAFE
	pthread_mutex_lock(&lp->mutex);
#endif

	dprintf(dlevel,"item: %p, first: %p, last: %p, next: %p\n", item, lp->first, lp->last, lp->next);

	found = 0;
	ip = lp->first;
	while(ip) {
		list_checkitem(ip);
		dprintf(dlevel,"item: %p, ip->item: %p\n", item, ip->item);
		if (item == ip->item) {
			dprintf(dlevel,"found\n");
			_delete_item(lp,ip);
			found = 1;
			break;
		}
		ip = ip->next;                  /* Next item, please. */
	}
#if THREAD_SAFE
	pthread_mutex_unlock(&lp->mutex);
#endif
	dprintf(dlevel,"first: %p, last: %p, next: %p\n", lp->first, lp->last, lp->next);
	dprintf(dlevel,"found: %d\n", found);
	return (found ? 0 : 1);
}

int list_purge(list lp) {
	list_item ip,next;

	if (!lp) return -1;

#if THREAD_SAFE
	pthread_mutex_lock(&lp->mutex);
#endif

	ip = lp->first;                         /* Start at beginning */
	while(ip) {
		if (ip->size) lp->item_free(ip->item);	/* Free the item data */
		next = ip->next;                /* Get next pointer */
		free(ip);			/* Free current item */
		ip = next;                      /* Set current item to next */
	}

	/* fixup first/last/next */
	lp->first = lp->last = lp->next = (list_item) 0;

#if THREAD_SAFE
	pthread_mutex_unlock(&lp->mutex);
#endif
	return 0;
}

int list_destroy(list lp) {
	list_item ip,next;

	dprintf(dlevel,"lp: %p\n", lp);
	if (!lp) return -1;

#if THREAD_SAFE
	pthread_mutex_lock(&lp->mutex);
#endif

	dprintf(dlevel,"lp->first: %p\n", lp->first);
	ip = lp->first;                         /* Start at beginning */
	while(ip) {
		if (ip->size) lp->item_free(ip->item);	/* Free the item data */
		next = ip->next;                /* Get next pointer */
		free(ip);			/* Free current item */
		ip = next;                      /* Set current item to next */
	}

#if THREAD_SAFE
	pthread_mutex_unlock(&lp->mutex);
#endif
	free(lp);				/* Free list */
	return 0;
}

list list_dup(list lp) {
	list new_list;
	list_item ip;

	if (!lp) return 0;

	/* Create a new list */
	new_list = list_create();
	if (!new_list) return 0;

#if THREAD_SAFE
	pthread_mutex_lock(&lp->mutex);
#endif

	/* Get each item from the old list and copy it to the new one */
	ip = lp->first;
	while(ip) {
		list_add(new_list,ip->item,ip->size);
		ip = ip->next;
	}

#if THREAD_SAFE
	pthread_mutex_unlock(&lp->mutex);
#endif

	/* Return the new list */
	time(&new_list->last_update);
	return new_list;
}

void *list_pop(list lp) {
	list_item ip;
	void *item;

	dprintf(dlevel,"lp: %p\n",lp);
	if (!lp) return 0;

#if THREAD_SAFE
	dprintf(dlevel,"locking...\n");
	pthread_mutex_lock(&lp->mutex);
#endif

	item = 0;
	dprintf(dlevel,"lp->first: %p\n",lp->first);
	if (lp->first) {
		ip = lp->first;
		item = ip->item;
		_delete_item(lp,ip);
	}

#if THREAD_SAFE
	dprintf(dlevel,"unlocking...\n");
	pthread_mutex_unlock(&lp->mutex);
#endif
	dprintf(dlevel,"returning: %p\n", item);
	return item;
}

void *list_get_first(list lp) {
	list_item ip;
	void *item;

	dprintf(dlevel,"lp: %p\n",lp);
	if (!lp) return 0;

#if THREAD_SAFE
	dprintf(dlevel,"locking...\n");
	pthread_mutex_lock(&lp->mutex);
#endif

	item = 0;
	dprintf(dlevel,"lp->first: %p\n",lp->first);
	if (lp->first) {
		ip = lp->first;
		item = ip->item;
	}

#if THREAD_SAFE
	dprintf(dlevel,"unlocking...\n");
	pthread_mutex_unlock(&lp->mutex);
#endif
	dprintf(dlevel,"returning: %p\n", item);
	return item;
}

void *list_get_next(list lp) {
	list_item ip;
	void *item;

	dprintf(dlevel,"lp: %p\n",lp);
	if (!lp) return 0;

#if THREAD_SAFE
	dprintf(dlevel,"locking...\n");
	pthread_mutex_lock(&lp->mutex);
#endif

	item = 0;
	dprintf(dlevel,"lp->next: %p\n",lp->next);
	if (lp->next) {
		ip = lp->next;
		dprintf(dlevel,"ip: %p\n", ip);
		item = ip->item;
		lp->next = ip->next;
	}

#if THREAD_SAFE
	dprintf(dlevel,"unlocking...\n");
	pthread_mutex_unlock(&lp->mutex);
#endif
	dprintf(dlevel,"returning: %p\n", item);
	return item;
}

int list_is_next(list lp) {
	if (!lp) return -1;
	return(lp->next ? 1 : 0);
}

int list_reset(list lp) {

	if (!lp) return -1;

	lp->next = lp->first;
	return 0;
}

//static int _compare(list_item item1, list_item item2) {
static int _compare(void *i1, void *i2) {
	char *item1 = i1;
	char *item2 = i2;
	register int val;

//	val = strcmp(item1->item,item2->item);
	val = strcmp(item1,item2);
	if (val < 0)
		val =  -1;
	else if (val > 0)
		val = 1;
	return val;
}


int list_sort(list lp, list_compare compare, int order) {
	int comp,swap,save_size;
	list_item save_item;
	register list_item ip1,ip2;

	if (!lp) return -1;

#if THREAD_SAFE
	pthread_mutex_lock(&lp->mutex);
#endif

	/* If compare is null, use internal compare (strcmp) */
	if (!compare) compare = _compare;

	/* Set comp negative or positive, depending upon order */
	comp = (order == 0 ? 1 : -1);

	/* Sort the list */
	for(ip1 = lp->first; ip1;) {
		swap = 0;
		for(ip2 = ip1->next; ip2; ip2 = ip2->next) {
			if (compare(ip1->item,ip2->item) == comp) {
				swap = 1;
				break;
			}
		}
		dprintf(dlevel,"swap: %d\n", swap);
		if (swap) {
			/* LOL dont swap the list_items ... just swap the items */
			save_item = ip1->item;
			save_size = ip1->size;
			ip1->item = ip2->item;
			ip1->size = ip2->size;
			ip2->item = save_item;
			ip2->size = save_size;
		} else
			ip1 = ip1->next;
	}
	time(&lp->last_update);
#if THREAD_SAFE
	pthread_mutex_unlock(&lp->mutex);
#endif
	return 0;
}

void list_save_next(list lp) {
	if (!lp) return;
#if THREAD_SAFE
	pthread_mutex_lock(&lp->mutex);
#endif
	lp->saved_next = lp->next;
#if THREAD_SAFE
	pthread_mutex_unlock(&lp->mutex);
#endif
}

void list_restore_next(list lp) {
	if (!lp) return;
#if THREAD_SAFE
	pthread_mutex_lock(&lp->mutex);
#endif
	lp->next = lp->saved_next;
#if THREAD_SAFE
	pthread_mutex_unlock(&lp->mutex);
#endif
}

#if 0
#ifdef JS
static JSClass js_list_array_class = {
	"Array",		/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	js_list_array_getprop,	/* getProperty */
	js_list_array_setprop,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *NewListArray(JSContext *cx, JSObject *obj, list l) {
}
#endif
#endif
