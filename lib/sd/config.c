
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 6
#include "debug.h"

#include "common.h"
#include "config.h"
#include <strings.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define _set_flag(f,n)     ((f) |= (CONFIG_FLAG_##n))
#define _clear_flag(f,n)   ((f) &= (~CONFIG_FLAG_##n))
#define _check_flag(f,n)   (((f) & CONFIG_FLAG_##n) != 0)

static list configs = 0;
#ifdef JS
static list js_ctxs = 0;
#endif

char *config_get_errmsg(config_t *cp) { return cp->errmsg; }

void config_dump_property(config_property_t *p, int level) {
	char value[1024], flagstr[1024], *str;

	dprintf(level,"p: %p\n", p);
	if (!p) return;
	if (level > debug) return;
	printf("%-20.20s: %s\n", "name", p->name);
	printf("%-20.20s: %s\n", "type", typestr(p->type));
	printf("%-20.20s: %p\n", "dest", p->dest);
	if (p->dest) conv_type(DATA_TYPE_STRING,value,sizeof(value),p->type,p->dest,p->len);
	else value[0] = 0;
	printf("%-20.20s: %s\n", "value", value);
	printf("%-20.20s: %d\n", "dsize", p->dsize);
	printf("%-20.20s: %s\n", "def", (char *)p->def);
	*flagstr = 0;
	str = flagstr;
	if (p->flags & CONFIG_FLAG_READONLY) str += sprintf(str,"%sREADONLY",*flagstr ? "," : "");
	if (p->flags & CONFIG_FLAG_NOSAVE) str += sprintf(str,"%sNOSAVE",*flagstr ? "," : "");
	if (p->flags & CONFIG_FLAG_NOID) str += sprintf(str,"%sNOID",*flagstr ? "," : "");
	if (p->flags & CONFIG_FLAG_FILE) str += sprintf(str,"%sFILE",*flagstr ? "," : "");
	if (p->flags & CONFIG_FLAG_FILEONLY) str += sprintf(str,"%sFILEONLY",*flagstr ? "," : "");
	if (p->flags & CONFIG_FLAG_ALLOC) str += sprintf(str,"%sALLOC",*flagstr ? "," : "");
	if (p->flags & CONFIG_FLAG_NOPUB) str += sprintf(str,"%sNOPUB",*flagstr ? "," : "");
	if (p->flags & CONFIG_FLAG_NODEF) str += sprintf(str,"%sNODEF",*flagstr ? "," : "");
	if (p->flags & CONFIG_FLAG_ALLOCDEST) str += sprintf(str,"%sALLOCDEST",*flagstr ? "," : "");
	if (p->flags & CONFIG_FLAG_NOINFO) str += sprintf(str,"%sNOINFO",*flagstr ? "," : "");
	if (p->flags & CONFIG_FLAG_PUB) str += sprintf(str,"%sPUB",*flagstr ? "," : "");
	if (p->flags & CONFIG_FLAG_NOWARN) str += sprintf(str,"%sNOWARN",*flagstr ? "," : "");
	if (p->flags & CONFIG_FLAG_NOTRIG) str += sprintf(str,"%sNOTRIG",*flagstr ? "," : "");
	if (p->flags & CONFIG_FLAG_VALUE) str += sprintf(str,"%sVALUE",*flagstr ? "," : "");
	if (p->flags & CONFIG_FLAG_IN_TRIG) str += sprintf(str,"%sIN_TRIG",*flagstr ? "," : "");

	printf("%-20.20s: 0x%x(%s)\n", "flags", p->flags, flagstr);
	printf("%-20.20s: %s\n", "scope", p->scope);
	printf("%-20.20s: %s\n", "values", p->values);
	printf("%-20.20s: %s\n", "labels", p->labels);
	printf("%-20.20s: %s\n", "units", p->units);
	printf("%-20.20s: %f\n", "scale", p->scale);
	printf("%-20.20s: %d\n", "precision", p->precision);
	printf("%-20.20s: %d\n", "len", p->len);
	printf("%-20.20s: %d\n", "id", p->id);
	printf("%-20.20s: %d\n", "dirty", p->dirty);
	printf("\n");
}

static void _config_dump_prop(config_property_t *p) {
	char value[1024];

	if (p->dest) conv_type(DATA_TYPE_STRING,value,sizeof(value)-1,p->type, p->dest, p->len);
	else *value = 0;
	printf("  prop: id: %d, name: %s, type: %x(%s), value: %s, flags: %x\n", p->id, p->name, p->type, typestr(p->type), value,p->flags);
}

void config_dump_props(config_property_t *props) {
	config_property_t *p;

	for(p = props; p->name; p++) _config_dump_prop(p);
}

#if 0
static int _cmpprop(void *i1, void *i2) {
	config_property_t *p1 = i1;
	config_property_t *p2 = i2;

	if (p1->id < p2->id) return -1;
	else if (p1->id == p2->id) return 0;
	else return 1;
#if 0
	int r;

	r = strcmp(p1->name,p2->name);
	if (r < 0)
		return -1;
	else if (r == 0)
		return 0;
	else
		return 1;
#endif
//	return r;
}
#endif

void config_dump(config_t *cp) {
	config_section_t *section;
	config_property_t *p;

	if (!cp) return;

	dprintf(dlevel,"dumping...\n");

	list_reset(cp->sections);
	while((section = list_get_next(cp->sections)) != 0) {
		printf("section: %s\n", section->name);
		list_reset(section->items);
		while((p = list_get_next(section->items)) != 0) _config_dump_prop(p);
	}
}

config_function_t *config_function_get(config_t *cp, char *name) {
	config_function_t *f;

	dprintf(dlevel,"name: %s\n", name);

	list_reset(cp->funcs);
	while((f = list_get_next(cp->funcs)) != 0) {
		dprintf(dlevel,"f->name: %s, name: %s\n", f->name, name);
		if (strcasecmp(f->name,name) == 0) {
			dprintf(dlevel,"found\n");
			return f;
		}
	}
	dprintf(dlevel,"NOT found\n");
	return 0;
}

config_property_t *config_section_get_property(config_section_t *s, char *name) {
	config_property_t *p;

	int ldlevel = dlevel;

	if (!s) return 0;

	dprintf(ldlevel,"==> LOOKING FOR: section: %s, name: %s\n", s->name, name);

	list_save_next(s->items);
	list_reset(s->items);
	while((p = list_get_next(s->items)) != 0) {
//		dprintf(dlevel+1,"p: %p\n", p);
		dprintf(ldlevel+2,"CHECKING: p->name: %s, name: %s\n", p->name, name);
		if (strcasecmp(p->name,name)==0) {
			dprintf(ldlevel,"found: %p\n",p);
			list_restore_next(s->items);
			return p;
		}
	}
	list_restore_next(s->items);
	dprintf(ldlevel,"NOT found\n");
	return 0;
}

int config_section_get_properties(config_section_t *s, config_property_t *props) {
	config_property_t *p,*pp;

	if (!s) return 0;

	dprintf(dlevel,"section: %s\n", s->name);

	for(pp = props; pp->name; pp++) {
//		printf(1,"pp->name: %s\n", pp->name);
		p = config_section_get_property(s, pp->name);
		if (!p) {
			if (pp->def && pp->dest) pp->len = conv_type(pp->type, pp->dest, pp->dsize, DATA_TYPE_STRING, pp->def, strlen(pp->def));
			continue;
		}
		pp->len = conv_type(pp->type, pp->dest, pp->dsize, p->type, p->dest, p->len);
	}
	return 0;
}

config_property_t *config_get_property(config_t *cp, char *sname, char *name) {
	config_section_t *s;
//	config_property_t *p;

	dprintf(dlevel,"sname: %s, name: %s\n", sname, name);

	s = config_get_section(cp, sname);
	dprintf(dlevel,"s: %p\n", s);
	if (!s) return 0;

	return config_section_get_property(s,name);
#if 0
	dprintf(dlevel,"item count: %d\n", list_count(s->items));
	list_reset(s->items);
	while((p = list_get_next(s->items)) != 0) {
		dprintf(dlevel+1,"p->name: %s\n", p->name);
		if (strcasecmp(p->name,name) == 0) {
			dprintf(dlevel,"found\n");
			return p;
		}
	}
	dprintf(dlevel,"NOT found\n");
	return 0;
#endif
}

int config_delete_property(config_t *cp, char *sname, char *name) {
	config_section_t *s;
	config_property_t *p;

	dprintf(dlevel,"sname: %s, name: %s\n", sname, name);

	s = config_get_section(cp, sname);
	if (!s) return 1;

	list_reset(s->items);
	while((p = list_get_next(s->items)) != 0) {
		if (strcasecmp(p->name,name) == 0) {
			dprintf(dlevel,"found\n");
			if (cp->map && p->id >=0 && p->id < cp->map_maxid) cp->map[p->id] = 0;
			list_delete(s->items,p);
			return 0;
		}
	}
	dprintf(dlevel,"NOT found\n");
	return 1;
}

int config_section_set_property(config_section_t *s, config_property_t *p) {
	config_property_t *pp;

	if (!s) return 1;

	pp = config_section_get_property(s, p->name);
	if (!pp) return 1;
	p->len = conv_type(p->type, p->dest, p->dsize, pp->type, pp->dest, pp->len);
	return 0;
}

static void *_destdup(config_property_t *p) {
	void *dest;
	int size = 0;

	/* If they're both strings, size may change */
	dprintf(dlevel,"type: %s\n", typestr(p->type));
	if (p->type == DATA_TYPE_STRING) size = strlen((char *)p->dest)+1;
	if (!size) {
		dprintf(dlevel,"size: %d\n", size);
		if (!size) {
			size = typesize(p->type);
			dprintf(dlevel,"NEW size: %d\n", size);
		}
		if (!size) {
			size = 128;
			dprintf(dlevel,"NEW size: %d\n", size);
		}
	}
	if (p->type & DATA_TYPE_MLIST) dest = list_create();
	else dest = malloc(size);
	dprintf(dlevel,"dest: %p\n", dest);

	conv_type(p->type,dest,size,p->type,p->dest,p->len ? p->len : p->dsize);
	return dest;
}

int config_property_set_value(config_property_t *p, int type, void *src, int size, int dirty, int trigger) {
	void *old_dest;
	int new_dsize;

	int ldlevel = dlevel;

	dprintf(ldlevel,"***************************************\n");
	dprintf(ldlevel,"p->name: %s, trigger: %d\n", p->name, trigger);
#if 0
	{
		char value[1024];

		if (src) {
			dprintf(ldlevel,"src: type: %s, src: %p, size: %d\n", typestr(type), src, size);
			conv_type(DATA_TYPE_STRING,value,sizeof(value),type,src,size);
		} else {
			*value = 0;
		}
		dprintf(ldlevel,"%s new value: %s\n", p->name, value);
	}
#endif

	/* Make a copy of dest */
	old_dest = p->dest ? _destdup(p) : 0;

	/* XXX */
	if (type == DATA_TYPE_STRING) size++;

	/* If the dsize changed realloc */
	new_dsize = p->dsize;
	if (p->type == DATA_TYPE_STRING && type == DATA_TYPE_STRING && new_dsize < size) new_dsize = size;
	if (!new_dsize) {
		new_dsize = typesize(p->type);
		dprintf(ldlevel,"new_dsize: %d (from typesize)\n", new_dsize);
		if (!new_dsize) {
			void *dummy = malloc(1024);
			/* conv_type should return a good dsize */
			new_dsize = conv_type(p->type,dummy,1024,type,src,size);
			dprintf(ldlevel,"new_dsize: %d (from conv)\n", new_dsize);
			free(dummy);
		}
	}
	dprintf(ldlevel,"new_dsize: %d, p->type: %s\n", new_dsize, typestr(p->type));
	if (!new_dsize && p->type == DATA_TYPE_STRING) {
		if (type == DATA_TYPE_STRING && src != 0) new_dsize = strlen(src)+1;
		new_dsize = 1;
	}
	dprintf(ldlevel,"new_dsize: %d, p->dsize: %d\n", new_dsize, p->dsize);
	if (p->dsize < new_dsize) {
		if (p->dest && (p->flags & CONFIG_FLAG_ALLOCDEST)) {
			dprintf(ldlevel,"freeing dest of %d for %s\n", p->dsize, p->name);
			if (p->type & DATA_TYPE_MLIST) list_destroy((list)p->dest);
			else free(p->dest);
			p->dest = 0;
		}
		p->dsize = new_dsize;
	}
	if (!p->dest) {
		dprintf(ldlevel,"allocating dest of %d for %s\n", p->dsize, p->name);
		if (p->type & DATA_TYPE_MLIST) p->dest = list_create();
		else p->dest = malloc(p->dsize);
		dprintf(ldlevel,"p->dest: %p\n", p->dest);
		if (!p->dest) return 1;
		p->flags |= CONFIG_FLAG_ALLOCDEST;
	}
#if 0
	dprintf(ldlevel,"p->dest: %p, p->flags: %x\n", p->dest, p->flags);
	if (p->dest && (p->flags & CONFIG_FLAG_ALLOCDEST)) {
		dprintf(ldlevel,"destroying dest...\n");
		if (p->type & DATA_TYPE_MLIST) list_destroy((list)p->dest);
		else free(p->dest);
		p->dest = 0;
	}

	dprintf(ldlevel,"p->dest: %p, type: %s, ISNUMBER: %d\n", p->dest, typestr(p->type), DATA_TYPE_ISNUMBER(p->type));
// || !DATA_TYPE_ISNUMBER(p->type)) {
	if (!p->dest) {
		dprintf(ldlevel,"allocating dest...\n");
		dprintf(ldlevel,"p->type: %s, type: %s\n", typestr(p->type), typestr(type));
		/* If they're both strings, size may change */
		if (p->type == DATA_TYPE_STRING) {
			size++;
			if (type == DATA_TYPE_STRING && p->dsize != size) p->dsize = size;
		}
		dprintf(ldlevel,"p->dsize: %d, size: %d\n", p->dsize, size);
		if (!p->dsize) {
			p->dsize = typesize(p->type);
			dprintf(ldlevel,"p->dsize: %d\n", p->dsize);
			/* XXX LOL WTF */
			if (!p->dsize) {
				if (p->type & DATA_TYPE_MLIST) p->dsize = sizeof(void *);
				else p->dsize = 128;
			}
		}
		if (p->type & DATA_TYPE_MLIST) p->dest = list_create();
		else p->dest = malloc(p->dsize);
		dprintf(ldlevel,"p->dest: %p\n", p->dest);
		if (!p->dest) return 1;
		p->flags |= CONFIG_FLAG_ALLOCDEST;
	}
#endif

	dprintf(ldlevel,"p->dest: %p, p->dsize: %d\n", p->dest, p->dsize);
//	dprintf(dlevel,"p->type: %s, src type: %s\n", typestr(p->type), typestr(type));
	p->len = conv_type(p->type,p->dest,p->dsize,type,src,size);
	p->flags |= CONFIG_FLAG_VALUE;
	p->dirty = dirty;
//	config_dump_property(p,0);
	dprintf(dlevel,"==> trigger: %d, p->dest: %p, p->trigger: %p\n", trigger, p->dest, p->trigger);
	if (trigger && p->dest && !_check_flag(p->flags,NOTRIG) && p->trigger) {
		dprintf(ldlevel,"calling trigger!\n");
		p->trigger(p->ctx,p,old_dest);
	}

	dprintf(ldlevel,"old_dest: %p\n", old_dest);
	if (old_dest) {
		dprintf(ldlevel,"destroying old_dest...\n");
		if (p->type & DATA_TYPE_MLIST) list_destroy((list)old_dest);
		else free(old_dest);
	}

#if 0
	{
		char value[1024];

		if (p->dest) {
			dprintf(ldlevel,"dest: type: %s, dest: %p, len: %d\n", typestr(p->type), p->dest, p->len);
			conv_type(DATA_TYPE_STRING,value,sizeof(value),p->type,p->dest,p->len);
		} else {
			*value = 0;
		}
		dprintf(dlevel,"%s = %s\n", p->name, value);
	}
#endif
	return 0;
}

int config_set_property(config_t *cp, char *sname, char *name, int type, void *src, int size) {
	config_property_t *p;

	p = config_get_property(cp, sname, name);
	if (!p) return 1;
	return config_property_set_value(p,type,src,size,true,cp->triggers);
}

config_property_t *config_combine_props(config_property_t *p1, config_property_t *p2) {
	config_property_t *pp1,*pp2,*newp,*npp;
	int count,size;
	bool found;

	/* XXX important: p2 overrides p1 */

#if 0
	if (p1) {
		dprintf(-1,"*** P1 ****\n");
		for(pp1=p1; pp1->name; pp1++) {
			dprintf(-1,"P1: name: %s, type: %x, dest: %p\n", pp1->name, pp1->type, pp1->dest);
		}
	}
	if (p2) {
		dprintf(-1,"*** P2 ****\n");
		for(pp2=p2; pp2->name; pp2++) {
			dprintf(-1,"P1: name: %s, type: %x, dest: %p\n", pp2->name, pp2->type, pp2->dest);
		}
	}
#endif
	/* put the props into a list of unique entries, get the size and copy to new struct */
	count = 0;
	if (p2) {
		for(pp2=p2; pp2->name; pp2++) count++;
	}
	if (p1) {
		/* dont count if found in p2 */
		for(pp1=p1; pp1->name; pp1++) {
			found = false;
			if (p2) {
				for(pp2=p2; pp2->name; pp2++) {
					if (strcmp(pp2->name,pp1->name) == 0) {
						found = true;
						break;
					}
				}
			}
			if (!found) count++;
			else {
			}
		}
	}
	size = (count + 1) * sizeof(config_property_t);
	dprintf(dlevel,"count: %d,  size: %d\n", count, size);
	newp = malloc(size);
	if (!newp) {
		log_syserror("config_combine_props: malloc(%d)",size);
		return 0;
	}
	npp = newp;
	if (p2) {
		for(pp2=p2; pp2->name; pp2++) {
			*npp = *pp2;
			if (p1) {
			for(pp1=p1; pp1->name; pp1++) {
				if (strcmp(pp1->name,npp->name) == 0) {
					dprintf(dlevel+2,"p1: name: %s, dest: %p(%d), allocdest: %d\n", pp1->name, pp1->dest, pp1->dsize, check_bit(pp1->flags,CONFIG_FLAG_ALLOCDEST));
					dprintf(dlevel+2,"npp: name: %s, dest: %p(%d)\n", npp->name, npp->dest, npp->dsize);
					/* if p1's dest is static && p2's dest is empty */
					if (pp1->dest && !check_bit(pp1->flags,CONFIG_FLAG_ALLOCDEST) && !npp->dest) {
						npp->dest = pp1->dest;
						npp->dsize = pp1->dsize;
						dprintf(dlevel+2,"NEW npp->dest: %p(%d)\n", npp->dest, npp->dsize);
					}
					break;
				}
			}
			}
			npp++;
		}
	}
	if (p1) {
		for(pp1=p1; pp1->name; pp1++) {
			found = false;
			if (p2) {
				for(pp2=p2; pp2->name; pp2++) {
					if (strcmp(pp2->name,pp1->name) == 0) {
						found = true;
						break;
					}
				}
			}
			if (!found) *npp++ = *pp1;
		}
	}
	npp->name = 0;
#if 0
	dprintf(-1,"*** FINAL ****\n");
	for(npp=newp; npp->name; npp++) {
		dprintf(-1,"newp: name: %s, type: %x, dest: %p\n", npp->name, npp->type, npp->dest);
	}
#endif
	return newp;
};

