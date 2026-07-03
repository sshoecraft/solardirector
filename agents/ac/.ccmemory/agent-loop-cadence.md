---
name: agent-loop-cadence
description: SD agent loop: run.js every 1s, read.js/write.js + direct_main every interval secs; timed work belongs in run.js
metadata:
  type: reference
tags: [agent, loop, cadence, run.js, read.js, direct]
---

The SD agent main loop (`agent_run` in lib/sd/agent.c) iterates once per second (`sleep(1)` at the bottom of the while loop). Each iteration:

- **run.js (run_main) runs every iteration = ~1 second** (called unconditionally, agent.c ~1054).
- **read.js (read_main) and write.js run only when `time() - last_read >= ap->interval`** — i.e. every `interval` seconds (agent.c ~1014). AC agent default interval = 10s (agents/ac/main.c; agent.c's own default is 30).
- Loaded JS scripts are hot-reload checked every ~10 iterations.

Consequence for agents/ac: as of 2026-06-10 `direct_main()` is called from **run.js** (~1s), moved out of read.js so the direct-mode state machine runs at the same cadence as the fan/pump/unit machines (which have always been inline in run.js). Anything timing-sensitive (e.g. cutting the timed valve power pulse in `dg_valve_power_check` at ~10s ±1s) must run from run.js, not read.js. Sub-interval `time()` diffs in run.js (`diff % 5 == 0`, "diff > 3 loops") rely on the 1s cadence.
