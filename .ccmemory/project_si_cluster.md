---
name: project-si-cluster
description: Sunny Island cluster topology — 4x SI 6048-US in 2-phase-4 config on solardirector host
metadata: 
  node_type: memory
  type: project
  originSessionId: fe056e56-968e-4b78-bf56-39f493a051c3
---

Steve's SI deployment: 4x Sunny Island 6048-US inverters arranged in a 2-phase-4 cluster.

The SI agent runs on host `solardirector` (ssh solardirector).
Live log: `/opt/sd/log/si.log` on that host (NOT journalctl, same pattern as [[reference_ac_log]]).

**Why:** This is the production cluster the `agents/si/` code in this repo targets. When debugging or reasoning about behavior, assume 4-inverter 2-phase config, 48V nominal pack.

**How to apply:** When asked about "the SI" or "the inverters", default to checking the live log on solardirector rather than guessing from source. Battery voltages in the 55–56V range and SoC in the 80s% are normal operating values.