void config_destroy_property(config_property_t *p) {

	if (p->dest && (p->flags & CONFIG_FLAG_ALLOCDEST)) {
		if (p->type & DATA_TYPE_MLIST) list_destroy((list)p->dest);
		else free(p->dest);
	}

#ifdef JS
	if (p->ctx && js_ctxs) list_delete(js_ctxs,p->ctx);
#endif
	dprintf(dlevel,"p->flags: %x\n", p->flags);
	if ((p->flags & CONFIG_FLAG_ALLOC) == 0) return;

	if (p->name) free(p->name);
	if (p->def) free(p->def);
	if (p->scope) free(p->scope);
	if (p->values) free(p->values);
	if (p->labels) free(p->labels);
	if (p->units) free(p->units);
	free(p);
}

config_property_t *config_new_property(config_t *cp, char *name, int type, void *dest, int dsize, int len, char *def, int flags,
		char *scope, char *values, char *labels, char *units, float scale, int precision) {
	config_property_t *newp;

	dprintf(dlevel,"cp: %p, name: %s, type: %d, dest: %p, dsize: %d, len: %d, def: %s, flags: %x, scope: %s, values: %s, labels: %s, units: %s, scale: %f, precision: %d\n", cp, name, type, dest, dsize, len, def, flags, scope, values, labels, units, scale, precision);
	if (!name) return 0;

	newp = malloc(sizeof(*newp));
	dprintf(dlevel,"newp: %p\n", newp);
	if (!newp) return 0;
	memset(newp,0,sizeof(*newp));
	newp->cp = cp;
	newp->flags = CONFIG_FLAG_ALLOC;
	newp->name = strdup(name);
	if (!newp->name) goto _newp_error;
	newp->type = type;
	if (dest) {
                char value[1024];

		dprintf(dlevel,"type: %s, dest: %p, size: %d\n", typestr(type), dest, dsize);
                conv_type(DATA_TYPE_STRING,value,sizeof(value),type,dest,dsize);
                dprintf(dlevel,"%s value: %s\n", newp->name, value);

		config_property_set_value(newp,type,dest,dsize,true,(cp ? cp->triggers : false));
	}
	if (def) {
		newp->def = strdup(def);
		dprintf(dlevel,"newp->def: %p\n", newp->def);
		if (!newp->def) goto _newp_error;
	}
	newp->flags |= flags;
	if (scope) {
		newp->scope = strdup(scope);
		dprintf(dlevel,"newp->scope: %p\n", newp->scope);
		if (!newp->scope) goto _newp_error;
	}
	if (values) {
		newp->values = strdup(values);
		dprintf(dlevel,"newp->values: %p\n", newp->values);
		if (!newp->values) goto _newp_error;
	}
	if (labels) {
		newp->labels = strdup(labels);
		dprintf(dlevel,"newp->labels: %p\n", newp->labels);
		if (!newp->labels) goto _newp_error;
	}
	if (units) {
		newp->units = strdup(units);
		dprintf(dlevel,"newp->units: %p\n", newp->units);
		if (!newp->units) goto _newp_error;
	}
	newp->scale = scale;
	newp->precision = precision;
	return newp;
_newp_error:
	config_destroy_property(newp);
	return 0;
}

int config_section_add_property(config_t *cp, config_section_t *s, config_property_t *p, int flags) {
	config_property_t *pp;
	int trig;

	dprintf(dlevel,"cp: %p, s: %p, p: %p\n", cp, s, p);
	if (!cp || !s || !p) return 1;

	dprintf(dlevel,"cp->triggers: %d, NOTRIG: %d\n", cp->triggers, _check_flag(flags,NOTRIG));
	trig = cp->triggers && ((flags & CONFIG_FLAG_NOTRIG) == 0);
	/* Dont apply this to the property flags */
	_clear_flag(flags,NOTRIG);
	dprintf(dlevel,"p->name: %s, flags: %x, trig: %d\n", p->name, flags, trig);
	p->sp = s;

	/* Alread there? maybe from file? replace it */
	pp = config_section_get_property(s, p->name);
	dprintf(dlevel,"p: %p, pp: %p\n", p, pp);
	/* Item already added (via dup?) */
	if (pp == p) return 0;
	if (pp) {
		dprintf(dlevel,"FILE: %d, NOWARN: %d\n", _check_flag(pp->flags,FILE),_check_flag(pp->flags,NOWARN));
		if (!check_bit(pp->flags,CONFIG_FLAG_FILE) && !check_bit(pp->flags,CONFIG_FLAG_NOWARN))
			log_warning("duplicate property: %s, replacing.\n", pp->name);
		dprintf(dlevel,"pp->dest: %p, p->dest: %p\n", pp->dest, p->dest);
		if (pp->dest && !p->dest && config_property_set_value(p,pp->type,pp->dest,pp->dsize,true,trig)) return 1;
		_clear_flag(pp->flags,FILEONLY);
//		_clear_flag(pp->flags,ALLOC);
//		_clear_flag(pp->flags,ALLOCDEST);
		_clear_flag(pp->flags,VALUE);
		_clear_flag(pp->flags,NOPUB);
		flags |= pp->flags;
//		if (pp->flags & CONFIG_FLAG_FILE) p->flags |= CONFIG_FLAG_FILE;
		config_destroy_property(pp);
		list_delete(s->items,pp);
	}
	p->flags |= flags;
	dprintf(dlevel,"%s: dest: %p, def: %s, flags: %x\n", p->name, p->dest, p->def, p->flags);
	if (!_check_flag(p->flags,VALUE) && p->def && !_check_flag(p->flags,NODEF)) {
		config_property_set_value(p,DATA_TYPE_STRING,p->def,strlen(p->def),false,trig);
		_clear_flag(p->flags,VALUE);
	}
	if ((flags & CONFIG_FLAG_NOID) == 0) p->id = cp->id++;
	list_add(s->items, p, p->flags & CONFIG_FLAG_ALLOC ? 0 : sizeof(*p));
//	config_dump_property(p,0);
	return 0;
}

void config_destroy_props(config_property_t *props) {
	config_property_t *p;

	for(p = props; p->name; p++) config_destroy_property(p);
	free(props);
}

void config_build_propmap(config_t *cp) {
	config_section_t *s;
	config_property_t *p;
	int size;

	dprintf(dlevel,"cp->map: %p\n", cp->map);
	if (cp->map) free(cp->map);
	size = cp->id * sizeof(void *);
	dprintf(dlevel,"cp->id: %d, size: %d\n", cp->id, size);
	cp->map = malloc(size);
	dprintf(dlevel,"cp->map: %p\n", cp->map);
	if (!cp->map) {
		log_syserror("config_build_propmap: malloc");
		return;
	}
	memset(cp->map,0,size);
	list_reset(cp->sections);
	while((s = list_get_next(cp->sections)) != 0) {
		list_reset(s->items);
		while((p = list_get_next(s->items)) != 0) {
//			dprintf(dlevel,"p: id: %d, name: %s, flags: %x\n", p->id, p->name, p->flags)
			if (p->id < 0 || p->id > cp->id) continue;
			dprintf(dlevel+1,"adding: %s as ID %d\n", p->name, p->id);
			cp->map[p->id] = p;
		}
	}
	cp->map_maxid = cp->id;
	{
		dprintf(dlevel,"map count: %d\n", cp->map_maxid);
		for(int i=0; i < cp->map_maxid; i++) dprintf(dlevel+1,"map[%d]: %s\n", i, cp->map[i] ? (cp->map[i])->name : "(null)");
	}
}

config_section_t *config_get_section(config_t *cp,char *name) {
	config_section_t *section;

	if (!cp) return 0;

	/* Find the section */
	dprintf(dlevel,"looking for section: %s\n", name);
	list_reset(cp->sections);
	while( (section = list_get_next(cp->sections)) != 0) {
		dprintf(dlevel,"name: %s\n", section->name);
		if (strcasecmp(section->name,name)==0) {
			dprintf(dlevel,"found\n");
			*cp->errmsg = 0;
			return section;
		}
	}
	dprintf(dlevel,"NOT found!\n");
	sprintf(cp->errmsg,"unable to find config section: %s", name);
	return 0;
}

config_section_t *config_new_section(char *name, int flags) {
	config_section_t *s;

	dprintf(dlevel,"name: %s, flags: %02x\n", name, flags);

	/* Name can be null */
	s = malloc(sizeof(*s));
	if (!s) {
		log_error("config_new_section: malloc");
		return 0;
	}
	memset(s,0,sizeof(*s));
	if (name) strncpy(s->name,name,sizeof(s->name)-1);
	s->flags = flags;
	s->items = list_create();

	return s;
}

config_section_t *config_create_section(config_t *cp, char *name, int flags) {
	config_section_t newsec;

	dprintf(dlevel,"name: %s, flags: %02x\n", name, flags);

	/* Name can be null */
	memset(&newsec,0,sizeof(newsec));
	if (name) strncpy(newsec.name,name,sizeof(newsec.name)-1);
	newsec.flags = flags;
	newsec.items = list_create();
	newsec.cp = cp;

	/* Add the new section to the list */
	return list_add(cp->sections,&newsec,sizeof(newsec));
}

int config_delete_section(config_t *cp, char *name) {
	config_section_t *section;

	if (!cp) return 0;

	/* Find the section */
	dprintf(dlevel,"looking for section: %s\n", name);
	list_reset(cp->sections);
	while( (section = list_get_next(cp->sections)) != 0) {
		dprintf(dlevel,"name: %s\n", section->name);
		if (strcasecmp(section->name,name)==0) {
			dprintf(dlevel,"found\n");
			list_delete(cp->sections,section);
			*cp->errmsg = 0;
			return 0;
		}
	}
	dprintf(dlevel,"NOT found!\n");
	sprintf(cp->errmsg,"unable to find config section: %s", name);
	return 1;
}

void config_add_props(config_t *cp, char *section_name, config_property_t *props, int flags) {
	config_property_t *pp;
	config_section_t *section;

	dprintf(dlevel,"cp: %p, props: %p, flags: %x\n", cp, props, flags);
	if (!cp || !props) return;

	section = config_get_section(cp,section_name);
	dprintf(dlevel,"section: %p\n", section);
	if (!section) section = config_create_section(cp,section_name,flags);
	dprintf(dlevel,"section: %p\n", section);
	if (!section) return;
	for(pp = props; pp->name; pp++) {
//		dprintf(dlevel,"pp->name: %s\n",  pp->name)
		if (!pp->name) continue;
//		dprintf(0,"pp->name: %s, flags: %x\n", pp->name, flags);
		config_section_add_property(cp, section, pp, flags);
	}
	if (0) {
		config_property_t *p;
		config_section_t *s;
		char *t;
		int i;

		list_reset(cp->sections);
		while((s = list_get_next(cp->sections)) != 0) {
			list_reset(s->items);
			i = 0;
			while((p = list_get_next(s->items)) != 0) {
				t = typestr(p->type);
				dprintf(dlevel,"%02d: s->name: %s, p->name(%p): %s, id: %d, type: %s, flags: %x\n", i++, s->name, p->name, p->name, p->id, typestr(p->type), p->flags);
				if (strcmp(t,"*BAD TYPE*") == 0) exit(1);
			}
		}
	}

}

config_function_t *config_combine_funcs(config_function_t *f1, config_function_t *f2) {
	config_function_t *fp,*newf,*nfp;
	int count,size;

	count = 0;
	if (f1) {
		for(fp=f1; fp->name; fp++) count++;
	}
	if (f2) {
		for(fp=f2; fp->name; fp++) count++;
	}
	size = (count + 1) * sizeof(config_function_t);
	dprintf(dlevel,"count: %d,  size: %d\n", count, size);
	newf = malloc(size);
	if (!newf) {
		log_syserror("config_combine_funcs: malloc(%d)",size);
		return 0;
	}
	memset(newf,0,size);
	nfp = newf;
	if (f1) {
		for(fp=f1; fp->name; fp++) *nfp++ = *fp;
	}
	if (f2) {
		for(fp=f2; fp->name; fp++) *nfp++ = *fp;
	}
	nfp->name = 0;
	return newf;
};

void config_add_funcs(config_t *cp, config_function_t *funcs) {
	config_function_t *f;

	if (!cp || !funcs) return;
	for(f = funcs; f->name; f++) list_add(cp->funcs, f, sizeof(*f));
}

int config_delete_function(config_t *cp, char *name) {
	config_function_t *func;

	if (!cp) return 1;

	/* Find the function */
	dprintf(dlevel,"looking for function: %s\n", name);
	list_reset(cp->funcs);
	while( (func = list_get_next(cp->funcs)) != 0) {
		dprintf(dlevel,"name: %s\n", func->name);
		if (strcasecmp(func->name,name)==0) {
			dprintf(dlevel,"found\n");
			list_delete(cp->funcs,func);
			*cp->errmsg = 0;
			return 0;
		}
	}
	dprintf(dlevel,"NOT found!\n");
	sprintf(cp->errmsg,"unable to find function: %s", name);
	return 1;
}

list config_get_funcs(config_t *cp) {
	return cp->funcs;
}

void config_destroy_funcs(config_function_t *funcs) {
	free(funcs);
}

config_t *config_init(char *section_name, config_property_t *props, config_function_t *funcs) {
	config_t *cp;

	cp = calloc(sizeof(*cp),1);
	if (!cp) {
		log_syserror("config_init: calloc");
		return 0;
	}
	cp->sections = list_create();
	cp->funcs = list_create();
	/* must start at one for JS */
	cp->id = 1;
	cp->triggers = true;
	if (props) config_add_props(cp,section_name,props,0);
	if (funcs) config_add_funcs(cp,funcs);
	if (!configs) configs = list_create();
	list_add(configs, cp, 0);

	return cp;
}

void config_destroy_config(config_t *cp) {
	config_section_t *sp;
//	config_property_t *p;

	int ldlevel = dlevel;

	dprintf(ldlevel,"cp: %p\n", cp);
	list_reset(cp->sections);
	while((sp = list_get_next(cp->sections)) != 0) {
		list_reset(sp->items);
//		while((p = list_get_next(sp->items)) != 0) config_destroy_property(p);
		list_destroy(sp->items);
	}
	list_destroy(cp->sections);
#if 0
	dprintf(dlevel,"funcs count: %d\n", list_count(cp->funcs));
	if (list_count(cp->funcs)) {
		config_function_t *f;

		list_reset(cp->funcs);
		while((f = list_get_next(cp->funcs)) != 0) {
			dprintf(dlevel,"name: %s, flags: %x\n", f->name, f->flags);
			if (f->flags & CONFIG_FUNCTION_FLAG_ALLOCNAME) free(f->name);
			list_delete(cp->funcs,f);
		}
	}
#endif
	list_destroy(cp->funcs);
	if (cp->map) free(cp->map);

	if (configs) list_delete(configs,cp);
	free(cp);
}

void config_shutdown(void) {
        config_t *cp;

	if (configs) {
		while(true) {
			list_reset(configs);
			cp = list_get_next(configs);
			if (!cp) break;
			config_destroy_config(cp);
		}
		list_destroy(configs);
		configs = 0;
	}
#ifdef JS
	dprintf(dlevel,"js_ctxs: %d\n", js_ctxs);
	if (js_ctxs) {
//		void *p;
//		list_reset(js_ctxs);
//		while((p = list_get_next(js_ctxs)) != 0) free(p);
		list_destroy(js_ctxs);
		js_ctxs = 0;
	}
#endif
}

int config_read_ini(void *ctx) {
	config_t *cp = ctx;
	cfg_info_t *cfg;
	config_section_t *sec;
	cfg_section_t *cfgsec;
	cfg_item_t *item;
	config_property_t *p;

	dprintf(dlevel,"cp: %p\n", cp);
	if (!cp) return 1;

	dprintf(dlevel,"filename: %s\n", cp->filename);
	cfg = cfg_read(cp->filename);
	dprintf(dlevel,"cfg: %p\n", cfg);
	if (!cfg) {
		sprintf(cp->errmsg, "unable to read ini file");
		return 1;
	}
	list_reset(cfg->sections);
	while((cfgsec = list_get_next(cfg->sections)) != 0) {
		dprintf(dlevel,"cfgsec->name: %s\n", cfgsec->name);
		sec = config_get_section(cp, cfgsec->name);
		if (!sec) sec = config_create_section(cp, cfgsec->name,CONFIG_FLAG_FILEONLY);
		list_reset(cfgsec->items);
		while((item = list_get_next(cfgsec->items)) != 0) {
			dprintf(dlevel,"item->keyword: %s\n", item->keyword);
			p = config_section_get_property(sec,item->keyword);
			dprintf(dlevel,"p: %p\n", p);
			if (p) {
				dprintf(dlevel,"value: %s\n", item->value);
				p->len = conv_type(p->type,p->dest,p->dsize,DATA_TYPE_STRING,item->value,strlen(item->value));
				p->flags |= CONFIG_FLAG_FILE;
//				config_dump_property(p,0);
			} else {
 				p = config_new_property(cp,item->keyword,DATA_TYPE_STRING,item->value,strlen(item->value)+1,strlen(item->value),0,(CONFIG_FLAG_FILE | CONFIG_FLAG_FILEONLY | CONFIG_FLAG_NOPUB),0,0,0,0,0,0);
				config_section_add_property(cp, sec, p, 0);
			}
		}
	}
	dprintf(dlevel,"building map...\n");
	config_build_propmap(cp);

	cfg_destroy(cfg);
	return 0;
}

