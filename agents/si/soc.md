# SOC System - Design Document

## System Overview

14S NMC lithium pack, 1050 Ah total capacity, 4+ years old.
Usable range: 252 Ah (24%, empty anchor) to 934.5 Ah (89%, full anchor).
4.01V/cell max (56.2V) — never charge to 4.2V to protect aging cells.
3.22V/cell (~45.1V) is the practical floor where voltage falls off a cliff.
Charging method: CCCV. CV cutoff at ~31.5A (3% of capacity, ~C/33).

## Current State (as of 2026-03-09)

### Session 1: Adaptive table + Kalman bins

1. **Replaced static table with adaptive table** (`soc_table.js` rewrite)
   - Removed all EMA code (was never connected to SOC anyway)
   - Added per-0.25V-bin Kalman filters that update from complete cycles
   - Adaptive table persisted to `adaptive_table.dat`, seeded from old `ah_table.dat`
   - Kalman R=25 (process noise), Q=100 (measurement noise), hardcoded

2. **Fixed drift correction formula**
   - Old: `expected_ah = charge_end_ah - ah_from_full` → produced 402 Ah at 45.1V (wrong)
   - New: proportional mapping through usable range: `fraction = ah/max_table_ah`, then map to `charge_start_ah..charge_end_ah`

3. **Anchor protection** — drift correction skipped when anchor fires (flag in charge.js)

4. **Cycle curve recording** — during complete anchor-to-anchor cycles, voltage→Ah points recorded at each 0.25V bin, folded into adaptive table via Kalman at cycle completion

### Session 2: Continuous correction (EV-style)

Replaced threshold-based drift correction with continuous correction every interval (10s):

1. **Removed threshold gating** — no more drift_threshold_pct (was 3%), drift_check_interval (was 60s), or drift_correction_rate (was 0.05). Every interval applies a correction.

2. **Continuous proportional correction** — `correction = gain × confidence × error` where:
   - `gain` = 0.01 (configurable via `correction_gain`)
   - `error` = table_expected_ah - battery_ah
   - Convergence: ~45% after 10 min, ~83% after 30 min, ~97% after 1 hour

3. **Confidence weighting by state**:
   - CC charge: 1.0 (voltage correlates well with SOC under load)
   - Discharge: 0.8 (voltage under load, moderate reliability)
   - At rest: 1.5 (OCV is the best SOC indicator — averages both tables)
   - CV charge: 0.5 (current decay is informative but less direct than voltage)
   - All configurable via `*_correction_confidence` config props

4. **CV current→Ah table** — new adaptive table section:
   - During CV, voltage is constant (useless). Current decays monotonically instead.
   - Table maps current_bin (5A steps) → Ah_pumped_since_CV_start
   - Learned from complete charge cycles, folded via same per-bin Kalman
   - `charge_start_cv()` records `si.cv_start_ah` for CV tracking
   - Correction during CV: expected_ah = cv_start_ah + cv_table_lookup(current)

5. **At-rest correction** — previously skipped when |power| < 100W. Now corrects using OCV: averages charge and discharge table predictions at current voltage, with highest confidence (1.5×)

6. **CV curve recording** — during charge cycles, CV phase records current→Ah points in `cycle_state.cv_curve_points`. Folded into `adaptive_table.cv` on cycle completion via `fold_cv_into_table()`.

## How EVs Solve This

EVs use CCCV and face the same drift problem. Their solution: **continuous model-based correction every measurement cycle, with no threshold.**

An Extended Kalman Filter (EKF) runs a battery model that predicts what voltage (during CC/discharge) or current (during CV) *should be* at the current SOC estimate. The difference between predicted and actual is the correction signal, applied every cycle.

- During CC: model predicts voltage given SOC + current → compare to measured voltage → nudge SOC
- During CV: model predicts current given SOC + held voltage → compare to measured current → nudge SOC
- During discharge: model predicts voltage given SOC + load current → compare to measured voltage → nudge SOC

Key insight: **they never let drift accumulate.** No threshold. Every measurement is a correction. The Kalman gain automatically balances trust between coulomb counting and the model.

