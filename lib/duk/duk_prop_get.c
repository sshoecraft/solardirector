/*
 *  [[Get]] and GetValue() for properties
 *
 *  Property reads are performance critical, and there are also many
 *  sometimes conflicting concerns:
 *
 *  - Pointer stability; arguments from value stack or outside of it.
 *
 *  - Coercion, and keeping possibly primitive base values ('receiver'
 *    of getter and Proxy), reads on primitive values (GetValue).
 *
 *  - Fast pathing numbers, and in general minimizing coercion and
 *    string interning.
 *
 *  - Exotic behaviors for [[Get]] and [[GetOwnProperty]] modifying
 *    the ordinary behavior.
 *
 *  - Several useful argument / value stack behaviors depending on call
 *    site, need to optimize for executor especially.
 *
 *  Pointer stability is handled as follows:
 *
 *  - The receiver and output are assumed to be value stack slots and are
 *    referenced only by indices so they are both reachable and stable at
 *    all times.
 *
 *  - The key is a duk_tval pointer but it gets very quickly coerced
 *    (stabilizing if necessary) and after that we only deal with either an
 *    index or a reachable key (stored in value stack or a compiled function
 *    constants table) throughout.
 *
 *  - While walking the prototype chain the current 'target' object may only
 *    be reachable via a prototype chain leading to the original receiver.
 *    This is safe because no side effects are invoked while we walk.  If a
 *    getter is found, calling the getter may have arbitrary side effects.
 *    For example, a getter may edit the inheritance chain of the original
 *    receiver, and may cause the 'target' to become unreachable.  However,
 *    once a getter is found, this no longer matters.
 *
 *  - Writing the final output value to 'idx_out' may have arbitrary side
 *    effects so it must happen last (these may be caused by the old value
 *    getting finalized).  'idx_out' may also contain the target object or
 *    the key which gets overwritten with the property value.
 *
 *  - Proxies are the most difficult case.  Just checking for a (non-existent)
 *    proxy trap may have arbitrary side effects because the trap property in
 *    the handler object may be a getter or the handler itself may be a Proxy.
 *    The side effects may cause the current object to be freed, so we can
 *    no longer safely resume the prototype walk even if no trap is found.
 *
 *  - To handle Proxies it becomes necessary to stabilize the current object
 *    with a strong reference when doing a Proxy trap check -and- at every
 *    step after that, even for ordinary objects because the initial trap
 *    check may have edited the inheritance chain arbitrarily, stranding the
 *    current object.
 *
 *  - Because we don't want to burden normal fast path property reads with the
 *    stabilization only needed for Proxies, the [[Get]] operation is in
 *    effect duplicated into a Proxy-supporting side effect resistant variant.
 *    This duplication is not done for size optimized builds; for them, the
 *    stabilization is always done to reduce code size.
 *
 * There are other possible approaches too:
 *
 *  - Always stabilize the current object on the value stack.  This has
 *    a ~10% impact on property get performance (with interpreter overhead
 *    included), which is not negligible.
 *
 *  - Prevent side effects during [[Get]] (reset the protection if an error is
 *    thrown).  This would be straightforward but it would be unfortunate to
 *    prevent finalization entirely e.g. inside getters or Proxy trap calls
 *    which may do significant work.
 *
 *  - For Proxy traps specifically, one could first try to do a side effect
 *    free lookup (which would work in the vast majority of cases).  If a trap
 *    was not found, and side effects were known not to actually happen, one
 *    could stay in the faster unsafe path.
 */

#include "duk_internal.h"

/* Outcome for [[GetOwnProperty]] check. */
#define DUK__GETOWN_NOTFOUND          0 /* not found, continue to parent */
#define DUK__GETOWN_FOUND             1 /* found, stop walk */
#define DUK__GETOWN_DONE_NOTFOUND     2 /* not found, stop walk */
#define DUK__GETOWN_CONTINUE_ORDINARY -1 /* not found, check from ordinary property table */

typedef duk_bool_t (
    *duk__get_ownprop_idxkey_htype)(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv);
typedef duk_bool_t (
    *duk__get_ownprop_strkey_htype)(duk_hthread *thr, duk_hobject *obj, duk_hstring *key, duk_idx_t idx_out, duk_idx_t idx_recv);

DUK_LOCAL_DECL duk_bool_t
duk__prop_getvalue_plainstr_index(duk_hthread *thr, duk_idx_t idx_recv, duk_uarridx_t idx, duk_idx_t idx_out, duk_hstring *h);
DUK_LOCAL_DECL duk_bool_t
duk__prop_get_strkey_unsafe(duk_hthread *thr, duk_hobject *target, duk_hstring *key, duk_idx_t idx_out, duk_idx_t idx_recv);
DUK_LOCAL_DECL duk_bool_t
duk__prop_get_strkey_safe(duk_hthread *thr, duk_hobject *target, duk_hstring *key, duk_idx_t idx_out, duk_idx_t idx_recv);
DUK_LOCAL_DECL duk_bool_t
duk__prop_get_idxkey_unsafe(duk_hthread *thr, duk_hobject *target, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv);
DUK_LOCAL_DECL duk_bool_t
duk__prop_get_idxkey_safe(duk_hthread *thr, duk_hobject *target, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv);

DUK_NORETURN(DUK_LOCAL_DECL DUK_COLD duk_bool_t duk__prop_get_error_objidx_strkey(duk_hthread *thr,
                                                                                  duk_idx_t idx_obj,
                                                                                  duk_hstring *key));
DUK_NORETURN(DUK_LOCAL_DECL DUK_COLD duk_bool_t duk__prop_get_error_objidx_idxkey(duk_hthread *thr,
                                                                                  duk_idx_t idx_obj,
                                                                                  duk_uarridx_t idx));
DUK_NORETURN(DUK_LOCAL_DECL DUK_COLD duk_bool_t duk__prop_get_error_objidx_tvkey(duk_hthread *thr,
                                                                                 duk_idx_t idx_obj,
                                                                                 duk_tval *tv_key));

#if defined(DUK_USE_PARANOID_ERRORS)
DUK_NORETURN(DUK_LOCAL_DECL void duk__prop_get_error_shared(duk_hthread *thr, duk_idx_t idx_obj));
DUK_LOCAL void duk__prop_get_error_shared(duk_hthread *thr, duk_idx_t idx_obj) {
	const char *str1 = duk_get_type_name(thr, idx_obj);
	DUK_ERROR_FMT1(thr, DUK_ERR_TYPE_ERROR, "cannot read property of %s", str1);
}
DUK_LOCAL DUK_COLD duk_bool_t duk__prop_get_error_objidx_strkey(duk_hthread *thr, duk_idx_t idx_obj, duk_hstring *key) {
	DUK_UNREF(key);
	duk__prop_get_error_shared(thr, idx_obj);
	DUK_WO_NORETURN(return 0;);
}
DUK_LOCAL DUK_COLD duk_bool_t duk__prop_get_error_objidx_idxkey(duk_hthread *thr, duk_idx_t idx_obj, duk_uarridx_t idx) {
	DUK_UNREF(idx);
	duk__prop_get_error_shared(thr, idx_obj);
	DUK_WO_NORETURN(return 0;);
}
DUK_LOCAL DUK_COLD duk_bool_t duk__prop_get_error_objidx_tvkey(duk_hthread *thr, duk_idx_t idx_obj, duk_tval *tv_key) {
	DUK_UNREF(tv_key);
	duk__prop_get_error_shared(thr, idx_obj);
	DUK_WO_NORETURN(return 0;);
}
#elif defined(DUK_USE_VERBOSE_ERRORS)
DUK_LOCAL DUK_COLD duk_bool_t duk__prop_get_error_objidx_strkey(duk_hthread *thr, duk_idx_t idx_obj, duk_hstring *key) {
	const char *str1 = duk_push_readable_idx(thr, idx_obj);
	const char *str2 = duk_push_readable_hstring(thr, key);
	DUK_ERROR_FMT2(thr, DUK_ERR_TYPE_ERROR, "cannot read property %s of %s", str2, str1);
	DUK_WO_NORETURN(return 0;);
}
DUK_LOCAL DUK_COLD duk_bool_t duk__prop_get_error_objidx_idxkey(duk_hthread *thr, duk_idx_t idx_obj, duk_uarridx_t idx) {
	const char *str1 = duk_push_readable_idx(thr, idx_obj);
	DUK_ERROR_FMT2(thr, DUK_ERR_TYPE_ERROR, "cannot read property %lu of %s", (unsigned long) idx, str1);
	DUK_WO_NORETURN(return 0;);
}
DUK_LOCAL DUK_COLD duk_bool_t duk__prop_get_error_objidx_tvkey(duk_hthread *thr, duk_idx_t idx_obj, duk_tval *tv_key) {
	const char *str1 = duk_push_readable_idx(thr, idx_obj);
	const char *str2 = duk_push_readable_tval(thr, tv_key);
	DUK_ERROR_FMT2(thr, DUK_ERR_TYPE_ERROR, "cannot read property %s of %s", str2, str1);
	DUK_WO_NORETURN(return 0;);
}
#else
DUK_NORETURN(DUK_LOCAL_DECL void duk__prop_get_error_shared(duk_hthread *thr));
DUK_LOCAL void duk__prop_get_error_shared(duk_hthread *thr) {
	DUK_ERROR_TYPE(thr, DUK_STR_CANNOT_READ_PROPERTY);
}
DUK_LOCAL DUK_COLD duk_bool_t duk__prop_get_error_objidx_strkey(duk_hthread *thr, duk_idx_t idx_obj, duk_hstring *key) {
	DUK_UNREF(idx_obj);
	DUK_UNREF(key);
	duk__prop_get_error_shared(thr);
	DUK_WO_NORETURN(return 0;);
}
DUK_LOCAL DUK_COLD duk_bool_t duk__prop_get_error_objidx_idxkey(duk_hthread *thr, duk_idx_t idx_obj, duk_uarridx_t idx) {
	DUK_UNREF(idx_obj);
	DUK_UNREF(idx);
	duk__prop_get_error_shared(thr);
	DUK_WO_NORETURN(return 0;);
}
DUK_LOCAL DUK_COLD duk_bool_t duk__prop_get_error_objidx_tvkey(duk_hthread *thr, duk_idx_t idx_obj, duk_tval *tv_key) {
	DUK_UNREF(idx_obj);
	DUK_UNREF(tv_key);
	duk__prop_get_error_shared(thr);
	DUK_WO_NORETURN(return 0;);
}
#endif /* error model */

DUK_LOCAL DUK_ALWAYS_INLINE duk_bool_t duk__prop_get_write_tval_result(duk_hthread *thr, duk_idx_t idx_out, duk_tval *tv_src) {
	duk_tval *tv_out;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(tv_src != NULL);

	tv_out = thr->valstack_bottom + idx_out;
	DUK_ASSERT(DUK_HTHREAD_TVAL_IN_VSFRAME(thr, tv_out));
	DUK_TVAL_SET_TVAL_UPDREF(thr, tv_out, tv_src);
	return 1;
}