static config_property_t *_json_to_prop(config_t *cp, config_section_t *sec, char *name, json_value_t *v) {
	config_property_t *p;
	int jtype;

	if (!cp) return 0;

	jtype = json_value_get_type(v);
	dprintf(dlevel,"type: %s\n", json_typestr(jtype));

	dprintf(dlevel,"name: %s\n", name);
	p = config_section_get_property(sec,name);
	dprintf(dlevel,"p: %p\n", p);
	if (p) {
		switch(jtype) {
		case JSON_TYPE_BOOLEAN:
			{
				bool b = json_value_get_boolean(v);
				dprintf(dlevel,"b: %d\n", b);
				config_property_set_value(p,DATA_TYPE_BOOL,&b,sizeof(b),true,cp->triggers);
			}
			break;
		case JSON_TYPE_NUMBER:
			{
				double d = json_value_get_number(v);
				if (double_isint(d)) {
					int i = d;
					dprintf(dlevel,"i: %d\n", i);
					config_property_set_value(p,DATA_TYPE_INT,&i,sizeof(i),true,cp->triggers);
				} else {
					dprintf(dlevel,"d: %f\n", d);
					config_property_set_value(p,DATA_TYPE_DOUBLE,&d,sizeof(d),true,cp->triggers);
				}
			}
			break;
		case JSON_TYPE_STRING:
			{
				char *str = json_value_get_string(v);
				dprintf(dlevel,"str: %s\n", str);
				config_property_set_value(p,DATA_TYPE_STRING,str,strlen(str),true,cp->triggers);
			}
			break;
		case JSON_TYPE_ARRAY:
			{
				json_array_t *a;
				int i,len;
				char *str;

				a = json_value_array(v);
				for(i=len=0; i < a->count; i++) {
					if (json_value_get_type(a->items[i]) != JSON_TYPE_STRING) continue;
					str = json_value_get_string(a->items[i]);
					len += strlen(str)+1;
				}
				str = malloc(len);
				if (!str) {
					log_syserr("config: _json_to_prop: malloc");
				} else {
					json_to_type(DATA_TYPE_STRING,str,len,v);
					config_property_set_value(p,DATA_TYPE_STRING,str,strlen(str),true,cp->triggers);
					free(str);
				}
			}
			break;
		default:
//			log_error("config:_json_to_prop: unhandled ptype: %s\n", json_typestr(jtype));
			break;
		}
		p->flags |= CONFIG_FLAG_FILE;
	} else {
		int type,dsize,len;
		void *dest;

		switch(jtype) {
		case JSON_TYPE_BOOLEAN:
			type = DATA_TYPE_BOOLEAN;
			dsize = typesize(type);
			dest = malloc(dsize);
			break;
		case JSON_TYPE_NUMBER:
			if (double_isint(json_value_get_number(v))) {
				type = DATA_TYPE_INT;
			} else {
				type = DATA_TYPE_DOUBLE;
			}
			dsize = typesize(type);
			dest = malloc(dsize);
			break;
		case JSON_TYPE_STRING:
			type = DATA_TYPE_STRING;
			dsize = strlen(json_value_get_string(v))+1;
			dest = malloc(dsize);
			break;
		case JSON_TYPE_ARRAY:
			{
				json_array_t *a;
				char *str;
				int i;

				a = json_value_array(v);
				for(i=dsize=0; i < a->count; i++) {
					if (json_value_get_type(a->items[i]) != JSON_TYPE_STRING) continue;
					str = json_value_get_string(a->items[i]);
					dsize += strlen(str)+1;
				}
				type = DATA_TYPE_STRING;
				dest = malloc(dsize);
			}
			break;
		default:
			log_error("config:_json_to_prop: unhandled type: %s\n", json_typestr(jtype));
			dest = 0;
			break;
		}
		if (dest) {
			len = json_to_type(type,dest,dsize,v);
			p = config_new_property(cp,name,type,dest,dsize,len,0,(CONFIG_FLAG_FILE | CONFIG_FLAG_FILEONLY | CONFIG_FLAG_NOPUB),0,0,0,0,0,0);
			free(dest);
			if (p) config_section_add_property(cp, sec, p, 0);
		}
	}
	return p;
}

int config_parse_json(config_t *cp, json_value_t *v) {
	json_object_t *o,*oo;
	config_section_t *sec;
	int i,j,type,have_sec;

	if (!cp) return 1;

	dprintf(dlevel,"\n**************************************************************\n");
	type = json_value_get_type(v);
	dprintf(dlevel,"type: %d (%s)\n", type, json_typestr(type));
	if (type != JSON_TYPE_OBJECT) {
		strcpy(cp->errmsg,"invalid json format");
		return 1;
	}
	o = json_value_object(v);
	if (!o) {
		strcpy(cp->errmsg,"invalid json format");
		return 1;
	}
	dprintf(dlevel,"object count: %d\n", o->count);
	have_sec = 0;
	sec = config_get_section(cp,"");
	for(i=0; i < o->count; i++) {
		/* if it's an object its a section */
		type = json_value_get_type(o->values[i]);
		dprintf(dlevel,"o[%d]: name %s, type: %s\n", i, o->names[i], json_typestr(type));
		if (type == JSON_TYPE_OBJECT) {
			sec = config_get_section(cp, o->names[i]);
			if (!sec) {
				sec = config_create_section(cp,o->names[i],CONFIG_FLAG_FILE | CONFIG_FLAG_FILEONLY);
				if (!sec) {
					sprintf(cp->errmsg,"unable to create section: %s",o->names[i]);
					return 1;
				}
			}
			dprintf(dlevel,"section: %s\n", sec->name);
			oo = json_value_object(o->values[i]);
			dprintf(dlevel,"count: %d\n", oo->count);
			for(j=0; j < oo->count; j++) _json_to_prop(cp,sec,oo->names[j],oo->values[j]);
			have_sec = 1;
		} else if (have_sec) {
			strcpy(cp->errmsg,"config file mixes sections and non-sections");
			return 1;
		} else {
			_json_to_prop(cp,sec,o->names[i],o->values[i]);
		}
	}
	dprintf(dlevel,"building map...\n");
	config_build_propmap(cp);

	return 0;
}

int config_load_json(config_t *cp, char *data) {
	json_value_t *v;
	int r;

	v = json_parse(data);
	if (!v) {
		sprintf(cp->errmsg,"error parsing JSON data");
		return 1;
	}

	r = config_parse_json(cp,v);
	json_destroy_value(v);

	return r;
}

int config_read_json(void *ctx) {
	config_t *cp = ctx;
	json_value_t *v;
	struct stat sb;
	int fd,size,bytes,r;
	char *buf;

	if (!cp) return 1;

	/* Unable to open is not an error... */
	dprintf(dlevel,"filename: %s\n", cp->filename);
	fd = open(cp->filename,O_RDONLY);
	dprintf(dlevel,"fd: %d\n", fd);
	if (fd < 0) return 0;

	/* Get file size */
	size = (fstat(fd, &sb) < 0 ?  size = 1048576 : sb.st_size);
	dprintf(dlevel,"size: %d\n", size);

	/* Alloc buffer to hold it */
	buf = malloc(size);
	if (!buf) {
		sprintf(cp->errmsg, "config_read_json: malloc(%d): %s\n", size, strerror(errno));
		close(fd);
		return 1;
	}

	/* Read entire file into buffer */
	bytes = read(fd, buf, size);
	dprintf(dlevel,"bytes: %d\n", bytes);
	if (bytes != size) {
		sprintf(cp->errmsg, "config_read_json: read bytes(%d) != size(%d)\n", bytes, size);
		free(buf);
		close(fd);
		return 1;
	}
	close(fd);

	/* Parse the buffer */
	v = json_parse(buf);
	free(buf);
	if (!v) {
		sprintf(cp->errmsg, "config_read_json: error parsing json data\n");
		return 1;
	}

	r = config_parse_json(cp,v);
	json_destroy_value(v);

	return r;
}

int config_write_ini(void *ctx) {
	config_t *cp = ctx;
	char value[1024];
	cfg_info_t *cfg;
	config_section_t *s;
	config_property_t *p;

	cfg = cfg_create(cp->filename);
	if (!cfg) {
		sprintf(cp->errmsg, "unable to create ini info: %s",strerror(errno));
		return 1;
	}
	list_reset(cp->sections);
	while((s = list_get_next(cp->sections)) != 0) {
		dprintf(dlevel,"s->flags: %02x\n", s->flags);
		if (s->flags & CONFIG_FLAG_NOSAVE) continue;
		list_reset(s->items);
		while((p = list_get_next(s->items)) != 0) {
			if (p->dest == (void *)0) continue;
			dprintf(dlevel,"p->name: %s, p->flags: %x, dirty: %d\n", p->name, p->flags, p->dirty);
			if (check_bit(p->flags,CONFIG_FLAG_NOSAVE)) continue;
			if (!p->dirty && !check_bit(p->flags,CONFIG_FLAG_FILE)) continue;
			dprintf(dlevel,"getting value...\n");
			conv_type(DATA_TYPE_STRING,value,sizeof(value)-1,p->type,p->dest,p->len);
			if (!strlen(value)) continue;
			dprintf(dlevel,"section: %s, item: %s, value(%d): %s\n", s->name, p->name, strlen(value), value);
			cfg_set_item(cfg, s->name, p->name, 0, value);
			p->dirty = 0;
		}
	}
	if (cfg_write(cfg)) {
		sprintf(cp->errmsg, "unable to write ini file: %s",strerror(errno));
		return 1;
	}
	return 0;
}

json_value_t *config_to_json(config_t *cp, int noflags, int dirty) {
	config_section_t *s;
	config_property_t *p;
	json_object_t *co,*so;
	json_value_t *v;
	list l;

	if (!cp) return 0;

	dprintf(dlevel,"noflags: %x, dirty: %d\n", noflags, dirty);

	co = json_create_object();
	list_reset(cp->sections);
	while((s = list_get_next(cp->sections)) != 0) {
		dprintf(dlevel,"section: name: %s, flags: %x, noflags: %x, result: %d\n", s->name, s->flags, noflags, (s->flags & noflags));
		if (s->flags & noflags) continue;
		dprintf(dlevel,"section: %s\n", s->name);
		if (!list_count(s->items)) continue;
		/* add ones that meet the criteria to temp list (so we dont add empty sections) */
		l = list_create();
		dprintf(dlevel,"item count: %d\n", list_count(s->items));
		list_reset(s->items);
		while((p = list_get_next(s->items)) != 0) {
			dprintf(dlevel,"p: name: %s, flags: %x, noflags: %x, dirty: %d, wantdirty: %d\n",
				p->name, p->flags, noflags, p->dirty, dirty);
			if (_check_flag(noflags,NOPUB) && _check_flag(p->flags,PUB)) {
				list_add(l,p,0);
				continue;
			}
			if (p->flags & noflags) continue;
//			dprintf(dlevel,"dest: %p\n", p->dest);
			if (!p->dest) continue;
//			dprintf(dlevel,"p->dirty: %d, dirty: %d\n", p->dirty, dirty);
			if (dirty && !p->dirty && (!(p->flags & CONFIG_FLAG_FILE))) continue;
			dprintf(dlevel,"adding...\n");
			list_add(l,p,0);
		}
		dprintf(dlevel,"count: %d\n", list_count(l));
		if (list_count(l)) {
			/* Add them */
			so = json_create_object();
			dprintf(dlevel,"so: %p\n", so);
			list_reset(l);
			while((p = list_get_next(l)) != 0) {
				v = json_from_type(p->type,p->dest,p->len);
				dprintf(dlevel,"v: %p\n", v);
				if (!v) continue;
				json_object_set_value(so,p->name,v);
			}
			list_destroy(l);
			json_object_set_object(co,s->name,so);
		}
	}
	dprintf(dlevel,"returning: %p\n", co);

	return json_object_value(co);
}

int config_write_json(void *ctx) {
	config_t *cp = ctx;
	char *data;
	json_value_t *v;
	FILE *fp;

	dprintf(dlevel,"config_write_json: %p\n", cp);
	if (!cp) return 1;

	v = config_to_json(cp, CONFIG_FLAG_NOSAVE, 1);
	if (!v) return 1;
	data = json_dumps(v,1);
	json_destroy_value(v);
	if (!data) {
		sprintf(cp->errmsg, "config_write_json: json_dumps: %s",strerror(errno));
		return 1;
	}

	fp = fopen(cp->filename, "w+");
	if (!fp) {
		char fn[64];
		strncpy(fn,cp->filename,sizeof(fn));
		sprintf(cp->errmsg,"config_write_json: fopen(%s): %s",fn,strerror(errno));
		free(data);
		return 1;
	}
	fprintf(fp,"%s\n",data);
	fclose(fp);
	free(data);
	return 0;
}

int config_set_reader(config_t *cp, config_rw_t func, void *ctx) {

	dprintf(dlevel,"cp: %p\n", cp);
	if (!cp) return 1;

	dprintf(dlevel,"func: %p, ctx: %p\n", func, ctx);
	cp->reader = func;
	cp->reader_ctx = ctx;
	return 0;
}

int config_set_writer(config_t *cp, config_rw_t func, void *ctx) {

	dprintf(dlevel,"cp: %p\n", cp);
	if (!cp) return 1;

	dprintf(dlevel,"func: %p, ctx: %p\n", func, ctx);
	cp->writer = func;
	cp->writer_ctx = ctx;
	return 0;
}

static void _set_rw(config_t *cp) {
_set_rw_again:
	dprintf(dlevel,"format: %d\n", cp->file_format);
	switch(cp->file_format) {
	case CONFIG_FILE_FORMAT_AUTO:
		{
			char *p;

			/* Try to determine format */
			p = strrchr(cp->filename,'.');
			if (p && (strcasecmp(p,".json") == 0)) {
				cp->file_format = CONFIG_FILE_FORMAT_JSON;
				goto _set_rw_again;
			} else {
				cp->file_format = CONFIG_FILE_FORMAT_INI;
				goto _set_rw_again;
			}
		}
		break;
	default:
	case CONFIG_FILE_FORMAT_INI:
		config_set_reader(cp, config_read_ini, cp);
		config_set_writer(cp, config_write_ini, cp);
		break;
	case CONFIG_FILE_FORMAT_JSON:
		config_set_reader(cp, config_read_json, cp);
		config_set_writer(cp, config_write_json, cp);
		break;
	case CONFIG_FILE_FORMAT_CUSTOM:
		if (!cp->reader) config_set_reader(cp, config_read_ini, cp);
		if (!cp->writer) config_set_writer(cp, config_write_ini, cp);
		break;
	}
}

int config_read(config_t *cp) {

	dprintf(dlevel,"cp: %p\n", cp);
	if (!cp) return 1;

	dprintf(dlevel,"reader: %p\n", cp->reader);
	if (!cp->reader) _set_rw(cp);
	if (cp->reader(cp->reader_ctx)) return 1;

	return 0;
}

int config_write(config_t *cp) {

	if (!cp) return 1;

	dprintf(dlevel,"writer: %p\n", cp->writer);
	if (!cp->writer) _set_rw(cp);
	return cp->writer(cp->writer_ctx);
}

int config_set_filename(config_t *cp, char *filename, enum CONFIG_FILE_FORMAT format) {

	dprintf(dlevel,"cp: %p, filename: %s, format: %d\n", cp, filename, format);
	if (!cp) return 1;

	strncpy(cp->filename, filename, sizeof(cp->filename)-1);
	cp->file_format = format;
	_set_rw(cp);
	return 0;
}

int config_read_file(config_t *cp, char *filename) {
	config_set_filename(cp,filename,CONFIG_FILE_FORMAT_AUTO);
	return config_read(cp);
}

config_property_t *config_find_property(config_t *cp, char *name) {
	config_section_t *s;
	config_property_t *p;

	if (!cp) return 0;

	dprintf(dlevel,"name: %s\n", name);
	if (strchr(name,'.')) {
		char sname[CONFIG_SECTION_NAME_SIZE];
		char pname[64];
		config_section_t *sec;

		strncpy(sname,strele(0,".",name),sizeof(sname)-1);
		strncpy(pname,strele(1,".",name),sizeof(pname)-1);
		dprintf(dlevel,"sname: %s, pname: %s\n", sname, pname);
		sec = config_get_section(cp,sname);
		dprintf(dlevel,"sec: %p\n", sec);
		if (sec) {
			p = config_section_get_property(sec, pname);
			if (p) return p;
		}
	}
	list_reset(cp->sections);
	while((s = list_get_next(cp->sections)) != 0) {
		dprintf(dlevel,"s->name: %s\n",s->name);
		list_reset(s->items);
		while((p = list_get_next(s->items)) != 0) {
			dprintf(dlevel,"p->name: %s, name: %s\n", p->name, name);
			if (!p->name) {
				list_delete(s->items,p);
				continue;
			}
			if (strcmp(p->name,name)==0) {
				dprintf(dlevel,"found\n");
				return p;
			}
		}
	}
	dprintf(dlevel,"NOT found\n");
	return 0;
}

