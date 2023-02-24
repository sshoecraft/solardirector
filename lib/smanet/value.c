
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#define dlevel 6
#include "debug.h"

#include "smanet_internal.h"
#include <assert.h>

/*
Spot value
 ---------------------------------------------------------------------------------
 |             |               |               |      |           |                       |
 | KanalTyp    | Kanalindex    | DatensatzAnz  | Zeit | Zeitbasis | Wert1  Wert2 ...  Wertn |
 | ChannelType | Channel index | Record number | Time | Time base | Value1 value2 ... valuen |
 ---------------------------------------------------------------------------------

Params
 --------------------------------------------------------------------
 |          |            |              |                           |
 | KanalTyp | Kanalindex | DatensatzAnz | Wert1 Wert2 ... WertN     |
 | ChannelType | Channel index | Record number | Value1 value2 ... valueN |
 --------------------------------------------------------------------
*/

/* Channel format */
#define CH_BYTE		0x00
#define CH_SHORT	0x01
#define CH_LONG		0x02
#define CH_FLOAT	0x04
#define CH_DOUBLE	0x05
#define CH_ARRAY	0x08

static int _read_values(smanet_session_t *s, smanet_channel_t *cx) {
	smanet_packet_t *p;
	register uint8_t *sptr,*eptr;
	uint8_t req[3], index;
	uint16_t mask,count,m1,m2;
	time_t timestamp;
	long time_base;
	int i,r,rcount,dsize,retries;
//	smanet_channel_t *vc;
	smanet_value_t *v;

	dprintf(dlevel,"id: %d, c->id, c->mask: %04x, index: %02x\n", cx->id, cx->mask, cx->index);

	p = smanet_alloc_packet(2048);
	if (!p) {
		sprintf(s->errmsg,"error allocating network packet");
		return 1;
	}

#if 0
	/* if no values allocated yet, do so now */
	if (!s->values) {
		size_t vsize = list_count(s->channels)*sizeof(smanet_value_t);
		printf("===> vsize: %ld\n", vsize);
		s->values = malloc(vsize);
		if (!s->values) {
			log_write(LOG_SYSERR,"_smanet_getvals: malloc(%d)\n",vsize);
			smanet_free_packet(p);
			return 1;
		}
		memset(s->values,0,vsize);
	}
#endif

//	smanet_syn_online(s);

	/* Go through each channel and determine how many results we should get (and how large) */
	dsize = 5;
	for(rcount=i=0; i < s->chancount; i++) {
		m1 = (s->chans[i].mask | 0x0f);
		m2 = (cx->mask | 0x0f);
//		dprintf(dlevel,"chan mask: %04x, req mask: %04x\n", m1, m2);
		if (m1 == m2) {
			smanet_channel_t *c = &s->chans[i];
//			dprintf(dlevel,"adding: chan: id: %d, name: %s, index: %02x, mask: %04x, format: %04x, level: %d, type: %d(%s), count: %d\n", c->id, c->name, c->index, c->mask, c->format, c->level, c->type, typestr(c->type), c->count);
			switch(c->type) {
			case DATA_TYPE_BYTE:
				dsize += 1;
				break;
			case DATA_TYPE_SHORT:
				dsize += 2;
				break;
			case DATA_TYPE_LONG:
				dsize += 4;
				break;
			case DATA_TYPE_FLOAT:
				dsize += 4;
				break;
			case DATA_TYPE_DOUBLE:
				dsize += 8;
				break;
			default:
				log_error("_read_values: unhandled type: %d(%s)\n", cx->type, typestr(cx->type));
				continue;
			}
			rcount++;
		}
	}
	if (cx->mask & CH_SPOT) dsize += 8;
	dprintf(dlevel,"dsize: %d\n", dsize);
	dprintf(dlevel,"rcount: %d\n", rcount);

	_putu16(&req[0],(cx->mask | 0x0f));
	/* My SI6048 wont return a single value - only for a group (index 0) wtf  */
	req[2] = 0;
	for(retries=3; retries >= 0; retries--) {
		r = smanet_command(s,CMD_GET_DATA,p,req,3);
		dprintf(dlevel,"smanet_command r: %d\n", r);
		if (r) return r;
		if (debug >= dlevel+1) bindump("values",p->data,p->dataidx);
		if (p->dataidx == dsize) break;
		else dprintf(dlevel,"dataidx: %d, dsize: %d\n", p->dataidx, dsize);
		p->dataidx = 0;
	}
	dprintf(dlevel,"retries: %d\n", retries);
	if (retries < 0) {
		sprintf(s->errmsg,"_read_values: retries exhausted");
		return 1;
	}

	sptr = p->data;
	eptr = p->data + p->dataidx;
	mask = _getu16(sptr);
	sptr += 2;
	index = _getu8(sptr++);
	count = _getu16(sptr);
	sptr += 2;
	dprintf(dlevel,"mask: %04x, index: %02x, count: %d\n",mask,index,count);
	if (mask & CH_SPOT) {
		timestamp = _getu32(sptr);
		sptr += 4;
		time_base = _getu32(sptr);
		sptr += 4;
		dprintf(dlevel,"ts: %ld, time_base: %ld\n", timestamp, time_base);
	}
	for(rcount=i=0; i < s->chancount; i++) {
		smanet_channel_t *c = &s->chans[i];
		m1 = (c->mask | 0x0f);
		m2 = (cx->mask | 0x0f);
//		dprintf(dlevel+1,"chan: %s, mask: %04x, req mask: %04x\n", m1, m2);
		if (m1 != m2) continue;
//		dprintf(dlevel,"offset: %04x\n", sptr - p->data);
//		dprintf(dlevel,"sptr: %p, eptr: %p\n", sptr, eptr);
		if (sptr >= eptr) {
			log_info("internal error: _read_values: sptr >= eptr");
			smanet_free_packet(p);
			return 1;
		}
		v = (smanet_value_t *) &s->values[c->id];
		switch(c->type) {
		case DATA_TYPE_BYTE:
			v->bval = _getu8(sptr);
//			dprintf(dlevel,"bval: %d (%02X)\n", v->bval, v->bval);
			dprintf(dlevel,"%s: %d\n", c->name, v->bval);
			sptr += 1;
			break;
		case DATA_TYPE_SHORT:
			v->wval = _getu16(sptr);
//			dprintf(dlevel,"wval: %d (%04x)\n", v->wval, v->wval);
			dprintf(dlevel,"%s: %d\n", c->name, v->wval);
#if 0
			if (strcmp(c->name,"TotExtCur") == 0) {
				bindump("wval",&v->wval,2);
				printf("%s: %d\n", c->name, v->wval);
			}
#endif
			sptr += 2;
			break;
		case DATA_TYPE_LONG:
			v->lval = _getu32(sptr);
//			dprintf(dlevel,"lval: %ld (%08lx)\n", v->lval, v->lval);
			dprintf(dlevel,"%s: %ld\n", c->name, v->lval);
			sptr += 4;
			break;
		case DATA_TYPE_FLOAT:
			v->fval = _getf32(sptr);
//			dprintf(dlevel,"fval: %f\n", v->fval);
			dprintf(dlevel,"%s: %f\n", c->name, v->fval);
			sptr += 4;
			break;
		case DATA_TYPE_DOUBLE:
			v->dval = _getf64(sptr);
//			dprintf(dlevel,"dval: %lf\n", v->dval);
			dprintf(dlevel,"%s: %f\n", c->name, v->dval);
			sptr += 8;
			break;
		default:
			log_error("SMANET: _read_values: unhandled type: %d(%s)\n", c->type, typestr(c->type));
			sprintf(s->errmsg,"_read_values: unhandled type: %d(%s)", c->type, typestr(c->type));
			return 1;
			break;
		}
		v->type = c->type;
		time(&v->timestamp);
	}
	smanet_free_packet(p);
	return 0;
}

