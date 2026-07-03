---
name: project-si-can-2phase4-missing
description: "2Phase4 is ONE production cluster (master+3 slaves) powering the house; CAN aggregate 0x300-0x30A incomplete; can.c +t/3 fudge makes input_power unreliable; per-inverter IDs 0x1xx-0x2xx undecoded"
metadata: 
  node_type: memory
  type: project
  originSessionId: fe056e56-968e-4b78-bf56-39f493a051c3
---

Steve's 4x SI 6048-US in `ClstCfg = 2Phase4` (see [[project_si_cluster]], [[project_si_topology]]) is **ONE cluster**: one master + 3 slaves, split-phase 120/240, 2 units per phase. It is the ONLY cluster — it powers the house in production, and **taking it down is not an option** (any fix must be testable without cluster downtime; passive candump captures are fine).

**CAUTION about the Oct 2025 docs** (`~/CAN_CLUSTER_FINDINGS.md`, `~/CAN_CLUSTER_PROTOCOL.md` on solardirector, see [[reference_si_can_docs]]): they frame the system as a "2x2 multicluster" (Cluster 1 / Cluster 2) with "Inverter #2 DOWN" and "Cluster 2 uses different encoding". That analysis was done during/around a failed standalone-unit test experiment (Steve pulled a unit to test standalone in 2025; it didn't work out). The multicluster framing is WRONG, the "down" inverter was likely the pulled test unit, and the "different encoding" conclusion is suspect — likely wrong ID→inverter/field assignment on an abnormal system state. The raw CAN ID map and captures are still useful; the interpretation is not.

The real problem (confirmed July 2026): aggregate CAN IDs 0x300-0x30A don't carry complete per-phase grid power for all 4 units. `agents/si/can.c` handles `CLUSTER_CONFIG_2PHASE4` with a `t + t/3` fudge (~lines 316-328) to synthesize the missing share; it misfires when phases are imbalanced (e.g. Active Grid 700/2700/800 W). Consequence: `input_power` (`ac2_power`, derived) doesn't tie out with `load_power` (`TotLodPwr` from 0x308, authoritative) — visible even at night with no PV.

Architecture constraints for any fix (code scan July 2026):
- `si.h:217`: `struct can_frame frames[16]` direct-mapped by `fidx = can_id - 0x300`; recv-thread check at can.c:441 protects against negative-index memcpy corruption — the cache only has slots for the currently-unfiltered IDs
- Kernel filter: `CAN_CONFIG_SET_RANGE` (lib/sd/can.c:178) builds ONE can_filter entry per ID; naive widening to 0x180-0x30F = 399 kernel filter entries (cap 512)
- Only 16 sparse per-inverter IDs matter: 0x182,183,192,193 / 0x1B2,1B3,1E2,1E3 / 0x1C2,1C3,1F2,1F3 / 0x202,203,222,223 (which block = which of master/slave1/2/3 still unverified)

Proposed fix shape: sparse id→slot lookup (switch) replacing direct-map, frames[32], explicit 32-ID list via CAN_CONFIG_SET_FILTER (also rejects sync chatter better than a range). Remote path (`si_can_get_remote`) needs no change. Per-inverter decode still open — re-derive from a fresh capture with all 4 units up (passive, safe); alternative is SMANet per-inverter polling via siutil.

**Why:** Steve investigated in Oct 2025 and left docs, but corrected their framing in July 2026. Don't re-derive the mismatch, and don't trust the multicluster story.

**How to apply:** When someone reports load_power ≠ input_power on the SI, this is the known cause. Any CAN experimentation must be passive (candump) — no cluster restarts. Related: [[reference_si_can_docs]].
