# agents/si — CAN Protocol & can.c

CAN-bus interface to the Sunny Island cluster (500 kbps, `can0`). Shared by the SI
agent and `utils/siutil` (which compiles this file directly via vpath).

## 2Phase4 CAN protocol map (verified 2026-07)

Verified by simultaneous `candump` capture + SMANet per-slave reads
(`InvVtgSlv1-3`, `ExtPwrAtSlv1-3`, `ExtCurSlv1-3`, ...) — values matched exactly.
Supersedes the exploratory notes in `~/CAN_CLUSTER_*.md` on solardirector (Oct 2025),
whose "2x2 multicluster" framing was wrong.

### Aggregate frames 0x300–0x30A (~0.8 Hz)

**The three 16-bit slots are per-UNIT: [master][slave1][slave2] — NOT phases.**
Slave3 does not fit in the 8-byte frame and is absent. This applies to 0x300
(active grid/gen power), 0x301 (active SI power), 0x302/0x303 (reactive),
0x304 (AC1 voltage), 0x309 (AC2 voltage). Values are int16 LE two's complement.

0x308 `TotLodPwr` and 0x305 battery data are cluster-wide (master-computed) and complete.

`ac2_voltage = slot1 + slot2` (master + slave1) yields the correct split-phase total
only because master and slave1 sit on opposite phases in this installation.

### Per-slave telemetry 0x1xx/0x2xx (~1.5 Hz)

Sub-id x1/x2/x3 = slave1/2/3. Each frame: two int32 LE SIGNED values (true two's
complement — unlike SMANet's offset-binary, see `lib/smanet/docs/smanet.md`).

| IDs | lo dword | hi dword |
|---|---|---|
| 0x181-0x183 | InvVtg (AC1) mV | ExtVtg (AC2) mV |
| 0x191-0x193 | slow pair, likely temperatures | (tentative) |
| 0x1B1-0x1B3 | gen-side — all zeros when no generator | |
| 0x1C1-0x1C3 | InvFrq mHz | InvCur mA (tentative) |
| 0x1E1-0x1E3 | **ExtPwrAt W** (grid power per slave) | ExtPwrRt var |
| 0x1F1-0x1F3 | apparent power VA (= √(P²+Q²) of 0x20x) | large counter (TBD, non-monotonic) |
| 0x201-0x203 | InvPwrAt W | InvPwrRt var |
| 0x221-0x223 | ExtCur mA | constant 0x00010003 (status) |

Master's own per-unit values: slot 1 of the 0x300-range frames.

### Other traffic
~80 Hz sync (0x010/0x031/0x032/0x033/0x070), ~8 Hz control (0x001/0x002/0x020/0x04x/0x051),
BMS out (0x351/0x355/0x35A/0x35B).

## can.c architecture

- `s->frames[SI_NFRAMES]` cache (si.h): slots 0–15 = 0x300–0x30F direct-mapped,
  slots 16+ = per-slave ids via `si_can_id2slot()` sparse lookup against `si_slave_ids[]`.
- Local transport: background recv thread caches matching frames; kernel-side filter is
  an explicit id list (`CAN_CONFIG_SET_FILTER`, 16 aggregate + 24 per-slave ids), which
  also rejects the high-rate sync chatter.
- Remote transport (rdev): `si_can_read_data()` polls ids individually; additionally
  fetches 0x1E3 (slave3 grid power) when input source is calculated.

### input_power / ac2_power (2Phase4)

`ac2_power = sum(0x300 slots) + slave3 ExtPwrAt from 0x1E3` (then negated, historical
convention). If the 0x1E3 frame is stale (>5 s: remote transport without per-slave ids,
or just after startup), falls back to the pre-2.05 estimate `t + t/3` (assumes slave3 ≈
mean of the others — the source of the historical load/input mismatch, off by >1 kW
under phase imbalance).

## History

- 2021: initial implementation (Stephen Shoecraft)
- 2026-07-02 (v2.05): decoded per-slave CAN ids and the [master][slv1][slv2] slot layout;
  replaced the 2Phase4 `t + t/3` estimate with slave3's actual 0x1E3 grid power; widened
  frame cache to SI_NFRAMES=40 with sparse id→slot lookup; kernel filter switched from
  range to explicit id list. Per-slave frames are cached but not yet published — future
  work: per-inverter fields in pub.js/InfluxDB, decode 0x19x pair and 0x1Fx counter.
- 2026-07-02 (v2.05, cont): same slave3 fix applied to ac1_power (output side, 0x301
  slots + 0x203); ac1/ac2_voltage use phase-pair averages ((master+slv2)/2 +
  (slv1+slv3)/2) when slave3's 0x183 frame is fresh; remote (rdev) path fetches
  0x203/0x183 too. smanet.c fallback: ac1_power/ac2_power now map from
  TotInvPwrAt/TotExtPwrAt (cluster-complete) with kW→W and negation to match the CAN
  sign convention (old I×V fallback had the OPPOSITE sign); GdOn detects grid activity
  in either direction. All verified read-only via rdevserver against the live cluster.

## IMPORTANT — production config

`cluster_config` MUST be 4 (2Phase4) in si.json for any of the 2Phase4 handling to
engage. As of 2026-07-02 production had `cluster_config: 3` — that is 2Phase**2**
(enum: 0=Unknown, 1=1Phase1, 2=1Phase2, 3=2Phase2, 4=2Phase4, 5=3Phase), which falls
into the default branch: a straight 3-slot sum, silently missing slave3 entirely.
Fix along with the v2.05 deploy: `sdconfig si set cluster_config 4`.
