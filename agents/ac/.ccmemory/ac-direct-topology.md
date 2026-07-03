---
name: ac-direct-topology
description: AC direct mode = closed loop between AC BPHX and air handler, 4 three-way valves on 2 relay sets, BOTH pumps run in series
metadata:
  type: reference
---

**Physical plumbing of AC "direct mode" (the source of truth — do not re-derive from code).**

Hardware:
- AC unit → **brazed-plate heat exchanger (BPHX)**. A **½ HP AC pump** pushes water through the BPHX.
- Air handler (AH) inside the house has a **water-to-air heat exchanger**. A **¾ HP AH pump** pushes water through it.
- **Four 3-way valves**, wired as **two relay sets** (each set = the directional two-relay power+select model, [[ac-direct-valve-rework]]):
  - **valve1 set (SUPPLY pair)** = BPHX-output valve **+** AH-pump-input valve. Firing valve1 flips **both** together: BPHX output → AH pump input.
  - **valve2 set (RETURN pair)** = AH-output valve **+** AC-pump-input valve. Firing valve2 flips **both** together: AH output → AC pump input (this **closes the loop**).

Modes:
- **Storage (valves at rest):** two independent tank loops.
  - AC: `tank → AC-pump-in → AC pump → BPHX → BPHX-out → tank` (charges the tank).
  - AH: `tank → AH-pump-in → AH pump → AH coil → AH-out → tank` (conditions the house from the tank).
- **Direct (both sets flipped, tank fully bypassed):** one **closed loop** —
  `AC pump → BPHX → AH pump → AH coil → AC pump`, with **both pumps in series** around it.

Operating concept: by day the **solar** system runs the AC unit to heat/cool (charge) the **storage tank**; after dark the **AH pulls stored thermal energy from the tank** to condition the house; when the **tank's thermal energy is exhausted**, the valves switch to the **direct closed loop** between AC and AH (real-time conditioning, tank bypassed).

**KEY behavior change (2026-07):** direct mode must run **BOTH pumps at the same time**. The old code stopped the AH pump and pushed the whole loop with the ½ HP AC pump alone — the new hardware chokes if the AH pump is idle (idle AH pump chokes the AC discharge → BPHX under-flow → vapor-temp freeze trip). The two-stage seal (fire valve1/supply → confirm → fire valve2/return → primer air-bleed) and the freeze-safe ordering stay; only the AH-pump handling inverts (start it, keep it running) — see [[ac-direct-bothpumps-rework]].

Closed loop has no open reservoir → thermal expansion/air handled by the high-point air-relief valve + primer bleed. Confirm expansion provision before first wet run.</body>
