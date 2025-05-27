
///////////////////////////////////////////////////////////////////////////////
// \author (c) Marco Paland (info@paland.com)
//             2014-2019, PALANDesign Hannover, Germany
//
// \license The MIT License (MIT)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// \brief Tiny printf, sprintf and (v)snprintf implementation, optimized for speed on
//        embedded systems with a very limited resources. These routines are thread
//        safe and reentrant!
//        Use this instead of the bloated standard/newlib printf cause these use
//        malloc for printf (and may not be thread safe).
//
///////////////////////////////////////////////////////////////////////////////

#define DEBUG_THIS 1
#define DEBUG_JSPRINTF 0

#ifdef DEBUG
#undef DEBUG
#define DEBUG DEBUG_THIS
#endif

#include "debug.h"

#define dlevel 8

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

//#include "printf.h"
#include "jsengine.h"
#include "jscntxt.h"
#include "jsinterp.h"
#include "jsscript.h"
#include "jsdbgapi.h"
#include "jsdtracef.h"
#include "jsprf.h"
#include "jsutil.h"

// define this globally (e.g. gcc -DPRINTF_INCLUDE_CONFIG_H ...) to include the
// printf_config.h header file
// default: undefined
#ifdef PRINTF_INCLUDE_CONFIG_H
#include "printf_config.h"
#endif


// 'ntoa' conversion buffer size, this must be big enough to hold one converted
// numeric number including padded zeros (dynamically created on stack)
// default: 32 byte
#ifndef PRINTF_NTOA_BUFFER_SIZE
#define PRINTF_NTOA_BUFFER_SIZE    32U
#endif

// 'ftoa' conversion buffer size, this must be big enough to hold one converted
// float number including padded zeros (dynamically created on stack)
// default: 32 byte
#ifndef PRINTF_FTOA_BUFFER_SIZE
#define PRINTF_FTOA_BUFFER_SIZE    32U
#endif

// support for the floating point type (%f)
// default: activated
#ifndef PRINTF_DISABLE_SUPPORT_FLOAT
#define PRINTF_SUPPORT_FLOAT
#endif

// support for exponential floating point notation (%e/%g)
// default: activated
#ifndef PRINTF_DISABLE_SUPPORT_EXPONENTIAL
#define PRINTF_SUPPORT_EXPONENTIAL
#endif

// define the default floating point precision
// default: 6 digits
#ifndef PRINTF_DEFAULT_FLOAT_PRECISION
#define PRINTF_DEFAULT_FLOAT_PRECISION  6U
#endif

// define the largest float suitable to print with %f
// default: 1e9
#ifndef PRINTF_MAX_FLOAT
#define PRINTF_MAX_FLOAT  1e9
#endif

// support for the long long types (%llu or %p)
// default: activated
#ifndef PRINTF_DISABLE_SUPPORT_LONG_LONG
#define PRINTF_SUPPORT_LONG_LONG
#endif

// support for the ptrdiff_t type (%t)
// ptrdiff_t is normally defined in <stddef.h> as long or long long type
// default: activated
#ifndef PRINTF_DISABLE_SUPPORT_PTRDIFF_T
#define PRINTF_SUPPORT_PTRDIFF_T
#endif

///////////////////////////////////////////////////////////////////////////////

// internal flag definitions
#define FLAGS_ZEROPAD   (1U <<  0U)
#define FLAGS_LEFT      (1U <<  1U)
#define FLAGS_PLUS      (1U <<  2U)
#define FLAGS_SPACE     (1U <<  3U)
#define FLAGS_HASH      (1U <<  4U)
#define FLAGS_UPPERCASE (1U <<  5U)
#define FLAGS_CHAR      (1U <<  6U)
#define FLAGS_SHORT     (1U <<  7U)
#define FLAGS_LONG      (1U <<  8U)
#define FLAGS_LONG_LONG (1U <<  9U)
#define FLAGS_PRECISION (1U << 10U)
#define FLAGS_ADAPT_EXP (1U << 11U)


// import float.h for DBL_MAX
#if defined(PRINTF_SUPPORT_FLOAT)
#include <float.h>
#endif

struct _bufinfo {
	JSContext *cx;
	char *ptr;
	int size;
};
typedef struct _bufinfo bufinfo_t;
#define BUF_CHUNKSIZE 256

// output function type
typedef void (*out_fct_type)(char character, bufinfo_t *b, size_t idx);


// wrapper (used as buffer) for output function type
typedef struct {
  void  (*fct)(char character, void* arg);
  void* arg;
} out_fct_wrap_type;


// internal buffer output
static inline void _out_buffer(char ch, bufinfo_t *b, size_t idx) {
	dprintf(dlevel,"idx: %ld, size: %ld\n", idx, b->size);
        if (idx >= b->size) {
		char *newbuf;
		int newsize;

		newsize = (idx / 4096) + 1;
		newsize *= 4096;
		dprintf(dlevel,"newsize: %d\n", newsize);
		newbuf = JS_realloc(b->cx, b->ptr, newsize);
		if (!newbuf) {
			log_syserror("_out_buffer: JS_realloc(buffer,%d)",newsize);
			return;
		}
		b->ptr = newbuf;
		b->size = newsize;
	}
//	*(*buffer + idx) = character;
	b->ptr[idx] = ch;
}


