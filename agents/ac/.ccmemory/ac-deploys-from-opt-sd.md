---
name: ac-deploys-from-opt-sd
description: ac agent loads JS from /opt/sd/lib/agents/ac/ (root-owned, sudo to deploy); hot-reload updates inline logic only, NOT sdconfig-callable funcs
metadata:
  type: reference
---

The running `ac` agent loads its JS from **`/opt/sd/lib/agents/ac/`** (NOT `/opt/sd/agents/ac` — that path doesn't exist), separate from the NFS source tree at `/System/Volumes/Data/src/sd/agents/ac/`.

Deploying a JS edit:
- Files are **root:root, mode 755** — `scp`/`cat >` as `steve` fails with permission denied. Stage to `/tmp` then `sudo cp` into place (steve has passwordless sudo on the `ac` host). Back up first: `sudo cp -p direct.js direct.js.backup`.
- The agent watches its JS files and emits `jsengine.c JS_EngineCheckLoaded: Script ... was modified, re-executing...` on change. **This hot-reload only updates inline/top-level logic and `jconfig_load` callbacks (e.g. `_direct_init`, `_pump_init`).** It does NOT update functions registered via `config.add_funcs` (everything callable via `sdconfig`/MQTT) — those are frozen at init and need a full **restart**. See [[sd-config-funcs-frozen-at-init]]. When unsure whether an edit is live, restart.
- Config files (e.g. `/opt/sd/etc/ac_directs.json`) ARE writable by the agent via `sdconfig ... set`/`_set`/`_disable`.

See [[ac-direct-init-enabled-bug]].</body>
<parameter name="tags">["ac", "deploy", "opt-sd", "hot-reload", "sudo", "restart"]
