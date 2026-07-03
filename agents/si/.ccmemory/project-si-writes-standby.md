---
name: project-si-writes-standby
description: Some SI params (e.g. GdRvPwr) only writable with cluster in standby — CMD_SET_DATA times out silently (no NAK) if running; GdVtgMin was writable live
metadata:
  type: project
tags: [si, smanet, standby, writes]
---

The SMA SI cluster silently refuses writes for certain safety-relevant parameters while operating: CMD_SET_DATA gets NO response at all (r=2 timeout after retries at smanet_command level), not a NAK. The cluster must be in standby — which cuts house power — so such changes need a maintenance window.

Observed: `GdRvPwr` refuses while running (July 2026, tried 500 and 200). `GdVtgMin` accepted a write while running. There is no protocol-level error code distinguishing the cases — repeated cmd 0c timeouts on an otherwise-healthy session (visible with `siutil -d 5 <param> <value>`) is the signature.

**How to apply:** If a siutil set silently doesn't take or `-f` reports "error setting value" on a specific param while others work, suspect standby-lock. Schedule, don't retry. Steve's intended settings live in `/root/si_settings.dat` on solardirector; `siutil -v -f` diffs, `siutil -f` applies. Related: [[project-si-grid-trips]], [[user-steve-smanet]].
