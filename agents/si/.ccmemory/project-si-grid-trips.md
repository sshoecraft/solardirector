---
name: project-si-grid-trips
description: Grid-disconnect diagnosis (Jul 2026): under-voltage trips (heavy AC + low PV) + reverse-power trips (cloud-clear PV ramps); GdVtgMin 112→108 applied…
metadata:
  type: project
tags: [si, grid, trips, diagnosis]
---

Diagnosed Steve's recurring grid disconnects (May-July 2026). The SI cluster disconnects ITSELF (protection trips), the utility is not dropping. Two distinct mechanisms, confirmed from InfluxDB (power db, `inverter` + `pvinverter` measurements) around each EVENT Grid Off in /opt/sd/log/si.log:

1. **Under-voltage**: heavy load (8+ kW, AC running) + fading PV → 6 kW grid import → AC2 sags below GdVtgMin. Per-phase sag ~1 V per 2 kW import (service impedance ~50 mΩ — worth a physical look at the lugs someday).
2. **Reverse-power**: cloud-clear PV ramps (9→12.6 kW in 30 s observed) outrun the SI's import throttle → momentary export > GdRvPwr(100 W) for > GdRvTm(5 s) → trip.

Both produce the stereotyped ~65-75 s Off→On (GdVldTm=30 s + ExtLkTm=1 min reconnect lockout). Longer outages in the log (hours) are real utility events.

**Applied live 2026-07-02:** GdVtgMin 112 → 108 (via `siutil -f /root/si_settings.dat`; the settings file already had the intended values — the SI had drifted/never gotten them).
**Pending (standby-only param, see [[project-si-writes-standby]]):** GdRvPwr 100 → 500. Requires cluster standby = house power off = Sunday-type maintenance window.

Settings doctrine: `/root/si_settings.dat` on solardirector is the intended-state file; `siutil -v -f` diffs it against the live SI, `siutil -f` applies. Frequency was ruled out (59.94-60.03 Hz all day vs 59.5/60.5 window). Related: [[project-si-cluster]] (file-based memory), [[project-si-topology]].