config_property_t *config_get_propbyid(config_t *cp, int id) {
	config_section_t *s;
	config_property_t *p;

	if (!cp) return 0;

#if 0
	dprintf(dlevel,"cp->map: %p, id: %d, map_maxid: %d, cp->id: %d\n", cp->map, id, cp->map_maxid, cp->id);
	if (cp->map && (id >= 0 && id < cp->map_maxid)) {
		dprintf(dlevel,"cp->map[%d]: %p\n", id, cp->map[id]);
	}
	dprintf(dlevel,"checking map...\n");
	if (cp->map && id >= 0 && id < cp->map_maxid && cp->map[id]) {
		dprintf(dlevel,"found: %s\n", cp->map[id]->name);
		return cp->map[id];
	}
	dprintf(dlevel,"NOT found, checking lists\n");
#endif

	dprintf(dlevel,"id: %d\n", id);
	list_reset(cp->sections);
	while((s = list_get_next(cp->sections)) != 0) {
		dprintf(dlevel,"section: %s, prop count: %d\n", s->name, list_count(s->items));
		list_reset(s->items);
		while((p = list_get_next(s->items)) != 0) {
			dprintf(dlevel,"p: id: %d, name: %s\n", p->id, p->name);
			if (p->id == id) {
				dprintf(dlevel,"found\n");
				return p;
			}
		}
	}
	dprintf(dlevel,"NOT found\n");
	return 0;
}

#if 0
static void json_dump(json_value_t *v) {
	char *str;

	str = json_dumps(v,1);
	printf("%s\n", str);
	free(str);
}
#endif

static json_array_t *_get_json_section(json_array_t *a, char *name) {
	json_object_t *so;
	int i,type;

	dprintf(dlevel,"name: %s\n", name);

	/* Search the configuration array for a section */
	/* MUST be an array of objects */
	for(i=0; i < a->count; i++) {
		type = json_value_get_type(a->items[i]);
		dprintf(dlevel,"type: %s\n", json_typestr(type));
		if (type != JSON_TYPE_OBJECT) return 0;
		/* section object has only 1 value and is an array */
		so = json_value_object(a->items[i]);
		if (strcmp(so->names[0],name) == 0) {
			if (so->count != 1 || json_value_get_type(so->values[0]) != JSON_TYPE_ARRAY) return 0;
			return json_value_array(so->values[0]);
		}
	}
	dprintf(dlevel,"NOT found\n");
	return 0;
}

json_object_t *prop_to_json(config_property_t *p) {
	json_object_t *to;

	if (!p) return 0;

	to = json_create_object();

	json_object_set_string(to,"name",p->name);
	json_object_set_string(to,"type",typestr(p->type));
	if (p->dsize) json_object_set_number(to,"size",p->dsize);
	if (p->def) json_object_set_string(to,"default",p->def);
	if (p->scope) json_object_set_string(to,"scope",p->scope);
	if (p->values) json_object_set_string(to,"values",p->values);
	if (p->labels) json_object_set_string(to,"labels",p->labels);
	if (p->units) json_object_set_string(to,"units",p->units);
	if ((int)p->scale != 0) json_object_set_number(to,"scale",p->scale);
	return to;
}

static json_object_t *_get_json_prop(json_array_t *a, char *name) {
	json_object_t *o;
	char *p;
	int i;

	dprintf(dlevel,"name: %s\n", name);

	for(i=0; i < a->count; i++) {
		o = json_value_object(a->items[i]);
//		dprintf(dlevel+1,"o: %p\n", o);
		if (!o) continue;
//		for(j=0; j < o->count; j++) dprintf(dlevel+1,"names[%d]: %s\n", j, o->names[j]);
		p = json_object_get_string(o, name);
//		dprintf(dlevel+1,"p: %p\n", p);
		if (!p) continue;
		if (strcmp(p, name) == 0) return o;
	}
	return 0;
}

int config_service_get_value(void *ctx, list args, char *errmsg, json_object_t *results) {
	config_t *cp = ctx;
	config_arg_t *arg;
	char *name;
	config_property_t *p;
	json_value_t *v;

	dprintf(dlevel,"args count: %d\n", list_count(args));
	list_reset(args);
	while((arg = list_get_next(args)) != 0) {
		name = arg->argv[0];
		p = config_find_property(cp, name);
		dprintf(dlevel,"p: %p\n", p);
		if (!p) {
			sprintf(errmsg,"property %s not found",name);
			return 1;
		}
		v = json_from_type(p->type,p->dest,p->len);
		if (v) json_object_set_value(results,p->name,v);
	}
	return 0;
}

int config_service_set_value(void *ctx, list args, char *errmsg, json_object_t *results) {
	config_t *cp = ctx;
	config_arg_t *arg;
	char *value;
	config_property_t *p;
	bool doit;

	doit = false;
	list_reset(args);
	while((arg = list_get_next(args)) != 0) {
		p = config_find_property(cp, arg->argv[0]);
		dprintf(dlevel,"p: %p\n", p);
		if (!p) {
			sprintf(errmsg,"property %s not found",arg->argv[0]);
			return 1;
		}
		if (p->flags & CONFIG_FLAG_READONLY) {
			sprintf(errmsg,"property %s is readonly",arg->argv[0]);
			return 1;
		}
		value = arg->argv[1];
		dprintf(dlevel,"name: %s, value: %s\n", p->name, value);
                if (strcasecmp(value,"default")==0) {
			if (!p->def) {
				sprintf(errmsg,"property %s has no default",arg->argv[0]);
				return 1;
			}
                        value = p->def;
                }
		if (!config_property_set_value(p,DATA_TYPE_STRING,value,strlen(value),true,cp->triggers)) {
			p->flags |= CONFIG_FLAG_FILE;
			doit = true;
		}
	}
	if (doit) config_write(cp);
	return 0;
}

int config_service_clear_value(void *ctx, list args, char *errmsg, json_object_t *results) {
	config_t *cp = ctx;
	config_arg_t *arg;
        char *name,*def;
	config_property_t *p;
	void *old_dest;

        dprintf(dlevel,"args count: %d\n", list_count(args));
        list_reset(args);
        while((arg = list_get_next(args)) != 0) {
		name = arg->argv[0];
                dprintf(dlevel,"name: %s\n", name);
                p = config_find_property(cp, name);
                dprintf(dlevel,"p: %p\n", p);
                if (!p) {
                        sprintf(errmsg,"property %s not found",name);
                        return 1;
                }

		/* Make a copy of dest */
		old_dest = p->dest ? _destdup(p) : 0;

		/* Set default */
		def = p->def ? p->def : "";
		config_property_set_value(p,DATA_TYPE_STRING,def,strlen(def),false,cp->triggers);
		_clear_flag(p->flags,FILE);
		_clear_flag(p->flags,FILEONLY);
		_clear_flag(p->flags,VALUE);
		p->dirty = 0;
        	dprintf(dlevel,"p->dest: %p, p->trigger: %p\n", p->dest, p->trigger);
	        if (p->dest && !_check_flag(p->flags,NOTRIG) && p->trigger) {
			dprintf(dlevel,"*** CALLING TRIGGER ***\n");
			p->trigger(p->ctx,p,old_dest);
		}

		dprintf(dlevel,"old_dest: %p\n", old_dest);
		if (old_dest) {
			dprintf(dlevel,"destroying old_dest...\n");
			if (p->type & DATA_TYPE_MLIST) list_destroy((list)old_dest);
			else free(old_dest);
		}

	}
	config_write(cp);

	return 0;
}

static int config_load(void *ctx, list args, char *errmsg, json_object_t *results) {
	config_t *cp = ctx;
	config_arg_t *arg;

	arg = list_get_first(args);
	dprintf(dlevel,"arg: %p\n", arg);
	if (!arg) return 1;
	if (config_load_json(cp,arg->argv[0])) return 1;
	log_info("new config loaded\n");
	config_write(cp);
	return 0;
}

static void _addgetset(config_t *cp) {
	int have_get, have_set, have_clear, have_load;
	config_function_t newfunc,*f;

	list_reset(cp->funcs);
	have_get = have_set = have_clear = have_load = 0;
	while((f = list_get_next(cp->funcs)) != 0) {
		if (strcasecmp(f->name,"get")==0) have_get = 1;
		if (strcasecmp(f->name,"set")==0) have_set = 1;
		if (strcasecmp(f->name,"clear")==0) have_clear = 1;
		if (strcasecmp(f->name,"load")==0) have_load = 1;
	}
	dprintf(dlevel,"have: get: %d, set: %d, clear: %d, load: %d\n", have_get, have_set, have_clear, have_load);
	if (!have_get) {
		newfunc.name = "get";
		newfunc.func = config_service_get_value;
		newfunc.ctx = cp;
		newfunc.nargs = 1;
		list_add(cp->funcs,&newfunc,sizeof(newfunc));
	}
	if (!have_set) {
		newfunc.name = "set";
		newfunc.func = config_service_set_value;
		newfunc.ctx = cp;
		newfunc.nargs = 2;
		list_add(cp->funcs,&newfunc,sizeof(newfunc));
	}
	if (!have_clear) {
		newfunc.name = "clear";
		newfunc.func = config_service_clear_value;
		newfunc.ctx = cp;
		newfunc.nargs = 1;
		list_add(cp->funcs,&newfunc,sizeof(newfunc));
	}
	if (!have_load) {
		newfunc.name = "load";
		newfunc.func = config_load;
		newfunc.ctx = cp;
		newfunc.nargs = 1;
		list_add(cp->funcs,&newfunc,sizeof(newfunc));
	}
}

int config_add_info(config_t *cp, json_object_t *ro) {
	config_section_t *s;
	config_property_t *p;
	config_function_t *f;
	json_array_t *ca,*sa;
	json_object_t *so,*po;
	int addsa;

	if (!cp) return 1;

	ca = json_create_array();
	dprintf(dlevel,"ca: %p\n", ca);
	if (!ca) {
		sprintf(cp->errmsg,"unable to create json array(configuration): %s",strerror(errno));
		return 1;
	}

	list_reset(cp->sections);
	while((s = list_get_next(cp->sections)) != 0) {
		if (s->flags & (CONFIG_FLAG_NOINFO)) continue;
		/* it's an array of objects */
		sa = _get_json_section(ca,s->name);
		if (!sa) {
		        so = json_create_object();
		        if (!so) {
				sprintf(cp->errmsg,"unable to create json section object(%s): %s",s->name,strerror(errno));
				return 1;
			}
		        sa = json_create_array();
		        if (!sa) {
				sprintf(cp->errmsg,"unable to create json section array(%s): %s",s->name,strerror(errno));
				return 1;
			} addsa = 1;
		} else {
			addsa = 0;
		}
		list_reset(s->items);
		while((p = list_get_next(s->items)) != 0) {
			dprintf(dlevel+1,"name: %s, flags: %x\n", p->name, p->flags);
			if (p->flags & (CONFIG_FLAG_NOINFO | CONFIG_FLAG_FILEONLY)) {
//				dprintf(dlevel+1,"NOT adding...\n");
				continue;
			}
//			dprintf(dlevel+1,"adding...\n");
			if (_get_json_prop(sa,p->name)) continue;
			dprintf(dlevel,"adding prop: %s\n", p->name);
			po = prop_to_json(p);
			if (!po) {
				sprintf(cp->errmsg,"unable to create json from property(%s): %s",s->name,strerror(errno));
				return 1;
			}
			json_array_add_object(sa, po);
		}
		if (addsa) {
			json_object_set_array(so, s->name, sa);
			json_array_add_object(ca, so);
		}
	}

	json_object_set_array(ro, "configuration", ca);

	/* Add the functions */
	_addgetset(cp);
	sa = json_create_array();
	list_reset(cp->funcs);
	while((f = list_get_next(cp->funcs)) != 0) {
		if (!f->func) continue;
		so = json_create_object();
		json_object_set_string(so,"name",f->name);
		json_object_set_number(so,"nargs",f->nargs);
		json_array_add_object(sa,so);
	}
	json_object_set_array(ro, "functions", sa);

	return 0;
}

int config_process_request(config_t *cp, char *request, json_object_t *status) {
	json_value_t *v,*vv,*vvv;
	json_object_t *o,*results;
	json_array_t *a;
	config_arg_t newarg;
	int type,i,j,errcode;
        config_function_t *f;
//	char *args[32];
//	int nargs;
	list fargs;

	fargs = 0;
	errcode = 1;
	results = json_create_object();
	dprintf(dlevel,"parsing json data...\n");
	v = json_parse(request);
	if (!v) {
		strcpy(cp->errmsg,"invalid json format");
		goto _process_error;
	}

	/* nargs = 1: Object with function name and array of arguments */
	/* { "func": [ "arg", "argn", ... ] } */
	/* nargs > 1: Object with function name and array of argument arrays */
	/* { "func": [ [ "arg", "argn", ... ], [ "arg", "argn" ], ... ] } */
	type = json_value_get_type(v);
	dprintf(dlevel,"type: %d (%s)\n",type, json_typestr(type));
	if (type != JSON_TYPE_OBJECT) {
		strcpy(cp->errmsg,"invalid json format");
		goto _process_error;
	}
	o = json_value_object(v);
	dprintf(dlevel,"object count: %d\n", o->count);
	for(i=0; i < o->count; i++) {
        	f = config_function_get(cp, o->names[i]);
		if (!f) {
			sprintf(cp->errmsg,"invalid function: %s",o->names[i]);
			goto _process_error;
		}
		fargs = list_create();

		/* Value MUST be array */
		vv = o->values[i];
		type = json_value_get_type(vv);
		dprintf(dlevel,"value[%d]: type: %d (%s)\n", i, type, json_typestr(type));
		if (type != JSON_TYPE_ARRAY) {
			strcpy(cp->errmsg,"invalid json format");
			goto _process_error;
		}

		/* Array values can be strings/objects/arrays */
		a = json_value_array(vv);
		dprintf(dlevel,"array count: %d\n", a->count);
		if (f->nargs != 0 && a->count == 0) {
			sprintf(cp->errmsg,"error: function %s requires %d parameters",f->name,f->nargs);
			goto _process_error;
		}
		for(j=0; j < a->count; j++) {
			vvv = a->items[j];
			type = json_value_get_type(vvv);
			newarg.argc = f->nargs;
			newarg.argv = malloc(newarg.argc * sizeof(char *));
			dprintf(dlevel,"f->nargs: %d\n", f->nargs);
			if (f->nargs == 0) {
				sprintf(cp->errmsg,"function %s takes 0 arguments but %d passed\n", f->name, (int)a->count);
				free(newarg.argv);
				goto _process_error;
			} else if (f->nargs == 1) {
//				char *str;

				/* Strings only */
				if (type != JSON_TYPE_STRING) {
					strcpy(cp->errmsg,"must be array of strings for nargs = 1");
					goto _process_error;
				}
//				str = json_value_get_string(vvv);
//				list_add(fargs,str,strlen(str)+1);
				newarg.argv[0] = json_value_get_string(vvv);
			} else if (f->nargs > 1) {
				json_array_t *aa;
				int k;
//				char *str;

				/* Arrays */
				if (type != JSON_TYPE_ARRAY) {
					strcpy(cp->errmsg,"internal error: must be array of arrays for nargs > 1");
					goto _process_error;
				}
				aa = json_value_array(vvv);

				dprintf(dlevel,"count / nargs: %f\n", aa->count % f->nargs);
				if ((aa->count % f->nargs) != 0) {
					sprintf(cp->errmsg,"function %s takes %d arguments but %d passed\n",
						f->name, f->nargs, (int)aa->count);
					free(newarg.argv);
					goto _process_error;
				}

				/* Strings */
//				nargs = 0;
				dprintf(dlevel,"aa->count: %d\n", aa->count);
				for(k=0; k < aa->count; k++) {
					if (json_value_get_type(aa->items[k]) != JSON_TYPE_STRING) {
						strcpy(cp->errmsg,"must be array of string arrays for nargs > 2");
						goto _process_error;
					}
//					str = json_value_get_string(aa->items[k]);
//					args[nargs++] = str;
					newarg.argv[k] = json_value_get_string(aa->items[k]);
				}
//				args[nargs] = 0;
//				list_add(fargs,args,sizeof(args));
			}
			list_add(fargs,&newarg,sizeof(newarg));
		}
		dprintf(dlevel,"fargs count: %d\n", list_count(fargs));
		if (f->func(f->ctx, fargs, cp->errmsg, results)) goto _process_error;
	}

	errcode = 0;
	strcpy(cp->errmsg,"success");

_process_error:
	if (fargs) list_destroy(fargs);
	json_destroy_value(v);
        json_object_set_number(status,"status",errcode);
        json_object_set_string(status,"message",cp->errmsg);
	dprintf(dlevel,"results->count: %d\n", results->count);
//	if (results->count) json_object_set_object(status,"results",results);
//	else json_destroy_object(results);
	json_object_set_object(status,"results",results);
	dprintf(dlevel,"errcode: %d\n", errcode);
	return errcode;
}

#ifdef JS
#include "jsobj.h"
#include "jsstr.h"
#include "jsatom.h"
#include "jsfun.h"
#include "jsarray.h"
#include "jsdbgapi.h"
#include "jsinterp.h"
#include "jsscript.h"
#include "jsjson.h"

static js_config_func_ctx_t *_js_config_get_func_ctx(JSContext *cx, jsval func, char *name) {
	js_config_func_ctx_t newctx;

	memset(&newctx,0,sizeof(newctx));
	newctx.cx = cx;
	newctx.func = func;
	strncpy(newctx.name,name,sizeof(newctx.name));
	if (!js_ctxs) js_ctxs = list_create();
	return list_add(js_ctxs,&newctx,sizeof(newctx));
}

static JSObject *js_config_property_new(JSContext *cx, JSObject *parent, config_property_t *p);

JSPropertySpec *js_config_to_props(config_t *cp, JSContext *cx, char *name, JSPropertySpec *add) {
	JSPropertySpec *props,*pp,*app;
	config_section_t *section;
	config_property_t *p;
	int count,size;

	if (!cp) return 0;

	section = config_get_section(cp,name);
	if (!section) {
		sprintf(cp->errmsg,"unable to find config section: %s\n", name);
		return 0;
	}

	/* Get total count */
	count = list_count(section->items);
	if (add) for(app = add; app->name; app++) count++;
	/* Add 1 for termination */
	count++;
	size = count * sizeof(JSPropertySpec);
	dprintf(dlevel,"count: %d, size: %d\n", count, size);
	props = malloc(size);
	if (!props) {
		sprintf(cp->errmsg,"malloc props(%d): %s\n", size, strerror(errno));
		return 0;
	}
	memset(props,0,size);

	dprintf(dlevel,"adding config...\n");
	pp = props;
	list_reset(section->items);
	while((p = list_get_next(section->items)) != 0) {
//		_config_dump_prop(p);
//		if (p->flags & CONFIG_FLAG_FILEONLY) continue;
		pp->name = p->name;
		pp->tinyid = p->id;
		pp->flags = JSPROP_ENUMERATE;
		if (p->flags & CONFIG_FLAG_READONLY) pp->flags |= JSPROP_READONLY;
		pp++;
	}

	/* Now add the addl props, if any */
	if (add) for(app = add; app->name; app++) *pp++ = *app;

	dprintf(dlevel+1,"props dump:\n");
	for(pp = props; pp->name; pp++) 
		dprintf(dlevel+1,"prop: name: %s, id: %d, flags: %02x\n", pp->name, pp->tinyid, pp->flags);

	dprintf(dlevel,"building map...\n");
	config_build_propmap(cp);

	return props;
}

