---
name: ac-direct-bothpumps-rework
description: AC direct both-pumps rework DEPLOYED+verified live 2026-07-02; fan-pump ref via dg.direct_fan_pump_ref; pub.js valve1/2=Direct/Storage inline from dg…
metadata:
  type: project
---

Rework of `agents/ac/direct.js` (2026-07-02, plan `idempotent-sniffing-cook`). Topology in [[ac-direct-topology]]. Surgical change to the existing two-stage closed-loop machinery — **kept** RUNNING_OPEN / ACTIVATE_PIN2 / primer / WAIT_WATER_TEMP / CONFIRM_FLOW / directional two-relay valves; the real change is running **both pumps** instead of bypassing the AH pump.

State machine: `STOP_FAN_PUMP`/`WAIT_FAN_PUMP` → **`START_FAN_PUMP`/`WAIT_FAN_PUMP`** (start the AH pump, wait RUNNING; tolerate transitional, ERROR/RESERVE-timeout → fail), repositioned **after CONFIRM_FLOW, before ACTIVATE_VALVE** (both pumps RUNNING before any valve flips). `direct_on` → straight to PREPARE_UNIT.

**Fan-pump refcount ownership.** sc.1 FRESH START (fan dormant, run.js:93 skips it, pump 0 refs) vs sc.2 ALREADY RUNNING (fan owns 1 ref, blower on). `dg.direct_fan_pump_ref` tracks whether direct owns the ref. START_FAN_PUMP keys on `fan.state==FAN_STATE_RUNNING`: RUNNING → fan owns, skip; else pump_start + own. Blower `fan_start` at RUNNING guarded on `fan.state!=RUNNING` (`direct_start_blower`), then ref→false. `direct_release_fan_pump(dg)` (every error path at/after START_FAN_PUMP + direct_off): clears pump.direct unconditionally, force-stops only if direct owns. Deleted direct_off's old `pump_start(fan_pump)`. `pump.direct` set in START_FAN_PUMP for BOTH scenarios (deferred from direct_on). Also: clear `unit.in_direct*` BEFORE `unit_stop`; run.js FAN_STATE_RUNNING monitors `fan.pump`; direct RUNNING monitor watches BOTH pumps; cycle.js only `pump_start`s STOPPED pumps; `direct_off` `in_teardown` re-entry guard. Fixed latent OLD bug: sc.2 double-reffed the fan via unconditional `fan_start`.

**Valve-position reporting (pub.js) — INLINE, no helper funcs.** Dropped the 8 relay-pin fields (they read 0 at rest for a holding valve). pub.js reports `valve1`/`valve2` = "Direct"/"Storage"/"None", computed **inline from `dg.active` (valve1) and `dg.state` (valve2 = Direct only START_PRIMER..RUNNING)**. NO helper functions — LESSON: a NEW function in direct.js does NOT hot-reload (frozen until a full restart, see [[ac-deploys-from-opt-sd]] / [[sd-config-funcs-frozen-at-init]]); pub.js top-level/inline logic DOES hot-reload. First attempt added `direct_valve{1,2}_direct` helpers in direct.js → pub.js threw `ReferenceError: direct_valve1_direct is not defined` because only pub.js was redeployed. Inlining keeps it pub.js-only. Distinction that matters: **`dg.active`** = direct running (valves flipped) = what the valves track; **`dg.enabled`** = group merely allowed (can be enabled but idle in storage).

**Status:** both-pumps rework **DEPLOYED to /opt/sd (ac host) 2026-07-02 14:24 + verified live** (dg1 full cycle: valve1 14:27 → RUNNING_OPEN ~3.5min for the 2°F change → valve2 14:30 → loop closed → running, sc.2 fast path). `make test` green: test_direct 48, full suite 152. **pub.js valve-reporting is pub.js-only: deploy pub.js, hot-reload, NO restart.**

**Still open:** `RUNNING_OPEN` has NO timeout — if AH temp never changes by `temp_threshold`, hangs with valve1 open / valve2 never firing / no air. Minor: `dg_valve_power_check` logs `group=%s` with `dg.fan` not the group name.</body>