DUK_LOCAL DUK_ALWAYS_INLINE duk_bool_t duk__prop_get_write_u32_result(duk_hthread *thr, duk_idx_t idx_out, duk_uint32_t val) {
	duk_tval *tv_out;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));

	tv_out = thr->valstack_bottom + idx_out;
	DUK_ASSERT(DUK_HTHREAD_TVAL_IN_VSFRAME(thr, tv_out));
	DUK_TVAL_SET_U32_UPDREF(thr, tv_out, val);
	return 1;
}

DUK_LOCAL DUK_ALWAYS_INLINE duk_bool_t duk__prop_get_write_notfound_result(duk_hthread *thr, duk_idx_t idx_out) {
	duk_tval *tv_out;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));

	tv_out = thr->valstack_bottom + idx_out;
	DUK_ASSERT(DUK_HTHREAD_TVAL_IN_VSFRAME(thr, tv_out));
	DUK_TVAL_SET_UNDEFINED_UPDREF(thr, tv_out);
	return 0;
}

DUK_LOCAL duk_bool_t duk__prop_get_write_plainstr_length(duk_hthread *thr, duk_hstring *h, duk_idx_t idx_out) {
	duk_tval *tv_out;
	duk_uint32_t len;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(h != NULL);
	DUK_ASSERT(DUK_HEAPHDR_IS_ANY_STRING((duk_heaphdr *) h));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));

	tv_out = thr->valstack_bottom + idx_out;
	DUK_ASSERT(DUK_HTHREAD_TVAL_IN_VSFRAME(thr, tv_out));
	len = (duk_uint32_t) duk_hstring_get_charlen(h);
	DUK_TVAL_SET_U32_UPDREF(thr, tv_out, len);
	return 1;
}

DUK_LOCAL DUK_ALWAYS_INLINE duk_bool_t duk__get_own_prop_found_getter_helper(duk_hthread *thr,
                                                                             duk_hobject *obj,
                                                                             duk_hstring *key,
                                                                             duk_uarridx_t idx,
                                                                             duk_idx_t idx_out,
                                                                             duk_idx_t idx_recv,
                                                                             duk_propvalue *pv,
                                                                             duk_uint8_t attrs,
                                                                             duk_uint_t use_key) {
	duk_propaccessor *pa;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(obj != NULL);
	/* key may be NULL */
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));
	DUK_ASSERT(pv != NULL);
	DUK_ASSERT_VALSTACK_SPACE(thr, DUK_HOBJECT_PROP_VALSTACK_SPACE);
	DUK_ASSERT((attrs & DUK_PROPDESC_FLAG_ACCESSOR) != 0U);
	DUK_ASSERT((use_key && key != NULL) || (!use_key && key == NULL));
	DUK_UNREF(obj);
	DUK_UNREF(attrs);

	pa = &pv->a;
	if (DUK_LIKELY(pa->get != NULL)) {
		duk_push_hobject(thr, pa->get); /* get function */
		duk_dup(thr, idx_recv); /* receiver; original uncoerced base */
#if defined(DUK_USE_NONSTD_GETTER_KEY_ARGUMENT)
		if (use_key) {
			DUK_ASSERT(key != NULL);
			duk_push_hstring(thr, key);
		} else {
			(void) duk_push_u32_tostring(thr, idx);
		}
		duk_call_method(thr, 1); /* [ getter receiver(= this) key ] -> [ retval ] */
#else
		DUK_UNREF(key);
		DUK_UNREF(idx);
		DUK_UNREF(use_key);
		duk_call_method(thr, 0); /* [ getter receiver(= this) ] -> [ retval ] */
#endif
	} else {
		/* If getter is missing, return undefined. */
		duk_push_undefined(thr);
	}
	duk_replace_posidx_unsafe(thr, idx_out);
	return DUK__GETOWN_FOUND;
}

#if defined(DUK_USE_NONSTD_GETTER_KEY_ARGUMENT)
DUK_LOCAL DUK_NOINLINE duk_bool_t duk__get_own_prop_found_getter_withkey(duk_hthread *thr,
                                                                         duk_hobject *obj,
                                                                         duk_hstring *key,
                                                                         duk_idx_t idx_out,
                                                                         duk_idx_t idx_recv,
                                                                         duk_propvalue *pv,
                                                                         duk_uint8_t attrs) {
	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(obj != NULL);
	DUK_ASSERT(key != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));
	DUK_ASSERT(pv != NULL);

	return duk__get_own_prop_found_getter_helper(thr, obj, key, 0, idx_out, idx_recv, pv, attrs, 1 /*use_key*/);
}
DUK_LOCAL DUK_NOINLINE duk_bool_t duk__get_own_prop_found_getter_withidx(duk_hthread *thr,
                                                                         duk_hobject *obj,
                                                                         duk_uarridx_t idx,
                                                                         duk_idx_t idx_out,
                                                                         duk_idx_t idx_recv,
                                                                         duk_propvalue *pv,
                                                                         duk_uint8_t attrs) {
	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(obj != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));
	DUK_ASSERT(pv != NULL);

	return duk__get_own_prop_found_getter_helper(thr, obj, NULL, idx, idx_out, idx_recv, pv, attrs, 0 /*use_key*/);
}
#else
DUK_LOCAL DUK_NOINLINE duk_bool_t duk__get_own_prop_found_getter_nokey(duk_hthread *thr,
                                                                       duk_hobject *obj,
                                                                       duk_idx_t idx_out,
                                                                       duk_idx_t idx_recv,
                                                                       duk_propvalue *pv,
                                                                       duk_uint8_t attrs) {
	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(obj != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));
	DUK_ASSERT(pv != NULL);

	return duk__get_own_prop_found_getter_helper(thr, obj, NULL, 0, idx_out, idx_recv, pv, attrs, 0 /*use_key*/);
}
#endif

DUK_LOCAL duk_bool_t
duk__get_own_prop_strkey_ordinary(duk_hthread *thr, duk_hobject *obj, duk_hstring *key, duk_idx_t idx_out, duk_idx_t idx_recv) {
	duk_propvalue *pv;
	duk_uint8_t attrs;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(thr->heap != NULL);
	DUK_ASSERT(obj != NULL);
	DUK_ASSERT(key != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));
	DUK_ASSERT_VALSTACK_SPACE(thr, DUK_HOBJECT_PROP_VALSTACK_SPACE);

	if (DUK_LIKELY(duk_hobject_lookup_strprop_val_attrs(thr, obj, key, &pv, &attrs) != 0)) {
		if (DUK_LIKELY(attrs & DUK_PROPDESC_FLAG_ACCESSOR) == 0) {
			return duk__prop_get_write_tval_result(thr, idx_out, &pv->v);
		} else {
#if defined(DUK_USE_NONSTD_GETTER_KEY_ARGUMENT)
			return duk__get_own_prop_found_getter_withkey(thr, obj, key, idx_out, idx_recv, pv, attrs);
#else
			return duk__get_own_prop_found_getter_nokey(thr, obj, idx_out, idx_recv, pv, attrs);
#endif
		}
	} else {
		return DUK__GETOWN_NOTFOUND;
	}
}

DUK_LOCAL duk_bool_t
duk__get_own_prop_idxkey_ordinary(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	duk_propvalue *pv;
	duk_uint8_t attrs;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(thr->heap != NULL);
	DUK_ASSERT(obj != NULL);
	DUK_ASSERT_ARRIDX_VALID(idx);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));
	DUK_ASSERT_VALSTACK_SPACE(thr, DUK_HOBJECT_PROP_VALSTACK_SPACE);

	if (DUK_LIKELY(duk_hobject_lookup_idxprop_val_attrs(thr, obj, idx, &pv, &attrs) != 0)) {
		if (DUK_LIKELY(attrs & DUK_PROPDESC_FLAG_ACCESSOR) == 0) {
			return duk__prop_get_write_tval_result(thr, idx_out, &pv->v);
		} else {
#if defined(DUK_USE_NONSTD_GETTER_KEY_ARGUMENT)
			return duk__get_own_prop_found_getter_withidx(thr, obj, idx, idx_out, idx_recv, pv, attrs);
#else
			return duk__get_own_prop_found_getter_nokey(thr, obj, idx_out, idx_recv, pv, attrs);
#endif
		}
	} else {
		return DUK__GETOWN_NOTFOUND;
	}
}

#if defined(DUK_USE_PROXY_POLICY)
DUK_LOCAL void duk__prop_get_own_proxy_policy(duk_hthread *thr, duk_hobject *obj) {
	duk_hobject *target;
	duk_small_int_t attrs;

	/* [ ... key result ] */

	target = duk_proxy_get_target_autothrow(thr, (duk_hproxy *) obj);
	DUK_ASSERT(target != NULL);

	attrs = duk_prop_getowndesc_obj_tvkey(thr, target, duk_get_tval(thr, -2 /*trap key*/));
	target = NULL; /* Potentially invalidated. */

	/* [ ... key result value ] OR [ ... key result get set ] */

	if (attrs >= 0) {
		duk_small_uint_t uattrs = (duk_small_uint_t) attrs;
		if ((uattrs & (DUK_PROPDESC_FLAG_ACCESSOR | DUK_PROPDESC_FLAG_WRITABLE | DUK_PROPDESC_FLAG_CONFIGURABLE)) == 0) {
			/* Non-configurable, data property, non-writable: trap value must compare SameValue() to target value. */
			if (!duk_samevalue(thr, -1 /*target value*/, -2 /*trap result*/)) {
				goto reject;
			}
		} else if ((uattrs & (DUK_PROPDESC_FLAG_ACCESSOR | DUK_PROPDESC_FLAG_CONFIGURABLE)) == DUK_PROPDESC_FLAG_ACCESSOR) {
			/* Non-configurable, accessor property; if [[Get]] is null, trap value must be undefined. */
			if (duk_is_nullish(thr, -2 /*get*/) && !duk_is_undefined(thr, -3 /*trap result*/)) {
				goto reject;
			}
		}
	} else {
		/* If target has no 'key', then no restrictions are applied. */
	}

	duk_prop_pop_propdesc(thr, attrs);
	return;

reject:
	DUK_ERROR_TYPE_PROXY_REJECTED(thr);
}
#endif

DUK_LOCAL duk_bool_t duk__prop_get_own_proxy_tail(duk_hthread *thr, duk_hobject *obj, duk_idx_t idx_out, duk_idx_t idx_recv) {
	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));

	/* [ ... trap handler target key ] */

	duk_dup_top(thr);
	duk_insert(thr, -5); /* Stash key for policy check. */

	/* [ ... key trap handler target key ] */

	duk_dup(thr, idx_recv);
	duk_call_method(thr, 3); /* [ ... key trap handler target key recv ] -> [ ... key result ] */

