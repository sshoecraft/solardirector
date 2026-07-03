---
name: project-si-site
description: Site: 4x SI 6048-US 2Phase4 cluster (ONE cluster, powers the house) on solardirector + 3x Sunny Boy AC-coupled (pvcombiner); si.log at /opt/sd/log/si…
metadata:
  type: project
tags: [si, site, topology, ac-coupled]
---

Steve's production site, AC-coupled micro-grid:
- **Battery/grid**: 4x SMA Sunny Island 6048-US in a single `ClstCfg = 2Phase4` cluster (master + 3 slaves, split-phase 120/240, 2 units per phase; phase pairing: master+slave2 on one leg, slave1+slave3 on the other). Powers the house — downtime requires a planned window.
- **PV**: 3x SMA Sunny Boy (`sb1`, `sb2`, `sb3`), AC-coupled onto the bus; totals under `pvinverter` measurement, `"name"='pvcombiner'` in InfluxDB (`power` db on solardirector).
- Host: `solardirector` (ssh); eth0/wired = 192.168.1.168, wlan0 = .164 (mDNS resolves to wifi). SI agent log: `/opt/sd/log/si.log` (NOT journalctl). Agent config: /opt/sd/etc/si.json.
- Because AC-coupled, the SI only sees NET power at its bus: real house load = pvcombiner output + grid import − export. The SI's `load_power` (TotLodPwr) is net and can go negative when PV exceeds load.
- Loads: whole-house including a large AC unit + two air handlers (~8-10 kW on hot days).

**How to apply:** Interpret all SI power figures as net-of-PV. For actual consumption cross-reference pvcombiner. Related: [[project-si-can-protocol-map]], [[feedback-dyngrid-meaning]].