static int _get_value(smanet_session_t *s, smanet_channel_t *c, smanet_value_t *v, int cache) {
	bool doit;

	dprintf(dlevel,"chan: id: %d, name: %s, index: %02x, mask: %04x, format: %04x, level: %d, type: %d(%s), count: %d\n",
                        c->id, c->name, c->index, c->mask, c->format, c->level, c->type, typestr(c->type), c->count);
//	if ((!s->values || s->values[c->id].timestamp == 0) && _read_values(s,c)) return 1;
	doit = (s->values[c->id].timestamp == 0 || cache == 0) ? true : false;
	dprintf(dlevel,"c->mask & CH_PARA: %d\n", ((c->mask & CH_PARA) != 0));
	if (s->values[c->id].timestamp && (c->mask & CH_PARA)) {
		time_t now,diff;

		time(&now);
		diff = now - s->values[c->id].timestamp;
		dprintf(dlevel,"diff: %d, param_timeout: %d\n", diff, s->param_timeout);
		if (s->param_timeout < 0 || diff < s->param_timeout) doit = false;
	}
	if (doit) if (_read_values(s,c)) return 1;
	if (v) *v = s->values[c->id];
	return 0;
}

static void _getval(smanet_session_t *s, smanet_channel_t *c, double *dval, char **text, smanet_value_t *v) {
	double val;

	/* Get dval */
	switch(v->type) {
	case DATA_TYPE_BYTE:
		val = v->bval;
		break;
	case DATA_TYPE_SHORT:
		val = v->wval;
		break;
	case DATA_TYPE_LONG:
		val = v->lval;
		break;
	case DATA_TYPE_FLOAT:
		val = v->fval;
		break;
	case DATA_TYPE_DOUBLE:
		val = v->dval;
		break;
	default:
		dprintf(dlevel,"unhandled type(%d)!\n",v->type);
		val = 0.0;
	}
	dprintf(dlevel,"val: %f\n", val);

	dprintf(dlevel,"c->mask & CH_PARA: %d\n", c->mask & CH_PARA);
	if ((c->mask & CH_PARA) == 0) {
		dprintf(dlevel,"c->mask & CH_COUNTER: %d\n", c->mask & CH_COUNTER);
		if (c->mask & CH_COUNTER) {
			dprintf(dlevel,"gain: %f\n", c->gain);
			val *= c->gain;
		}
		dprintf(dlevel,"c->mask & CH_ANALOG: %d\n", c->mask & CH_ANALOG);
		if (c->mask & CH_ANALOG) {
			dprintf(dlevel,"gain: %f, offset: %f\n", c->gain, c->offset);
			val = (val * c->gain) + c->offset;
		}
	}
	dprintf(dlevel,"val: %f\n", val);
	dprintf(dlevel,"%s: %f\n", c->name, val);
	if (dval) *dval = val;

	/* Get the text value, if any */
	if (text) *text = 0;
	dprintf(dlevel,"c->mask & CH_STATUS: %d\n", c->mask & CH_STATUS);
	if ((c->mask & CH_STATUS) && text) {
		register char *p;
		register int i;

		i = 0;
		list_reset(c->strings);
		while((p = list_get_next(c->strings)) != 0) {
			dprintf(dlevel,"p: %s\n", p);
			if (i == val) {
				*text = p;
				break;
			}
			i++;
		}
	}
	if (text) dprintf(dlevel,"text: %s\n", *text);
}