static char *js_string(JSContext *cx, jsval val) {
        JSString *str;

        str = JS_ValueToString(cx, val);
        if (!str) return JS_FALSE;
        return JS_EncodeString(cx, str);
}

static int _jscallfunc(config_t *cp, config_function_t *f, JSContext *cx, int argc, jsval *argv, jsval *rval) {
	char errmsg[128];
	json_object_t *results;
	register char *str;
	list l;
	int i,j,r;
	float m;

	results = json_create_object();
	dprintf(dlevel,"results: %p\n", results);
	if (!results) {
		JS_ReportError(cx,"internal error: unable to create JSON object");
		return JS_FALSE;
	}

	/* argc must be multiple of nargs */
	dprintf(dlevel,"nargs: %d, argc: %d\n", f->nargs, argc);
	if (f->nargs) {
		m = argc % f->nargs;
		dprintf(dlevel,"m: %f\n", m);
		if ((argc % f->nargs) != 0) {
			JS_ReportError(cx,"function %s takes %d arguments but %d passed", f->name, f->nargs, argc);
			json_destroy_object(results);
			return JS_FALSE;
		}
	}
	l = list_create();
	dprintf(dlevel,"l: %p\n", l);
	if (!l) {
		JS_ReportError(cx, "_jscallfunc: error creating argslist");
		json_destroy_object(results);
		return JS_FALSE;
	}

	i = 0;
	while(i < argc) {
		dprintf(dlevel,"i: %d, argc: %d\n", i, argc);
		if (f->nargs == 1) {
			/* list of char * */
			str = js_string(cx,argv[i++]);
			dprintf(dlevel,"str: %s\n", str);
			list_add(l,str,0);
		} else if (f->nargs > 1) {
			char *args[32];
			int nargs;

			/* list of char ** */
			nargs = 0;
			for(j=0; j < f->nargs && i < argc; j++) {
				str = js_string(cx,argv[i++]);
				dprintf(dlevel,"str: %s\n", str);
				args[nargs++] = str;
			}
			args[nargs] = 0;
			list_add(l,args,0);
		}
	}

	/* make the call */
	dprintf(dlevel,"func: %p\n", f->func);
	if (!f->func) {
		JS_ReportError(cx,"_jscallfunc: func %s is null", f->name);
		r = 1;
	} else {
		r = f->func(f->ctx, l, errmsg, results);
	}
	dprintf(dlevel,"r: %d\n", r);

	/* free the js_strings */
	list_reset(l);
	if (f->nargs == 1) {
		while((str = list_get_next(l)) != 0)
			JS_free(cx,str);
	} else if (f->nargs > 1) {
		char **args;

		while((args = list_get_next(l)) != 0) {
			for(i=0; i < f->nargs; i++)
				JS_free(cx,args[i]);
		}
	}
	list_destroy(l);

	/* turn results into object */
	dprintf(dlevel,"results->count: %d\n", results->count);
	if (results->count) {
		char *j;
		JSString *str;
		jsval rootVal;
		JSONParser *jp;
		jsval reviver = JSVAL_NULL;
		JSBool ok;

		/* Convert from C JSON type to JS JSON type */
		j = json_dumps(json_object_value(results),0);
		if (!j) {
			JS_ReportError(cx, "unable to create JSON data\n");
			return JS_FALSE;
		}
		dprintf(dlevel,"j: %p\n", j);
		jp = js_BeginJSONParse(cx, &rootVal);
		dprintf(dlevel,"jp: %p\n", jp);
		if (!jp) {
			JS_ReportError(cx, "unable init JSON parser\n");
			free(j);
			return JS_FALSE;
		}
		str = JS_NewStringCopyZ(cx,j);
		ok = js_ConsumeJSONText(cx, jp, JS_GetStringChars(str), JS_GetStringLength(str));
		dprintf(dlevel,"ok: %d\n", ok);
		ok = js_FinishJSONParse(cx, jp, reviver);
		dprintf(dlevel,"ok: %d\n", ok);
		free(j);
		*rval = rootVal;
	} else {
		*rval = JSVAL_VOID;
	}
	json_destroy_object(results);
	return (r ? JS_FALSE : JS_TRUE);
}

/* This function must be called by the agent/client to get the cp from session private */
JSBool js_config_callfunc(config_t *cp, JSContext *cx, uintN argc, jsval *vp) {
	JSFunction *fun;
	char *name;
	config_function_t *f;

	/* Get the name of the function */
	fun = JS_ValueToFunction(cx, vp[0]);
	dprintf(dlevel,"fun: %p\n", fun);
	if (!fun) {
		JS_ReportError(cx, "js_config_callfunc: internal error: funcptr is null!");
		return JS_FALSE;
	}
	name = JS_EncodeString(cx, ATOM_TO_STRING(fun->atom));
	dprintf(dlevel,"name: %s\n", name);
	if (!name) {
		JS_ReportError(cx, "js_config_callfunc: internal error: name is null!");
		return JS_FALSE;
	}

	/* find the func */
	dprintf(dlevel,"count: %d\n", list_count(cp->funcs));
	list_reset(cp->funcs);
	while((f = list_get_next(cp->funcs)) != 0) {
		dprintf(dlevel,"f->name: %s, name: %s\n", f->name, name);
		if (strcmp(f->name, name) == 0) {
			jsval *jsargv = vp + 2;

			JS_free(cx,name);
			return _jscallfunc(cp, f, cx, argc, jsargv, vp);
		}
	}

	JS_ReportError(cx, "function not found");
	JS_free(cx,name);
	return JS_FALSE;
}

JSFunctionSpec *js_config_to_funcs(config_t *cp, JSContext *cx, js_config_callfunc_t *func, JSFunctionSpec *add) {
	JSFunctionSpec *funcs,*pp,*app;
	config_function_t *f;
	int count,size;

	dprintf(dlevel,"cp: %p\n", cp);
	if (!cp) return 0;

	/* Get total count */
	count = list_count(cp->funcs);
	dprintf(dlevel,"count: %d\n", count);
	if (add) for(app = add; app->name; app++) count++;
	/* Add 1 for termination */
	count++;
	size = count * sizeof(JSFunctionSpec);
	dprintf(dlevel,"count: %d, size: %d\n", count, size);
	funcs = JS_malloc(cx,size);
	if (!funcs) {
		sprintf(cp->errmsg,"calloc funcs(%d): %s\n", size, strerror(errno));
		return 0;
	}
	memset(funcs,0,size);

	dprintf(dlevel,"adding config...\n");
	pp = funcs;
	list_reset(cp->funcs);
	while((f = list_get_next(cp->funcs)) != 0) {
		pp->name = f->name;
		pp->call = (JSNative)func;
		pp->nargs = f->nargs;
		pp->flags = JSFUN_FAST_NATIVE | JSFUN_STUB_GSOPS;
		pp->extra = f->nargs << 16;
		pp++;
	}

	/* Now add the addl funcs, if any */
	if (add) for(app = add; app->name; app++) *pp++ = *app;

	dprintf(dlevel,"dump:\n");
	for(pp = funcs; pp->name; pp++) 
		dprintf(dlevel,"func: name: %s, call: %p, nargs: %d, flags: %x, extra: %x\n",
			pp->name, pp->call, pp->nargs, pp->flags, pp->extra);

	return funcs;
}

JSBool js_config_common_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval, config_t *cp, char *sname) {
	config_property_t *p;

	if (!cp) return JS_TRUE;

	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	p = 0;
	if(JSVAL_IS_INT(id)) {
		int prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		p = CONFIG_GETMAP(cp,prop_id);
		if (!p) p = config_get_propbyid(cp,prop_id);
		if (!p) {
			JS_ReportError(cx, "internal error: property %d not found", prop_id);
			return JS_FALSE;
		}
	} else if (JSVAL_IS_STRING(id)) {
		char *name;

		/* if sname not specified try to derive it from the object */
		dprintf(dlevel,"sname: %p\n", sname);
		if (!sname) {
			JSClass *classp = OBJ_GET_CLASS(cx, obj);
			sname = (char *)classp->name;
			dprintf(dlevel,"NEW sname: %p\n", sname);
		}

		name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
		dprintf(dlevel,"name: %s\n", name);
		if (name) {
			p = config_get_property(cp, sname, name);
			JS_free(cx,name);
		}
	}
	if (p && p->dest) {
		dprintf(dlevel,"p->name: %s, type: %s\n", p->name, typestr(p->type));
		*rval = type_to_jsval(cx,p->type,p->dest,p->len);
	}
	return JS_TRUE;
}

JSBool js_config_property_set_value(config_property_t *p, JSContext *cx, jsval val,bool trig) {
	bool b;
	int type,i;
	double d;
	char *s;

	dprintf(dlevel,"name: %s, val: %x\n", p->name, val);

	if (!val) val = JSVAL_VOID;
	type = JS_TypeOfValue(cx, val);
	dprintf(dlevel,"type: %d (%s)\n", type, jstypestr(cx,val));
	switch(type) {
	case JSTYPE_STRING:
		jsval_to_type(DATA_TYPE_STRINGP,&s,0,cx,val);
		config_property_set_value(p,DATA_TYPE_STRING,s,strlen(s),true,trig);
		if (s) JS_free(cx,s);
		break;
	case JSTYPE_NUMBER:
		if (JSVAL_IS_INT(val)) {
			jsval_to_type(DATA_TYPE_INT,&i,0,cx,val);
			config_property_set_value(p,DATA_TYPE_INT,&i,0,true,trig);
		} else {
			jsval_to_type(DATA_TYPE_DOUBLE,&d,0,cx,val);
			config_property_set_value(p,DATA_TYPE_DOUBLE,&d,0,true,trig);
		}
		break;
	case JSTYPE_BOOLEAN:
		jsval_to_type(DATA_TYPE_BOOLEAN,&b,0,cx,val);
		config_property_set_value(p,DATA_TYPE_BOOLEAN,&b,0,true,trig);
		break;
	case JSTYPE_VOID:
		i = p->dsize;
		if (!i) i = typesize(p->type);
		dprintf(dlevel,"p->dest: %p, i: %d\n", p->dest, i);
		if (p->dest && i) memset(p->dest,0,i);
		break;
	case JSTYPE_OBJECT:
		if (OBJ_IS_DENSE_ARRAY(cx,JSVAL_TO_OBJECT(val))) {
#if 0
			char value[2048];
			jsval_to_type(DATA_TYPE_STRING, value, sizeof(value), cx, val);
			config_property_set_value(p,DATA_TYPE_STRING,value,sizeof(value),true,trig);
#else
			jsval_to_type(p->type, p->dest, p->len, cx, val);
			p->flags |= CONFIG_FLAG_VALUE;
			p->dirty = true;
#endif
		} else {
			JS_ReportError(cx,"property_set_value only supports array objects");
			return JS_FALSE;
		}
		break;
	default:
		log_error("js_config_property_set_value: unhandled switch type: %d (%s)\n", type, jstypestr(cx,val));
		JS_ReportError(cx,"internal error: unhandled switch type in js_config_property_set_value");
		return JS_FALSE;
	}
	return JS_TRUE;
}

JSBool js_config_common_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp, config_t *cp, char *sname) {
	config_property_t *p;

	int ldlevel = dlevel;

	dprintf(ldlevel,"cp: %p\n", cp);
	if (!cp) return JS_TRUE;

	dprintf(ldlevel,"id type: %s\n", jstypestr(cx,id));
	p = 0;
	if(JSVAL_IS_INT(id)) {
		int prop_id = JSVAL_TO_INT(id);
		dprintf(ldlevel,"prop_id: %d\n", prop_id);
		p = CONFIG_GETMAP(cp,prop_id);
		dprintf(ldlevel,"p: %p\n", p);
		if (!p) p = config_get_propbyid(cp,prop_id);
		dprintf(ldlevel,"p: %p\n", p);
		if (!p) {
			JS_ReportError(cx, "js_config_common_setprop: internal error: property %d not found", prop_id);
			config_dump(cp);
			return JS_FALSE;
		}
	} else if (JSVAL_IS_STRING(id)) {
		char *name;

		/* if sname not specified try to derive it from the object */
		dprintf(dlevel,"sname: %p\n", sname);
		if (!sname) {
			JSClass *classp = OBJ_GET_CLASS(cx, obj);
			sname = (char *)classp->name;
			dprintf(dlevel,"NEW sname: %p\n", sname);
		}

		name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
		dprintf(dlevel,"name: %s\n", name);
		if (name) {
			p = config_get_property(cp, sname, name);
			JS_free(cx,name);
		}
	}
	dprintf(ldlevel,"p: %p\n", p);
	if (p) return js_config_property_set_value(p,cx,*vp,cp->triggers);

	return JS_TRUE;
}

static int _js_config_trigger(void *_ctx, config_property_t *p, void *old_value) {
	js_config_func_ctx_t *ctx = _ctx;
	JSContext *cx = ctx->cx;
	JSBool ok;
	jsval rval;
	jsval argv[3];
//	const char *fname;

	int ldlevel = dlevel;

	dprintf(ldlevel,"p->flags: %x, IN_TRIG\n", p->flags, CONFIG_FLAG_IN_TRIG);
	if (check_bit(p->flags, CONFIG_FLAG_IN_TRIG)) {
		JS_ReportError(cx,"nested trigger failed on property: %s", ctx->name);
		return 1;
	}

	argv[0] = (p->arg ? p->arg : JSVAL_VOID);
	argv[1] = OBJECT_TO_JSVAL(js_config_property_new(cx, JS_GetGlobalObject(cx), p));
	if (old_value)
		argv[2] = type_to_jsval(cx,p->type,old_value,p->len ? p->len : p->dsize);
	else
		argv[2] = JSVAL_VOID;

	set_bit(p->flags, CONFIG_FLAG_IN_TRIG);
	dprintf(ldlevel,"func: %x\n", ctx->func);
//	fname = JS_GetFunctionName(js_ValueToFunction(cx, &ctx->func, 0));
//	if (!fname) fname = JS_strdup(ctx->cx,"unknown");
//	dprintf(ldlevel,"calling function: %s\n", fname);
	ok = JS_CallFunctionValue(cx, JS_GetGlobalObject(cx), ctx->func, 3, argv, &rval);
	clear_bit(p->flags, CONFIG_FLAG_IN_TRIG);
	JS_ReportPendingException(cx);
	dprintf(ldlevel,"call ok: %d\n", ok);
	if (!ok) {
//		log_error("calling trigger function %s failed\n", fname);
		log_error("calling trigger function failed\n");
		return 1;
	}

	dprintf(ldlevel,"rval: %x (%s)\n", rval, jstypestr(cx,rval));
	if (rval != JSVAL_VOID) {
		int status;

		JS_ValueToInt32(cx,rval,&status);
		dprintf(ldlevel,"status: %d\n", status);
		if (status != 0) {
			JS_ReportError(cx,"trigger function for property %s returned %d", ctx->name, status);
			return 1;
		}
	}
	return 0;
}

int js_config_property_set_trigger(JSContext *cx, config_property_t *p, jsval func, jsval arg) {

	int ldlevel = dlevel;

	dprintf(ldlevel,"name: %s, func: %lx, arg: %lx\n", p->name, func, arg);

	p->trigger = _js_config_trigger;
	p->ctx = _js_config_get_func_ctx(cx, func, p->name);
	p->arg = arg;

	/* Root the function */
	dprintf(ldlevel,"adding func root: %s\n", p->name);
	JS_EngineAddRoot(cx, p->name, &((js_config_func_ctx_t *)p->ctx)->func);

	/* Root the arg */
	if (arg) {
		char temp[128];
		snprintf(temp,sizeof(temp),"%s_arg",p->name);
		dprintf(ldlevel,"adding arg root: %s\n", temp);
		JS_EngineAddRoot(cx, temp, &p->arg);
	}
	dprintf(dlevel,"done!\n");
	return 0;
}

enum CONFIG_PROPERTY_PROPERTY_ID {
	CONFIG_PROPERTY_PROPERTY_ID_NAME=1,
	CONFIG_PROPERTY_PROPERTY_ID_VALUE,
	CONFIG_PROPERTY_PROPERTY_ID_TYPE,
	CONFIG_PROPERTY_PROPERTY_ID_DSIZE,
	CONFIG_PROPERTY_PROPERTY_ID_DEFAULT,
	CONFIG_PROPERTY_PROPERTY_ID_FLAGS,
	CONFIG_PROPERTY_PROPERTY_ID_SCOPE,
	CONFIG_PROPERTY_PROPERTY_ID_VALUES,
	CONFIG_PROPERTY_PROPERTY_ID_LABELS,
	CONFIG_PROPERTY_PROPERTY_ID_UNITS,
	CONFIG_PROPERTY_PROPERTY_ID_SCALE,
	CONFIG_PROPERTY_PROPERTY_ID_PRECISION,
	CONFIG_PROPERTY_PROPERTY_ID_LEN,
	CONFIG_PROPERTY_PROPERTY_ID_ID,
	CONFIG_PROPERTY_PROPERTY_ID_DIRTY,
	CONFIG_PROPERTY_PROPERTY_ID_TRIGGER,
	CONFIG_PROPERTY_PROPERTY_ID_ARG,
};

