---
name: project-si-deploy-pending
description: SUNDAY CHECKLIST: deploy si v2.05 + set cluster_config 4 + write GdRvPwr 500 in standby; rdevserver new binary installed (service disabled); no si re…
metadata:
  type: project
tags: [si, deploy, sunday, checklist]
---

**Steve cannot restart the production si agent or take the cluster down before Sunday 2026-07-05 (power-outage risk). Do not suggest or perform either before then.**

Sunday maintenance checklist (order matters):
1. Build + install si agent v2.05 on solardirector: `cd /src/sd/agents/si && make && sudo make install`
2. `sudo systemctl restart si`
3. `sdconfig si set cluster_config 4` — fixes the stale 2Phase2 value ([[project-si-cluster-config-wrong]]); hot-reloadable, can also be done before the restart
4. If taking the cluster to standby anyway (house power off): write the pending reverse-power setting — `siutil GdRvPwr 500` (or `siutil -f /root/si_settings.dat`), standby-only per [[project-si-writes-standby]]. This stops the cloud-clear reverse-power trips ([[project-si-grid-trips]]).
5. Verify after: (a) log shows `slv3 grid` values with -d 2 if debugging; (b) InfluxDB `inverter.input_power` ties out with `load_power` at night (no PV); (c) grid Off events become rare.

State as of 2026-07-02 evening:
- v2.05 built at `~/.build/sd/agents/si/si` on solardirector AND tested read-only from the Mac via rdev ([[project-rdev-remote-testing]]); production still 2.04.
- rdevserver: NEW binary installed at /opt/sd/bin/rdevserver (old at rdevserver.backup), service left disabled+stopped; conf has widened filter + buffer=no (backup at rdevserver.conf.backup).
- GdVtgMin 112→108 already applied live (under-voltage trips should already be reduced).
- lib changes also in v2.05 scope: lib/smanet (comment only), lib/sd/can.c bitmap alloc fix — sdjs/other agents pick these up whenever they're next rebuilt; nothing depends on it urgently.