#if 0
// internal output function wrapper
static inline void _out_fct(char character, void*b, size_t idx, size_t maxlen)
{
  (void)idx; (void)maxlen;
  if (character) {
    // buffer is the output fct pointer
    ((out_fct_wrap_type*)buffer)->fct(character, ((out_fct_wrap_type*)buffer)->arg);
  }
}
#endif


// internal secure strlen
// \return The length of the string (excluding the terminating 0) limited by 'maxsize'
static inline unsigned int _strnlen_s(const char* str, size_t maxsize)
{
  const char* s;
  for (s = str; *s && maxsize--; ++s);
  return (unsigned int)(s - str);
}


// internal test if char is a digit (0-9)
// \return true if char is a digit
static inline bool _is_digit(char ch)
{
  return (ch >= '0') && (ch <= '9');
}


// internal ASCII string to unsigned int conversion
static unsigned int _atoi(const char** str)
{
  unsigned int i = 0U;
  while (_is_digit(**str)) {
    i = i * 10U + (unsigned int)(*((*str)++) - '0');
  }
  return i;
}


// output the specified string in reverse, taking care of any zero-padding
static size_t _out_rev(out_fct_type out, bufinfo_t *b, size_t idx, const char* buf, size_t len, unsigned int width, unsigned int flags)
{
  const size_t start_idx = idx;
	size_t i;

  // pad spaces up to given width
  if (!(flags & FLAGS_LEFT) && !(flags & FLAGS_ZEROPAD)) {
    for (i = len; i < width; i++) {
      out(' ',b, idx++);
    }
  }

  // reverse string
  while (len) {
    out(buf[--len],b, idx++);
  }

  // append pad spaces up to given width
  if (flags & FLAGS_LEFT) {
    while (idx - start_idx < width) {
      out(' ',b, idx++);
    }
  }

  return idx;
}


// internal itoa format
static size_t _ntoa_format(out_fct_type out, bufinfo_t *b, size_t idx, char* buf, size_t len, bool negative, unsigned int base, unsigned int prec, unsigned int width, unsigned int flags)
{
  // pad leading zeros
  if (!(flags & FLAGS_LEFT)) {
    if (width && (flags & FLAGS_ZEROPAD) && (negative || (flags & (FLAGS_PLUS | FLAGS_SPACE)))) {
      width--;
    }
    while ((len < prec) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
      buf[len++] = '0';
    }
    while ((flags & FLAGS_ZEROPAD) && (len < width) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
      buf[len++] = '0';
    }
  }

  // handle hash
  if (flags & FLAGS_HASH) {
    if (!(flags & FLAGS_PRECISION) && len && ((len == prec) || (len == width))) {
      len--;
      if (len && (base == 16U)) {
        len--;
      }
    }
    if ((base == 16U) && !(flags & FLAGS_UPPERCASE) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
      buf[len++] = 'x';
    }
    else if ((base == 16U) && (flags & FLAGS_UPPERCASE) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
      buf[len++] = 'X';
    }
    else if ((base == 2U) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
      buf[len++] = 'b';
    }
    if (len < PRINTF_NTOA_BUFFER_SIZE) {
      buf[len++] = '0';
    }
  }

  if (len < PRINTF_NTOA_BUFFER_SIZE) {
    if (negative) {
      buf[len++] = '-';
    }
    else if (flags & FLAGS_PLUS) {
      buf[len++] = '+';  // ignore the space if the '+' exists
    }
    else if (flags & FLAGS_SPACE) {
      buf[len++] = ' ';
    }
  }

  return _out_rev(out,b, idx, buf, len, width, flags);
}


// internal itoa for 'long' type
static size_t _ntoa_long(out_fct_type out, bufinfo_t *b, size_t idx, unsigned long value, bool negative, unsigned long base, unsigned int prec, unsigned int width, unsigned int flags)
{
  char buf[PRINTF_NTOA_BUFFER_SIZE];
  size_t len = 0U;

  // no hash for 0 values
  if (!value) {
    flags &= ~FLAGS_HASH;
  }

  // write if precision != 0 and value is != 0
  if (!(flags & FLAGS_PRECISION) || value) {
    do {
      const char digit = (char)(value % base);
      buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
      value /= base;
    } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
  }

  return _ntoa_format(out,b, idx, buf, len, negative, (unsigned int)base, prec, width, flags);
}


// internal itoa for 'long long' type
#if defined(PRINTF_SUPPORT_LONG_LONG)
static size_t _ntoa_long_long(out_fct_type out, bufinfo_t *b, size_t idx, unsigned long long value, bool negative, unsigned long long base, unsigned int prec, unsigned int width, unsigned int flags)
{
  char buf[PRINTF_NTOA_BUFFER_SIZE];
  size_t len = 0U;

  // no hash for 0 values
  if (!value) {
    flags &= ~FLAGS_HASH;
  }

  // write if precision != 0 and value is != 0
  if (!(flags & FLAGS_PRECISION) || value) {
    do {
      const char digit = (char)(value % base);
      buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
      value /= base;
    } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
  }

  return _ntoa_format(out,b, idx, buf, len, negative, (unsigned int)base, prec, width, flags);
}
#endif  // PRINTF_SUPPORT_LONG_LONG


#if defined(PRINTF_SUPPORT_FLOAT)

