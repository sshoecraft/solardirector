---
name: never_restart_services
description: NEVER restart any SD agent service - always ask the user to do it. Use sdconfig for error clearing.
type: feedback
---

**NEVER restart any SD agent service yourself.** If a restart is needed (e.g. new config properties that require init), ask the user to do it. Do not run `systemctl restart` on any agent service.

To clear error states on the AC agent, use `sdconfig ac clear_error all` followed by `sdconfig ac enable_unit ac1` — no restart needed.

**Why:** SD agents control real hardware (compressors, pumps, valves, inverters, BMS). A restart stops everything and forces a full re-initialization, disrupting the physical system. New config properties added via `config.add_props` only load at init time — hot-reload won't pick them up — but the decision to restart must be the user's since it affects real equipment.

**How to apply:**
- Error state → use sdconfig to clear errors and re-enable components
- New config property added → tell the user a restart is needed for the property to load, let them decide when
- Agent crashed/unresponsive → ask the user to restart