static JSBool js_config_property_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	config_property_t *p;
	int prop_id;

	p = JS_GetPrivate(cx,obj);
	if (!p) {
		JS_ReportError(cx, "config_property_setprop: private is null!");
		return JS_FALSE;
	}

	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case CONFIG_PROPERTY_PROPERTY_ID_NAME:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,p->name));
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_VALUE:
			*rval = type_to_jsval(cx, p->type, p->dest, p->len);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_TYPE:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,typestr(p->type)));
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_DSIZE:
			*rval = INT_TO_JSVAL(p->dsize);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_DEFAULT:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,p->def));
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_FLAGS:
			*rval = INT_TO_JSVAL(p->flags);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_SCOPE:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,p->scope));
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_VALUES:
//			*rval = type_to_jsval(cx, p->type | DATA_TYPE_ARRAY, p->values, p->nvalues);
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,p->values));
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_LABELS:
//			*rval = type_to_jsval(cx, DATA_TYPE_STRING_ARRAY, p->labels, p->nlabels);
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,p->labels));
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_UNITS:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,p->units));
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_SCALE:
			JS_NewDoubleValue(cx, p->scale, rval);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_PRECISION:
			*rval = INT_TO_JSVAL(p->precision);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_LEN:
			*rval = INT_TO_JSVAL(p->len);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_ID:
			*rval = INT_TO_JSVAL(p->id);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_DIRTY:
			*rval = BOOLEAN_TO_JSVAL(p->dirty);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_TRIGGER:
			if (p->trigger == _js_config_trigger) {
				*rval = p->ctx ? ((js_config_func_ctx_t *)p->ctx)->func : JSVAL_VOID;
			} else {
				*rval = JSVAL_VOID;
			}
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_ARG:
			*rval = p->arg;
			break;
		default:
			JS_ReportError(cx, "property not found");
			return JS_FALSE;
		}
	}
	return JS_TRUE;
}

static JSBool js_config_property_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	config_property_t *p;
	int prop_id;

	p = JS_GetPrivate(cx,obj);
	if (!p) {
		JS_ReportError(cx, "config_property_setprop: private is null!");
		return JS_FALSE;
	}

	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
#if 0
		case CONFIG_PROPERTY_PROPERTY_ID_NAME:
			jsval_to_type(DATA_TYPE_STRING,&p->name,sizeof(p->name)-1,cx,*vp);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_TYPE:
			jsval_to_type(DATA_TYPE_S32,&p->type,0,cx,*vp);
			break;
#endif
		case CONFIG_PROPERTY_PROPERTY_ID_VALUE:
			js_config_property_set_value(p,cx,*vp,true);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_DSIZE:
			jsval_to_type(DATA_TYPE_S32,&p->dsize,0,cx,*vp);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_DEFAULT:
			jsval_to_type(DATA_TYPE_STRINGP,&p->def,0,cx,*vp);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_FLAGS:
			jsval_to_type(DATA_TYPE_U16,&p->flags,0,cx,*vp);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_SCOPE:
			jsval_to_type(DATA_TYPE_STRINGP,&p->scope,0,cx,*vp);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_VALUES:
			jsval_to_type(DATA_TYPE_STRINGP,&p->values,0,cx,*vp);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_LABELS:
			jsval_to_type(DATA_TYPE_STRINGP,&p->labels,0,cx,*vp);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_UNITS:
			jsval_to_type(DATA_TYPE_STRINGP,&p->units,0,cx,*vp);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_SCALE:
			jsval_to_type(DATA_TYPE_FLOAT,&p->scale,0,cx,*vp);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_PRECISION:
			jsval_to_type(DATA_TYPE_S32,&p->precision,0,cx,*vp);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_DIRTY:
			jsval_to_type(DATA_TYPE_BOOLEAN,&p->dirty,0,cx,*vp);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_TRIGGER:
			js_config_property_set_trigger(cx,p,*vp,0);
			break;
		case CONFIG_PROPERTY_PROPERTY_ID_ARG:
			p->arg = *vp;
			break;
		default:
			break;
		}
	}
	return JS_TRUE;
}

static JSClass js_config_property_class = {
	"ConfigProperty",	/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	js_config_property_getprop,	/* getProperty */
	js_config_property_setprop,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool js_config_property_dump(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	config_property_t *p;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	p = JS_GetPrivate(cx, obj);
	if (!p) return JS_FALSE;

	config_dump_property(p,0);
	return JS_TRUE;
}

static JSObject *js_config_property_new(JSContext *cx, JSObject *parent, config_property_t *p) {
	JSObject *newobj;

	/* Create the new object */
	newobj = JS_NewObject(cx, &js_config_property_class, 0, parent);
	if (!newobj) return 0;

	JS_SetPrivate(cx,newobj,p);
	return newobj;
}

static JSBool js_config_property_ctor(JSContext *cx, JSObject *parent, uintN argc, jsval *argv, jsval *rval) {
	char *name, *def;
	int type, flags;
	config_property_t *p;
	JSObject *newobj;

	name = def = 0;
	type = flags = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "s u / s u", &name, &type, &def, &flags)) return JS_FALSE;
	dprintf(dlevel,"name: %s, type: %d, def: %s, flags: %x\n", name, type, def, flags);

	p = config_new_property(0,name,type,0,0,0,def,flags,0,0,0,0,1,0);

	newobj = js_config_property_new(cx,parent,p);
	*rval = OBJECT_TO_JSVAL(newobj);

	return JS_TRUE;
}

static JSObject *js_InitConfigPropertyClass(JSContext *cx, JSObject *parent) {
	JSPropertySpec props[] = {
		{ "name",CONFIG_PROPERTY_PROPERTY_ID_NAME,JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "type",CONFIG_PROPERTY_PROPERTY_ID_TYPE,JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "value",CONFIG_PROPERTY_PROPERTY_ID_VALUE,JSPROP_ENUMERATE },
		{ "dsize",CONFIG_PROPERTY_PROPERTY_ID_DSIZE,JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "default",CONFIG_PROPERTY_PROPERTY_ID_DEFAULT,JSPROP_ENUMERATE },
		{ "flags",CONFIG_PROPERTY_PROPERTY_ID_FLAGS,JSPROP_ENUMERATE },
		{ "scope",CONFIG_PROPERTY_PROPERTY_ID_SCOPE,JSPROP_ENUMERATE },
		{ "values",CONFIG_PROPERTY_PROPERTY_ID_VALUES,JSPROP_ENUMERATE },
		{ "labels",CONFIG_PROPERTY_PROPERTY_ID_LABELS,JSPROP_ENUMERATE },
		{ "units",CONFIG_PROPERTY_PROPERTY_ID_UNITS,JSPROP_ENUMERATE },
		{ "scale",CONFIG_PROPERTY_PROPERTY_ID_SCALE,JSPROP_ENUMERATE },
		{ "precision",CONFIG_PROPERTY_PROPERTY_ID_PRECISION,JSPROP_ENUMERATE },
		{ "len",CONFIG_PROPERTY_PROPERTY_ID_LEN,JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "id",CONFIG_PROPERTY_PROPERTY_ID_ID,JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "dirty",CONFIG_PROPERTY_PROPERTY_ID_DIRTY,JSPROP_ENUMERATE },
		{ "trigger",CONFIG_PROPERTY_PROPERTY_ID_TRIGGER,JSPROP_ENUMERATE },
		{ "arg",CONFIG_PROPERTY_PROPERTY_ID_ARG,JSPROP_ENUMERATE },
		{ 0 }
	};
	JSFunctionSpec funcs[] = {
		JS_FN("dump",js_config_property_dump,0,0,0),
		{ 0 }
	};
	JSObject *obj;

	dprintf(2,"defining %s object\n",js_config_property_class.name);
	obj = JS_InitClass(cx, parent, 0, &js_config_property_class, js_config_property_ctor, 2, props, funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s class", js_config_property_class.name);
		return 0;
	}
	dprintf(dlevel,"done!\n");
	return obj;
}

enum CONFIG_SECTION_PROPERTY_ID {
	CONFIG_SECTION_PROPERTY_ID_NAME,
	CONFIG_SECTION_PROPERTY_ID_ITEMS,
};

static JSObject *js_create_property_array(JSContext *cx, JSObject *parent, list l) {
	JSObject *aobj;
	int i;
	config_property_t *p;
	jsval val;

	dprintf(dlevel,"count: %d\n", list_count(l));
	aobj = JS_NewArrayObject(cx, 0, NULL);
	dprintf(dlevel,"aobj: %p\n", aobj);
	if (!aobj) return 0;
	i = 0;
	list_reset(l);
	while( (p = list_next(l)) != 0) {
#if 0
		dprintf(dlevel,"p->jsval: %x\n", p->jsval);
		if (p->jsval == JSVAL_NULL) {
			p->jsval = OBJECT_TO_JSVAL(js_config_property_new(cx,parent,p));
			dprintf(dlevel,"NEW p->jsval: %x\n", p->jsval);
		}
		JS_SetElement(cx, aobj, i++, &p->jsval);
#endif
		val = OBJECT_TO_JSVAL(js_config_property_new(cx,parent,p));
		JS_SetElement(cx,aobj,i++,&val);
	}
	return aobj;
}

static JSBool js_config_section_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	config_section_t *sec;
	int prop_id;

	sec = JS_GetPrivate(cx,obj);
	if (!sec) {
		JS_ReportError(cx, "js_config_section_getprop: private is null!");
		return JS_FALSE;
	}

	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case CONFIG_SECTION_PROPERTY_ID_NAME:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,sec->name));
			break;
		case CONFIG_SECTION_PROPERTY_ID_ITEMS:
			if (list_updated(sec->items) != sec->items_updated) {
				sec->jsval = OBJECT_TO_JSVAL(js_create_property_array(cx,obj,sec->items));
				sec->items_updated = list_updated(sec->items);
			}
			*rval = sec->jsval;
			break;
		default:
			JS_ReportError(cx, "property not found");
			return JS_FALSE;
		}
#if 0
        } else if (JSVAL_IS_STRING(id)) {
		char *sname, *name;
		JSClass *classp = OBJ_GET_CLASS(cx, obj);

		sname = (char *)classp->name;
		name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
		dprintf(0,"sname: %s, name: %s\n", sname, name);
		if (name) JS_free(cx,name);
#endif
	}
	return JS_TRUE;
}

static JSBool js_config_section_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	config_section_t *sec;
	int prop_id;

	sec = JS_GetPrivate(cx,obj);
	if (!sec) {
		JS_ReportError(cx, "js_config_section_getprop: private is null!");
		return JS_FALSE;
	}
	dprintf(dlevel,"sec: %p\n", sec);

	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case CONFIG_SECTION_PROPERTY_ID_ITEMS:
			{
				JSObject *arr;
				unsigned int count,i;
				jsval val;
				JSObject *pobj;
//				char *name;
				config_property_t *p;

				arr = JSVAL_TO_OBJECT(*vp);
				dprintf(dlevel,"arr: %p\n", arr);
				if (!JS_IsArrayObject(cx,arr)) {
					JS_ReportError(cx,"_js_config_section_setprop: items must be an array");
					return JS_FALSE;
				}
				if (!js_GetLengthProperty(cx, arr, &count)) {
					JS_ReportError(cx,"_js_config_section_setprop: unable to get items array length");
					return JS_FALSE;
				}
				dprintf(dlevel,"count: %d\n", count);
				list_destroy(sec->items);
				sec->items = list_create();
				for(i=0; i < count; i++) {
					JS_GetElement(cx, arr, i, &val);
					dprintf(dlevel,"val[%d]: %x, isobj: %d\n", i, val, JSVAL_IS_OBJECT(val));
					if (!JSVAL_IS_OBJECT(val)) {
						log_warning("%s item %d is not a %s object, ignoring...\n",
							__FUNCTION__,i,js_config_property_class.name);
						continue;
					}
					pobj = JSVAL_TO_OBJECT(val);
					dprintf(dlevel,"pobj: %p\n",pobj);
#if 0
					name = JS_GetObjectName(cx,pobj);
					dprintf(dlevel,"name: %s\n", name);
					if (strcmp(name,js_config_property_class.name) != 0) {
						log_warning("%s item %d is not a %s object, ignoring...\n",
							__FUNCTION__,i,js_config_property_class.name);
						continue;
					}
#endif
					p = JS_GetPrivate(cx,pobj);
					if (!p) {
						log_warning("%s item %d private is null?\n",
							__FUNCTION__,i);
						continue;
					}
					list_add(sec->items,p,0);
//					jsval_to_type(type, dest, size, cx, val);
				}
			}
			break;
		default:
			JS_ReportError(cx, "property not found");
			return JS_FALSE;
		}
#if 0
        } else if (JSVAL_IS_STRING(id)) {
		char *sname, *name;
		JSClass *classp = OBJ_GET_CLASS(cx, obj);

		sname = (char *)classp->name;
		name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
		dprintf(0,"sname: %s, name: %s\n", sname, name);
		if (name) JS_free(cx,name);
#endif
	}
	return JS_TRUE;
}

static JSClass js_config_section_class = {
	"ConfigSection",	/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	js_config_section_getprop, /* getProperty */
	js_config_section_setprop, /* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *js_config_section_new(JSContext *cx, JSObject *parent, config_section_t *sec) {
	JSObject *newobj;

	/* Create the new object */
	newobj = JS_NewObject(cx, &js_config_section_class, 0, parent);
	if (!newobj) return 0;

	JS_SetPrivate(cx,newobj,sec);
	return newobj;
}

static JSBool js_config_section_ctor(JSContext *cx, JSObject *parent, uintN argc, jsval *argv, jsval *rval) {
	char *name;
	int flags;
	config_section_t *s;
	JSObject *newobj;

	name = 0;
	flags = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "s / u", &name, &flags)) return JS_FALSE;
	dprintf(dlevel,"name: %s, flags: %x\n", name, flags);

	s = config_new_section(name,flags);

	newobj = js_config_section_new(cx,parent,s);
	*rval = OBJECT_TO_JSVAL(newobj);

	return JS_TRUE;
}

static JSObject *js_InitConfigSectionClass(JSContext *cx, JSObject *parent) {
	JSPropertySpec props[] = {
		{ "name",CONFIG_SECTION_PROPERTY_ID_NAME,JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "items",CONFIG_SECTION_PROPERTY_ID_ITEMS,JSPROP_ENUMERATE },
		{ 0 }
	};
	JSFunctionSpec funcs[] = {
//		JS_FN("dump",js_config_property_dump,0,0,0),
		{ 0 }
	};
	JSObject *obj;

	dprintf(2,"defining %s object\n",js_config_property_class.name);
	obj = JS_InitClass(cx, parent, 0, &js_config_section_class, js_config_section_ctor, 1, props, funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s class", js_config_section_class.name);
		return 0;
	}
	dprintf(dlevel,"done!\n");
	return obj;
}

enum CONFIG_PROPERTY_ID {
	CONFIG_PROPERTY_ID_SECTIONS,
	CONFIG_PROPERTY_ID_FILENAME,
	CONFIG_PROPERTY_ID_FILE_FORMAT,
	CONFIG_PROPERTY_ID_ERRMSG,
	CONFIG_PROPERTY_ID_TRIG,
	CONFIG_PROPERTY_ID_FUNCS,
};

static JSBool js_config_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *rval) {
	config_t *cp;
	int prop_id;

	cp = JS_GetPrivate(cx,obj);
	if (!cp) return JS_FALSE;
	dprintf(dlevel,"id type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case CONFIG_PROPERTY_ID_SECTIONS:
			{
				config_section_t *sec;
				JSObject *rows;
				int i;

				rows = JS_NewArrayObject(cx, 0, NULL);
				i = 0;
				list_reset(cp->sections);
				while( (sec = list_next(cp->sections)) != 0) {
					if (sec->jsval == JSVAL_NULL) sec->jsval = OBJECT_TO_JSVAL(js_config_section_new(cx,obj,sec));
					JS_SetElement(cx, rows, i++, &sec->jsval);
				}
				*rval = OBJECT_TO_JSVAL(rows);
			}
                        break;
		case CONFIG_PROPERTY_ID_FILENAME:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,cp->filename));
			break;
		case CONFIG_PROPERTY_ID_FILE_FORMAT:
			*rval = INT_TO_JSVAL(cp->file_format);
			break;
		case CONFIG_PROPERTY_ID_ERRMSG:
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,cp->errmsg));
			break;
		case CONFIG_PROPERTY_ID_TRIG:
			*rval = BOOLEAN_TO_JSVAL(cp->triggers);
			break;
		case CONFIG_PROPERTY_ID_FUNCS:
			{
				config_function_t *f;
				JSObject *rows;
				jsval node;
				int i;

				rows = JS_NewArrayObject(cx, 0, NULL);
				i = 0;
				list_reset(cp->funcs);
				while( (f = list_next(cp->funcs)) != 0) {
//					if (!f->jsval) f->jsval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,cp->errmsg));
//					node = f->jsval;
					node = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,f->name));
					JS_SetElement(cx, rows, i++, &node);
				}
				*rval = OBJECT_TO_JSVAL(rows);
			}
//			*rval = JSVAL_VOID;
                        break;
		default:
			JS_ReportError(cx, "property not found");
			return JS_FALSE;
		}
        } else if (JSVAL_IS_STRING(id)) {
		char *sname, *name;
		JSClass *classp = OBJ_GET_CLASS(cx, obj);

		sname = (char *)classp->name;
		name = JS_EncodeString(cx, JSVAL_TO_STRING(id));
		dprintf(dlevel,"sname: %s, name: %s\n", sname, name);
//		if (classp && name) p = config_get_property(cp, sname, name);
		if (name) JS_free(cx,name);
        }
	return JS_TRUE;
}