#if defined(PRINTF_SUPPORT_EXPONENTIAL)
// forward declaration so that _ftoa can switch to exp notation for values > PRINTF_MAX_FLOAT
static size_t _etoa(out_fct_type out, bufinfo_t *b, size_t idx, double value, unsigned int prec, unsigned int width, unsigned int flags);
#endif


// internal ftoa for fixed decimal floating point
static size_t _ftoa(out_fct_type out, bufinfo_t *b, size_t idx, double value, unsigned int prec, unsigned int width, unsigned int flags)
{
  char buf[PRINTF_FTOA_BUFFER_SIZE];
  size_t len  = 0U;
  double diff = 0.0;

  // powers of 10
  static const double pow10[] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000 };

  // test for special values
  if (value != value)
    return _out_rev(out,b, idx, "nan", 3, width, flags);
  if (value < -DBL_MAX)
    return _out_rev(out,b, idx, "fni-", 4, width, flags);
  if (value > DBL_MAX)
    return _out_rev(out,b, idx, (flags & FLAGS_PLUS) ? "fni+" : "fni", (flags & FLAGS_PLUS) ? 4U : 3U, width, flags);

  // test for very large values
  // standard printf behavior is to print EVERY whole number digit -- which could be 100s of characters overflowing your buffers == bad
  if ((value > PRINTF_MAX_FLOAT) || (value < -PRINTF_MAX_FLOAT)) {
#if defined(PRINTF_SUPPORT_EXPONENTIAL)
    return _etoa(out,b, idx, value, prec, width, flags);
#else
    return 0U;
#endif
  }

  // test for negative
  bool negative = false;
  if (value < 0) {
    negative = true;
    value = 0 - value;
  }

  // set default precision, if not set explicitly
  if (!(flags & FLAGS_PRECISION)) {
    prec = PRINTF_DEFAULT_FLOAT_PRECISION;
  }
  // limit precision to 9, cause a prec >= 10 can lead to overflow errors
  while ((len < PRINTF_FTOA_BUFFER_SIZE) && (prec > 9U)) {
    buf[len++] = '0';
    prec--;
  }

  int whole = (int)value;
  double tmp = (value - whole) * pow10[prec];
  unsigned long frac = (unsigned long)tmp;
  diff = tmp - frac;

  if (diff > 0.5) {
    ++frac;
    // handle rollover, e.g. case 0.99 with prec 1 is 1.0
    if (frac >= pow10[prec]) {
      frac = 0;
      ++whole;
    }
  }
  else if (diff < 0.5) {
  }
  else if ((frac == 0U) || (frac & 1U)) {
    // if halfway, round up if odd OR if last digit is 0
    ++frac;
  }

  if (prec == 0U) {
    diff = value - (double)whole;
    if ((!(diff < 0.5) || (diff > 0.5)) && (whole & 1)) {
      // exactly 0.5 and ODD, then round up
      // 1.5 -> 2, but 2.5 -> 2
      ++whole;
    }
  }
  else {
    unsigned int count = prec;
    // now do fractional part, as an unsigned number
    while (len < PRINTF_FTOA_BUFFER_SIZE) {
      --count;
      buf[len++] = (char)(48U + (frac % 10U));
      if (!(frac /= 10U)) {
        break;
      }
    }
    // add extra 0s
    while ((len < PRINTF_FTOA_BUFFER_SIZE) && (count-- > 0U)) {
      buf[len++] = '0';
    }
    if (len < PRINTF_FTOA_BUFFER_SIZE) {
      // add decimal
      buf[len++] = '.';
    }
  }

  // do whole part, number is reversed
  while (len < PRINTF_FTOA_BUFFER_SIZE) {
    buf[len++] = (char)(48 + (whole % 10));
    if (!(whole /= 10)) {
      break;
    }
  }

  // pad leading zeros
  if (!(flags & FLAGS_LEFT) && (flags & FLAGS_ZEROPAD)) {
    if (width && (negative || (flags & (FLAGS_PLUS | FLAGS_SPACE)))) {
      width--;
    }
    while ((len < width) && (len < PRINTF_FTOA_BUFFER_SIZE)) {
      buf[len++] = '0';
    }
  }

  if (len < PRINTF_FTOA_BUFFER_SIZE) {
    if (negative) {
      buf[len++] = '-';
    }
    else if (flags & FLAGS_PLUS) {
      buf[len++] = '+';  // ignore the space if the '+' exists
    }
    else if (flags & FLAGS_SPACE) {
      buf[len++] = ' ';
    }
  }

  return _out_rev(out,b, idx, buf, len, width, flags);
}


