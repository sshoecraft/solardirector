---
name: reference_sdconfig_no_jsexec
description: sdconfig dispatches to registered agent functions; there is no jsexec and remote JS exec is intentionally disabled
metadata: 
  node_type: memory
  type: reference
  originSessionId: a419db0f-05be-4efb-b0d1-dedb0e0e4e9b
---

`sdconfig <agent> <verb> [args...]` (utils/sdconfig/sdconfig.js) invokes a **registered** agent function or the special `get`/`set`. It cannot run arbitrary JavaScript — there is **no `jsexec` verb**.

The remote `exec` config function (`cf_agent_exec`) was deliberately commented out of `agent_funcs[]` in `lib/sd/agent.c` (~line 579) as a "horrible security hole"; the handler still exists (~line 405) but is unreachable. The `-e` flag is also commented out in `lib/sd/client.c` (~line 197).

Base remotely-callable funcs: `ping`, `exit`, `repub`, `get`, `set`, `clear`, `log_open`, plus each agent's own registered functions.

To run ad-hoc JS you need shell access on the host: the `sdjs` REPL, or the agent binary's **local-only** `-e '<js>'` / `-X <script.js>` flags (still active in agent.c). For `add_*`/`set_*` functions the `opts` arg is colon-delimited `key=value` pairs (jconfig_set_opts), NOT JSON — e.g. `sdconfig ac add_unit ac1 "pump=ac1:coolpin=22"`.
