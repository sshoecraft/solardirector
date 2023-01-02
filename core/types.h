
/*
Copyright (c) 2021, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

#ifndef __SD_TYPES_H
#define __SD_TYPES_H

#include <stdint.h>
#include <sys/types.h>
#include "osendian.h"

#define DATA_TYPE_NUMBER	0x01000000
#define DATA_TYPE_ARRAY		0x02000000
#define DATA_TYPE_MLIST		0x04000000
#define DATA_ATTR_PTR		0x08000000

#define DATA_TYPE_NULL		0x00000001		/* No value (void) */
#define DATA_TYPE_STRING	0x00000002		/* Array of chars w/null */

#define DATA_TYPE_S8	(DATA_TYPE_NUMBER | 0x00000001)	/* 8 bit signed int (byte) */
#define DATA_TYPE_S16	(DATA_TYPE_NUMBER | 0x00000002)	/* 16 bit signed int (short) */
#define DATA_TYPE_S32	(DATA_TYPE_NUMBER | 0x00000004)	/* 32 bit signed int (long) */
#define DATA_TYPE_S64	(DATA_TYPE_NUMBER | 0x00000008)	/* 64 bit signed int (quad) */
#define DATA_TYPE_U8	(DATA_TYPE_NUMBER | 0x00000010)	/* 8 bit unsigned int */
#define DATA_TYPE_U16	(DATA_TYPE_NUMBER | 0x00000020)	/* 16 bit unsigned int */
#define DATA_TYPE_U32	(DATA_TYPE_NUMBER | 0x00000040)	/* 32 bit unsigned int */
#define DATA_TYPE_U64	(DATA_TYPE_NUMBER | 0x00000080)	/* 64 bit unsigned int */
#define DATA_TYPE_F32	(DATA_TYPE_NUMBER | 0x00000100)	/* 32 bit floating point (float) */
#define DATA_TYPE_F64	(DATA_TYPE_NUMBER | 0x00000200)	/* 64 bit floating point (double) */
#define DATA_TYPE_F128	(DATA_TYPE_NUMBER | 0x00000400)	/* 128 bit floating point (long double) */
#define DATA_TYPE_BOOL	(DATA_TYPE_NUMBER | 0x00008000)	/* 32 bit signed int */

#define DATA_TYPE_STRINGP (DATA_ATTR_PTR | DATA_TYPE_STRING)
#define DATA_TYPE_VOIDP (DATA_ATTR_PTR | DATA_TYPE_NULL)
#define DATA_TYPE_S8P	(DATA_ATTR_PTR | DATA_TYPE_S8)
#define DATA_TYPE_S16P	(DATA_ATTR_PTR | DATA_TYPE_S16)
#define DATA_TYPE_S32P	(DATA_ATTR_PTR | DATA_TYPE_S32)
#define DATA_TYPE_S64P	(DATA_ATTR_PTR | DATA_TYPE_S64)
#define DATA_TYPE_U8P	(DATA_ATTR_PTR | DATA_TYPE_U8)
#define DATA_TYPE_U16P	(DATA_ATTR_PTR | DATA_TYPE_U16)
#define DATA_TYPE_U32P	(DATA_ATTR_PTR | DATA_TYPE_U32)
#define DATA_TYPE_U64P	(DATA_ATTR_PTR | DATA_TYPE_U64)
#define DATA_TYPE_F32P	(DATA_ATTR_PTR | DATA_TYPE_F32)
#define DATA_TYPE_F64P	(DATA_ATTR_PTR | DATA_TYPE_F64)
#define DATA_TYPE_F128P	(DATA_ATTR_PTR | DATA_TYPE_F128)
#define DATA_TYPE_BOOLP	(DATA_ATTR_PTR | DATA_TYPE_BOOL)

#define DATA_TYPE_STRING_ARRAY	(DATA_TYPE_ARRAY | DATA_TYPE_STRING)
#define DATA_TYPE_VOIDP_ARRAY	(DATA_TYPE_ARRAY | DATA_TYPE_VOIDP)