#if defined(PRINTF_SUPPORT_EXPONENTIAL)
// internal ftoa variant for exponential floating-point type, contributed by Martijn Jasperse <m.jasperse@gmail.com>
static size_t _etoa(out_fct_type out, bufinfo_t *b, size_t idx, double value, unsigned int prec, unsigned int width, unsigned int flags)
{
  // check for NaN and special values
  if ((value != value) || (value > DBL_MAX) || (value < -DBL_MAX)) {
    return _ftoa(out,b, idx, value, prec, width, flags);
  }

  // determine the sign
  const bool negative = value < 0;
  if (negative) {
    value = -value;
  }

  // default precision
  if (!(flags & FLAGS_PRECISION)) {
    prec = PRINTF_DEFAULT_FLOAT_PRECISION;
  }

  // determine the decimal exponent
  // based on the algorithm by David Gay (https://www.ampl.com/netlib/fp/dtoa.c)
  union {
    uint64_t U;
    double   F;
  } conv;

  conv.F = value;
  int exp2 = (int)((conv.U >> 52U) & 0x07FFU) - 1023;           // effectively log2
  conv.U = (conv.U & ((1ULL << 52U) - 1U)) | (1023ULL << 52U);  // drop the exponent so conv.F is now in [1,2)
  // now approximate log10 from the log2 integer part and an expansion of ln around 1.5
  int expval = (int)(0.1760912590558 + exp2 * 0.301029995663981 + (conv.F - 1.5) * 0.289529654602168);
  // now we want to compute 10^expval but we want to be sure it won't overflow
  exp2 = (int)(expval * 3.321928094887362 + 0.5);
  const double z  = expval * 2.302585092994046 - exp2 * 0.6931471805599453;
  const double z2 = z * z;
  conv.U = (uint64_t)(exp2 + 1023) << 52U;
  // compute exp(z) using continued fractions, see https://en.wikipedia.org/wiki/Exponential_function#Continued_fractions_for_ex
  conv.F *= 1 + 2 * z / (2 - z + (z2 / (6 + (z2 / (10 + z2 / 14)))));
  // correct for rounding errors
  if (value < conv.F) {
    expval--;
    conv.F /= 10;
  }

  // the exponent format is "%+03d" and largest value is "307", so set aside 4-5 characters
  unsigned int minwidth = ((expval < 100) && (expval > -100)) ? 4U : 5U;

  // in "%g" mode, "prec" is the number of *significant figures* not decimals
  if (flags & FLAGS_ADAPT_EXP) {
    // do we want to fall-back to "%f" mode?
    if ((value >= 1e-4) && (value < 1e6)) {
      if ((int)prec > expval) {
        prec = (unsigned)((int)prec - expval - 1);
      }
      else {
        prec = 0;
      }
      flags |= FLAGS_PRECISION;   // make sure _ftoa respects precision
      // no characters in exponent
      minwidth = 0U;
      expval   = 0;
    }
    else {
      // we use one sigfig for the whole part
      if ((prec > 0) && (flags & FLAGS_PRECISION)) {
        --prec;
      }
    }
  }

  // will everything fit?
  unsigned int fwidth = width;
  if (width > minwidth) {
    // we didn't fall-back so subtract the characters required for the exponent
    fwidth -= minwidth;
  } else {
    // not enough characters, so go back to default sizing
    fwidth = 0U;
  }
  if ((flags & FLAGS_LEFT) && minwidth) {
    // if we're padding on the right, DON'T pad the floating part
    fwidth = 0U;
  }

  // rescale the float value
  if (expval) {
    value /= conv.F;
  }

  // output the floating part
  const size_t start_idx = idx;
  idx = _ftoa(out,b, idx, negative ? -value : value, prec, fwidth, flags & ~FLAGS_ADAPT_EXP);

  // output the exponent part
  if (minwidth) {
    // output the exponential symbol
    out((flags & FLAGS_UPPERCASE) ? 'E' : 'e',b, idx++);
    // output the exponent value
    idx = _ntoa_long(out,b, idx, (expval < 0) ? -expval : expval, expval < 0, 10, 0, minwidth-1, FLAGS_ZEROPAD | FLAGS_PLUS);
    // might need to right-pad spaces
    if (flags & FLAGS_LEFT) {
      while (idx - start_idx < width) out(' ',b, idx++);
    }
  }
  return idx;
}
#endif  // PRINTF_SUPPORT_EXPONENTIAL
#endif  // PRINTF_SUPPORT_FLOAT

static int _error(JSContext *cx, char *format, ...) {
	char msg[128];
	va_list ap;

	va_start(ap,format);
	vsnprintf(msg,sizeof(msg)-1,format,ap);
	JS_ReportError(cx, msg);
	va_end(ap);
	return 1;
}

enum JSARG_TYPE {
	JSARG_TYPE_CHAR,
	JSARG_TYPE_SHORT,
	JSARG_TYPE_UCHAR,
	JSARG_TYPE_USHORT,
	JSARG_TYPE_INT,
	JSARG_TYPE_UINT,
	JSARG_TYPE_DOUBLE,
	JSARG_TYPE_STRING,
	JSARG_TYPE_MAX
};

#if DEBUG_JSPRINTF
static char *_typenames[JSARG_TYPE_MAX] = { "Char","Short","UChar","UShort","Int","UInt","Double","String" };
static char *_typestr(int type) {
	if (type < 0 || type >= JSARG_TYPE_MAX) return "unknown";
	return _typenames[type];
}
#endif

#define _getnum(dtype) \
	if (JSVAL_IS_BOOLEAN(argv[*idx])) *((dtype *)dest) = (JSVAL_TO_BOOLEAN(argv[*idx]) ? 1 : 0); \
	else if (JSVAL_IS_INT(argv[*idx])) *((dtype *)dest) = JSVAL_TO_INT(argv[*idx]); \
	else if (JSVAL_IS_DOUBLE(argv[*idx])) *((dtype *)dest) = *JSVAL_TO_DOUBLE(argv[*idx]); \
	else return _error(cx, "argument %d is not a number",*idx);

