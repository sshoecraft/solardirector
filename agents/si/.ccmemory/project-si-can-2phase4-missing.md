---
name: project-si-can-2phase4-missing
description: 2Phase4 is the ONE production cluster (master+3 slaves) powering the house — no downtime allowed; CAN aggregate incomplete; +t/3 fudge; Oct 2025 docs…
metadata:
  type: project
tags: [si, can, 2phase4, cluster]
---

Steve's 4x SI 6048-US in `ClstCfg = 2Phase4` is **ONE cluster**: one master + 3 slaves, split-phase 120/240, 2 units per phase. It is the ONLY cluster — it powers the house in production, and **taking it down is not an option** (fixes must be testable without cluster downtime; passive candump captures are fine).

**CAUTION about the Oct 2025 docs** (`~/CAN_CLUSTER_FINDINGS.md`, `~/CAN_CLUSTER_PROTOCOL.md` on solardirector): they describe a "2x2 multicluster" (Cluster 1/Cluster 2), "Inverter #2 DOWN", and "Cluster 2 uses different encoding". That analysis coincided with a failed standalone-unit test experiment (Steve pulled a unit to test standalone; didn't work out). The multicluster framing is WRONG, the "down" inverter was likely the pulled test unit, and the "different encoding" conclusion is suspect — probably wrong ID→inverter/field assignment on an abnormal system state. Raw ID maps/captures still useful; interpretation is not.

Real problem (confirmed July 2026): CAN aggregate 0x300-0x30A doesn't carry complete per-phase grid power for all 4 units. `agents/si/can.c` `CLUSTER_CONFIG_2PHASE4` uses a `t + t/3` fudge (~lines 316-328); misfires on imbalance (e.g. Active Grid 700/2700/800 W). So `input_power` (ac2_power, derived) never ties out with `load_power` (TotLodPwr from 0x308, authoritative) — visible even at night.

Constraints for any fix (code scan July 2026):
- `si.h:217`: `struct can_frame frames[16]` direct-mapped `fidx = can_id - 0x300`; can.c:441 range check protects against negative-index memcpy corruption — cache only fits currently-unfiltered IDs
- Kernel filter: CAN_CONFIG_SET_RANGE (lib/sd/can.c:178) builds one can_filter per ID; naive 0x180-0x30F = 399 entries (cap 512)
- Only 16 sparse per-inverter IDs matter: 0x182,183,192,193 / 0x1B2,1B3,1E2,1E3 / 0x1C2,1C3,1F2,1F3 / 0x202,203,222,223 (block→which-inverter mapping unverified)

Fix shape: sparse id→slot lookup (switch), frames[32], explicit 32-ID CAN_CONFIG_SET_FILTER list (also rejects sync chatter). Remote path needs no change. Per-inverter decode still open — re-derive from fresh capture with all 4 units up (passive, safe); alternative is SMANet per-inverter polling via siutil.

**How to apply:** load_power ≠ input_power on the SI → this is the known cause. CAN experimentation must be passive (candump) — no cluster restarts. Related: [[reference-si-can-docs]].
