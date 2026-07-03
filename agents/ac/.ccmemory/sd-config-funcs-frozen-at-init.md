---
name: sd-config-funcs-frozen-at-init
description: config.add_funcs JS funcs were frozen at init (captured+rooted); FIXED in source via late-binding-by-name in js_config_call_func - pending build/depl…
metadata:
  type: reference
---

Affects EVERY SD agent. Any JS function registered via `config.add_funcs(agent, funcs, driver)` — everything callable through `sdconfig <agent> <func>` or an MQTT command — was historically bound to the JS function **object** at registration and could not be updated by a hot-reload; only a full restart picked up source changes.

Original mechanism (`lib/sd/config.c`): `_js_config_get_func` captured the func as a raw `jsval` (config.c:3498), stored it in `_js_config_get_func_ctx` (`newctx.func = func`, 2002), and `JS_EngineAddRoot(cx, name, &ctx->func)` (3520) pinned it so GC wouldn't prune it (a C-held jsval is invisible to the collector → without the root it got reclaimed and the next call crashed on a dangling object). Dispatch (`js_config_callfunc`, 2216-2223) returns the first name match; `_jscallfunc`→`js_config_call_func` called the pinned `ctx->func`.

**FIX (2026-06-16, in source, NOT yet built/deployed):** `js_config_call_func` (config.c:3442) now does **late binding** — resolves the function fresh from the global object at call time via `JS_GetProperty(global, ctx->name, &fval)` + `VALUE_IS_FUNCTION` check, and calls that. Top-level `function name(){}` is a permanent global property, so it stays GC-reachable without a C-side root. The old `ctx->func` call sites are commented out (kept); registration capture + `JS_EngineAddRoot` left intact as the fallback's safety net. Fallback to `ctx->func` is NOT silent — `log_warning` fires every call when the name can't be resolved (restart-required notice). Compiles clean with `-DJS -DXP_UNIX`.

Once `lib/sd` is rebuilt + agents reinstalled + restarted (one last restart), edits to sdconfig/MQTT-callable functions hot-reload like inline logic. Until then the old freeze-at-init behavior is what's running live. See [[ac-deploys-from-opt-sd]].</body>
<parameter name="tags">["config", "spidermonkey", "hot-reload", "add_funcs", "late-binding", "fixed", "system-wide"]