static int _getarg(void *dest, JSContext *cx, int *idx, int argc, jsval *argv, enum JSARG_TYPE type) {

	dprintf(dlevel,"idx: %d, argc: %d\n", *idx, argc);
	if (*idx >= argc) return _error(cx, "at least 1 argument is required");

	dprintf(dlevel,"arg[%d] type: %s\n", *idx, jstypestr(cx, argv[*idx]));

	dprintf(dlevel,"arg type: %d(%s)\n", type, jstypestr(cx,type));
	switch(type) {
	case JSARG_TYPE_CHAR:
		jsval_to_type(DATA_TYPE_S8,dest,1,cx,argv[*idx]);
		break;
	case JSARG_TYPE_UCHAR:
		jsval_to_type(DATA_TYPE_U8,dest,1,cx,argv[*idx]);
		break;
	case JSARG_TYPE_SHORT:
		jsval_to_type(DATA_TYPE_S16,dest,2,cx,argv[*idx]);
		break;
	case JSARG_TYPE_USHORT:
		jsval_to_type(DATA_TYPE_U16,dest,2,cx,argv[*idx]);
		break;
	case JSARG_TYPE_INT:
		jsval_to_type(DATA_TYPE_S32,dest,4,cx,argv[*idx]);
		break;
	case JSARG_TYPE_UINT:
		jsval_to_type(DATA_TYPE_U32,dest,4,cx,argv[*idx]);
		break;
	case JSARG_TYPE_DOUBLE:
		jsval_to_type(DATA_TYPE_F64,dest,8,cx,argv[*idx]);
		break;
	case JSARG_TYPE_STRING:
		{
			JSString *str;
			char *text;

			str = JS_ValueToString(cx, argv[*idx]);
			dprintf(dlevel,"str: %p\n", str);
			if (!str) return _error(cx, "unable to convert argument %d to string",*idx);
			text = JS_EncodeString(cx, str);
			if (!text) return _error(cx, "js_vsnprintf: JS_EncodeString returned 0!\n");
			*((char **)dest) = text;
		}
		break;
	default:
		log_error("_getarg: unhandled type: %d!\n", type);
		return 1;
	}
	*idx = *idx + 1;
	return 0;
}

