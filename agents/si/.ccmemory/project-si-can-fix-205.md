---
name: project-si-can-fix-205
description: SI agent v2.05 can.c fix implemented+built: ac2_power now uses slave3's real 0x1E3 grid power; per-slave frames cached (slots 16-39); NOT yet deploye…
metadata:
  type: project
tags: [si, can, fix, deployment]
---

Implemented July 2026 (per [[project-si-can-protocol-map]]): `agents/si/can.c` + `si.h` v2.05:
- `si_slave_ids[]` table (24 per-slave ids) + `si_can_id2slot()` sparse lookup; `frames[SI_NFRAMES=40]` (slots 0-15 = 0x300-0x30F unchanged, 16-39 = per-slave)
- Kernel filter: explicit 40-id `CAN_CONFIG_SET_FILTER` list replaces `CAN_CONFIG_SET_RANGE 0x300-0x30F`
- 2Phase4 `ac2_power` = sum of 0x300 slots + slave3 ExtPwrAt from 0x1E3 lo dword (W, signed); falls back to old `t+t/3` estimate only if 0x1E3 stale >5s. Same fix in the CLUSTER_CONFIG_UNKNOWN 2phase4-assumed branch. Remote (rdev) path also polls 0x1E3.
- Docs: `agents/si/docs/can.md`. Version: si 2.04→2.05, siutil 1.0→1.0.1.

Verified via siutil rebuild (compiles agent's can.c): thread caches per-slave frames correctly (`-d 8` showed 0x181→slot 16 ... 0x223→slot 39), aggregates unaffected. The ac2_power calc path itself couldn't fire in siutil (bare info mode has input.source=0; CALCULATED comes from agent config /opt/sd/etc/si.json `input_current_source: "calculated"`), but the formula was hand-verified against live data (t=+210W + slv3 1311W = 1521W vs SMANet truth ~1.5kW; old fudge gave 280W).

**Deployment state: built at ~/.build/sd/agents/si/si on solardirector but NOT installed — production /opt/sd/bin/si still runs 2.04.** Needs `make install` + service restart at a moment Steve chooses (agent restart ≠ cluster downtime, but feed/charge JS state resets — his call).

**LESSON: never run the si agent binary with a guessed flag** — unknown args (e.g. `--version`) start a FULL AGENT instance. I did this and had to kill the stray (production was unaffected). Use siutil for read-only checks.

After deploy, watch: InfluxDB `inverter.input_power` should tie out with `load_power` at night (no PV), and per-slave frames enable future per-inverter publishing (pub.js work, TBD).
