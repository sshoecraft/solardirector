---
name: project-si-topology
description: "Steve's site is AC-coupled — 4x SI 6048-US cluster + 3x Sunny Boy PV inverters (sb1/sb2/sb3) combined at pvcombiner"
metadata: 
  node_type: memory
  type: project
  originSessionId: fe056e56-968e-4b78-bf56-39f493a051c3
---

Steve's solar site is **AC-coupled**:
- **Battery/grid side**: 4× SI 6048-US in 2-phase-4 cluster (see [[project_si_cluster]])
- **PV side**: 3× SMA Sunny Boy inverters named `sb1`, `sb2`, `sb3` in the `pvinverter` InfluxDB measurement, totalized at `name='pvcombiner'`

InfluxDB on solardirector (`power` database):
- `inverter` measurement = SI cluster (input_power = grid net, load_power = SI-side net of AC1 load after PV cancellation, output_power = SI sourcing)
- `pvinverter` measurement with `"name" = 'pvcombiner'` = total PV output. Use single-quotes around values in InfluxQL, double-quotes around identifiers.

**Why:** Discovered during May 2026 grid-disconnect investigation. The SI's `load_power` field looked impossibly low (1-4 kW on a hot day with AC running) until we realized PV-side power isn't included in that field; total real load = `pvcombiner.output_power + (-inverter.input_power) - (-inverter.output_power)`.

**How to apply:** When interpreting power data, the SI cluster only sees the net at its bus. To reason about actual house load, factor in `pvcombiner` separately. AC-coupled means SB inverters frequency-track the SI in island mode and feed AC1 in grid-tied. Related: [[project_si_cluster]], [[feedback_dyngrid_meaning]].
