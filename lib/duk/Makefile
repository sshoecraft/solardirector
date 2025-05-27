
include ../../make.opts

ifeq ($(DUK),yes)
LIBNAME=$(shell basename $(shell pwd))
SRCS=duk_alloc_default.c duk_api_buffer.c duk_api_bytecode.c duk_api_call.c duk_api_codec.c duk_api_compile.c duk_api_debug.c duk_api_heap.c duk_api_inspect.c duk_api_memory.c duk_api_object.c duk_api_random.c duk_api_readable.c duk_api_stack.c duk_api_string.c duk_api_time.c duk_bi_array.c duk_bi_boolean.c duk_bi_buffer.c duk_bi_cbor.c duk_bi_date.c duk_bi_date_unix.c duk_bi_date_windows.c duk_bi_duktape.c duk_bi_encoding.c duk_bi_error.c duk_bi_function.c duk_bi_global.c duk_bi_json.c duk_bi_math.c duk_bi_number.c duk_bi_object.c duk_bi_performance.c duk_bi_pointer.c duk_bi_promise.c duk_bi_proxy.c duk_bi_reflect.c duk_bi_regexp.c duk_bi_string.c duk_bi_symbol.c duk_bi_thread.c duk_bi_thrower.c duk_debug_fixedbuffer.c duk_debugger.c duk_debug_macros.c duk_debug_vsnprintf.c duk_error_augment.c duk_error_longjmp.c duk_error_macros.c duk_error_misc.c duk_error_throw.c duk_hbuffer_alloc.c duk_hbuffer_assert.c duk_hbuffer_ops.c duk_hbufobj_misc.c duk_heap_alloc.c duk_heap_finalize.c duk_heap_hashstring.c duk_heaphdr_assert.c duk_heap_markandsweep.c duk_heap_memory.c duk_heap_misc.c duk_heap_refcount.c duk_heap_stringcache.c duk_heap_stringtable.c duk_hobject_alloc.c duk_hobject_array.c duk_hobject_assert.c duk_hobject_class.c duk_hobject_lookup.c duk_hobject_misc.c duk_hobject_pc2line.c duk_hobject_props.c duk_hobject_proxy.c duk_hobject_resize.c duk_hstring_assert.c duk_hstring_misc.c duk_hthread_alloc.c duk_hthread_builtins.c duk_hthread_misc.c duk_hthread_stacks.c duk_js_arith.c duk_js_call.c duk_js_compiler.c duk_js_executor.c duk_js_ops.c duk_js_prop.c duk_js_var.c duk_lexer.c duk_numconv.c duk_prop_defown.c duk_prop_delete.c duk_prop_enum.c duk_prop_get.c duk_prop_getown.c duk_prop_has.c duk_prop_ownpropkeys.c duk_prop_set.c duk_prop_util.c duk_regexp_compiler.c duk_regexp_executor.c duk_replacements.c duk_selftest.c duk_tval.c duk_unicode_support.c duk_unicode_tables.c duk_unicode_wtf8.c duk_util_bitdecoder.c duk_util_bitencoder.c duk_util_bufwriter.c duk_util_cast.c duk_util_double.c duk_util_hashbytes.c duk_util_memory.c duk_util_memrw.c duk_util_misc.c duk_util_tinyrandom.c

#CFLAGS+=-DJS_THREADSAFE

#CLEANFILES+=

include ../../Makefile.sd
else
all install clean:
endif
