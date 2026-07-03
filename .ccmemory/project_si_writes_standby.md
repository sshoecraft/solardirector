---
name: project-si-writes-standby
description: Some SI parameters (e.g. GdRvPwr) require the cluster to be in standby to accept writes — siutil shows protocol timeout if attempted while running
metadata: 
  node_type: memory
  type: project
  originSessionId: fe056e56-968e-4b78-bf56-39f493a051c3
---

The SMA SI cluster silently refuses writes (CMD_SET_DATA times out, no NAK) for certain safety-relevant parameters while the cluster is operating. To change them, the cluster must be put in standby mode — which cuts house power.

Observed for: `GdRvPwr` (May 2026 investigation). Other safety-critical grid-code-equivalent params likely behave the same. `GdVtgMin` was writable while running — so not ALL grid protection params lock.

**Why:** Discovered during a live cluster tune in May 2026; Steve confirmed it as expected SMA behavior. Affects how settings in `/root/si_settings.dat` get applied — `siutil -f` will print "error setting value" for any standby-only params if you try to push them while cluster is running.

**How to apply:** When proposing changes to SI parameters, don't assume every write will land. If `siutil -d 5 <name> <value>` shows repeated `cmd: 0c` timeouts, the param needs a maintenance window with cluster in standby. Schedule rather than retry. Related: [[project_si_cluster]], [[user_steve_smanet]].