#define DATA_TYPE_S8_ARRAY	(DATA_TYPE_ARRAY | DATA_TYPE_S8)
#define DATA_TYPE_S16_ARRAY	(DATA_TYPE_ARRAY | DATA_TYPE_S16)
#define DATA_TYPE_S32_ARRAY	(DATA_TYPE_ARRAY | DATA_TYPE_S32)
#define DATA_TYPE_S64_ARRAY	(DATA_TYPE_ARRAY | DATA_TYPE_S64)
#define DATA_TYPE_U8_ARRAY	(DATA_TYPE_ARRAY | DATA_TYPE_U8)
#define DATA_TYPE_U16_ARRAY	(DATA_TYPE_ARRAY | DATA_TYPE_U16)
#define DATA_TYPE_U32_ARRAY	(DATA_TYPE_ARRAY | DATA_TYPE_U32)
#define DATA_TYPE_U64_ARRAY	(DATA_TYPE_ARRAY | DATA_TYPE_U64)
#define DATA_TYPE_F32_ARRAY	(DATA_TYPE_ARRAY | DATA_TYPE_F32)
#define DATA_TYPE_F64_ARRAY	(DATA_TYPE_ARRAY | DATA_TYPE_F64)
#define DATA_TYPE_F128_ARRAY	(DATA_TYPE_ARRAY | DATA_TYPE_F128)
#define DATA_TYPE_BOOL_ARRAY	(DATA_TYPE_ARRAY | DATA_TYPE_BOOL)

#define DATA_TYPE_STRING_LIST	(DATA_TYPE_MLIST | DATA_TYPE_STRING) 	/* list of character strings */

#define DATA_TYPE_ISNUMBER(t) ((t & DATA_TYPE_NUMBER) != 0)
#define DATA_TYPE_ISARRAY(t) ((t & DATA_TYPE_ARRAY) != 0)
#define DATA_TYPE_ISLIST(t) ((t & DATA_TYPE_MLIST) != 0)

/* Aliases */
#define NULLCH '\0'
#define DATA_TYPE_UNKNOWN DATA_TYPE_NULL
#define DATA_TYPE_VOID DATA_TYPE_NULL
#define DATA_TYPE_BYTE DATA_TYPE_S8
#define DATA_TYPE_BYTE_ARRAY DATA_TYPE_S8_ARRAY
#define	DATA_TYPE_SHORT DATA_TYPE_S16
#define	DATA_TYPE_SHORT_ARRAY DATA_TYPE_S16_ARRAY
#define	DATA_TYPE_INT DATA_TYPE_S32
#define	DATA_TYPE_INT_ARRAY DATA_TYPE_S32_ARRAY
#define	DATA_TYPE_LONG DATA_TYPE_S32
#define	DATA_TYPE_LONG_ARRAY DATA_TYPE_S32_ARRAY
#define	DATA_TYPE_QUAD DATA_TYPE_S64
#define	DATA_TYPE_QUAD_ARRAY DATA_TYPE_S64_ARRAY
#define DATA_TYPE_UBYTE DATA_TYPE_U8
#define DATA_TYPE_USHORT DATA_TYPE_U16
#define DATA_TYPE_UINT DATA_TYPE_U32
#define DATA_TYPE_ULONG DATA_TYPE_U32
#define DATA_TYPE_UQUAD DATA_TYPE_U64
#define DATA_TYPE_FLOAT DATA_TYPE_F32
#define DATA_TYPE_DOUBLE DATA_TYPE_F64
#define DATA_TYPE_BOOLEAN DATA_TYPE_BOOL
#define DATA_TYPE_LOGICAL DATA_TYPE_BOOL
#define DATA_TYPE_FLOAT_ARRAY DATA_TYPE_F32_ARRAY
#define DATA_TYPE_DOUBLE_ARRAY DATA_TYPE_F64_ARRAY

//typedef int data_type_t;

//typedef uint8_t bool_t;
#ifdef bool
#undef bool
#endif
typedef int bool;

char *typestr(int);
int typesize(int);

#ifndef TARGET_ENDIAN
#define TARGET_ENDIAN BYTE_ORDER
#endif

#if TARGET_ENDIAN == LITTLE_ENDIAN

#if BYTE_ORDER == LITTLE_ENDIAN