#if defined(DUK_USE_PROXY_POLICY)
	duk__prop_get_own_proxy_policy(thr, obj);
#else
	DUK_DD(DUK_DDPRINT("proxy policy check for 'get' trap disabled in configuration"));
#endif

	/* [ ... key result ] */

	duk_replace_posidx_unsafe(thr, idx_out);
	duk_pop_known(thr);

	return DUK__GETOWN_FOUND;
}

DUK_LOCAL DUK_NOINLINE duk_bool_t
duk__get_own_prop_strkey_proxy_actual(duk_hthread *thr, duk_hobject *obj, duk_hstring *key, duk_idx_t idx_out, duk_idx_t idx_recv) {
	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(obj != NULL);
	DUK_ASSERT(DUK_HOBJECT_GET_HTYPE(obj) == DUK_HTYPE_PROXY);
	DUK_ASSERT(key != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));

	if (duk_proxy_trap_check_strkey(thr, (duk_hproxy *) obj, key, DUK_STRIDX_GET)) {
		duk_push_hstring(thr, key);
		return duk__prop_get_own_proxy_tail(thr, obj, idx_out, idx_recv);
	} else {
		return DUK__GETOWN_NOTFOUND;
	}
}

DUK_LOCAL DUK_NOINLINE duk_bool_t duk__get_own_prop_idxkey_proxy_actual(duk_hthread *thr,
                                                                        duk_hobject *obj,
                                                                        duk_uarridx_t idx,
                                                                        duk_idx_t idx_out,
                                                                        duk_idx_t idx_recv) {
	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(obj != NULL);
	DUK_ASSERT(DUK_HOBJECT_GET_HTYPE(obj) == DUK_HTYPE_PROXY);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));

	if (duk_proxy_trap_check_idxkey(thr, (duk_hproxy *) obj, idx, DUK_STRIDX_GET)) {
		(void) duk_push_u32_tostring(thr, idx);
		return duk__prop_get_own_proxy_tail(thr, obj, idx_out, idx_recv);
	} else {
		return DUK__GETOWN_NOTFOUND;
	}
}

DUK_LOCAL duk_bool_t duk__prop_get_check_arguments_map_for_get(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx) {
	duk_hobject *map;
	duk_hobject *env;
	duk_hstring *varname;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(obj != NULL);
	DUK_ASSERT(DUK_HEAPHDR_GET_HTYPE((duk_heaphdr *) obj) == DUK_HTYPE_ARGUMENTS);
	DUK_ASSERT_ARRIDX_VALID(idx);

	varname = duk_prop_arguments_map_prep_idxkey(thr, obj, idx, &map, &env);
	if (varname == NULL) {
		return 0;
	}

	/* Getvar can have arbitrary side effects, as it may be captured
	 * e.g. by a with(proxy).
	 */
	(void) duk_js_getvar_envrec(thr, env, varname, 1 /*throw*/); /* -> [ ... value this_binding ] */
	duk_pop_known(thr);

	/* Leave result on stack top. */
	return 1;
}

DUK_LOCAL DUK_NOINLINE duk_bool_t
duk__get_own_prop_idxkey_arguments(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	/* Conceptually: first perform OrdinaryGetOwnProperty() and then
	 * apply arguments exotic map handling if the property is found.
	 * This order only matters for getter properties, otherwise the
	 * order is not observable.
	 *
	 * The Arguments map lookup is side effect free so we can do that
	 * first and only then look at own properties.  If an index is found
	 * in the map we must still perform a normal property lookup and
	 * ignore the map result (returning 'undefined') if the own property
	 * no longer exists.
	 *
	 * Note in particular that we can't first write an initial result
	 * to 'idx_out' and then do the map lookup.  The initial result
	 * write may directly overwrite either key or receiver (they may
	 * have the same slot) and may also invoke arbitrary side effects
	 * through finalizers, potentially making 'obj' unreachable.
	 */

	duk_harray *a = (duk_harray *) obj;
	duk_bool_t rc;
	duk_bool_t rc_map;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(obj != NULL);
	DUK_ASSERT(DUK_HEAPHDR_GET_HTYPE((duk_heaphdr *) obj) == DUK_HTYPE_ARGUMENTS);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));

	/* Stabilize 'obj' against side effects.  Note that this does not
	 * protect the inheritance chain from being modified, it just
	 * guarantees 'obj' won't go away prematurely if side effects happen.
	 */
	duk_push_hobject(thr, obj);

	/* Arbitrary side effects. */
	rc_map = duk__prop_get_check_arguments_map_for_get(thr, obj, idx);

	/* [ obj mappedvalue? ] */

	if (DUK_LIKELY(DUK_HOBJECT_HAS_ARRAY_ITEMS(obj))) {
		if (DUK_LIKELY(idx < DUK_HARRAY_GET_ITEMS_LENGTH(a))) {
			duk_tval *tv = DUK_HARRAY_GET_ITEMS(thr->heap, a) + idx;
			if (DUK_LIKELY(!DUK_TVAL_IS_UNUSED(tv))) {
				duk__prop_get_write_tval_result(thr, idx_out, tv);
				goto found_do_map_check;
			}
		}
	} else {
		rc = duk__get_own_prop_idxkey_ordinary(thr, obj, idx, idx_out, idx_recv);
		if (rc) {
			/* If property is found, arbitrary side effects may have
			 * happened.  'obj' is still safe because it's stabilized
			 * on the value stack (above).
			 */
			goto found_do_map_check;
		}
	}

	/* Pop stabilized 'obj' and possible map lookup result.
	 *
	 * In this path we didn't find a property (in particular didn't hit a
	 * getter) so no side effects have occurred and caller is safe to
	 * continue with 'obj'.  (Had side effects happened, 'obj' might now
	 * be unreachable except for this value stack reference.)
	 */
	if (rc_map) {
		duk_pop_2_known(thr);
	} else {
		duk_pop_known(thr);
	}
	return DUK__GETOWN_NOTFOUND;

found_do_map_check:
	if (rc_map) {
		/* idx_out already contains the result from a normal property
		 * lookup, replace it with value found from arguments map.
		 */
		duk_replace(thr, idx_out);
	}
	duk_pop_known(thr);

	/* Side effects have occurred so all bets are off.  This is OK in this
	 * path because caller will terminate property lookup anyway.
	 */
	return DUK__GETOWN_FOUND;
}

DUK_LOCAL duk_bool_t
duk__get_own_prop_idxkey_arrayitems(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	duk_harray *a;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(obj != NULL);
	DUK_ASSERT(DUK_HOBJECT_GET_HTYPE(obj) == DUK_HTYPE_ARRAY);
	DUK_ASSERT(DUK_HOBJECT_HAS_ARRAY_ITEMS(obj));
	DUK_ASSERT_ARRIDX_VALID(idx);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));
	DUK_UNREF(idx_recv);

	a = (duk_harray *) obj;

	/* Comprehensiveness assumption: if array items exists, no index
	 * keys are allowed in index part.
	 */
	if (DUK_LIKELY(idx < DUK_HARRAY_GET_ITEMS_LENGTH(a))) {
		duk_tval *tv = DUK_HARRAY_GET_ITEMS(thr->heap, a) + idx;
		if (DUK_LIKELY(!DUK_TVAL_IS_UNUSED(tv))) {
			return duk__prop_get_write_tval_result(thr, idx_out, tv);
		}
	}
	return DUK__GETOWN_NOTFOUND;
}

DUK_LOCAL DUK_NOINLINE duk_small_int_t
duk__get_own_prop_idxkey_stringobj(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	duk_hstring *h;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(obj != NULL);
	DUK_ASSERT(DUK_HOBJECT_GET_HTYPE(obj) == DUK_HTYPE_STRING_OBJECT);
	DUK_ASSERT_ARRIDX_VALID(idx);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));

	h = duk_hobject_lookup_intvalue_hstring(thr, obj);
	if (DUK_LIKELY(h != NULL)) {
		if (DUK_LIKELY(!DUK_HSTRING_HAS_SYMBOL(h) && idx < duk_hstring_get_charlen(h))) {
			return (duk_small_int_t) duk__prop_getvalue_plainstr_index(thr, idx_recv, idx, idx_out, h);
		}
	}

	return DUK__GETOWN_CONTINUE_ORDINARY; /* Continue to ordinary idxprops. */
}

DUK_LOCAL DUK_NOINLINE duk_bool_t duk__get_own_prop_strkey_stringobj_length(duk_hthread *thr,
                                                                            duk_hobject *obj,
                                                                            duk_hstring *key,
                                                                            duk_idx_t idx_out,
                                                                            duk_idx_t idx_recv) {
	duk_hstring *h;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(obj != NULL);
	DUK_ASSERT(DUK_HOBJECT_GET_HTYPE(obj) == DUK_HTYPE_STRING_OBJECT);
	DUK_ASSERT(key != NULL);
	DUK_ASSERT(key == DUK_HTHREAD_STRING_LENGTH(thr));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));
	DUK_UNREF(key);
	DUK_UNREF(idx_recv);

	h = duk_hobject_lookup_intvalue_hstring(thr, obj);
	if (DUK_LIKELY(h != NULL)) {
		return duk__prop_get_write_plainstr_length(thr, h, idx_out);
	}
	return DUK__GETOWN_NOTFOUND;
}

DUK_LOCAL duk_bool_t
duk__get_own_prop_strkey_switch(duk_hthread *thr, duk_hobject *obj, duk_hstring *key, duk_idx_t idx_out, duk_idx_t idx_recv) {
	duk_small_uint_t htype;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(obj != NULL);
	DUK_ASSERT(key != NULL);
	DUK_ASSERT(!DUK_HSTRING_HAS_ARRIDX(key));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));

	htype = DUK_HEAPHDR_GET_HTYPE((duk_heaphdr *) obj);
	DUK_ASSERT(DUK_HTYPE_IS_ANY_OBJECT(htype));

#if 0
	if (DUK_LIKELY(htype == DUK_HTYPE_OBJECT)) {
		goto skip_htype_check;
	}
