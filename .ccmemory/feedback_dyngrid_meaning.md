---
name: feedback-dyngrid-meaning
description: dyngrid in agents/si throttles charge amps to cap grid import — it does NOT add battery discharge to support loads
metadata: 
  node_type: memory
  type: feedback
  originSessionId: fe056e56-968e-4b78-bf56-39f493a051c3
---

`dyngrid` (in `agents/si/grid.js`) dynamically adjusts the BATTERY CHARGE AMPS to hold grid power at a constant amount (`si.grid_max_power`). It works by giving up charging headroom when import would otherwise exceed the cap.

**Why:** Steve corrected an earlier mischaracterization — I called it a "grid_max_power" knob and implied it could cap load-driven import. It can't. Once `charge_amps` hits `min_charge_amps` there's no more room to absorb additional draw; loads above (grid_max_power + PV) still pull through.

**How to apply:** When discussing grid-import limits or solutions for high-load events on the SI cluster, do NOT propose `dyngrid` / `grid_max_power` as a load-side cap — it only modulates charging. Reducing load-driven import requires a different mechanism (feed-side discharge, load shedding, or SI-internal limits). Related: [[project_si_cluster]].