// internal vsnprintf
static int js_vsnprintf(out_fct_type out, bufinfo_t *b, JSContext *cx, JSObject *obj, uintN argc, jsval *argv) {
	unsigned int flags, width, precision, n;
	size_t idx = 0U;
	const char *format,*_jsfmt;
	int argvidx = 0;

	if (_getarg(&_jsfmt,cx,&argvidx,argc,argv,JSARG_TYPE_STRING)) return -1;
	format = _jsfmt;
	dprintf(dlevel,"format: %s\n", format);

	while (*format) {
		// format specifier?  %[flags][width][.precision][length]
		if (*format != '%') {
			// no
			out(*format,b, idx++);
			format++;
			continue;
		} else {
			// yes, evaluate it
			format++;
		}

		// evaluate flags
		flags = 0U;
		do {
			switch (*format) {
			case '0': flags |= FLAGS_ZEROPAD; format++; n = 1U; break;
			case '-': flags |= FLAGS_LEFT;    format++; n = 1U; break;
			case '+': flags |= FLAGS_PLUS;    format++; n = 1U; break;
			case ' ': flags |= FLAGS_SPACE;   format++; n = 1U; break;
			case '#': flags |= FLAGS_HASH;    format++; n = 1U; break;
			default :                                   n = 0U; break;
			}
		} while (n);

		// evaluate width field
		width = 0U;
		if (_is_digit(*format)) {
			width = _atoi((const char **)&format);
		} else if (*format == '*') {
//			const int w = va_arg(va, int);
			int w;
			if (_getarg(&w, cx, &argvidx, argc, argv, JSARG_TYPE_INT)) goto js_vsnprintf_error;
			if (w < 0) {
				flags |= FLAGS_LEFT;    // reverse padding
				width = (unsigned int)-w;
			} else {
				width = (unsigned int)w;
			}
			format++;
		}

		// evaluate precision field
		precision = 0U;
		if (*format == '.') {
			flags |= FLAGS_PRECISION;
			format++;
			if (_is_digit(*format)) {
				precision = _atoi(&format);
			} else if (*format == '*') {
//				const int prec = (int)va_arg(va, int);
				int prec;
				if (_getarg(&prec, cx, &argvidx, argc, argv, JSARG_TYPE_INT)) goto js_vsnprintf_error;
				precision = prec > 0 ? (unsigned int)prec : 0U;
				format++;
			}
		}

		// evaluate length field
		switch (*format) {
		case 'l' :
			flags |= FLAGS_LONG;
			format++;
			if (*format == 'l') {
				flags |= FLAGS_LONG_LONG;
				format++;
			}
			break;
		case 'h' :
			flags |= FLAGS_SHORT;
			format++;
			if (*format == 'h') {
			  flags |= FLAGS_CHAR;
			  format++;
			}
			break;
#if defined(PRINTF_SUPPORT_PTRDIFF_T)
		case 't' :
			flags |= (sizeof(ptrdiff_t) == sizeof(long) ? FLAGS_LONG : FLAGS_LONG_LONG);
			format++;
			break;
#endif
		case 'j' :
			flags |= (sizeof(intmax_t) == sizeof(long) ? FLAGS_LONG : FLAGS_LONG_LONG);
			format++;
			break;
		case 'z' :
			flags |= (sizeof(size_t) == sizeof(long) ? FLAGS_LONG : FLAGS_LONG_LONG);
			format++;
			break;
		default :
			break;
		}

		// evaluate specifier
		switch (*format) {
		case 'd' :
		case 'i' :
		case 'u' :
		case 'x' :
		case 'X' :
		case 'o' :
		case 'b' : {
			// set the base
			unsigned int base;
			if (*format == 'x' || *format == 'X') {
				base = 16U;
			} else if (*format == 'o') {
				base =  8U;
			} else if (*format == 'b') {
				base =  2U;
			} else {
				base = 10U;
				flags &= ~FLAGS_HASH;   // no hash for dec format
			}
			// uppercase
			if (*format == 'X') {
				flags |= FLAGS_UPPERCASE;
			}

			// no plus or space flag for u, x, X, o, b
			if ((*format != 'i') && (*format != 'd')) {
			  flags &= ~(FLAGS_PLUS | FLAGS_SPACE);
			}

			// ignore '0' flag when precision is given
			if (flags & FLAGS_PRECISION) {
			  flags &= ~FLAGS_ZEROPAD;
			}

			// convert the integer
			if ((*format == 'i') || (*format == 'd')) {
			  // signed
			  if (flags & FLAGS_LONG_LONG) {
			#if defined(PRINTF_SUPPORT_LONG_LONG)
	//		    const long long value = va_arg(va, long long);
			    long long value;
				if (_getarg(&value, cx, &argvidx, argc, argv, JSARG_TYPE_INT)) goto js_vsnprintf_error;
			    idx = _ntoa_long_long(out,b, idx, (unsigned long long)(value > 0 ? value : 0 - value), value < 0, base, precision, width, flags);
			#endif
			  }
			  else if (flags & FLAGS_LONG) {
	//		    const long value = va_arg(va, long);
			    long value;
				if (_getarg(&value, cx, &argvidx, argc, argv, JSARG_TYPE_INT)) goto js_vsnprintf_error;
			    idx = _ntoa_long(out,b, idx, (unsigned long)(value > 0 ? value : 0 - value), value < 0, base, precision, width, flags);
			  } else {
	//		    const int value = (flags & FLAGS_CHAR) ? (char)va_arg(va, int) : (flags & FLAGS_SHORT) ? (short int)va_arg(va, int) : va_arg(va, int);
			    int value;
				if (flags & FLAGS_CHAR) {
					if (_getarg(&value, cx, &argvidx, argc, argv, JSARG_TYPE_CHAR)) goto js_vsnprintf_error;
				} else if (flags & FLAGS_SHORT) {
					if (_getarg(&value, cx, &argvidx, argc, argv, JSARG_TYPE_SHORT)) goto js_vsnprintf_error;
				} else {
					if (_getarg(&value, cx, &argvidx, argc, argv, JSARG_TYPE_INT)) goto js_vsnprintf_error;
				}
			    idx = _ntoa_long(out,b, idx, (unsigned int)(value > 0 ? value : 0 - value), value < 0, base, precision, width, flags);
			  }
			}
			else {
			  // unsigned
			  if (flags & FLAGS_LONG_LONG) {
			#if defined(PRINTF_SUPPORT_LONG_LONG)
	//		    idx = _ntoa_long_long(out,b, idx, va_arg(va, unsigned long long), false, base, precision, width, flags);
			    long long value;
				if (_getarg(&value, cx, &argvidx, argc, argv, JSARG_TYPE_INT)) goto js_vsnprintf_error;
				
			    idx = _ntoa_long_long(out,b, idx, value, false, base, precision, width, flags);
			#endif
			  }
			  else if (flags & FLAGS_LONG) {
	//		    idx = _ntoa_long(out,b, idx, va_arg(va, unsigned long), false, base, precision, width, flags);
			    long long value;
				if (_getarg(&value, cx, &argvidx, argc, argv, JSARG_TYPE_INT)) goto js_vsnprintf_error;
			    idx = _ntoa_long(out,b, idx, value, false, base, precision, width, flags);
			  }
			  else {
	//		    const unsigned int value = (flags & FLAGS_CHAR) ? (unsigned char)va_arg(va, unsigned int) : (flags & FLAGS_SHORT) ? (unsigned short int)va_arg(va, unsigned int) : va_arg(va, unsigned int);
			    unsigned int value;
				if (flags & FLAGS_CHAR) {
					if (_getarg(&value, cx, &argvidx, argc, argv, JSARG_TYPE_UCHAR)) goto js_vsnprintf_error;
				} else if (flags & FLAGS_SHORT) {
					if (_getarg(&value, cx, &argvidx, argc, argv, JSARG_TYPE_USHORT)) goto js_vsnprintf_error;
				} else {
					if (_getarg(&value, cx, &argvidx, argc, argv, JSARG_TYPE_UINT)) goto js_vsnprintf_error;
				}
			    idx = _ntoa_long(out,b, idx, value, false, base, precision, width, flags);
			  }
			}
			format++;
			break;
		}
#if defined(PRINTF_SUPPORT_FLOAT)
		case 'f' :
		case 'F' :
			{
				double value;

				if (*format == 'F') flags |= FLAGS_UPPERCASE;
//				idx = _ftoa(out,b, idx, va_arg(va, double), precision, width, flags);
				if (_getarg(&value, cx, &argvidx, argc, argv, JSARG_TYPE_DOUBLE)) goto js_vsnprintf_error;
				idx = _ftoa(out,b, idx, value, precision, width, flags);
				format++;
				break;
			}
#if defined(PRINTF_SUPPORT_EXPONENTIAL)
		case 'e':
		case 'E':
		case 'g':
		case 'G':
//			idx = _etoa(out,b, idx, va_arg(va, double), precision, width, flags);
			{
				double value;
				if ((*format == 'g')||(*format == 'G')) flags |= FLAGS_ADAPT_EXP;
				if ((*format == 'E')||(*format == 'G')) flags |= FLAGS_UPPERCASE;
				if (_getarg(&value, cx, &argvidx, argc, argv, JSARG_TYPE_DOUBLE)) goto js_vsnprintf_error;
				idx = _etoa(out,b, idx, value, precision, width, flags);
				format++;
				break;
			}
#endif  // PRINTF_SUPPORT_EXPONENTIAL
#endif  // PRINTF_SUPPORT_FLOAT
		case 'c' : {
		unsigned int l = 1U;
		// pre padding
		if (!(flags & FLAGS_LEFT)) {
		  while (l++ < width) {
		    out(' ',b, idx++);
		  }
		}
		// char output
//		out((char)va_arg(va, int),b, idx++);
		{
			char ch;
			if (_getarg(&ch, cx, &argvidx, argc, argv, JSARG_TYPE_CHAR)) goto js_vsnprintf_error;
			out(ch,b, idx++);
		}
		// post padding
		if (flags & FLAGS_LEFT) {
			while (l++ < width) {
				out(' ',b, idx++);
			}
		}
		format++;
		break;
		}

		case 's' : {
//			const char* p = va_arg(va, char*);
			const char *p, *_jsp;
//			dprintf(dlevel,"debugmem: calling _getarg for string...\n");
			if (_getarg(&_jsp, cx, &argvidx, argc, argv, JSARG_TYPE_STRING)) goto js_vsnprintf_error;
			p = _jsp;
			unsigned int l = _strnlen_s(p, precision ? precision : (size_t)-1);
			// pre padding
			if (flags & FLAGS_PRECISION) {
			  l = (l < precision ? l : precision);
			}
			if (!(flags & FLAGS_LEFT)) {
			  while (l++ < width) {
			    out(' ',b, idx++);
			  }
			}
			// string output
			while ((*p != 0) && (!(flags & FLAGS_PRECISION) || precision--)) {
			  out(*(p++),b, idx++);
			}
			// post padding
			if (flags & FLAGS_LEFT) {
			  while (l++ < width) {
			    out(' ',b, idx++);
			  }
			}
			format++;
//			dprintf(dlevel,"debugmem: freeing string...\n");
			JS_free(cx,(char *)_jsp);
			break;
		}

		case 'p' : {
			// p is not supported
			_error(cx,"p specifier not supported\n");
			return -1;
#if 0
			width = sizeof(void*) * 2U;
			flags |= FLAGS_ZEROPAD | FLAGS_UPPERCASE;
#if defined(PRINTF_SUPPORT_LONG_LONG)
			const bool is_ll = sizeof(uintptr_t) == sizeof(long long);
			if (is_ll) {
			  idx = _ntoa_long_long(out,b, idx, (uintptr_t)va_arg(va, void*), false, 16U, precision, width, flags);
			} else {
#endif
			  idx = _ntoa_long(out,b, idx, (unsigned long)((uintptr_t)va_arg(va, void*)), false, 16U, precision, width, flags);
#if defined(PRINTF_SUPPORT_LONG_LONG)
			}
#endif
			format++;
#endif
			break;
		}

		case '%' :
		out('%',b, idx++);
		format++;
		break;

		default :
		out(*format, b, idx++);
		format++;
		break;
		}
	}

	// termination
	out((char)0,b, idx++);

	// return written chars without terminating \0
	JS_free(cx,(char *)_jsfmt);
	return (int)idx;

js_vsnprintf_error:
	JS_free(cx,(char *)_jsfmt);
	return -1;
}