#endif

	switch (htype) {
	case DUK_HTYPE_ARRAY:
		if (DUK_HSTRING_HAS_LENGTH(key)) {
			duk_harray *a = (duk_harray *) obj;
			duk_uint32_t len = (duk_uint32_t) DUK_HARRAY_GET_LENGTH(a);
			return duk__prop_get_write_u32_result(thr, idx_out, len);
		}
		break;
	case DUK_HTYPE_ARGUMENTS:
		/* In ES2017+ Arguments objects no longer contain a throwing .caller
		 * property or exotic behavior related to it.  The only exotic behavior
		 * is for index properties, which don't come here.
		 */
		break;
	case DUK_HTYPE_PROXY:
		/* Consult 'get' trap if one exists.  If not, move on to the
		 * Proxy target.  Note that 'getPrototypeOf' is not checked
		 * by Proxy [[Get]].
		 *
		 * This is a difficult case because of side effects.  Just
		 * checking for the trap may invalidate caller's 'target'.
		 *
		 * Handle specially: return "not found" and handle the actual
		 * trap lookup in the caller (exploiting the fact that a
		 * Proxy's internal prototype is NULL) so that the caller can
		 * stabilize the 'target' if necessary.
		 */
		return DUK__GETOWN_NOTFOUND;
	case DUK_HTYPE_COMPFUNC:
	case DUK_HTYPE_NATFUNC:
	case DUK_HTYPE_BOUNDFUNC:
		/* No exotic behaviors in ES2015+. */
		break;
	case DUK_HTYPE_STRING_OBJECT:
		/* String objects have exotic behavior for string indices
		 * (CanonicalNumericIndexString), but unlike typed arrays
		 * non-matching indices continue through to the ordinary
		 * algorithm.  In this path CanonicalNumericIndexStrings
		 * never match actual indices so they are just passed on.
		 *
		 * Exotic behavior for String [[GetOwnProperty]] comes after
		 * OrdinaryGetOwnProperty() but this doesn't seem to matter
		 * because the index properties are never found as concrete
		 * properties (attempt to define them fails as they're
		 * non-writable and non-configurable).  So we implement the
		 * exotic behavior first.
		 */
		if (DUK_HSTRING_HAS_LENGTH(key)) {
			return duk__get_own_prop_strkey_stringobj_length(thr, obj, key, idx_out, idx_recv);
		}
		break;
#if defined(DUK_USE_BUFFEROBJECT_SUPPORT)
	case DUK_HTYPE_ARRAYBUFFER:
	case DUK_HTYPE_DATAVIEW:
		/* ArrayBuffer and DataView .byteLength is an accessor, but it's
		 * not generally used in loops because there are no index keys
		 * for ArrayBuffer or DataView.
		 *
		 * No virtual .byteLength for now.
		 */
		break;
	case DUK_HTYPE_INT8ARRAY:
	case DUK_HTYPE_UINT8ARRAY:
	case DUK_HTYPE_UINT8CLAMPEDARRAY:
	case DUK_HTYPE_INT16ARRAY:
	case DUK_HTYPE_UINT16ARRAY:
	case DUK_HTYPE_INT32ARRAY:
	case DUK_HTYPE_UINT32ARRAY:
	case DUK_HTYPE_FLOAT32ARRAY:
	case DUK_HTYPE_FLOAT64ARRAY:
		/* Technically integer-indexed exotic object [[GetOwnProperty]]
		 * does not have any special behavior for 'length' property.
		 * It is inherited from %TypedArray%.prototype as a getter.
		 *
		 * But because it's potentially used in tight loops, it's
		 * provided as a virtual own property here.  Another option
		 * would be special case length reads but track "tainting"
		 * of the prototype .length getter and the prototype chain
		 * so the fast path would be fully transparent.
		 *
		 * Array index keys don't come here, but the exotic behavior
		 * applies to all CanonicalNumericIndexString values which
		 * must be handled here.
		 */
		if (DUK_HSTRING_HAS_LENGTH(key)) {
			duk_hbufobj *h = (duk_hbufobj *) obj;
			duk_uint32_t len = (duk_uint32_t) DUK_HBUFOBJ_GET_LOGICAL_LENGTH(h);
			return duk__prop_get_write_u32_result(thr, idx_out, len);
		}
		if (DUK_HSTRING_HAS_CANNUM(key)) {
			/* We could actually skip this check if property
			 * write path rejects any attempts to establish
			 * concrete CanonicalNumericIndexString properties.
			 * We would then return "not found" from the concrete
			 * property check (performance of that is irrelevant
			 * for these keys).  In any case we must not continue
			 * prototype walk to the parent.
			 */
			return DUK__GETOWN_DONE_NOTFOUND; /* Short circuit, don't continue walk. */
		}
		break;
#endif /* DUK_USE_BUFFEROBJECT_SUPPORT */
	default:
		break;
	}

#if 0
skip_htype_check:
#endif
	return duk__get_own_prop_strkey_ordinary(thr, obj, key, idx_out, idx_recv);
}

DUK_LOCAL duk_bool_t
duk__get_own_prop_idxkey_switch(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	duk_small_uint_t htype;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(obj != NULL);
	DUK_ASSERT_ARRIDX_VALID(idx);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));

	htype = DUK_HEAPHDR_GET_HTYPE((duk_heaphdr *) obj);
	DUK_ASSERT(DUK_HTYPE_IS_ANY_OBJECT(htype));

	/* A lookup with an arridx is expected to hit some exotic behavior
	 * (array or buffers) so there's probably no point in fast pathing
	 * plain objects or another subset of non-exotic objects.  It might
	 * be useful to fast path arrays and/or buffers.
	 */

	switch (htype) {
	case DUK_HTYPE_ARRAY:
		if (DUK_LIKELY(DUK_HOBJECT_HAS_ARRAY_ITEMS(obj))) {
			return duk__get_own_prop_idxkey_arrayitems(thr, obj, idx, idx_out, idx_recv);
		}
		break;
	case DUK_HTYPE_ARGUMENTS:
		return duk__get_own_prop_idxkey_arguments(thr, obj, idx, idx_out, idx_recv);
	case DUK_HTYPE_PROXY:
		/* Handled by caller; see comments for Proxy in the strkey path. */
		return DUK__GETOWN_NOTFOUND;
	case DUK_HTYPE_STRING_OBJECT: {
		duk_small_int_t str_rc;

		str_rc = duk__get_own_prop_idxkey_stringobj(thr, obj, idx, idx_out, idx_recv);
		if (str_rc < 0) {
			/* Continue with normal property table lookup. */
			break;
		}
		return (duk_bool_t) str_rc;
	}
#if defined(DUK_USE_BUFFEROBJECT_SUPPORT)
	case DUK_HTYPE_ARRAYBUFFER:
	case DUK_HTYPE_DATAVIEW:
		break;
	case DUK_HTYPE_INT8ARRAY:
	case DUK_HTYPE_UINT8ARRAY:
	case DUK_HTYPE_UINT8CLAMPEDARRAY:
	case DUK_HTYPE_INT16ARRAY:
	case DUK_HTYPE_UINT16ARRAY:
	case DUK_HTYPE_INT32ARRAY:
	case DUK_HTYPE_UINT32ARRAY:
	case DUK_HTYPE_FLOAT32ARRAY:
	case DUK_HTYPE_FLOAT64ARRAY:
		/* All arridx are captured and don't reach OrdinaryGetOwnProperty(). */
		if (DUK_LIKELY(duk_hbufobj_validate_and_read_push(thr, (duk_hbufobj *) obj, idx))) {
			duk_replace_posidx_unsafe(thr, idx_out);
			return DUK__GETOWN_FOUND;
		}
		return DUK__GETOWN_DONE_NOTFOUND; /* Short circuit. */
#endif /* DUK_USE_BUFFEROBJECT_SUPPORT */
	default:
		break;
	}

	return duk__get_own_prop_idxkey_ordinary(thr, obj, idx, idx_out, idx_recv);
}

/*
 *  Index key htype handlers.
 */

DUK_LOCAL duk_bool_t
duk__get_ownprop_idxkey_error(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	DUK_UNREF(thr);
	DUK_UNREF(obj);
	DUK_UNREF(idx);
	DUK_UNREF(idx_out);
	DUK_UNREF(idx_recv);

	return DUK__GETOWN_NOTFOUND;
}

DUK_LOCAL duk_bool_t
duk__get_ownprop_idxkey_ordinary(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	return duk__get_own_prop_idxkey_ordinary(thr, obj, idx, idx_out, idx_recv);
}

DUK_LOCAL duk_bool_t
duk__get_ownprop_idxkey_array(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	if (DUK_LIKELY(DUK_HOBJECT_HAS_ARRAY_ITEMS(obj))) {
		return duk__get_own_prop_idxkey_arrayitems(thr, obj, idx, idx_out, idx_recv);
	}
	return duk__get_own_prop_idxkey_ordinary(thr, obj, idx, idx_out, idx_recv);
}

DUK_LOCAL duk_bool_t
duk__get_ownprop_idxkey_arguments(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	return duk__get_own_prop_idxkey_arguments(thr, obj, idx, idx_out, idx_recv);
}

DUK_LOCAL duk_bool_t
duk__get_ownprop_idxkey_proxy(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	/* Handled by caller; see comments for Proxy in the strkey path. */
	DUK_UNREF(thr);
	DUK_UNREF(obj);
	DUK_UNREF(idx);
	DUK_UNREF(idx_out);
	DUK_UNREF(idx_recv);
	return DUK__GETOWN_NOTFOUND;
}

DUK_LOCAL duk_bool_t
duk__get_ownprop_idxkey_stringobj(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	duk_small_int_t str_rc;

	str_rc = duk__get_own_prop_idxkey_stringobj(thr, obj, idx, idx_out, idx_recv);
	if (str_rc < 0) {
		/* Continue with normal property table lookup. */
		return duk__get_own_prop_idxkey_ordinary(thr, obj, idx, idx_out, idx_recv);
	}
	return (duk_bool_t) str_rc;
}

#if defined(DUK_USE_BUFFEROBJECT_SUPPORT)
DUK_LOCAL duk_bool_t
duk__get_ownprop_idxkey_typedarray(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	/* All arridx are captured and don't reach OrdinaryGetOwnProperty(). */
	DUK_UNREF(idx_recv);
	if (DUK_LIKELY(duk_hbufobj_validate_and_read_push(thr, (duk_hbufobj *) obj, idx))) {
		duk_replace_posidx_unsafe(thr, idx_out);
		return DUK__GETOWN_FOUND;
	}
	return DUK__GETOWN_DONE_NOTFOUND; /* Short circuit. */
}
#else
DUK_LOCAL duk_bool_t
duk__get_ownprop_idxkey_typedarray(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	return duk__get_ownprop_idxkey_error(thr, obj, idx, idx_out, idx_recv);
}
#endif /* DUK_USE_BUFFEROBJECT_SUPPORT */

#if defined(DUK_USE_BUFFEROBJECT_SUPPORT)
DUK_LOCAL duk_bool_t
duk__get_ownprop_idxkey_int8array(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	return duk__get_ownprop_idxkey_typedarray(thr, obj, idx, idx_out, idx_recv);
}
#else
DUK_LOCAL duk_bool_t
duk__get_ownprop_idxkey_int8array(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	return duk__get_ownprop_idxkey_error(thr, obj, idx, idx_out, idx_recv);
}
#endif /* DUK_USE_BUFFEROBJECT_SUPPORT */

