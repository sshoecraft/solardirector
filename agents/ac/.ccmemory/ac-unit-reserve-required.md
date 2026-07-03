---
name: ac-unit-reserve-required
description: AC units need reserve set (ac1=6500W); reserve=0 orphaned PA reservations → revoke storm. Config fixed + unit_revoke now acks PA on reserve=0
metadata:
  type: reference
---

**Symptom (hit 2026-07-02):** house not cooling, ah1 fan not running, PA spamming `revoke request received for unit ac1 (immediate: no)` every ~15s (PA interval) with `amount: 0` — even after ac1 was disabled. PA showed `reserved=0`; ac1 kept start↔revoke cycling with charge.js.

**Root cause:** `ac1` had **no `reserve` in `/opt/sd/etc/ac_units.json`** (defaults 0). With `reserve=0` the unit skips BOTH `UNIT_STATE_RESERVE` and `UNIT_STATE_RELEASE` (run.js: `if (unit.reserve) …RESERVE… else …START_PUMP`). So it never reserves and, crucially, **never releases**. A stale/orphaned ac1 entry in PA's `pa.reservations` (from a prior life when ac1 did reserve) then never clears:
- PA deficit-revoke loop (`agents/pa/read.js` ~225-231) re-fires a revoke for EVERY entry each cycle **without removing it** — unlike the battery-hard-limit path (~188-195) which `splice()`s. It relies on the client to release in response.
- reserve-0 ac1 has nothing to release → never acks → PA's orphan re-revokes forever.

**Fixes:**
1. **Config:** set `reserve=6500` on ac1 (compressor draw) + persist to `ac_units.json`. Closes the reserve→release loop. Verified working 2026-07-02.
2. **Code (DONE 2026-07-02):** `agents/ac/unit.js` `unit_revoke` now `pa_client_release(ac,"unit",name,amount)` when `!unit.reserve` — acks PA so a stale entry clears even for a reserve-0 unit. PA's `pa_release` (agents/pa/funcs.js:175) matches **by id, not amount** (splices the entry), so the amount passed is irrelevant. reserve>0 units still ack via the RELEASE state (guarded to avoid a premature double-release). Tests: test_unit.js `test_revoke_reserve0_acks_pa_release` / `test_revoke_with_reserve_does_not_release_in_revoke` (harness now spies pa_client_reserve/release/repri). Full suite 154 green. **Deploy: copy unit.js → /opt/sd/lib/agents/ac/, hot-reload** (unit_revoke is late-bound by name via pa_client_revoke `global[func_name]`, so hot-reload picks it up; restart if unsure — [[ac-deploys-from-opt-sd]]).

**NOT caused by the direct-mode both-pumps rework** ([[ac-direct-bothpumps-rework]]). PA (grid+battery from SI `SolarD/Agents/si/Data`, solar from pvc `SolarD/Agents/pvc/Data`) was healthy — `avail` just rides near 0 as solar drops late afternoon → intermittent deficit. PA runs on **solardirector**; its `/opt/sd/log/pa.log` there is live (the `ac`-host copy is stale, and the `ac`-host local mosquitto broker does NOT see SI/pvc/pa topics — inspect on PA's host).

**Still open:** (1) PA-side: deficit-revoke should drop/timeout a reservation it keeps revoking (belt to the AC ack). (2) `charge_get_pri()` clamps to 1 (== comfort fan priority) whenever water_temp > cool_high_temp — charging should stay below the comfort fan.</body>