int smanet_get_chanvalue(smanet_session_t *s, smanet_channel_t *c, double *val, char **text) {
	smanet_value_t v;

	if (_get_value(s, c, &v, 0)) return 1;
	_getval(s,c,val,text,&v);
	return 0;
}

int smanet_get_value(smanet_session_t *s, char *name, double *dest, char **text) {
	smanet_channel_t *c;

	dprintf(dlevel,"getting channel: %s\n", name);
	if ((c = smanet_get_channel(s, name)) == 0) {
		sprintf(s->errmsg,"SMANET channel not found");
		return 1;
	}
	return smanet_get_chanvalue(s,c,dest,text);
}

int smanet_get_multvalues(smanet_session_t *s, smanet_multreq_t *mr, int count) {
	smanet_channel_t *c;
	smanet_value_t v;
	int i;

	/* Always refresh spot values */
	for(i=0; i < count; i++) {
		if ((c = smanet_get_channel(s, mr[i].name)) == 0) {
			sprintf(s->errmsg,"channel not found: %s", mr[i].name);
			return 1;
		}
//		if (c->mask & CH_SPOT)) 
			s->values[c->id].timestamp = 0;
	}
	for(i=0; i < count; i++) {
		c = smanet_get_channel(s, mr[i].name);
		dprintf(dlevel,"getting value for: %s\n", mr[i].name);
		if (_get_value(s, c, &v, 1)) return 1;
		_getval(s,c,&mr[i].value,&mr[i].text,&v);
	}
	dprintf(dlevel,"done!\n");
	return 0;
}