#if defined(DUK_USE_BUFFEROBJECT_SUPPORT)
DUK_LOCAL duk_bool_t
duk__get_ownprop_idxkey_uint8array(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	duk_hbufobj *h = (duk_hbufobj *) obj;
	duk_uint8_t *data;

	DUK_UNREF(idx_recv);

	data = duk_hbufobj_uint8array_get_validated_data_ptr(thr, h, idx);
	if (DUK_LIKELY(data != NULL)) {
		duk_bool_t rc = duk__prop_get_write_u32_result(thr, idx_out, (duk_uint32_t) *data);
		DUK_ASSERT(rc == 1);
		DUK_ASSERT(DUK__GETOWN_FOUND == 1);
		return rc;
	} else {
		/* Out-of-bounds, detached, uncovered: treat as not found. */
		return DUK__GETOWN_DONE_NOTFOUND; /* Short circuit. */
	}
}
#else
DUK_LOCAL duk_bool_t
duk__get_ownprop_idxkey_uint8array(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	return duk__get_ownprop_idxkey_error(thr, obj, idx, idx_out, idx_recv);
}
#endif /* DUK_USE_BUFFEROBJECT_SUPPORT */

DUK_LOCAL duk_bool_t duk__get_ownprop_idxkey_uint8clampedarray(duk_hthread *thr,
                                                               duk_hobject *obj,
                                                               duk_uarridx_t idx,
                                                               duk_idx_t idx_out,
                                                               duk_idx_t idx_recv) {
	return duk__get_ownprop_idxkey_typedarray(thr, obj, idx, idx_out, idx_recv);
}

DUK_LOCAL duk_bool_t
duk__get_ownprop_idxkey_int16array(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	return duk__get_ownprop_idxkey_typedarray(thr, obj, idx, idx_out, idx_recv);
}

DUK_LOCAL duk_bool_t
duk__get_ownprop_idxkey_uint16array(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	return duk__get_ownprop_idxkey_typedarray(thr, obj, idx, idx_out, idx_recv);
}

DUK_LOCAL duk_bool_t
duk__get_ownprop_idxkey_int32array(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	return duk__get_ownprop_idxkey_typedarray(thr, obj, idx, idx_out, idx_recv);
}

DUK_LOCAL duk_bool_t
duk__get_ownprop_idxkey_uint32array(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	return duk__get_ownprop_idxkey_typedarray(thr, obj, idx, idx_out, idx_recv);
}

DUK_LOCAL duk_bool_t
duk__get_ownprop_idxkey_float32array(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	return duk__get_ownprop_idxkey_typedarray(thr, obj, idx, idx_out, idx_recv);
}

DUK_LOCAL duk_bool_t
duk__get_ownprop_idxkey_float64array(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	return duk__get_ownprop_idxkey_typedarray(thr, obj, idx, idx_out, idx_recv);
}

/* On 32-bit targets this takes 256 bytes.  There are only a few handlers so
 * this can be footprint optimized to 1*64 (one byte lookup) and 7*4=28 for
 * a table of 7 handlers, saving about 150 bytes.
 *
 * On 64-bit targets this typically takes 512 bytes.
 */
DUK_LOCAL const duk__get_ownprop_idxkey_htype duk__getown_idxkey_handlers[64] = {
	duk__get_ownprop_idxkey_error,        duk__get_ownprop_idxkey_error,       duk__get_ownprop_idxkey_error,
	duk__get_ownprop_idxkey_error,        duk__get_ownprop_idxkey_error,       duk__get_ownprop_idxkey_error,
	duk__get_ownprop_idxkey_error,        duk__get_ownprop_idxkey_error,

	duk__get_ownprop_idxkey_array,        duk__get_ownprop_idxkey_arguments,   duk__get_ownprop_idxkey_ordinary,
	duk__get_ownprop_idxkey_error,        duk__get_ownprop_idxkey_ordinary,    duk__get_ownprop_idxkey_ordinary,
	duk__get_ownprop_idxkey_ordinary,     duk__get_ownprop_idxkey_error,

	duk__get_ownprop_idxkey_ordinary,     duk__get_ownprop_idxkey_ordinary,    duk__get_ownprop_idxkey_ordinary,
	duk__get_ownprop_idxkey_ordinary,     duk__get_ownprop_idxkey_ordinary,    duk__get_ownprop_idxkey_ordinary,
	duk__get_ownprop_idxkey_ordinary,     duk__get_ownprop_idxkey_stringobj,

	duk__get_ownprop_idxkey_ordinary,     duk__get_ownprop_idxkey_ordinary,    duk__get_ownprop_idxkey_ordinary,
	duk__get_ownprop_idxkey_ordinary,     duk__get_ownprop_idxkey_ordinary,    duk__get_ownprop_idxkey_ordinary,
	duk__get_ownprop_idxkey_proxy,        duk__get_ownprop_idxkey_error,

	duk__get_ownprop_idxkey_error,        duk__get_ownprop_idxkey_error,       duk__get_ownprop_idxkey_error,
	duk__get_ownprop_idxkey_error,        duk__get_ownprop_idxkey_error,       duk__get_ownprop_idxkey_error,
	duk__get_ownprop_idxkey_error,        duk__get_ownprop_idxkey_error,

	duk__get_ownprop_idxkey_error,        duk__get_ownprop_idxkey_error,       duk__get_ownprop_idxkey_error,
	duk__get_ownprop_idxkey_error,        duk__get_ownprop_idxkey_error,       duk__get_ownprop_idxkey_error,
	duk__get_ownprop_idxkey_error,        duk__get_ownprop_idxkey_error,

	duk__get_ownprop_idxkey_ordinary,     duk__get_ownprop_idxkey_error,       duk__get_ownprop_idxkey_ordinary,
	duk__get_ownprop_idxkey_int8array,    duk__get_ownprop_idxkey_uint8array,  duk__get_ownprop_idxkey_uint8clampedarray,
	duk__get_ownprop_idxkey_int16array,   duk__get_ownprop_idxkey_uint16array,

	duk__get_ownprop_idxkey_int32array,   duk__get_ownprop_idxkey_uint32array, duk__get_ownprop_idxkey_float32array,
	duk__get_ownprop_idxkey_float64array, duk__get_ownprop_idxkey_error,       duk__get_ownprop_idxkey_error,
	duk__get_ownprop_idxkey_error,        duk__get_ownprop_idxkey_error
};

DUK_LOCAL duk_bool_t
duk__get_own_prop_idxkey_htypejump(duk_hthread *thr, duk_hobject *obj, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	duk_small_uint_t htype;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(obj != NULL);
	DUK_ASSERT_ARRIDX_VALID(idx);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));

	htype = DUK_HEAPHDR_GET_HTYPE((duk_heaphdr *) obj);
	DUK_ASSERT(DUK_HTYPE_IS_ANY_OBJECT(htype));

	return duk__getown_idxkey_handlers[htype](thr, obj, idx, idx_out, idx_recv);
}

/*
 *  String key htype handlers
 */

DUK_LOCAL duk_bool_t
duk__get_ownprop_strkey_error(duk_hthread *thr, duk_hobject *obj, duk_hstring *key, duk_idx_t idx_out, duk_idx_t idx_recv) {
	DUK_UNREF(thr);
	DUK_UNREF(obj);
	DUK_UNREF(key);
	DUK_UNREF(idx_out);
	DUK_UNREF(idx_recv);
	return DUK__GETOWN_NOTFOUND;
}

DUK_LOCAL duk_bool_t
duk__get_ownprop_strkey_ordinary(duk_hthread *thr, duk_hobject *obj, duk_hstring *key, duk_idx_t idx_out, duk_idx_t idx_recv) {
	return duk__get_own_prop_strkey_ordinary(thr, obj, key, idx_out, idx_recv);
}

DUK_LOCAL duk_bool_t
duk__get_ownprop_strkey_array(duk_hthread *thr, duk_hobject *obj, duk_hstring *key, duk_idx_t idx_out, duk_idx_t idx_recv) {
	if (DUK_HSTRING_HAS_LENGTH(key)) {
		duk_harray *a = (duk_harray *) obj;
		duk_uint32_t len = (duk_uint32_t) DUK_HARRAY_GET_LENGTH(a);
		return duk__prop_get_write_u32_result(thr, idx_out, len);
	}
	return duk__get_own_prop_strkey_ordinary(thr, obj, key, idx_out, idx_recv);
}

DUK_LOCAL duk_bool_t
duk__get_ownprop_strkey_stringobj(duk_hthread *thr, duk_hobject *obj, duk_hstring *key, duk_idx_t idx_out, duk_idx_t idx_recv) {
	if (DUK_HSTRING_HAS_LENGTH(key)) {
		return duk__get_own_prop_strkey_stringobj_length(thr, obj, key, idx_out, idx_recv);
	}
	return duk__get_own_prop_strkey_ordinary(thr, obj, key, idx_out, idx_recv);
}

DUK_LOCAL duk_bool_t
duk__get_ownprop_strkey_proxy(duk_hthread *thr, duk_hobject *obj, duk_hstring *key, duk_idx_t idx_out, duk_idx_t idx_recv) {
	/* Handled in call site specially. */
	DUK_UNREF(thr);
	DUK_UNREF(obj);
	DUK_UNREF(key);
	DUK_UNREF(idx_out);
	DUK_UNREF(idx_recv);
	return DUK__GETOWN_NOTFOUND;
}

#if defined(DUK_USE_BUFFEROBJECT_SUPPORT)
DUK_LOCAL duk_bool_t
duk__get_ownprop_strkey_typedarray(duk_hthread *thr, duk_hobject *obj, duk_hstring *key, duk_idx_t idx_out, duk_idx_t idx_recv) {
	if (DUK_HSTRING_HAS_LENGTH(key)) {
		duk_hbufobj *h = (duk_hbufobj *) obj;
		duk_uint32_t len = (duk_uint32_t) DUK_HBUFOBJ_GET_LOGICAL_LENGTH(h);
		return duk__prop_get_write_u32_result(thr, idx_out, len);
	}
	if (DUK_HSTRING_HAS_CANNUM(key)) {
		/* We could actually skip this check if property
		 * write path rejects any attempts to establish
		 * concrete CanonicalNumericIndexString properties.
		 * We would then return "not found" from the concrete
		 * property check (performance of that is irrelevant
		 * for these keys).  In any case we must not continue
		 * prototype walk to the parent.
		 */
		return DUK__GETOWN_DONE_NOTFOUND; /* Short circuit, don't continue walk. */
	}
	return duk__get_own_prop_strkey_ordinary(thr, obj, key, idx_out, idx_recv);
}
#else
DUK_LOCAL duk_bool_t
duk__get_ownprop_strkey_typedarray(duk_hthread *thr, duk_hobject *obj, duk_hstring *key, duk_idx_t idx_out, duk_idx_t idx_recv) {
	return duk__get_ownprop_strkey_error(thr, obj, key, idx_out, idx_recv);
}
#endif /* DUK_USE_BUFFEROBJECT_SUPPORT */

