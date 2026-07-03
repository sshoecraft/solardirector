---
name: feedback-dyngrid-meaning
description: dyngrid (agents/si/grid.js) throttles battery CHARGE amps to hold grid draw at grid_max_power — it does NOT discharge battery to offset loads
metadata:
  type: feedback
tags: [si, dyngrid, feedback]
---

`dyngrid` (agents/si/grid.js) dynamically adjusts BATTERY CHARGE AMPS to keep grid power at a constant level (`si.grid_max_power`). It works by giving up charging headroom when import would exceed the cap.

**Why:** Steve corrected me when I implied it could cap load-driven import. It can't — once charge_amps hits min_charge_amps there's no more room; loads above (grid_max_power + PV) still pull through the meter.

**How to apply:** Never propose dyngrid/grid_max_power as a load-side import cap. Load-driven import reduction needs feed-side discharge (feed.js) or load shedding. Related: [[project-si-site]].