/* TARGET_ENDIAN = LITTLE_ENDIAN & MY ENDIAN = LITTLE_ENDIAN */
#define _gets8(v) (int8_t) *((int8_t *)(v))
#define _gets16(v) (int16_t) *((int16_t *)(v))
#define _gets32(v) (int32_t) *((int32_t *)(v))
#define _gets64(v) (int64_t) *((int64_t *)(v))
#define _getu8(v) (uint8_t) *((uint8_t *)(v))
#define _getu16(v) (uint16_t) *((uint16_t *)(v))
#define _getu32(v) (uint32_t) *((uint32_t *)(v))
#define _getu64(v) (uint64_t) *((uint64_t *)(v))
#define _getf32(v) (float) *((float *)(v))
#define _getf64(v) (double) *( (double *)(v) )
#define _puts8(p,v) *((int8_t *)(p)) = (v)
#define _puts16(p,v) *((int16_t *)(p)) = (v)
#define _puts32(p,v) *((int32_t *)(p)) = (v)
#define _puts64(p,v)  *((int64_t *)(p)) = (v)
#define _putu8(p,v) *((uint8_t *)(p)) = (v)
//#define _putu16(p,v) { float tmp; *((p+1)) = ((uint16_t)(tmp = v) >> 8); *((p)) = (uint16_t)(tmp = v); }
//#define _putu16(p,v) { uint16_t n; *((uint16_t *)(p)) = (n = v); }
#define _putu16(p,v) *((uint16_t *)(p)) = (v)
#define _putu32(p,v) *((uint32_t *)(p)) = (v)
#define _putu64(p,v)  *((uint64_t *)(p)) = (v)
#define _putf32(p,v) *((float *)(p)) = (v)
#define _putf64(p,v) *((double *)(p)) = (v)

#else /* BYTE_ORDER != LITTLE_ENDIAN */

/* TARGET_ENDIAN = LITTLE_ENDIAN & MY ENDIAN = BIG_ENDIAN */
#define _gets8(v) (char)( ((char)(v)[0]) )
#define _gets16(v) (short)( ((short)(v)[1]) << 8 | ((short)(v)[0]) )
#define _gets32(v) (long)( ((long)(v)[3]) << 24 | ((long)(v)[2]) << 16 | ((long)(v)[1]) << 8 | ((long)(v)[0]) )
#define _gets64(v) (long long)( ((long long)(v)[7]) << 56 | ((long long)(v)[6]) << 48 | ((long long)(v)[5]) << 40 | ((long long)(v)[4]) << 32 | ((long long)(v)[3]) << 24 | ((long long)(v)[2]) << 16 | ((long long)(v)[1]) << 8 | ((long long)(v)[0]) )
#define _getu8(v) (unsigned char)( ((unsigned char)(v)[0]) )
#define _getu16(v) (unsigned short)( ((unsigned short)(v)[1]) << 8 | ((unsigned short)(v)[0]) )
#define _getu32(v) (unsigned long)( ((unsigned long)(v)[3]) << 24 | ((unsigned long)(v)[2]) << 16 | ((unsigned long)(v)[1]) << 8 | ((unsigned long)(v)[0]) )
#define _getu64(v) (unsigned long long)( ((unsigned long long)(v)[7]) << 56 | ((unsigned long long)(v)[6]) << 48 | ((unsigned long long)(v)[5]) << 40 | ((unsigned long long)(v)[4]) << 32 | ((unsigned long long)(v)[3]) << 24 | ((unsigned long long)(v)[2]) << 16 | ((unsigned long long)(v)[1]) << 8 | ((unsigned long long)(v)[0]) )
static inline float _getf32(unsigned char *v) {
	long val = ( ((long)v[3]) << 24 | ((long)v[2]) << 16 | ((long)v[1]) << 8 | ((long)v[0]) );
	return *(float *)&val;
}
static inline float _getf64(unsigned char *v) {
	long long val = ( ((long long)(v)[7]) << 56 | ((long long)(v)[6]) << 48 | ((long long)(v)[5]) << 40 | ((long long)(v)[4]) << 32 | ((long long)(v)[3]) << 24 | ((long long)(v)[2]) << 16 | ((long long)(v)[1]) << 8 | ((long long)(v)[0]) );
	return *(double *)&val;
}
//#define si_putlong(p,v) { *((p+3)) = ((int)(v) >> 24); *((p)+2) = ((int)(v) >> 16); *((p+1)) = ((int)(v) >> 8); *((p)) = ((int)(v) & 0x0F); }
#define _puts8(p,v) *((int8_t *)(p)) = (v)
#define _puts16(p,v) { float t; *((p+1)) = ((short)(t = v) >> 8); *((p)) = (short)(t = v); }
#define _puts32(p,v) { float t; *((p+1)) = ((long)(t = v) >> 24); *((p+1)) = ((long)(t = v) >> 16); *((p+1)) = ((long)(t = v) >> 8); *((p)) = (long)(t = v); }
#define _puts64(p,v) { double t; *((p+1)) = ((long long)(t = v) >> 56); *((p+1)) = ((long long)(t = v) >> 48); *((p+1)) = ((long long)(t = v) >> 40); *((p+1)) = ((long long)(t = v) >> 32); *((p+1)) = ((long long)(t = v) >> 24); *((p+1)) = ((long long)(t = v) >> 16); *((p+1)) = ((long long)(t = v) >> 8); *((p)) = (long long)(t = v); }
#define _putu8(p,v) *((uint8_t *)(p)) = (v)
#define _putu16(p,v) { float t; *((p+1)) = ((uint16_t)(t = v) >> 8); *((p)) = (uint16_t)(t = v); }
#define _putu32(p,v) { float t; *((p+1)) = ((uint32_t)(t = v) >> 24); *((p+1)) = ((uint32_t)(t = v) >> 16); *((p+1)) = ((uint32_t)(t = v) >> 8); *((p)) = (uint32_t)(t = v); }
#define _putu64(p,v) { double t; *((p+1)) = ((unsigned long long)(t = v) >> 56); *((p+1)) = ((unsigned long long)(t = v) >> 48); *((p+1)) = ((unsigned long long)(t = v) >> 40); *((p+1)) = ((unsigned long long)(t = v) >> 32); *((p+1)) = ((unsigned long long)(t = v) >> 24); *((p+1)) = ((unsigned long long)(t = v) >> 16); *((p+1)) = ((unsigned long long)(t = v) >> 8); *((p)) = (unsigned long long)(t = v); }
#define _putf32(p,v) _putu32(p,v)
#define _putf64(p,v) _putu64(p,v)

