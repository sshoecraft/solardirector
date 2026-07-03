---
name: home-warden-monitor-design
description: Design direction for a persistent AI that monitors/heals house control systems, modeled on /src/aitrader; three-layer docs + local-disk invariant
metadata:
  type: project
---

Design direction (discussed 2026-07-02, NOT yet built) for an AI that watches all house control systems (AC/thermal storage, power/PA, PV, water heater, NFS, etc.), self-scheduling its wake-ups, assessing health, healing within bounds, and emailing a daily incident report.

**Model it on `/src/aitrader`** (the persistent autonomous trader Steve built) — same machine, different mandate:
- Runtime = **ccloop** (never-stop; on context-fill relays to a FRESH session, not lossy compaction). Agent self-paces via a **scheduler MCP**'s blocking `wait_*` (sleep for hours if quiet) — replaces a fixed hourly cron.
- Hard **infrastructure vs cognition boundary**: dumb opinion-free tools (ssh-run, read remote log, `sdconfig get/set`, mqtt query, influx query, mount-check, ping) vs. all "is this healthy / what's wrong / heal it?" judgment done by reasoning. Never code a decision.
- **Journal** (sqlite, like aitrader's journal_db) = system of record: incident-shaped entries (subsystem, severity, observed, diagnosis, action, status open/resolved/escalated), read at top of each cycle (reconcile-on-wake), and the source for the UI + the daily email. Daily report = agent composes the narrative (cognition), a dumb mailer sends it (infra). A fixed-cadence "health snapshot" recorder (like aitrader's snapshot-recorder cron) keeps a baseline while the agent sleeps.

**LOCAL-DISK invariant (load-bearing):** the monitor's knowledge must live on storage that fails INDEPENDENTLY of what it watches — a monitor can't keep the NFS-runbook on NFS. Runtime reads only a local clone; source/NFS is build-time only. Observation (ssh/mqtt/influx) is a separate channel that's allowed to fail (a failure IS a signal). Runs on a box that isn't itself the thing most likely to die (Steve leaned: co-locate on **clyde** where aitrader runs, accept clyde-self-monitoring as a known blind spot).

**Three-layer docs (each a distinct job — same split we then used for the AC work):**
1. **Per-module markdown runbooks** = authoritative "how it works + how to check + how to heal" (e.g. `direct_mode.md`). Co-located with code, deployed to the monitor's local disk.
2. **ccmemory** = decision/lesson logbook (the "why", gotchas).
3. **project-awareness structural map** = the hub/index tying runbooks together — a central manifest (subsystem → absolute path to its runbook + how to reach it) that the generic cron prompt points at, so adding a subsystem = one manifest line.

Since the whole SD tree is one repo, the hub can be a single top-level doc (e.g. `/src/sd/docs/home-ops.md`). Keeping docs current is discipline (update the runbook in the same commit as the code), enforced by CLAUDE.md's "maintain docs/<module>.md" rule.</body>