static JSBool js_config_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
	config_t *cp;
	int prop_id;

	cp = JS_GetPrivate(cx,obj);
	if (!cp) return JS_FALSE;
	dprintf(dlevel,"type: %s\n", jstypestr(cx,id));
	if(JSVAL_IS_INT(id)) {
		prop_id = JSVAL_TO_INT(id);
		dprintf(dlevel,"prop_id: %d\n", prop_id);
		switch(prop_id) {
		case CONFIG_PROPERTY_ID_FILENAME:
			jsval_to_type(DATA_TYPE_STRING,cp->filename,sizeof(cp->filename),cx,*vp);
			break;
		case CONFIG_PROPERTY_ID_FILE_FORMAT:
			jsval_to_type(DATA_TYPE_INT,&cp->file_format,0,cx,*vp);
			break;
		case CONFIG_PROPERTY_ID_TRIG:
			jsval_to_type(DATA_TYPE_BOOLEAN,&cp->triggers,0,cx,*vp);
//			log_info("config: %s triggers...\n", (cp->triggers ? "Enabling" : "Disabling"));
			break;
		case CONFIG_PROPERTY_ID_ERRMSG:
			jsval_to_type(DATA_TYPE_STRING,cp->errmsg,sizeof(cp->errmsg),cx,*vp);
			break;
		default:
			JS_ReportError(cx, "property not found");
			break;
		}
	}
	return JS_TRUE;
}

static JSClass js_config_class = {
	"Config",		/* Name */
	JSCLASS_HAS_PRIVATE,	/* Flags */
	JS_PropertyStub,	/* addProperty */
	JS_PropertyStub,	/* delProperty */
	js_config_getprop,	/* getProperty */
	js_config_setprop,	/* setProperty */
	JS_EnumerateStub,	/* enumerate */
	JS_ResolveStub,		/* resolve */
	JS_ConvertStub,		/* convert */
	JS_FinalizeStub,	/* finalize */
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool js_config_read(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	config_t *cp;
	jsval *argv = vp + 2;
	JSString *fnstr;
	char *filename;
	int type,r;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	cp = JS_GetPrivate(cx, obj);

	fnstr = 0;
	type = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "S / i", &fnstr, &type)) return JS_FALSE;
	filename = JS_EncodeString(cx, fnstr);
	dprintf(dlevel,"filename: %s, type: %d\n", filename, type);
	config_set_filename(cp, filename, type);
	r = (config_read(cp) ? JS_FALSE : JS_TRUE);
	*vp = BOOLEAN_TO_JSVAL(r);
	JS_free(cx,filename);

	return JS_TRUE;
}

static JSBool js_config_write(JSContext *cx, uintN argc, jsval *vp) {
	JSObject *obj;
	config_t *cp;

	obj = JS_THIS_OBJECT(cx, vp);
	if (!obj) return JS_FALSE;
	cp = JS_GetPrivate(cx, obj);
	if (!cp) return JS_FALSE;

	config_write(cp);
	return JS_TRUE;
}

struct js_propinfo {
	char *name;
	int type;
	void *dest;
	int size;
	int req;
};

static void _getele(char *name, int type, void *dest, int size, JSContext *cx, JSObject *arr, unsigned int ele) {
	jsval val;

	dprintf(dlevel,"name: %s, ele: %d\n", name, ele);
	if (type == DATA_TYPE_NULL) {
		JS_GetElement(cx, arr, ele, dest);
	} else {
		JS_GetElement(cx, arr, ele, &val);
		jsval_to_type(type, dest, size, cx, val);
	}
}

static config_property_t *_js_config_get_prop(config_t *cp, JSContext *cx, JSObject *arr) {
	config_property_t *p;
	unsigned int count;
	char *name, *def;
	char *scope,*values,*labels,*units;
	int type,flags,precision;
	float scale;
	jsval func,arg;
	int i;

	int ldlevel = dlevel;

	dprintf(ldlevel,"cp: %p\n", cp);

	name = def = 0;
	scope = values = labels = units = 0;
	type = flags = precision = 0;
	scale = 1.0;

	func = JSVAL_VOID;
	arg = JSVAL_VOID;
	if (!js_GetLengthProperty(cx, arr, &count)) {
		if (cp) sprintf(cp->errmsg,"get_prop: unable to get property array length");
		else log_error("get_prop: internal error: unable to get property array length\n");
		return 0;
	}
	dprintf(ldlevel,"count: %d\n", count);
	if (count < 4) {
		if (cp) sprintf(cp->errmsg,"a min of 4 prop fields reqd (name,type,def,flags)");
		else log_error("a min of 4 prop fields reqd (name,type,def,flags) provided: %d\n",count);
		return 0;
	}
	i = 0;
	_getele("name",DATA_TYPE_STRINGP, &name, 0, cx, arr, i++);
	_getele("type",DATA_TYPE_INT, &type, 0, cx, arr, i++);
	_getele("def",DATA_TYPE_STRINGP, &def, 0, cx, arr, i++);
	_getele("flags",DATA_TYPE_INT, &flags, 0, cx, arr, i++);
	if (count > 4) _getele("func",DATA_TYPE_NULL, &func, 0, cx, arr, i++);
	if (count > 5) _getele("arg",DATA_TYPE_NULL, &arg, 0, cx, arr, i++);

/*
config_property_t *config_new_property(config_t *cp, char *name, int type, void *dest, int dsize, int len, char *def, int flags,
		char *scope, char *values, char *labels, char *units, float scale, int precision) {
*/

	dprintf(ldlevel,"prop: name: %s, type: %s, def: %s, flags: %x, func: %x, arg: %x\n", name, typestr(type), def, flags, func, arg);
	p = config_new_property(cp,name,type,0,0,0,def,flags,scope,values,labels,units,scale,precision);
	if (name) JS_free(cx,name);
	if (def) JS_free(cx,def);
	dprintf(ldlevel,"func: %x, void: %x\n", func, JSVAL_VOID);
	dprintf(ldlevel,"isfunc: %d\n", VALUE_IS_FUNCTION(cx, func));
	if (VALUE_IS_FUNCTION(cx, func) && js_config_property_set_trigger(cx,p,func,arg)) {
		config_destroy_property(p);
		return 0;
	}
	return p;
}

static int _js_config_add_props(config_t *cp, JSContext *cx, JSObject *parent, JSObject *arr, char *sname) {
	config_property_t *newprop;
	unsigned int count;
	config_section_t *sec;
	int i;
	jsval val;

	int ldlevel = dlevel;

	dprintf(ldlevel,"sname: %s\n", sname);
	sec = config_get_section(cp,sname);
	if (!sec) {
		sprintf(cp->errmsg,"add_props: section '%s' not found", sname);
		return 1;
	}

	if (!js_GetLengthProperty(cx, arr, &count)) {
		strcpy(cp->errmsg,"add_props: unable to get array length");
		return 1;
	}
	for(i=0; i < count; i++) {
		JS_GetElement(cx, arr, i, &val);
		dprintf(ldlevel,"arr[%d] type: %s\n", i, jstypestr(cx,val));
		if (!JSVAL_IS_OBJECT(val) || !OBJ_IS_ARRAY(cx,JSVAL_TO_OBJECT(val))) {
			sprintf(cp->errmsg,"add_props: element %d is not an array", i);
			return 1;
		}
		newprop = _js_config_get_prop(cp,cx,JSVAL_TO_OBJECT(val));
		if (!newprop) return 1;

		/* add the property but delay the trigger */
		config_section_add_property(cp, sec, newprop, CONFIG_FLAG_NOTRIG);

		/* If parent specified, create the prop in the parent */
		if (parent) {
			JSBool ok;
			int flags;

			if (newprop->dest) val = type_to_jsval(cx,newprop->type,newprop->dest,newprop->len);
			else val = JSVAL_VOID;
			dprintf(ldlevel,"val: %x, void: %x\n", val, JSVAL_VOID);
			flags = JSPROP_ENUMERATE;
			if (newprop->flags & CONFIG_FLAG_READONLY) flags |= JSPROP_READONLY;
			ok = JS_DefinePropertyWithTinyId(cx,parent,newprop->name,newprop->id,val,0,0,flags);
			dprintf(ldlevel,"define ok: %d\n", ok);
		}

		/* now call the trigger */
		dprintf(ldlevel,"val: %x, void: %x, trigger: %p, triggers: %d, NOTRIG: %d\n", val, JSVAL_VOID, newprop->trigger,
			cp->triggers, _check_flag(newprop->flags,NOTRIG));
		if (val != JSVAL_VOID && newprop->trigger && cp->triggers && !_check_flag(newprop->flags,NOTRIG) && newprop->trigger(newprop->ctx,newprop,0)) {
			sprintf(cp->errmsg, "add_props: error calling trigger");
			return 1;
		}
	}

	/* if updated parent props, must rebuild map */
	dprintf(ldlevel,"building map...\n");
	config_build_propmap(cp);

	dprintf(ldlevel,"returning ok!\n");
	return 0;
}

JSBool js_config_add_props(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	config_t *cp;
	char *sname;
	JSObject *dobj;
	int r;

	int ldlevel = dlevel;

	cp = JS_GetPrivate(cx, obj);
	dprintf(ldlevel,"cp: %p\n", cp);
	if (!cp) {
		JS_ReportError(cx, "config_add_props: private is null!");
		return JS_FALSE;
	}

	dprintf(ldlevel,"argc: %d\n", argc);
	if (argc < 3 || !JSVAL_IS_OBJECT(argv[0]) || !JSVAL_IS_OBJECT(argv[1]) || !OBJ_IS_ARRAY(cx,JSVAL_TO_OBJECT(argv[1]))) {
		JS_ReportError(cx, "add_props requires 2 arguments: object, properties(array of arrays), optional: section_name(string)");
		return JS_FALSE;
	}
	dobj = JSVAL_TO_OBJECT(argv[0]);

	/* If 3rd arguement specified, use it over object name */
	if (argc > 2) {
		JSString *str = JS_ValueToString(cx,argv[2]);
		sname = JS_EncodeString(cx,str);
	} else {
		dprintf(-1,"===> calling JS_GetObjectName!\n");
//		sname = JS_GetObjectName(cx,dobj);
		sname = JS_strdup(cx,"unknown");
	}
	dprintf(ldlevel,"sname: %s\n", sname);

	*cp->errmsg = 0;
	r = _js_config_add_props(cp,cx,dobj,JSVAL_TO_OBJECT(argv[1]),sname);
	JS_free(cx,sname);
	if (r) {
		JS_ReportError(cx,cp->errmsg);
		return JS_FALSE;
	}

	dprintf(ldlevel,"returning true!\n");
	return JS_TRUE;
}

config_property_t *js_config_obj2props(JSContext *cx, JSObject *arr) {
	config_property_t *props, *pp, *newprop;
	unsigned int count;
	int i,sz;
	jsval val;

	int ldlevel = dlevel;

	if (!js_GetLengthProperty(cx, arr, &count)) {
		log_error("obj2props: unable to get props array length\n");
		return 0;
	}
	dprintf(ldlevel,"count: %d\n", count);
	sz = sizeof(config_property_t)*(count+1);
	dprintf(ldlevel,"sz: %d\n", sz);
	pp = props = JS_malloc(cx,sz);
	memset(props,0,sz);
	for(i=0; i < count; i++) {
		JS_GetElement(cx, arr, i, &val);
		dprintf(ldlevel,"arr[%d] type: %s\n", i, jstypestr(cx,val));
		if (!JSVAL_IS_OBJECT(val) || !OBJ_IS_ARRAY(cx,JSVAL_TO_OBJECT(val))) {
			log_error("obj2props: props element is not an array\n");
			continue;
		}
		newprop = _js_config_get_prop(0,cx,JSVAL_TO_OBJECT(val));
		dprintf(ldlevel,"newprop: %p\n", newprop);
		if (!newprop) continue;
//		config_dump_property(newprop,0);
		*pp++ = *newprop;
	}

	dprintf(ldlevel,"returning: %p\n", props);
	return props;
}

static JSBool js_config_delete_property(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	config_t *cp;
	char *sname, *name;

	cp = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"cp: %p\n", cp);
	if (!cp) return JS_FALSE;

	dprintf(dlevel,"argc: %d\n", argc);
	if (argc < 2) {
		JS_ReportError(cx, "delete requires 2 arguements (section:string, property:string)");
		return JS_FALSE;
	}
	if (!JS_ConvertArguments(cx, argc, argv, "s s", &sname, &name)) return JS_FALSE;
	dprintf(dlevel,"sname: %s, name: %s\n", sname, name);

	if (!config_delete_property(cp, sname, name)) config_write(cp);
	return JS_TRUE;
}

static int _js_config_call_func_only(JSContext *cx, jsval func, int argc, jsval *jsargv, char *errmsg, char *rname, json_object_t *results) {
	JSBool ok;
	jsval rval;
	int jstype;

	ok = JS_CallFunctionValue(cx, JS_GetGlobalObject(cx), func, argc, jsargv, &rval);
	dprintf(dlevel,"call ok: %d\n", ok);
	if (!ok) return 1;

	// if its a number, use that as a status
	// string means json string into results
	dprintf(dlevel,"rval: %x (%s)\n", rval, jstypestr(cx,rval));
	jstype = JS_TypeOfValue(cx,rval);
	if (jstype == JSTYPE_NUMBER) {
		int status;

		JS_ValueToInt32(cx,rval,&status);
		dprintf(dlevel,"status: %d\n", status);
		if (status != 0) return 1;
	} else if (jstype == JSTYPE_STRING) {
		char *str;
		json_value_t *v;

		str = (char *)JS_EncodeString(cx, JSVAL_TO_STRING(rval));
		dprintf(dlevel,"str: %s\n", str);
		v = json_parse(str);
		dprintf(dlevel,"v: %p\n", v);
		if (v) {
			json_object_set_value(results,rname,v);
		} else {
			json_object_set_string(results,rname,str);
		}
		JS_free(cx,str);
	}
	return 0;
}

int js_config_call_func(void *_ctx, list args, char *errmsg, json_object_t *results) {
	js_config_func_ctx_t *ctx = _ctx;
	config_arg_t *arg;
	jsval *jsargv;
	int i;
	char *rname;

	dprintf(dlevel,"args count: %d\n", list_count(args));
	if (list_count(args)) {
		list_reset(args);
		while((arg = list_get_next(args)) != 0) {
			dprintf(dlevel,"argc: %d\n", arg->argc);
			jsargv = JS_malloc(ctx->cx,arg->argc * sizeof(jsval));
			for(i=0; i < arg->argc; i++) {
				dprintf(dlevel,"argv[%d]: %p\n", i, arg->argv[i]);
				jsargv[i] = type_to_jsval(ctx->cx,DATA_TYPE_STRING,arg->argv[i],strlen(arg->argv[i]));
			}
			if (arg->argc) rname = arg->argv[0];
			else rname = "value";
			i = _js_config_call_func_only(ctx->cx, ctx->func, arg->argc, jsargv, errmsg, rname, results);
			JS_free(ctx->cx,jsargv);
			return i;
		}
	} else {
		if (_js_config_call_func_only(ctx->cx, ctx->func, 0, 0, errmsg, "value", results)) return 1;
	}
	return 0;
}

static config_function_t *_js_config_get_func(config_t *cp, JSContext *cx, JSObject *arr) {
	config_function_t *f;
	char *name;
	int nargs;
	jsval func,arg;
	unsigned int count;
	int i;

	int ldlevel = dlevel;

	dprintf(ldlevel,"cp: %p\n", cp);

	name = 0;
	nargs = 0;
	func = JSVAL_VOID;

	if (!js_GetLengthProperty(cx, arr, &count)) {
		if (cp) sprintf(cp->errmsg,"get_prop: unable to get property array length");
		return 0;
	}
	dprintf(ldlevel,"count: %d\n", count);
	if (count < 3) {
		if (cp) sprintf(cp->errmsg,"a min of 3 func fields reqd (name,func,nargs)");
		return 0;
	}
	i = 0;
	_getele("name",DATA_TYPE_STRINGP, &name, 0, cx, arr, i++);
	_getele("func",DATA_TYPE_NULL, &func, 0, cx, arr, i++);
	_getele("nargs",DATA_TYPE_INT, &nargs, 0, cx, arr, i++);
	if (count > 3) _getele("arg",DATA_TYPE_NULL, &arg, 0, cx, arr, i++);
	dprintf(ldlevel,"func: name: %s, ptr: %p, nargs: %d, arg: %p\n", name, func, nargs, arg);

	/* create the func spec */
	f = JS_malloc(cx,sizeof(*f));
	if (!f) {
		if (cp) sprintf(cp->errmsg,"get_func: error allocating mem for func");
		return 0;
	}
	memset(f,0,sizeof(*f));
	f->name = strdup(name);
	f->flags = CONFIG_FUNCTION_FLAG_ALLOCNAME;
	f->nargs = nargs;
	/* this is the c function that gets called */
	f->func = js_config_call_func;

	/* the c function uses this ctx to call the js function */
	f->ctx = _js_config_get_func_ctx(cx, func, name);

	/* root it */
	JS_EngineAddRoot(cx,f->name,&((js_config_func_ctx_t *)f->ctx)->func);
#if 0
	ctx = malloc(sizeof(*ctx));
	dprintf(ldlevel,"ctx: %p\n", ctx);
	if (!ctx) {
		log_syserror("_js_config_get_func: malloc trigctx\n");
		return 0;
	}
	memset(ctx,0,sizeof(*ctx));
//	cx, func, name
	ctx->cx = cx;
	ctx->func = func;
	strncpy(ctx->name,name,CONFIG_CTX_NAME_SIZE);

	/* attach the ctx to the func */
	f->ctx = ctx;
#endif

	return f;
}

static int _js_config_add_funcs(config_t *cp, JSContext *cx, JSObject *arr, char *sname) {
	config_function_t *func;
	config_section_t *sec;
	unsigned int count;
	int i;
	jsval val;

	int ldlevel = dlevel;

	dprintf(ldlevel,"sname: %s\n", sname);
	sec = config_get_section(cp,sname);
	if (!sec) {
		sprintf(cp->errmsg,"add_funcs: section '%s' not found\n", sname);
		return 1;
	}

	if (!js_GetLengthProperty(cx, arr, &count)) {
		strcpy(cp->errmsg,"add_funcs: unable to get array length");
		return 1;
	}
	dprintf(ldlevel,"count: %d\n", count);
	for(i=0; i < count; i++) {
		JS_GetElement(cx, arr, i, &val);
		dprintf(ldlevel,"arr[%d] type: %s\n", i, jstypestr(cx,val));
		if (!JSVAL_IS_OBJECT(val) || !OBJ_IS_ARRAY(cx,JSVAL_TO_OBJECT(val))) {
			JS_ReportError(cx, "add_funcs: element %d is not an array", i);
			return 1;
		}

		/* get func */
		func = _js_config_get_func(cp,cx,JSVAL_TO_OBJECT(val));
		if (func) list_add(cp->funcs, func, sizeof(*func));
	}
	return 0;
}

