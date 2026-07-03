---
name: project-si-cluster-config-wrong
description: si.json cluster_config: 3 (2Phase2) is STALE — live ClstCfg reads 2Phase4; sync.js auto-detects only when unset so it froze wrong; set to 4 (Sunday o…
metadata:
  type: project
tags: [si, config, bug, production, cluster-config]
---

`cluster_config` (agents/si) tells can.c how to interpret the 3-slot CAN aggregate frames — used ONLY in the ac1_power/ac2_power calcs and the v2.05 voltage refinements. Enum (si.h:177): 0=Unknown, 1=1Phase1, 2=1Phase2, 3=2Phase2, 4=2Phase4, 5=3Phase.

It is AUTO-DETECTED: `sync.js:46-63` reads the SI's own SMANet `ClstCfg` parameter on first sync **if cluster_config is unset**, maps to the enum, and config.save()s it into si.json permanently. It never re-syncs after that.

**Production bug found 2026-07-02:** si.json has `cluster_config: 3` (2Phase2) but the live SI reports `ClstCfg = 2Phase4` (verified via siutil). The value froze stale — likely synced during the 2025 standalone-unit experiment. Consequence on the 2.04 agent: power calcs take the default branch = straight 3-slot sum ([master][slv1][slv2]), silently missing slave3 with no estimate. This is the PRIMARY cause of the historical input_power vs load_power mismatch.

**Fix:** `sdconfig si set cluster_config 4` — hot-reloadable, no trigger function, no restart needed. On the 2.04 binary this activates the +t/3 estimate (better than missing slv3); on v2.05 it enables the real slave3 data (0x1E3/0x203/0x183). Steve chose to defer changes; do it with the Sunday deploy ([[project-si-deploy-pending]]) unless he says otherwise. Watch out: if cluster config is ever re-detected wrong again, clearing to 0 makes sync.js re-derive from ClstCfg.

All five July-2 fixes recap: (1) ac1_power slave3 via 0x203, (2) smanet fallback TotInvPwrAt/TotExtPwrAt mapping ×(−1000) + GdOn both-directions, (3) phase-pair voltages via 0x183, (4) rdevserver new binary installed (service disabled), (5) influx history not fixable. Related: [[project-si-can-fix-205]], [[project-si-can-protocol-map]].
