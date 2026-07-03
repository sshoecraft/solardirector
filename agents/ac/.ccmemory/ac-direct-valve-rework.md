---
name: ac-direct-valve-rework
description: AC direct-mode valves rewired to two-relay (power+select) 24V model; new config valve{1,2}_{power,select}+valve_time; deploy pending 2026-06-10
metadata:
  type: project
tags: [ac, direct, valves, relay, config, deploy]
---

The AC direct-mode valves (dg1) are being replaced with bidirectional 3-wire actuators on a 24V bus. Each valve set uses **two relays**: a power relay (relay1, COMM=+24V, NO→select-relay COMM) and a select/direction relay (relay2, NC=storage, NO=direct). Move = set select relay, energize power relay, hold `valve_time` (default 10s), then cut power. Rest = both relays LOW (storage); valve holds position unpowered.

Replaced the old single-pin model. New `direct_props` fields (agents/ac/direct.js): `valve1_power`, `valve1_select`, `valve2_power`, `valve2_select` (stage 2 optional: power=-1), `valve_time`. Runtime: `valve1_power_time`/`valve2_power_time`. New helpers `dg_valve_move` / `dg_valve_power_check`; valve on/off now drive direction+power instead of a steady level. Also moved `direct_main()` from read.js to run.js (1s cadence) so the timed power-cut is precise — see [[agent-loop-cadence]].

Status as of 2026-06-10: code + tests + direct_mode.md done in the source tree (38/38 tests pass), NOT deployed. To deploy: copy direct.js/run.js/read.js/pub.js/start.js → /opt/sd/agents/ac/ and lib/sd/jconfig.js → /opt/sd/lib/sd/ (see [[ac-deploys-from-opt-sd]]), restart ac, then `sdconfig ac direct_set dg1 "valve1_power=..:valve1_select=..:valve2_power=..:valve2_select=..:valve_time=10"` and enable. dg1 currently enabled=false.