DUK_LOCAL const duk__get_ownprop_strkey_htype duk__getown_strkey_handlers[64] = {
	duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_error,
	duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_error,
	duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_error,

	duk__get_ownprop_strkey_array,      duk__get_ownprop_strkey_ordinary, /* Arguments: in ES2017+ no more strkey exotics, e.g.
	                                                                         .caller. */
	duk__get_ownprop_strkey_ordinary,   duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_ordinary,
	duk__get_ownprop_strkey_ordinary,   duk__get_ownprop_strkey_ordinary,   duk__get_ownprop_strkey_error,

	duk__get_ownprop_strkey_ordinary,   duk__get_ownprop_strkey_ordinary,   duk__get_ownprop_strkey_ordinary,
	duk__get_ownprop_strkey_ordinary,   duk__get_ownprop_strkey_ordinary,   duk__get_ownprop_strkey_ordinary,
	duk__get_ownprop_strkey_ordinary,   duk__get_ownprop_strkey_stringobj,

	duk__get_ownprop_strkey_ordinary,   duk__get_ownprop_strkey_ordinary,   duk__get_ownprop_strkey_ordinary,
	duk__get_ownprop_strkey_ordinary,   duk__get_ownprop_strkey_ordinary,   duk__get_ownprop_strkey_ordinary,
	duk__get_ownprop_strkey_proxy,      duk__get_ownprop_strkey_error,

	duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_error,
	duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_error,
	duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_error,

	duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_error,
	duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_error,
	duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_error,

	duk__get_ownprop_strkey_ordinary,   duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_ordinary,
	duk__get_ownprop_strkey_typedarray, duk__get_ownprop_strkey_typedarray, duk__get_ownprop_strkey_typedarray,
	duk__get_ownprop_strkey_typedarray, duk__get_ownprop_strkey_typedarray,

	duk__get_ownprop_strkey_typedarray, duk__get_ownprop_strkey_typedarray, duk__get_ownprop_strkey_typedarray,
	duk__get_ownprop_strkey_typedarray, duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_error,
	duk__get_ownprop_strkey_error,      duk__get_ownprop_strkey_error
};

DUK_LOCAL duk_bool_t
duk__get_own_prop_strkey_htypejump(duk_hthread *thr, duk_hobject *obj, duk_hstring *key, duk_idx_t idx_out, duk_idx_t idx_recv) {
	duk_small_uint_t htype;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(obj != NULL);
	DUK_ASSERT(key != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));

	htype = DUK_HEAPHDR_GET_HTYPE((duk_heaphdr *) obj);
	DUK_ASSERT(DUK_HTYPE_IS_ANY_OBJECT(htype));

	return duk__getown_strkey_handlers[htype](thr, obj, key, idx_out, idx_recv);
}

/*
 *  [[Get]] prototype walk
 */

DUK_LOCAL DUK_ALWAYS_INLINE duk_bool_t duk__prop_get_stroridx_helper(duk_hthread *thr,
                                                                     duk_hobject *target,
                                                                     duk_hstring *key,
                                                                     duk_uarridx_t idx,
                                                                     duk_idx_t idx_out,
                                                                     duk_idx_t idx_recv,
                                                                     duk_bool_t use_key,
                                                                     duk_bool_t side_effect_safe) {
	duk_bool_t rc;
	duk_small_uint_t sanity;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(target != NULL);
	if (use_key) {
		DUK_ASSERT(key != NULL);
		DUK_ASSERT(!DUK_HSTRING_HAS_ARRIDX(key));
	} else {
		DUK_ASSERT_ARRIDX_VALID(idx);
	}
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));

	if (side_effect_safe) {
		duk_push_hobject(thr, target);
#if 0
		tv_target = thr->valstack_top++;
		DUK_ASSERT(DUK_TVAL_IS_UNDEFINED(tv_target));
		DUK_TVAL_SET_OBJECT_INCREF(thr, tv_target, target);
#endif
	}

	/* The current 'target' may only be reachable via the prototype (and
	 * Proxy ->target) chain from 'obj', and any side effect involving
	 * finalizers may alter that chain.  So we must be careful not to
	 * trigger such side effects without stabilizing the 'target' reference.
	 *
	 * If a Proxy is encountered we switch to a model where the current
	 * target is on the stack top: a trap check (for a missing trap) may
	 * have side effects and may break the prototype chain from the
	 * original receiver to 'target'.  So after a trap check with side
	 * effects we need to keep the current target reachable on our own,
	 * even after the initial Proxy.
	 */

	sanity = DUK_HOBJECT_PROTOTYPE_CHAIN_SANITY;
	do {
		duk_hobject *next;

#if defined(DUK_USE_ASSERTIONS)
#if defined(DUK_USE_REFERENCE_COUNTING)
		DUK_ASSERT(DUK_HEAPHDR_GET_REFCOUNT((duk_heaphdr *) target) > 0);
		if (side_effect_safe && DUK_HEAPHDR_GET_REFCOUNT((duk_heaphdr *) target) == 1) {
			DUK_DD(DUK_DDPRINT("'target' is only reachable via stabilized value stack slot"));
		}
#endif
#endif

		DUK_GC_TORTURE(thr->heap);
		if (use_key) {
			DUK_UNREF(duk__get_own_prop_strkey_switch);
#if 0
			rc = duk__get_own_prop_strkey_switch(thr, target, key, idx_out, idx_recv);
#else
			rc = duk__get_own_prop_strkey_htypejump(thr, target, key, idx_out, idx_recv);
#endif
		} else {
			DUK_UNREF(duk__get_own_prop_idxkey_switch);
#if 0
			rc = duk__get_own_prop_idxkey_switch(thr, target, idx, idx_out, idx_recv);
#else
			rc = duk__get_own_prop_idxkey_htypejump(thr, target, idx, idx_out, idx_recv);
#endif
		}
		DUK_GC_TORTURE(thr->heap);
		DUK_ASSERT(rc == DUK__GETOWN_NOTFOUND || rc == DUK__GETOWN_FOUND || rc == DUK__GETOWN_DONE_NOTFOUND);

		if (rc != DUK__GETOWN_NOTFOUND) {
			if (DUK_LIKELY(rc == DUK__GETOWN_FOUND)) {
				goto found;
			} else {
				DUK_ASSERT(rc == DUK__GETOWN_DONE_NOTFOUND);
				goto not_found;
			}
		}

		/* The ordinary [[Get]] algorithm looks up [[GetPrototypeOf]]
		 * but only for ordinary objects, so Proxy 'getPrototypeOf' is
		 * never invoked here; instead we move on to the Proxy target
		 * after the 'get' trap, without invoking any more traps.
		 *
		 * Because of side effect issues Proxies are handled below
		 * (e.g. duk__get_own_prop_strkey() does nothing and returns 0
		 * == not found).  Take advantage of the fact that a Proxy has
		 * NULL as the raw internal prototype to avoid the Proxy check
		 * for inherit fast path.
		 *
		 * At present we always switch to the side effect safe path
		 * when encountering a Proxy.  This is unnecessary if the
		 * Proxy trap lookup doesn't actually trigger side effects
		 * (e.g. the handler itself is a Proxy); in the majority of
		 * cases we could actually remain on the unsafe path.
		 */
#if defined(DUK_USE_ASSERTIONS)
#if defined(DUK_USE_REFERENCE_COUNTING)
		DUK_ASSERT(DUK_HEAPHDR_GET_REFCOUNT((duk_heaphdr *) target) > 0);
		if (side_effect_safe && DUK_HEAPHDR_GET_REFCOUNT((duk_heaphdr *) target) == 1) {
			/* Useful in some test cases where we want to exercise
			 * the stabilization approach.
			 */
			DUK_DD(DUK_DDPRINT("'target' is only reachable via stabilized value stack slot"));
		}
#endif
#endif

		next = duk_hobject_get_proto_raw(thr->heap, target);
		if (next == NULL) {
			if (DUK_UNLIKELY(DUK_HOBJECT_HAS_EXOTIC_PROXYOBJ(target))) {
				if (side_effect_safe) {
					/* Actual Proxy handling is here. */
					if (use_key) {
						rc = duk__get_own_prop_strkey_proxy_actual(thr, target, key, idx_out, idx_recv);
					} else {
						rc = duk__get_own_prop_idxkey_proxy_actual(thr, target, idx, idx_out, idx_recv);
					}
					DUK_ASSERT(rc == DUK__GETOWN_NOTFOUND || rc == DUK__GETOWN_FOUND);
					if (rc != DUK__GETOWN_NOTFOUND) {
						goto found;
					} else {
						DUK_ASSERT(DUK_HOBJECT_HAS_EXOTIC_PROXYOBJ(target));
						next = duk_proxy_get_target_autothrow(thr, (duk_hproxy *) target);
						DUK_ASSERT(next != NULL);
					}
				} else {
					/* Handling for initial Proxy found, switch to side effect free path. */
					DUK_DD(DUK_DDPRINT("encountered Proxy, switch to side effect free variant"));
					goto switch_to_safe;
				}
			} else {
				goto not_found;
			}
		} else {
			DUK_ASSERT(DUK_HOBJECT_GET_HTYPE(target) != DUK_HTYPE_PROXY);
		}

		DUK_ASSERT(next != NULL);
		if (side_effect_safe) {
			target = duk_prop_switch_stabilized_target_top(thr, target, next);
		} else {
			target = next;
		}
	} while (--sanity > 0);

	DUK_ERROR_RANGE_PROTO_SANITY(thr);
	DUK_WO_NORETURN(return 0;);

not_found:
	if (side_effect_safe) {
		DUK_ASSERT(duk_get_hobject(thr, -1) == target);
		duk_pop_known(thr);
	}
	return duk__prop_get_write_notfound_result(thr, idx_out);

found:
	DUK_ASSERT(rc == 1);
	if (side_effect_safe) {
		duk_pop_known(thr);
		return 1;
	} else {
		return rc;
	}

switch_to_safe:
	if (use_key) {
		return duk__prop_get_strkey_safe(thr, target, key, idx_out, idx_recv);
	} else {
		return duk__prop_get_idxkey_safe(thr, target, idx, idx_out, idx_recv);
	}
}

DUK_LOCAL duk_bool_t
duk__prop_get_strkey_unsafe(duk_hthread *thr, duk_hobject *target, duk_hstring *key, duk_idx_t idx_out, duk_idx_t idx_recv) {
#if defined(DUK_USE_PREFER_SIZE)
	return duk__prop_get_strkey_safe(thr, target, key, idx_out, idx_recv);
#else
	return duk__prop_get_stroridx_helper(thr, target, key, 0, idx_out, idx_recv, 1 /*use_key*/, 0 /*side_effect_safe*/);
#endif
}