JSBool js_config_add_funcs(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	config_t *cp;
	JSObject *dobj;
	char *sname;
	int badarg,r;

	cp = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"cp: %p\n", cp);
	if (!cp) {
		JS_ReportError(cx, "config_add_funcs: private is null!");
		return JS_FALSE;
	}

	dprintf(dlevel,"argc: %d\n", argc);
	badarg = 0;
//	if (argc < 2 || !JSVAL_IS_OBJECT(argv[0]) || !JSVAL_IS_OBJECT(argv[1]) || !OBJ_IS_ARRAY(cx,JSVAL_TO_OBJECT(argv[1]))) {
	if (argc < 2) badarg = 1;
	else if (argc == 2) {
		dprintf(dlevel,"argv[0]: %p, argv[1]: %p\n", argv[0], argv[1]);
		if (!argv[0] || !argv[1]) badarg = 1;
		else if (!JSVAL_IS_OBJECT(argv[0]) || !JSVAL_IS_OBJECT(argv[1]) || !OBJ_IS_ARRAY(cx,JSVAL_TO_OBJECT(argv[1])))
			badarg = 1;
	}
	dprintf(dlevel,"badarg: %d\n", badarg);
	if (badarg) {
		JS_ReportError(cx, "add_funcs requires 2 arguments: object, properties(array of arrays), optional: section_name(string)");
		return JS_FALSE;
	}
	dobj = JSVAL_TO_OBJECT(argv[0]);
	dprintf(dlevel,"dobj: %p\n", dobj);

	/* If 3rd arguement specified, use it over object name */
	if (argc > 2) {
		JSString *str = JS_ValueToString(cx,argv[2]);
		sname = JS_EncodeString(cx,str);
	} else {
dprintf(-1,"===> JS_GetObjectName\n");
//		sname = JS_GetObjectName(cx,dobj);
		sname = "addfunc";
	}
	dprintf(dlevel,"sname: %s\n", sname);

	r = _js_config_add_funcs(cp,cx,JSVAL_TO_OBJECT(argv[1]),sname);
	JS_free(cx,sname);
	if (r) {
		JS_ReportError(cx, cp->errmsg);
		return JS_FALSE;
	}
	return JS_TRUE;
}

config_function_t *js_config_obj2funcs(JSContext *cx, JSObject *arr) {
	config_function_t *funcs, *fp, *newfunc;
	unsigned int count;
	int i,sz;
	jsval val;

	int ldlevel = dlevel;

	if (!js_GetLengthProperty(cx, arr, &count)) {
		log_error("obj2funcs: unable to get funcs array length\n");
		return 0;
	}
	dprintf(ldlevel,"count: %d\n", count);
	sz = sizeof(config_function_t)*(count+1);
	dprintf(ldlevel,"sz: %d\n", sz);
	fp = funcs = JS_malloc(cx,sz);
	memset(funcs,0,sz);
	for(i=0; i < count; i++) {
		JS_GetElement(cx, arr, i, &val);
		dprintf(ldlevel,"arr[%d] type: %s\n", i, jstypestr(cx,val));
		if (!JSVAL_IS_OBJECT(val) || !OBJ_IS_ARRAY(cx,JSVAL_TO_OBJECT(val))) {
			log_error("obj2funcs: funcs element is not an array\n");
			continue;
		}
		newfunc = _js_config_get_func(0,cx,JSVAL_TO_OBJECT(val));
		dprintf(ldlevel,"newfunc: %p\n", newfunc);
		if (!newfunc) continue;
		*fp++ = *newfunc;
	}

	dprintf(ldlevel,"returning: %p\n", funcs);
	return funcs;
}

static JSBool js_config_delete_function(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	config_t *cp;
	char *name;

	cp = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"cp: %p\n", cp);
	if (!cp) return JS_FALSE;

	dprintf(dlevel,"argc: %d\n", argc);
	if (argc < 1) {
		JS_ReportError(cx, "delete requires 1 arguement (function name:string)");
		return JS_FALSE;
	}
	if (!JS_ConvertArguments(cx, argc, argv, "s", &name)) return JS_FALSE;
	dprintf(dlevel,"name: %s\n", name);

	if (!config_delete_function(cp, name)) config_write(cp);
	return JS_TRUE;
}

static JSBool js_config_get_section(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	config_t *cp;
	char *sname;
	config_section_t *s;

	cp = JS_GetPrivate(cx, obj);
	if (!cp) {
		JS_ReportError(cx, "js_config_get_section: private is null!");
		return JS_FALSE;
	}

	sname = 0;
	dprintf(dlevel,"argc: %d\n", argc);
	if (!JS_ConvertArguments(cx, argc, argv, "s", &sname)) return JS_FALSE;
	dprintf(dlevel,"sname: %s\n", sname);

	s = config_get_section(cp, sname);
	JS_free(cx,sname);
	dprintf(dlevel,"s: %p\n", s);
	if (!s) {
		sprintf(cp->errmsg,"section not found");
		*rval = JSVAL_VOID;
		return JS_TRUE;
	}

	*cp->errmsg = 0;
	*rval = OBJECT_TO_JSVAL(js_config_section_new(cx,obj,s));
	return JS_TRUE;
}

static JSBool js_config_get_property(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	config_t *cp;
	char *pname;
	config_property_t *p;

	cp = JS_GetPrivate(cx, obj);
	if (!cp) {
		JS_ReportError(cx, "js_config_get_property: private is null!");
		return JS_FALSE;
	}

	pname = 0;
	dprintf(dlevel,"argc: %d\n", argc);
	if (!JS_ConvertArguments(cx, argc, argv, "s", &pname)) return JS_FALSE;

	dprintf(dlevel,"pname: %s\n", pname);
	p = config_find_property(cp, pname);
	JS_free(cx,pname);
	dprintf(dlevel,"p: %p\n", p);
	if (!p) {
		sprintf(cp->errmsg,"property not found");
		*rval = JSVAL_VOID;
		return JS_TRUE;
	}

	*cp->errmsg = 0;
//	dprintf(dlevel,"p->jsval: %x\n", p->jsval);
//	if (p->jsval == JSVAL_NULL) p->jsval = OBJECT_TO_JSVAL(js_config_property_new(cx,obj,p));
//	*rval = p->jsval;
	*rval = OBJECT_TO_JSVAL(js_config_property_new(cx,obj,p));
	return JS_TRUE;
}

static JSBool js_config_get_flags(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	config_t *cp;
	char *name;
	config_property_t *p;

	cp = JS_GetPrivate(cx,obj);
	if (!cp) {
		JS_ReportError(cx, "config_get_flags: private is null!");
		return JS_FALSE;
	}

	name = 0;
	if (!JS_ConvertArguments(cx, argc, argv, "s", &name)) return JS_FALSE;
	dprintf(dlevel,"name: %s\n", name);

	p = config_find_property(cp,name);
	if (!p) {
//		log_error("config_get_flags: unable to find property: %s\n",name);
		*rval = JSVAL_VOID;
		return JS_TRUE;
	}
	*rval = INT_TO_JSVAL(p->flags);
	return JS_TRUE;
}

static JSBool js_config_disable_trigger(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	config_t *cp;
	char *sname,*name;
	config_property_t *p;

	cp = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"cp: %p\n", cp);
	if (!cp) return JS_FALSE;

	dprintf(dlevel,"argc: %d\n", argc);
	if (argc < 2) {
		JS_ReportError(cx, "disable_trigger requires 2 arguements (section:string, property:string)");
		return JS_FALSE;
	}
	if (!JS_ConvertArguments(cx, argc, argv, "s s", &sname, &name)) return JS_FALSE;
	dprintf(dlevel,"sname: %s, name: %s\n", sname, name);

	p = config_get_property(cp,sname,name);
	if (!p) {
		strcpy(cp->errmsg,"property not found\n");
		*rval = BOOLEAN_TO_JSVAL(false);
	} else {
		_set_flag(p->flags,NOTRIG);
		*rval = BOOLEAN_TO_JSVAL(true);
	}
	return JS_TRUE;
}

static JSBool js_config_enable_trigger(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	config_t *cp;
	char *sname,*name;
	config_property_t *p;

	cp = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"cp: %p\n", cp);
	if (!cp) return JS_FALSE;

	dprintf(dlevel,"argc: %d\n", argc);
	if (argc < 2) {
		JS_ReportError(cx, "enable_trigger requires 2 arguements (section:string, property:string)");
		return JS_FALSE;
	}
	if (!JS_ConvertArguments(cx, argc, argv, "s s", &sname, &name)) return JS_FALSE;
	dprintf(dlevel,"sname: %s, name: %s\n", sname, name);

	p = config_get_property(cp,sname,name);
	if (!p) {
		strcpy(cp->errmsg,"property not found\n");
		*rval = BOOLEAN_TO_JSVAL(false);
	} else {
		_set_flag(p->flags,NOTRIG);
		*rval = BOOLEAN_TO_JSVAL(true);
	}
	return JS_TRUE;
}

static JSBool js_config_set_trigger(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	config_t *cp;
	char *pname;
	config_property_t *p;
	JSObject *funcobj;
	jsval func,arg;

	cp = JS_GetPrivate(cx, obj);
	if (!cp) {
		JS_ReportError(cx, "js_config_get_property: private is null!");
		return JS_FALSE;
	}

	pname = 0;
	dprintf(dlevel,"argc: %d\n", argc);
	if (!JS_ConvertArguments(cx, argc, argv, "s f / v", &pname, &funcobj, &arg)) return JS_FALSE;
	func = arg = 0;
	dprintf(dlevel,"pname: %s, funcobj: %x, arg: %x\n", pname, funcobj, arg);
	func = OBJECT_TO_JSVAL(funcobj);
	dprintf(dlevel,"is func: %d\n", VALUE_IS_FUNCTION(cx, func));
	if (!VALUE_IS_FUNCTION(cx, func)) {
		JS_ReportError(cx, "set_trigger: argument 2 is not a function");
		return JS_FALSE;
	}

	dprintf(dlevel,"pname: %s\n", pname);
	p = config_find_property(cp, pname);
	JS_free(cx,pname);
	dprintf(dlevel,"p: %p\n", p);
	if (!p) {
		sprintf(cp->errmsg,"property not found");
		return JS_TRUE;
	}
	dprintf(dlevel,"func: %x, arg: %x\n", func);
	js_config_property_set_trigger(cx,p,func,arg);
	return JS_TRUE;
}

static JSBool js_config_clear_trigger(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	config_t *cp;
	char *pname;
	config_property_t *p;

	cp = JS_GetPrivate(cx, obj);
	if (!cp) {
		JS_ReportError(cx, "js_config_get_property: private is null!");
		return JS_FALSE;
	}

	pname = 0;
	dprintf(dlevel,"argc: %d\n", argc);
	if (!JS_ConvertArguments(cx, argc, argv, "s", &pname)) return JS_FALSE;
	dprintf(dlevel,"pname: %s\n", pname);

	dprintf(dlevel,"pname: %s\n", pname);
	p = config_find_property(cp, pname);
	JS_free(cx,pname);
	dprintf(dlevel,"p: %p\n", p);
	if (!p) {
		sprintf(cp->errmsg,"property not found");
		return JS_TRUE;
	}
	p->trigger = 0;
	return JS_TRUE;
}

static JSBool js_config_delete_section(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	config_t *cp;
	char *name;

	cp = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"cp: %p\n", cp);
	if (!cp) return JS_FALSE;

	dprintf(dlevel,"argc: %d\n", argc);
	if (argc < 1) {
		JS_ReportError(cx, "delete requires 1 arguement (section name:string)");
		return JS_FALSE;
	}
	if (!JS_ConvertArguments(cx, argc, argv, "s", &name)) return JS_FALSE;
	dprintf(dlevel,"name: %s\n", name);

	if (!config_delete_section(cp, name)) config_write(cp);
	return JS_TRUE;
}

static JSBool js_config_dump(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	config_t *cp;

	cp = JS_GetPrivate(cx, obj);
	dprintf(dlevel,"cp: %p\n", cp);
	if (!cp) return JS_FALSE;

	config_dump(cp);
	return JS_TRUE;
}

static JSBool js_config_ctor(JSContext *cx, JSObject *parent, uintN argc, jsval *argv, jsval *rval) {
	config_t *cp;
	char *name, *filename;
	int filetype;
	JSObject *newobj;

	name = filename = 0;
	filetype = CONFIG_FILE_FORMAT_AUTO;
	if (!JS_ConvertArguments(cx, argc, argv, "s / s d", &filename, &filetype)) return JS_FALSE;
	dprintf(dlevel,"name: %s, file: %s, type: %d\n", name, filename, filetype);

	cp = config_init(name, 0, 0);
	dprintf(dlevel,"cp: %p\n", cp);
	if (!cp) {
		JS_ReportError(cx, "js_config_ctor: config_init returned null\n");
		return JS_FALSE;
	}
	if (name) JS_free(cx,name);

	newobj = js_config_new(cx,parent,cp);
	*rval = OBJECT_TO_JSVAL(newobj);

	if (filename) {
		config_set_filename(cp, filename, filetype);
		JS_free(cx,filename);
	}

	return JS_TRUE;
}

JSObject *js_InitConfigClass(JSContext *cx, JSObject *parent) {
	JSPropertySpec config_props[] = {
		{ "sections", CONFIG_PROPERTY_ID_SECTIONS,  JSPROP_ENUMERATE | JSPROP_READONLY },
		{ "filename", CONFIG_PROPERTY_ID_FILENAME,  JSPROP_ENUMERATE },
		{ "file_format", CONFIG_PROPERTY_ID_FILE_FORMAT,  JSPROP_ENUMERATE },
		{ "errmsg", CONFIG_PROPERTY_ID_ERRMSG,  JSPROP_ENUMERATE },
		{ "triggers", CONFIG_PROPERTY_ID_TRIG,  JSPROP_ENUMERATE },
		{ "functions", CONFIG_PROPERTY_ID_FUNCS,  JSPROP_ENUMERATE | JSPROP_READONLY },
		{ 0 }
	};
	JSFunctionSpec config_funcs[] = {
		JS_FN("read",js_config_read,1,1,0),
		JS_FN("write",js_config_write,0,0,0),
		JS_FN("save",js_config_write,0,0,0),
		JS_FS("get_section",js_config_get_section,1,1,0),
		JS_FS("get_property",js_config_get_property,2,2,0),
		JS_FS("add_props",js_config_add_props,2,2,0),
		JS_FS("delete_prop",js_config_delete_property,1,1,0),
		JS_FS("add_funcs",js_config_add_funcs,2,2,0),
		JS_FS("delete_func",js_config_delete_function,1,1,0),
		JS_FS("delete_section",js_config_delete_section,1,1,0),
		JS_FS("disable_trigger",js_config_disable_trigger,2,2,0),
		JS_FS("enable_trigger",js_config_enable_trigger,2,2,0),
		JS_FS("set_trigger",js_config_set_trigger,2,3,0),
		JS_FS("clear_trigger",js_config_clear_trigger,1,1,0),
		JS_FS("get_flags",js_config_get_flags,1,1,0),
		JS_FS("dump",js_config_dump,0,0,0),
		{ 0 }
	};
	JSConstantSpec config_consts[] = {
		JS_NUMCONST(CONFIG_FILE_FORMAT_AUTO),
		JS_NUMCONST(CONFIG_FILE_FORMAT_INI),
		JS_NUMCONST(CONFIG_FILE_FORMAT_JSON),
		JS_NUMCONST(CONFIG_FILE_FORMAT_CUSTOM),
		JS_NUMCONST(CONFIG_FLAG_READONLY),
		JS_NUMCONST(CONFIG_FLAG_NOSAVE),
		JS_NUMCONST(CONFIG_FLAG_NOID),
		JS_NUMCONST(CONFIG_FLAG_FILE),
		JS_NUMCONST(CONFIG_FLAG_FILEONLY),
		JS_NUMCONST(CONFIG_FLAG_ALLOC),
		JS_NUMCONST(CONFIG_FLAG_NOPUB),
		JS_NUMCONST(CONFIG_FLAG_NODEF),
		JS_NUMCONST(CONFIG_FLAG_ALLOCDEST),
		JS_NUMCONST(CONFIG_FLAG_NOINFO),
		JS_NUMCONST(CONFIG_FLAG_PUB),
		JS_NUMCONST(CONFIG_FLAG_NOWARN),
		JS_NUMCONST(CONFIG_FLAG_VALUE),
		JS_NUMCONST(CONFIG_FLAG_IN_TRIG),
		JS_NUMCONST(CONFIG_FLAG_PRIVATE),
		{ 0 }
	};
	JSObject *obj;

	dprintf(2,"defining %s class\n",js_config_class.name);
	obj = JS_InitClass(cx, parent, 0, &js_config_class, js_config_ctor, 1, config_props, config_funcs, 0, 0);
	if (!obj) {
		JS_ReportError(cx,"unable to initialize %s class", js_config_class.name);
		return 0;
	}

	/* Define global consts */
	if (!JS_DefineConstants(cx, parent, config_consts)) {
		JS_ReportError(cx,"unable to define global config constants");
		return 0;
	}

	dprintf(dlevel,"done!\n");
	return obj;
}

JSObject *js_config_new(JSContext *cx, JSObject *parent, config_t *cp) {
	JSObject *newobj;

	/* Create the new object */
	dprintf(2,"creating %s object...\n", js_config_class.name);
	newobj = JS_NewObject(cx, &js_config_class, 0, parent);
	if (!newobj) return 0;

	JS_SetPrivate(cx,newobj,cp);

	dprintf(dlevel,"done!\n");
	return newobj;
}

int config_jsinit(JSEngine *e) {
	JS_EngineAddInitClass(e, "js_InitConfigClass", js_InitConfigClass);
	JS_EngineAddInitClass(e, "js_InitConfigSectionClass", js_InitConfigSectionClass);
	JS_EngineAddInitClass(e, "js_InitConfigPropertyClass", js_InitConfigPropertyClass);
	return 0;
}
#endif
