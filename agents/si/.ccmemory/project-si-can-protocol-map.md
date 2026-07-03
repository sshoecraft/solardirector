---
name: project-si-can-protocol-map
description: CONFIRMED 2Phase4 CAN map: 0x300-range slots = [master][slv1][slv2] (slv3 missing!); per-slave blocks 0x?x1/2/3 decoded and verified vs SMANet; fix =…
metadata:
  type: project
tags: [si, can, protocol, 2phase4, confirmed]
---

Verified July 2026 by simultaneous candump + SMANet per-slave snapshot (siutil -g InvVtgSlv* ExtPwrAtSlv* etc.), values matched exactly. Supersedes the interpretations in the Oct 2025 docs AND my earlier guesses.

**Root cause of load_power vs input_power mismatch:** the 0x300-range "3-phase slots" are actually per-UNIT values [master][slave1][slave2] — slave3 does not fit in the 8-byte frame and is simply absent. Applies to 0x300 (active grid pwr), 0x301 (active SI pwr), 0x302/0x303 (reactive), 0x304 (AC1 V), 0x309 (AC2 V). The `t + t/3` fudge in agents/si/can.c guessed slave3 as the mean of the others. 0x308 TotLodPwr IS cluster-complete (master-computed). ac2_voltage = slot1+slot2 (can.c:246) works only because master and slv1 are on opposite phases.

**Per-slave telemetry blocks** (sub-ID x1/x2/x3 = slave1/2/3; two int32 LE SIGNED (true two's complement, unlike SMANet's offset-binary) per frame, ~1.5 Hz):
- 0x181/2/3: [InvVtg AC1 mV][ExtVtg AC2 mV]
- 0x191/2/3: slow pair, likely temperatures (~37.5/29.7) — TENTATIVE
- 0x1B1/2/3: gen-side — zeros when no generator (NOT a dead inverter)
- 0x1C1/2/3: [InvFrq mHz][InvCur mA (tentative)]
- 0x1E1/2/3: [ExtPwrAt W signed][ExtPwrRt var] ← grid power per slave, THE missing data
- 0x1F1/2/3: [apparent power VA = sqrt(P²+Q²) of 0x20x][large counter, TBD — observed non-monotonic]
- 0x201/2/3: [InvPwrAt W signed][InvPwrRt var]
- 0x221/2/3: [ExtCur mA][constant 0x00010003 status]
Master's own values: slot 1 of the 0x300-range frames (+ SMANet).

**The input_power fix:** ac2_power = sum of 0x300's three slots (×100, signed int16) + slave3 ExtPwrAt from 0x1E3 lo dword (W). Verified: −500+1300−590+1311 = +1521 W true vs old fudge +280 W.

Implementation constraints/plan: see [[project-si-can-2phase4-missing]] (sparse id→slot lookup in can.c, frames[32], explicit CAN_CONFIG_SET_FILTER list; test via siutil rebuild — compiles agent's can.c directly — zero cluster impact). Bus also carries ~80 Hz sync IDs (0x010/0x031/0x032/0x033/0x070), ~8 Hz control (0x001/0x002/0x020/0x04x/0x051), BMS 0x351/355/35A/35B.

Capture artifacts: /tmp/can_corr.txt + /tmp/snap1.txt/snap2.txt on solardirector (July 2026), ~/can_capture_10k.txt (Oct 2025).