DUK_LOCAL DUK_NOINLINE duk_bool_t
duk__prop_get_strkey_safe(duk_hthread *thr, duk_hobject *target, duk_hstring *key, duk_idx_t idx_out, duk_idx_t idx_recv) {
	return duk__prop_get_stroridx_helper(thr, target, key, 0, idx_out, idx_recv, 1 /*use_key*/, 1 /*side_effect_safe*/);
}

/* [[Get]] for any duk_hobject, with string/symbol key, excluding indexes handled by
 * duk__prop_get_idx() and variants.
 */
DUK_LOCAL duk_bool_t
duk__prop_get_str(duk_hthread *thr, duk_hobject *target, duk_hstring *key, duk_idx_t idx_out, duk_idx_t idx_recv) {
	DUK_STATS_INC(thr->heap, stats_get_strkey_count);
	return duk__prop_get_strkey_unsafe(thr, target, key, idx_out, idx_recv);
}

/* [[Get]] for duk_hobjects with ordinary behavior, with index key. */
DUK_LOCAL duk_bool_t
duk__prop_get_idxkey_unsafe(duk_hthread *thr, duk_hobject *target, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
#if defined(DUK_USE_PREFER_SIZE)
	return duk__prop_get_idxkey_safe(thr, target, idx, idx_out, idx_recv);
#else
	return duk__prop_get_stroridx_helper(thr, target, NULL, idx, idx_out, idx_recv, 0 /*use_key*/, 0 /*side_effect_safe*/);
#endif
}

DUK_LOCAL DUK_NOINLINE duk_bool_t
duk__prop_get_idxkey_safe(duk_hthread *thr, duk_hobject *target, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	return duk__prop_get_stroridx_helper(thr, target, NULL, idx, idx_out, idx_recv, 0 /*use_key*/, 1 /*side_effect_safe*/);
}

/* [[Get]] for any duk_hobject, with index key. */
DUK_LOCAL duk_bool_t
duk__prop_get_idx(duk_hthread *thr, duk_hobject *target, duk_uarridx_t idx, duk_idx_t idx_out, duk_idx_t idx_recv) {
	DUK_STATS_INC(thr->heap, stats_get_idxkey_count);
	return duk__prop_get_idxkey_unsafe(thr, target, idx, idx_out, idx_recv);
}

DUK_LOCAL duk_bool_t
duk__prop_getvalue_plainstr_index(duk_hthread *thr, duk_idx_t idx_recv, duk_uarridx_t idx, duk_idx_t idx_out, duk_hstring *h) {
	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));
	DUK_ASSERT_ARRIDX_VALID(idx);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	DUK_ASSERT(h != NULL);
	DUK_UNREF(idx_recv);

	duk_prop_push_plainstr_idx(thr, h, idx);
	duk_replace_posidx_unsafe(thr, idx_out);
	return 1;
}

DUK_LOCAL duk_bool_t duk__prop_getvalue_idxkey_outidx(duk_hthread *thr, duk_idx_t idx_recv, duk_uarridx_t idx, duk_idx_t idx_out) {
	duk_hobject *next;
	duk_small_uint_t next_bidx;
	duk_tval *tv_recv;
	duk_small_uint_t tag_recv;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));
	DUK_ASSERT_ARRIDX_VALID(idx);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));

	DUK_STATS_INC(thr->heap, stats_getvalue_idxkey_count);

	tv_recv = thr->valstack_bottom + idx_recv;
	DUK_ASSERT(DUK_HTHREAD_TVAL_IN_VSFRAME(thr, tv_recv));

	tag_recv = DUK_TVAL_GET_TAG(tv_recv);

#if 0
	if (DUK_LIKELY(tag_recv == DUK_TAG_OBJECT)) {
		/* Typical case. */
		next = DUK_TVAL_GET_OBJECT(tv_recv);
		goto go_next;
	}
#endif

	switch (tag_recv) {
	case DUK_TAG_UNUSED:
	case DUK_TAG_UNDEFINED:
	case DUK_TAG_NULL:
		return duk__prop_get_error_objidx_idxkey(thr, idx_recv, idx);
	case DUK_TAG_BOOLEAN:
		next_bidx = DUK_BIDX_BOOLEAN_PROTOTYPE;
		break;
	case DUK_TAG_POINTER:
		next_bidx = DUK_BIDX_POINTER_PROTOTYPE;
		break;
	case DUK_TAG_LIGHTFUNC:
		next_bidx = DUK_BIDX_NATIVE_FUNCTION_PROTOTYPE;
		break;
	case DUK_TAG_STRING: {
		duk_hstring *h = DUK_TVAL_GET_STRING(tv_recv);

		if (DUK_LIKELY(!DUK_HSTRING_HAS_SYMBOL(h))) {
			if (DUK_LIKELY(idx < duk_hstring_get_charlen(h))) {
				return duk__prop_getvalue_plainstr_index(thr, idx_recv, idx, idx_out, h);
			}
			next_bidx = DUK_BIDX_STRING_PROTOTYPE;
		} else {
			next_bidx = DUK_BIDX_SYMBOL_PROTOTYPE;
		}
		break;
	}
	case DUK_TAG_OBJECT:
#if 1
		/* Typical case. */
		next = DUK_TVAL_GET_OBJECT(tv_recv);
		goto go_next;
#else
		/* Never here. */
		goto not_found;
#endif
	case DUK_TAG_BUFFER: {
		duk_hbuffer *h = DUK_TVAL_GET_BUFFER(tv_recv);
		if (DUK_LIKELY(idx < DUK_HBUFFER_GET_SIZE(h))) {
			duk_uint8_t *buf = (duk_uint8_t *) DUK_HBUFFER_GET_DATA_PTR(thr->heap, h);
			duk_uint32_t val = (duk_uint32_t) buf[idx];
			return duk__prop_get_write_u32_result(thr, idx_out, val);
		} else {
			/* Out bounds: don't walk prototype, matches Uint8Array. */
			goto not_found;
		}
		break;
	}
	default:
		DUK_ASSERT(DUK_TVAL_IS_NUMBER(tv_recv));
		next_bidx = DUK_BIDX_NUMBER_PROTOTYPE;
		break;
	}

	next = thr->builtins[next_bidx];
	/* fall thru */

go_next:
	return duk__prop_get_idx(thr, next, idx, idx_out, idx_recv);

not_found:
	return duk__prop_get_write_notfound_result(thr, idx_out);
}

DUK_LOCAL duk_bool_t duk__prop_getvalue_strkey_outidx(duk_hthread *thr, duk_idx_t idx_recv, duk_hstring *key, duk_idx_t idx_out) {
	duk_hobject *next;
	duk_small_uint_t next_bidx;
	duk_tval *tv_recv;
	duk_small_uint_t tag_recv;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));
	DUK_ASSERT(key != NULL);
	DUK_ASSERT(!DUK_HSTRING_HAS_ARRIDX(key));
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));

	DUK_STATS_INC(thr->heap, stats_getvalue_strkey_count);

	tv_recv = thr->valstack_bottom + idx_recv;
	DUK_ASSERT(DUK_HTHREAD_TVAL_IN_VSFRAME(thr, tv_recv));

	tag_recv = DUK_TVAL_GET_TAG(tv_recv);

	/* Typical property reads occur on objects, not primitive values so
	 * optimize for object case.  Plain buffers are an important non-object
	 * base property read case (here, for the 'length' property).
	 */
#if 0
	if (DUK_LIKELY(tag_recv == DUK_TAG_OBJECT)) {
		/* Typical case. */
		next = DUK_TVAL_GET_OBJECT(tv_recv);
		goto go_next;
	}
#endif

	switch (tag_recv) {
	case DUK_TAG_UNUSED:
	case DUK_TAG_UNDEFINED:
	case DUK_TAG_NULL:
		return duk__prop_get_error_objidx_strkey(thr, idx_recv, key);
	case DUK_TAG_STRING: {
		duk_hstring *h = DUK_TVAL_GET_STRING(tv_recv);

		if (DUK_LIKELY(!DUK_HSTRING_HAS_SYMBOL(h))) {
			if (DUK_HSTRING_HAS_LENGTH(key)) {
				return duk__prop_get_write_plainstr_length(thr, h, idx_out);
			}
			next_bidx = DUK_BIDX_STRING_PROTOTYPE;
		} else {
			next_bidx = DUK_BIDX_SYMBOL_PROTOTYPE;
		}
		break;
	}
	case DUK_TAG_OBJECT:
#if 1
		/* Typical case. */
		next = DUK_TVAL_GET_OBJECT(tv_recv);
		goto go_next;
#else
		/* Never here. */
		goto not_found;
#endif
	case DUK_TAG_BUFFER:
		if (DUK_HSTRING_HAS_LENGTH(key)) {
			duk_hbuffer *h = (duk_hbuffer *) DUK_TVAL_GET_BUFFER(tv_recv);
			duk_uint32_t len = (duk_uint32_t) DUK_HBUFFER_GET_SIZE(h);
			return duk__prop_get_write_u32_result(thr, idx_out, len);
		}
		next_bidx = DUK_BIDX_UINT8ARRAY_PROTOTYPE;
		break;
	case DUK_TAG_BOOLEAN:
		next_bidx = DUK_BIDX_BOOLEAN_PROTOTYPE;
		break;
	case DUK_TAG_POINTER:
		next_bidx = DUK_BIDX_POINTER_PROTOTYPE;
		break;
	case DUK_TAG_LIGHTFUNC:
		next_bidx = DUK_BIDX_NATIVE_FUNCTION_PROTOTYPE;
		break;
	default:
		DUK_ASSERT(DUK_TVAL_IS_NUMBER(tv_recv));
		next_bidx = DUK_BIDX_NUMBER_PROTOTYPE;
	}

	next = thr->builtins[next_bidx];
	/* fall thru */

go_next:
	return duk__prop_get_str(thr, next, key, idx_out, idx_recv);
}

/*
 *  Exposed internal APIs.
 */

DUK_INTERNAL duk_bool_t duk_prop_getvalue_strkey_outidx(duk_hthread *thr, duk_idx_t idx_recv, duk_hstring *key, duk_idx_t idx_out) {
#if defined(DUK_USE_ASSERTIONS)
	duk_idx_t entry_top;
#endif

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));
	DUK_ASSERT(key != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
#if defined(DUK_USE_ASSERTIONS)
	entry_top = duk_get_top(thr);
#endif

	if (DUK_UNLIKELY(DUK_HSTRING_HAS_ARRIDX(key))) {
		duk_bool_t rc;

		rc = duk__prop_getvalue_idxkey_outidx(thr, idx_recv, duk_hstring_get_arridx_fast_known(key), idx_out);
		DUK_ASSERT(duk_get_top(thr) == entry_top);
		return rc;
	} else {
		duk_bool_t rc;

		DUK_ASSERT(!DUK_HSTRING_HAS_ARRIDX(key));
		rc = duk__prop_getvalue_strkey_outidx(thr, idx_recv, key, idx_out);
		DUK_ASSERT(duk_get_top(thr) == entry_top);
		return rc;
	}
}