#endif /* BYTE_ORDER == LITTLE_ENDIAN */

#else /* TARGET_ENDIAN != LITTLE_ENDIAN */

#if BYTE_ORDER == LITTLE_ENDIAN

/* TARGET_ENDIAN = BIG_ENDIAN & MY ENDIAN = LITTLE_ENDIAN */
#define _gets8(v) (char)( ((char)(v)[0]) )
#define _gets16(v) (short)( ((short)(v)[0]) << 8 | ((short)(v)[1]) )
#define _gets32(v) (long)( ((long)(v)[0]) << 24 | ((long)(v)[1]) << 16 | ((long)(v)[2]) << 8 | ((long)(v)[3]) )
#define _gets64(v) (long long)( ((long long)(v)[0]) << 56 | ((long long)(v)[1]) << 48 | ((long long)(v)[2]) << 40 | ((long long)(v)[3]) << 32 | ((long long)(v)[4]) << 24 | ((long long)(v)[5]) << 16 | ((long long)(v)[6]) << 8 | ((long long)(v)[7]) )
#define _getu8(v) (unsigned char)( ((unsigned char)(v)[0]) )
#define _getu16(v) (unsigned short)( ((unsigned short)(v)[0]) << 8 | ((unsigned short)(v)[1]) )
#define _getu32(v) (unsigned long)( ((unsigned long)(v)[0]) << 24 | ((unsigned long)(v)[1]) << 16 | ((unsigned long)(v)[2]) << 8 | ((unsigned long)(v)[3]) )
#define _getu64(v) (unsigned long long)( ((unsigned long long)(v)[0]) << 56 | ((unsigned long long)(v)[1]) << 48 | ((unsigned long long)(v)[2]) << 40 | ((unsigned long long)(v)[3]) << 32 | ((unsigned long long)(v)[4]) << 24 | ((unsigned long long)(v)[5]) << 16 | ((unsigned long long)(v)[6]) << 8 | ((unsigned long long)(v)[7]) )
static inline float _getf32(unsigned char *v) {
	long val = ( ((long)v[0]) << 24 | ((long)v[1]) << 16 | ((long)v[2]) << 8 | ((long)v[3]) );
	return *(float *)&val;
}
static inline float _getf64(unsigned char *v) {
	long long val = ( ((long long)(v)[0]) << 56 | ((long long)(v)[1]) << 48 | ((long long)(v)[2]) << 40 | ((long long)(v)[3]) << 32 | ((long long)(v)[4]) << 24 | ((long long)(v)[5]) << 16 | ((long long)(v)[6]) << 8 | ((long long)(v)[7]) );
	return *(double *)&val;
}
#define _puts8(p,v) *((int8_t *)(p)) = (v)
#define _puts16(p,v) { float t; *((p+0)) = ((short)(t = v) >> 8); *((p+1)) = (short)(t = v); }
#define _puts32(p,v) { float t; *((p+0)) = ((long)(t = v) >> 24); *((p+1)) = ((long)(t = v) >> 16); *((p+2)) = ((long)(t = v) >> 8); *((p+3)) = (long)(t = v); }
#define _puts64(p,v) { double t; *((p+0)) = ((long long)(t = v) >> 56); *((p+1)) = ((long long)(t = v) >> 48); *((p+2)) = ((long long)(t = v) >> 40); *((p+3)) = ((long long)(t = v) >> 32); *((p+4)) = ((long long)(t = v) >> 24); *((p+5)) = ((long long)(t = v) >> 16); *((p+6)) = ((long long)(t = v) >> 8); *((p+7)) = (long long)(t = v); }
#define _putu8(p,v) *((uint8_t *)(p)) = (v)
#define _putu16(p,v) { float t; *((p+0)) = ((unsigned short)(t = v) >> 8); *((p+1)) = (unsigned short)(t = v); }
#define _putu32(p,v) { float t; *((p+0)) = ((unsigned long)(t = v) >> 24); *((p+1)) = ((unsigned long)(t = v) >> 16); *((p+2)) = ((unsigned long)(t = v) >> 8); *((p+3)) = (unsigned long)(t = v); }
#define _putu64(p,v) { double t; *((p+0)) = ((unsigned long long)(t = v) >> 56); *((p+1)) = ((unsigned long long)(t = v) >> 48); *((p+2)) = ((unsigned long long)(t = v) >> 40); *((p+3)) = ((unsigned long long)(t = v) >> 32); *((p+4)) = ((unsigned long long)(t = v) >> 24); *((p+5)) = ((unsigned long long)(t = v) >> 16); *((p+6)) = ((unsigned long long)(t = v) >> 8); *((p+7)) = (unsigned long long)(t = v); }
#define _putf32(p,v) _putu32(p,v)
#define _putf64(p,v) _putu64(p,v)