#if 0
int smanet_get_valuebyname(smanet_session_t *s, char *name, double *dest) {
	smanet_channel_t *c;

	c = smanet_get_channelbyname(s,name);
	dprintf(dlevel,"c: %p\n", c);
	if (!c) return 0;
	return smanet_get_value(s,c,dest);
}

int smanet_get_optionval(smanet_session_t *s, smanet_channel_t *c, char *opt) {
	register char *p;
	register int i;
	int r;

	dprintf(dlevel,"opt: %s\n", opt);

	if ((c->mask & CH_STATUS) == 0) return -1;

	i = 0;
	r = -1;
	list_reset(c->strings);
	while((p = list_get_next(c->strings)) != 0) {
		dprintf(dlevel,"p: %s\n", p);
		if (strcmp(p,opt) == 0) {
			r = i;
			break;
		}
		i++;
	}
	dprintf(dlevel,"returning: %d\n", r);
	return r;
}
#endif

#if 0
int smanet_get_option(smanet_session_t *s,  smanet_channel_t *c, char *option, double *d) {
	if (smanet_get_value(s, c, d)) return 1;
}

int smanet_get_optionbyname(smanet_session_t *s, char *name, char *dest, int destlen) {
	smanet_channel_t chan, *c = &chan
	register char *p;
	register int i;
	double index;

	dprintf(dlevel,"name: %s\n", name);

//	c = smanet_get_channelbyname(s,name);
	if (smanet_get_channel(s,name,c)) return 1;
	if (smanet_get_value(s, c, &index)) return 1;
	dprintf(dlevel,"index: %f\n", index);
	i = 0;
	list_reset(c->strings);
	while((p = list_get_next(c->strings)) != 0) {
		dprintf(dlevel,"p: %s\n", p);
		if (i == index) {
			strncpy(dest,p,destlen);
			return 0;
		}
		i++;
	}
	return 1;
}
#endif

static int _write_value(smanet_session_t *s, smanet_channel_t *c, smanet_value_t *nv) {
	smanet_packet_t *p;
	uint8_t data[16];
	int i;

	dprintf(dlevel,"id: %d, name: %s, mask: %04x, index: %02x\n", c->id, c->name, c->mask, c->index);

#if 0
	{
		double v1,v2;
		/* Make sure we have a val to compare it to */
		if (!s->values && _read_values(s,c)) return 1;

		v1 = _getval(&s->values[c->id]);
		v2 = _getval(nv);
		dprintf(dlevel,"v1: %lf, v2: %lf\n", v1, v2);
		if (v1 == v2) return 0;
	}
#endif

	p = smanet_alloc_packet(16);
	if (!p) return 1;

	_putu16(&data[0],c->mask);
	_putu8(&data[2],c->index);
	_putu16(&data[3],1);
//	*((uint16_t *)&data[0]) = c->mask;
//	*((uint8_t *)&data[2]) = c->index;
//	*((uint16_t *)&data[3]) = 1;
	i = 5;
	dprintf(dlevel,"type: %d (%s)\n", nv->type, typestr(nv->type));
	switch(nv->type) {
	case DATA_TYPE_BYTE:
//		*((uint8_t *)&data[i]) = nv->bval;
//		_putu8(&data[i],nv->bval);
		data[i] = nv->bval;
		i += 1;
		break;
	case DATA_TYPE_SHORT:
//		*((short *)&data[i]) = nv->wval;
		_putu16(&data[i],nv->wval);
		i += 2;
		break;
	case DATA_TYPE_LONG:
//		*((long *)&data[i]) = nv->lval;
		_putu32(&data[i],nv->lval);
		i += 4;
		break;
	case DATA_TYPE_FLOAT:
//		*((float *)&data[i]) = nv->fval;
		_putf32(&data[i],nv->fval);
		i += 4;
		break;
	case DATA_TYPE_DOUBLE:
//		*((double *)&data[i]) = nv->dval;
		_putf64(&data[i],nv->dval);
		i += 8;
		break;
	default:
		dprintf(dlevel,"unhandled type: %d\n", nv->type);
		return 1;
	}
//	if (debug > dlevel+1) bindump("data",data,i);
	if (smanet_command(s,CMD_SET_DATA,p,data,i)) return 1;
	smanet_free_packet(p);
#if 0
	/* if no values allocated yet, do so now */
	if (!s->values) {
		s->values = calloc(1,list_count(s->channels)*sizeof(smanet_value_t));
		if (!s->values) {
			log_write(LOG_SYSERR,"_smanet_getvals: calloc(%d)\n",list_count(s->channels)*sizeof(smanet_value_t));
			smanet_free_packet(p);
			return 1;
		}
	}
#endif
	s->values[c->id] = *nv;
	time(&s->values[c->id].timestamp);
	return 0;
}