### What we have that maps to the EV approach

| EV System | Our System |
|-----------|-----------|
| Battery model (OCV curve) | Adaptive voltage→Ah table |
| Continuous EKF correction | Threshold-based correction every 60s |
| Current prediction during CV | We have current measurement during CV |
| Known chemistry model | 4 years of real operational data in InfluxDB |
| Lab-characterized parameters | Learned from actual charge/discharge cycles |

The adaptive table IS our battery model. We just need to use it the way EVs use theirs: continuous correction, no threshold, every interval.

## What Was Done (Phases 1-2 complete)

Phases 1 and 2 from the original plan are implemented. The system now has:
- Continuous correction every 10s interval (no threshold, no timer)
- Confidence-weighted correction for CC, discharge, rest, and CV states
- CV current→Ah adaptive table for current-based correction during CV
- At-rest OCV correction (was previously skipped entirely)

## Remaining Work / Future Improvements

### Phase 3: Time remaining accuracy (partially done)
- Time-to-full and time-to-empty already use battery_ah, which is now more accurate
- CV time remaining already uses cv_time countdown as a bound
- Could improve: use CV current decay model to estimate time-to-full during CV
  (predict when current will reach cutoff based on learned decay curve)

### Phase 4: Full Kalman filter (optional)
- Replace proportional correction with proper 1D Kalman filter for battery_ah state
- State = battery_ah, measurement = table-predicted Ah at current voltage/current
- Process noise = coulomb counting uncertainty (varies with current magnitude)
- Measurement noise = table uncertainty (from per-bin Kalman covariance)
- Would give adaptive gain that automatically adjusts based on confidence

### Tuning
- Monitor correction behavior over several charge/discharge cycles
- Adjust gain (0.01) and confidence weights if needed
- Watch for: overcorrection oscillation, slow convergence, anchor jumps
- The CV table will be empty until the first complete CCCV cycle — no CV correction until then

## Key Files

| File | Purpose |
|------|---------|
| `soc.js` | Coulomb counting, cycle tracking, curve recording, SOC calculation |
| `soc_table.js` | Adaptive table, Kalman per-bin, drift correction, table persistence |
| `charge.js` | Charge state machine, anchor points, CC/CV transitions |
| `pub.js` | Publishes battery_level to MQTT and InfluxDB ("inverter" measurement) |
| `current.js` | Writes to "current" measurement in InfluxDB |
| `kalman.js` | KalmanFilter class (in /src/sd/lib/sd/) |
| `adaptive_table.dat` | Persisted adaptive table (per-bin Kalman state) |
| `ah_table.dat` | Original static table (seed source, in /opt/sd/lib/agents/si/) |
| `battcp.dat` | Battery checkpoint (battery_ah, battery_in, battery_out) |

## Configuration

From `/opt/sd/etc/si.json`:
- `battery_capacity`: 1050
- `charge_start_level`: 24 → charge_start_ah = 252
- `charge_end_level`: 89 → charge_end_ah = 934.5
- `min_voltage`: 41, `max_voltage`: 58.1
- `charge_cc_voltage`: 57.1
- `charge_method`: 1 (CCCV)
- `cv_time`: 90 (minutes)
- `cv_cutoff`: 63 (amps, ~3% of capacity)
- `interval`: 10 (seconds)
- `influx_endpoint`: http://localhost:8086
- `influx_database`: power

## InfluxDB

- **inverter** measurement: battery_voltage, battery_power, battery_level, charge_mode, etc. (written by pub.js every interval)
- **current** measurement: current source values (written by current.js)
- Database: `power` on localhost:8086

## Old System Reference

The previous system (in `/opt/sd/src/agents/si/`) used single charge/discharge factors:
- `charge_factor` (default 1.04): multiplied against charging amps before coulomb counting
- `discharge_efficiency` (default 0.945): divided into discharging amps before coulomb counting
- Kalman-smoothed across cycles, primed from InfluxDB historical data at startup
- Problem: single factor can't capture nonlinear voltage curve, temperature shifts broke it