///////////////////////////////////////////////////////////////////////////////

static int _getbuf(bufinfo_t *b, JSContext *cx) {
	b->cx = cx;
	b->size = BUF_CHUNKSIZE;
	b->ptr = JS_malloc(cx, b->size);
	if (!b->ptr) {
		char errmsg[128];
		sprintf(errmsg,"error: JS_Printf: JS_malloc(%d): %s\n", b->size, strerror(errno));
		JS_ReportError(cx,errmsg);
		return 1;
	}
	return 0;
}

JSBool JS_Printf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	JSEngine *e;
	int ret, r;
	bufinfo_t buf;

	e = JS_GetPrivate(cx, obj);
	if (!e) {
		JS_ReportError(cx, "printf: global private is null!");
		return JS_FALSE;
	}
	r = JS_FALSE;

	if (_getbuf(&buf,cx)) goto JS_Printf_done;
	ret = js_vsnprintf(_out_buffer, &buf, cx, obj, argc, argv);
	dprintf(dlevel,"ret: %d\n", ret);
	if (ret < 0) {
		JS_ReportError(cx, "printf: not enough arguments for format");
		goto JS_Printf_done;
	}
	buf.ptr[ret] = 0;
	dprintf(dlevel,"output: %p\n", e->output);
	if (e->output) e->output("%s",buf.ptr);
	*rval = INT_TO_JSVAL(ret);
	r = JS_TRUE;