static void _setdval(smanet_value_t *v, double val) {
	dprintf(dlevel,"val : %f, type: %d(%s)\n", val, v->type, typestr(v->type));
	switch(v->type) {
	case DATA_TYPE_BYTE:
		v->bval = val;
		break;
	case DATA_TYPE_SHORT:
		v->wval = val;
		break;
	case DATA_TYPE_LONG:
		v->lval = val;
		break;
	case DATA_TYPE_FLOAT:
		v->fval = val;
		break;
	case DATA_TYPE_DOUBLE:
		v->dval = val;
		break;
	default:
		log_error("_setdval: unhandled type: %d(%s)!\n",v->type,typestr(v->type));
		v->dval = 0.0;
		break;
	}
}

int smanet_reset_value(smanet_session_t *s, char *name) {
	smanet_channel_t *c;
	smanet_value_t val;

	if (s->readonly) {
		sprintf(s->errmsg,"unable to reset value: readonly");
		return 1;
	}
	if ((c = smanet_get_channel(s, name)) == 0) return 1;

	_setdval(&val,-1);
	return _write_value(s, c, &val);
}

int smanet_set_value(smanet_session_t *s, char *name, double val, char *text) {
	smanet_channel_t *c;
	smanet_value_t v;

	if (s->readonly) {
		sprintf(s->errmsg,"unable to set value: readonly");
		return 1;
	}

	dprintf(dlevel,"name: %s, val: %f, text: %s\n", name, val, text);

	if ((c = smanet_get_channel(s,name)) == 0) {
		dprintf(dlevel,"channel not found\n");
		return 1;
	}
	if (c->mask & CH_STATUS) {
		if (!text) {
			dprintf(dlevel,"text is null!\n");
			return 1;
		} else {
			register char *p;
			int i,found;

			dprintf(dlevel,"text: %s\n", text);

			i=found=0;
			list_reset(c->strings);
			while((p = list_get_next(c->strings)) != 0) {
				dprintf(dlevel,"option: %s\n", p);
				if (strcmp(p,text) == 0) {
					val = i;
					found = 1;
					break;
				}
				i++;
			}
			dprintf(dlevel,"found: %d\n", found);
			if (!found) {
				sprintf(s->errmsg,"channel %s option %s not found", c->name, text);
				return 1;
			}
		}
	}
	v.type = c->type;
	switch(c->mask & 0xf) {
	case CH_ANALOG:
		if (c->mask & CH_PARA)
			_setdval(&v,val);
		else
			_setdval(&v,(val - c->offset) / c->gain);
		break;
	case CH_COUNTER:
		_setdval(&v, val / c->gain);
		break;
	default:
		_setdval(&v, val);
		break;
	}
	v.type = c->type;
	return _write_value(s,c,&v);
}

int smanet_set_and_verify_option(smanet_session_t *s, char *name, char *value) {
	int retry;
	char *text;
	double d;

	dprintf(dlevel,"name: %s, value: >>%s<<\n", name, value);

	for(retry=0; retry < 3; retry++) {
		dprintf(dlevel,"setting...retries: %d\n", retry);
		if (smanet_set_value(s, name, 0, value) == 0) {
			dprintf(dlevel,"verifying...\n");
			text = 0;
			if (smanet_get_value(s,name,&d,&text) == 0) {
				dprintf(dlevel,"d: %f, text: >>%s<<\n", d, text);
				if (text && strcmp(text,value) == 0) {
					dprintf(dlevel,"match!\n");
					return 0;
				}
			}
		}
	}
	return 0;
}