DUK_INTERNAL duk_bool_t duk_prop_getvalue_strkey_push(duk_hthread *thr, duk_idx_t idx_recv, duk_hstring *key) {
#if defined(DUK_USE_ASSERTIONS)
	duk_idx_t entry_top;
#endif
	duk_idx_t idx_out;
	duk_bool_t rc;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));
	DUK_ASSERT(key != NULL);
#if defined(DUK_USE_ASSERTIONS)
	entry_top = duk_get_top(thr);
#endif

	duk_push_undefined(thr);
	idx_out = (duk_idx_t) (thr->valstack_top - thr->valstack_bottom - 1);

	rc = duk_prop_getvalue_strkey_outidx(thr, idx_recv, key, idx_out);
	DUK_ASSERT(duk_get_top(thr) == entry_top + 1);
	return rc;
}

DUK_INTERNAL duk_bool_t duk_prop_getvalue_idxkey_outidx(duk_hthread *thr,
                                                        duk_idx_t idx_recv,
                                                        duk_uarridx_t idx,
                                                        duk_idx_t idx_out) {
#if defined(DUK_USE_ASSERTIONS)
	duk_idx_t entry_top;
#endif

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));
	DUK_ASSERT_ARRIDX_VALID(idx);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
#if defined(DUK_USE_ASSERTIONS)
	entry_top = duk_get_top(thr);
#endif

	if (DUK_LIKELY(idx <= DUK_ARRIDX_MAX)) {
		duk_bool_t rc = duk__prop_getvalue_idxkey_outidx(thr, idx_recv, idx, idx_out);
		DUK_ASSERT(duk_get_top(thr) == entry_top);
		return rc;
	} else {
		/* This happens for 0xffffffff specifically, which may be
		 * passed in even from application calls.  Another way to
		 * handle this corner case would be to handle 0xffffffff
		 * in the arridx path, but then special cases would be
		 * needed there to avoid arridx special behaviors.
		 */
		duk_bool_t rc;
		duk_hstring *key;

		DUK_DD(DUK_DDPRINT("corner case, input idx 0xffffffff is not an arridx, must coerce to string"));
		key = duk_push_u32_tohstring(thr, idx);
		rc = duk__prop_getvalue_strkey_outidx(thr, idx_recv, key, idx_out);
		duk_pop_known(thr);
		DUK_ASSERT(duk_get_top(thr) == entry_top);
		return rc;
	}
}

DUK_INTERNAL duk_bool_t duk_prop_getvalue_outidx(duk_hthread *thr, duk_idx_t idx_recv, duk_tval *tv_key, duk_idx_t idx_out) {
#if defined(DUK_USE_ASSERTIONS)
	duk_idx_t entry_top;
#endif
	duk_bool_t rc;
	duk_hstring *key;
	duk_uarridx_t idx;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));
	DUK_ASSERT(tv_key != NULL);
	/* tv_key may not be in value stack but it must be reachable and
	 * remain reachable despite arbitrary side effects (e.g. function
	 * constant table).
	 */
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
	/* Output index may overlap with receiver or key. */
#if defined(DUK_USE_ASSERTIONS)
	entry_top = duk_get_top(thr);
#endif

	/* Must check receiver (typically object) and key (typically
	 * string, symbol, or numeric index) efficiently.  Key is coerced
	 * using ToPropertyKey(), receiver using ToObject().  We want to
	 * avoid actual receiver coercion if possible.
	 *
	 * Specification requires the base is ToObject() coerced before the
	 * key, but this is inconvenient here.  The order doesn't really
	 * matter unless key coercion has side effects which only happen
	 * for undefined/null base.  So the slow path where key needs actual
	 * coercion checks undefined/null base which must cause an error
	 * before key coercion; other ToObject() coercions are side effect
	 * free.
	 *
	 * Expected common key types are: string, Symbol, fastint/number.
	 * Other keys are rare and not relevant for performance.
	 */

#if 0
	if (DUK_TVAL_IS_STRING(tv_key)) {
		key = DUK_TVAL_GET_STRING(tv_key);
		if (DUK_UNLIKELY(DUK_HSTRING_HAS_ARRIDX(key))) {
			idx = duk_hstring_get_arridx_fast_known(key);
			goto use_idx;
		} else {
			goto use_str;
		}
	} else if (DUK_TVAL_IS_FASTINT(tv_key)) {
		duk_int64_t fi = DUK_TVAL_GET_FASTINT(tv_key);
		if (fi >= 0 && fi <= (duk_int64_t) DUK_ARRIDX_MAX) {
			idx = (duk_uarridx_t) fi;
			goto use_idx;
		}
	}
#endif

	switch (DUK_TVAL_GET_TAG(tv_key)) {
	case DUK_TAG_STRING:
		key = DUK_TVAL_GET_STRING(tv_key);
		if (DUK_UNLIKELY(DUK_HSTRING_HAS_ARRIDX(key))) {
			idx = duk_hstring_get_arridx_fast_known(key);
			goto use_idx;
		} else {
			goto use_str;
		}
#if defined(DUK_USE_FASTINT)
	case DUK_TAG_FASTINT: {
		duk_int64_t fi = DUK_TVAL_GET_FASTINT(tv_key);
		if (fi >= 0 && fi <= (duk_int64_t) DUK_ARRIDX_MAX) {
			idx = (duk_uarridx_t) fi;
			goto use_idx;
		}
		break;
	}
#endif
#if !defined(DUK_USE_PACKED_TVAL)
	case DUK_TAG_NUMBER: {
		duk_double_t d = DUK_TVAL_GET_DOUBLE(tv_key);
		if (duk_prop_double_idx_check(d, &idx)) {
			goto use_idx;
		}
		break;
	}
#endif
	case DUK_TAG_UNUSED:
	case DUK_TAG_UNDEFINED:
	case DUK_TAG_NULL:
	case DUK_TAG_BOOLEAN:
	case DUK_TAG_POINTER:
	case DUK_TAG_LIGHTFUNC:
	case DUK_TAG_OBJECT:
	case DUK_TAG_BUFFER:
		break;
	default: {
#if defined(DUK_USE_PACKED_TVAL)
		duk_double_t d;
		DUK_ASSERT(DUK_TVAL_IS_NUMBER(tv_key));
#if defined(DUK_USE_FASTINT)
		DUK_ASSERT(!DUK_TVAL_IS_FASTINT(tv_key));
#endif
		d = DUK_TVAL_GET_DOUBLE(tv_key);
		if (duk_prop_double_idx_check(d, &idx)) {
			goto use_idx;
		}
#endif
		break;
	}
	}

	/* We need to coerce the key, and we need temporary value stack
	 * space to do it.  Do it on stack top and pop the coerced key
	 * afterwards.  This is an uncommon case.
	 *
	 * We could handle many cases with a fast path, e.g.
	 * ToPropertyKey(null) is always 'null', but it's probably
	 * not worth optimizing for in the switch-case above.
	 *
	 * Key coercion without storing the original key means that error
	 * messages can't reference the uncoerced key.  This trade-off
	 * seems acceptable considering the cost of the alternative.
	 */

	if (DUK_UNLIKELY(duk_is_nullish(thr, idx_recv))) {
		/* Must TypeError before key coercion side effects. */
		rc = duk__prop_get_error_objidx_tvkey(thr, idx_recv, tv_key);
		DUK_ASSERT(duk_get_top(thr) == entry_top);
		return rc;
	}

	duk_push_tval(thr, tv_key);
	tv_key = NULL;
	key = duk_to_property_key_hstring(thr, -1);
	DUK_ASSERT(key != NULL);
	rc = duk_prop_getvalue_strkey_outidx(thr, idx_recv, key, idx_out);
	duk_pop_known(thr);
	DUK_ASSERT(duk_get_top(thr) == entry_top);
	return rc;

use_idx:
	DUK_ASSERT_ARRIDX_VALID(idx);
	rc = duk__prop_getvalue_idxkey_outidx(thr, idx_recv, idx, idx_out);
	DUK_ASSERT(duk_get_top(thr) == entry_top);
	return rc;

use_str:
	DUK_ASSERT(!DUK_HSTRING_HAS_ARRIDX(key));
	rc = duk__prop_getvalue_strkey_outidx(thr, idx_recv, key, idx_out);
	DUK_ASSERT(duk_get_top(thr) == entry_top);
	return rc;
}

DUK_INTERNAL duk_bool_t duk_prop_getvalue_push(duk_hthread *thr, duk_idx_t idx_recv, duk_tval *tv_key) {
#if defined(DUK_USE_ASSERTIONS)
	duk_idx_t entry_top;
#endif
	duk_bool_t rc;
	duk_idx_t idx_out;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));
	DUK_ASSERT(tv_key != NULL);
#if defined(DUK_USE_ASSERTIONS)
	entry_top = duk_get_top(thr);
#endif

	duk_push_undefined(thr);
	idx_out = duk_get_top_index_known(thr);

	rc = duk_prop_getvalue_outidx(thr, idx_recv, tv_key, idx_out);
	DUK_ASSERT(duk_get_top(thr) == entry_top + 1);
	return rc;
}

DUK_INTERNAL duk_bool_t duk_prop_getvalue_stridx_outidx(duk_hthread *thr,
                                                        duk_idx_t idx_recv,
                                                        duk_small_uint_t stridx,
                                                        duk_idx_t idx_out) {
#if defined(DUK_USE_ASSERTIONS)
	duk_idx_t entry_top;
#endif
	duk_bool_t rc;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));
	DUK_ASSERT_STRIDX_VALID(stridx);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_out));
#if defined(DUK_USE_ASSERTIONS)
	entry_top = duk_get_top(thr);
#endif

	rc = duk_prop_getvalue_strkey_outidx(thr, idx_recv, DUK_HTHREAD_GET_STRING(thr, stridx), idx_out);
	DUK_ASSERT(duk_get_top(thr) == entry_top);
	return rc;
}

DUK_INTERNAL duk_bool_t duk_prop_getvalue_stridx_push(duk_hthread *thr, duk_idx_t idx_recv, duk_small_uint_t stridx) {
#if defined(DUK_USE_ASSERTIONS)
	duk_idx_t entry_top;
#endif
	duk_bool_t rc;

	DUK_ASSERT(thr != NULL);
	DUK_ASSERT(duk_is_valid_posidx(thr, idx_recv));
	DUK_ASSERT_STRIDX_VALID(stridx);
#if defined(DUK_USE_ASSERTIONS)
	entry_top = duk_get_top(thr);
#endif

	rc = duk_prop_getvalue_strkey_push(thr, idx_recv, DUK_HTHREAD_GET_STRING(thr, stridx));
	DUK_ASSERT(duk_get_top(thr) == entry_top + 1);
	return rc;
}
