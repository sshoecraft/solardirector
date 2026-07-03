---
name: ac-direct-init-enabled-bug
description: Fixed: direct.js _direct_init hardcoded dg.enabled=true, re-enabling direct mode on every load despite saved enabled:false
metadata:
  type: project
---

Bug (fixed 2026-06-16): `agents/ac/direct.js` `_direct_init()` had `dg.enabled = true;` hardcoded in its state-init block. Every load — startup OR hot-reload — clobbered the persisted/default value, so a `direct_disable` (which correctly writes `enabled:false` to `/opt/sd/etc/ac_directs.json`) never survived a reload. pump/fan/unit inits never touch `.enabled`; direct was the odd one out.

Fix: deleted the line so `jconfig_add_props` (default "true" when key absent) / the saved `false` stands. Regression tests `test_direct_init_preserves_disabled` / `_preserves_enabled` added to `test/test_direct.js` (40/40 pass). Deployed to `/opt/sd/lib/agents/ac/direct.js`, hot-reloaded live.

**Operational note:** `sdconfig ac direct_disable dg1` on a group the bug had force-enabled runs the full `direct_off` teardown — stops the unit, drives valves to storage, hands the unit back to charge.js. That physically **cycles the HVAC unit** (compressor drops, charge.js restarts it). The `sdconfig` call may time out ("agent not responding") waiting for the teardown but still succeeds. dg1 valves were configured (pins 25/24/23/22) but not physically wired yet as of this date.

See [[ac-deploys-from-opt-sd]], [[ac-direct-valve-rework]].</body>
<parameter name="tags">["ac", "direct", "bug", "enabled", "persistence", "fixed"]