#else

/* TARGET_ENDIAN = BIG_ENDIAN & MY ENDIAN = BIG_ENDIAN */
#define _gets8(v) (int8_t) *((int8_t *)(v))
#define _gets16(v) (int16_t) *((int16_t *)(v))
#define _gets32(v) (int32_t) *((int32_t *)(v))
#define _gets64(v) (int64_t) *((int64_t *)(v))
#define _getu8(v) (uint8_t) *((uint8_t *)(v))
#define _getu16(v) (uint16_t) *((uint16_t *)(v))
#define _getu32(v) (uint32_t) *((uint32_t *)(v))
#define _getu64(v) (uint64_t) *((uint64_t *)(v))
#define _getf32(v) (float) *((float *)(v))
#define _getf64(v) (double) *( (double *)(v) )
#define _puts8(p,v) *((int8_t *)(p)) = (v)
#define _puts16(p,v) *((int16_t *)(p)) = (v)
#define _puts32(p,v) *((int32_t *)(p)) = (v)
#define _puts64(p,v)  *((int64_t *)(p)) = (v)
#define _putu8(p,v) *((uint8_t *)(p)) = (v)
#define _putu16(p,v) *((uint16_t *)(p)) = (v)
#define _putu32(p,v) *((uint32_t *)(p)) = (v)
#define _putu64(p,v)  *(u(int64_t *)(p)) = (v)
#define _putf32(p,v) *((float *)(p)) = (v)
#define _putf64(p,v) *((double *)(p)) = (v)
#endif
#endif

#define _getshort(x) _gets16(x)
#define _getlong(x) _gets32(x)
#define _getfloat(x) _getf32(x)
#define _putshort(p,v) _puts16(p,v)
#define _putlong(p,v) _puts32(p,v)
#define _putfloat(p,v) _putf32(p,v)

#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

#ifdef JS
#include "jsapi.h"
int js_types_init(JSContext *cx, JSObject *parent, void *priv);
#endif

#endif /* __SD_TYPES_H */