JS_Printf_done:
	if (buf.ptr) JS_free(cx,buf.ptr);
	return r;
}

JSBool JS_SPrintf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	bufinfo_t buf;
	int ret,r;

	r = JS_FALSE;
	if (_getbuf(&buf,cx)) goto JS_SPrintf_done;
	ret = js_vsnprintf(_out_buffer, &buf, cx, obj, argc, argv);
	dprintf(dlevel,"ret: %d\n", ret);
	if (ret < 0) {
		JS_ReportError(cx, "sprintf: not enough arguments for format");
		goto JS_SPrintf_done;
	}
	buf.ptr[ret] = 0;
	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,buf.ptr));
	r = JS_TRUE;
JS_SPrintf_done:
	if (buf.ptr) JS_free(cx,buf.ptr);
	return r;
}

JSBool js_log_write(int flags, JSContext *cx, uintN argc, jsval *vp) {
	bufinfo_t buf;
	jsval *argv = vp + 2;
	int ret,r;
	JSObject *obj;

        obj = JS_THIS_OBJECT(cx, vp);
        if (!obj) return JS_FALSE;

	r = JS_FALSE;
	if (_getbuf(&buf,cx)) goto js_log_write_done;
	ret = js_vsnprintf(_out_buffer, &buf, cx, obj, argc, argv);
	dprintf(dlevel,"ret: %d\n", ret);
	if (ret < 0) {
		JS_ReportError(cx, "log_write: not enough arguments for format");
		goto js_log_write_done;
	}
	buf.ptr[ret] = 0;
	dprintf(dlevel,"flags: %04x, buffer: %s\n", flags, buf.ptr);
	log_write(flags,"%s",buf.ptr);
	r = JS_TRUE;
js_log_write_done:
	if (buf.ptr) JS_free(cx,buf.ptr);
	return r;
}

JSBool JS_DPrintf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
#if defined(DEBUG) && DEBUG > 0
	char prefix[256],*p;
	bufinfo_t buf;
	JSEngine *e;
	int ret, level, r;
        JSStackFrame *fp;

	r = JS_FALSE;

	e = JS_GetPrivate(cx, obj);
	if (!e) {
		JS_ReportError(cx, "dprintf: global private is null!");
		goto JS_DPrintf_done;
	}

	if (_getbuf(&buf,cx)) goto JS_DPrintf_done;

	/* get the debug level */
	if (argc < 1 || !JSVAL_IS_INT(argv[0])) {
		JS_ReportError(cx, "dprintf 1st argument must be an integer");
		goto JS_DPrintf_done;
	}
	level = JSVAL_TO_INT(argv[0]);
//	printf("level: %d, debug: %d\n", level, debug);
	if (level > debug) {
		r = JS_TRUE;
		goto JS_DPrintf_done;
	}
	argc--;
	argv++;

	ret = js_vsnprintf(_out_buffer, &buf, cx, obj, argc, argv);
	dprintf(dlevel,"ret: %d\n", ret);
	if (ret < 0) {
		JS_ReportError(cx, "dprintf: not enough arguments for format");
		goto JS_DPrintf_done;
	}
	buf.ptr[ret] = 0;

        /* Get the currently executing script's name, function, and line # */
	*prefix = 0;
	p = prefix;
	fp = JS_GetScriptedCaller(cx, NULL);
//	printf("fp: %p\n", fp);
	if (fp) {
		char *f = jsdtrace_filename(fp);
//		printf("f: %p\n", f);
		if (f) p += sprintf(p, "%s(%d)", f, jsdtrace_linenumber(cx, fp));
//		printf("fp->fun: %p\n", fp->fun);
		if (fp->fun) {
			char *fname = (char *)JS_GetFunctionName(fp->fun);
			if (fname && strcmp(fname,"anonymous") != 0) {
				p += sprintf(p, " %s", fname);
//				JS_free(cx,fname);
			}
		}
	}
//	printf("prefix: %s\n", prefix);

	if (e->output) ret = e->output("%s: %s",prefix,buf.ptr);
	*rval = INT_TO_JSVAL(ret);
	r = JS_TRUE;
JS_DPrintf_done:
	if (buf.ptr) JS_free(cx,buf.ptr);
	return r;
#else
	return JS_TRUE;
#endif
}

#if 0
jsval printf_exception(JSContext *cx, char *msg, ...){
        char *str=NULL;
        int len = 0;
        va_list args;
        JSString *exceptionDescription;

        va_start(args, msg);
        len = vsnprintf(NULL, 0, msg, args)+1;
        va_end(args);

        str = JS_malloc(cx, len);
        memset(str, 0, len);

        va_start(args, msg);
        vsnprintf(str, len, msg, args);
        va_end(args);

        exceptionDescription = JS_NewString(cx, str, len);
        return STRING_TO_JSVAL(exceptionDescription);
}
#endif