#if 0

int smanet_set_valuebyname(smanet_session_t *s, char *name, double v) {
	smanet_channel_t *c;

	c = smanet_get_channelbyname(s,name);
	if (!c) return 1;
	return smanet_set_value(s,c,v);
}

int smanet_set_optionbyname(smanet_session_t *s, char *name, char *opt) {
	smanet_channel_t *c;
	register char *p;
	smanet_value_t v;
	int i,found;

	dprintf(dlevel,"name: %s, opt: %s\n", name, opt);

	if (!s) return 1;
	/* My SI6048 simply refuses to set option 0 (no value/default) */
	if (strcmp(opt,"---") == 0) return 1;
	c = smanet_get_channelbyname(s,name);
	dprintf(dlevel,"c: %p\n", c);
	if (!c) return 1;
	if ((c->mask & CH_STATUS) == 0) return 1;
	dprintf(dlevel,"count: %d\n", list_count(c->strings));
	if (!list_count(c->strings)) return 1;
	i=found=0;
	list_reset(c->strings);
	while((p = list_get_next(c->strings)) != 0) {
		dprintf(dlevel,"option: %s\n", p);
		if (strcmp(p,opt) == 0) {
			found = 1;
			break;
		}
		i++;
	}
	dprintf(dlevel,"found: %d\n", found);
	if (!found) return 1;
	dprintf(dlevel,"i: %d\n", i);
	time(&v.timestamp);
	v.type = DATA_TYPE_BYTE;
	v.bval = i;
	return _smanet_setval(s,c,&v);
}
#endif

#if 0
double TChannel_GetValue(TChannel * me, TNetDevice * dev, int iValIndex)
{
   double dblValue;
   WORD wVal;
   DWORD dwVal;
   float fVal;
   void * value = TChanValRepo_GetValuePtr(dev->chanValRepo,me->Handle);

   if (iValIndex >= TChannel_GetValCnt(me)) goto err;

   /* Ist der Kanalwert ueberhaupt gueltig? */
   if (!TChannel_IsValueValid( me, dev )) return 0.0;

   /* Wert des Kanals holen... */
   /* Ach ja - (langer Seufzer...) Vererbung in 'C': "is nich..." */
   switch(me->wNType & CH_FORM)
   {
      case CH_BYTE :  dblValue =      *((BYTE*)  value + iValIndex); break;
      case CH_WORD :  MoveWORD (&wVal, ((WORD*)  value + iValIndex)); dblValue = wVal;  break;
      case CH_DWORD:  MoveDWORD(&dwVal,((DWORD*) value + iValIndex)); dblValue = dwVal; break;
      case CH_FLOAT4: MoveFLOAT(&fVal, ((float*) value + iValIndex)); dblValue = fVal; break;
      default: assert( 0 );
   }
   //YASDI_DEBUG((VERBOSE_BUGFINDER,"value is = %lf\n", dblValue));

   /*
   ** If the value is valid, calculate the gain and offset ....
   ** CAUTION (SMA?) TRAP:
   ** Parameter channels have no gain and offset!
   ** The two values ..are used here to record the maximum and minimum values
   ** (i.e. the range of values!)
   ** These are of course not allowed to be included in the channel value calculation
   ** include !!!!!!!!
   ** (The 3rd time fell in, 2 x SDC and now here ...)
   */
   if (!(me->wCType & CH_PARA ) && (dblValue != CHANVAL_INVALID))
   {
      //YASDI_DEBUG((VERBOSE_BUGFINDER,"Gain = %f\n", me->fGain));
      if (me->wCType & CH_COUNTER) dblValue = dblValue * (double)me->fGain;
      if (me->wCType & CH_ANALOG)  dblValue = dblValue * (double)me->fGain + (double)me->fOffset;
   }

   return dblValue;

   err:
   return 0.0;
}
#endif
