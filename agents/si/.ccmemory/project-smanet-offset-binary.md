---
name: project-smanet-offset-binary
description: SMANet analog values are offset-binary (excess-32767) with device-supplied offset -3276.7; lib is CORRECT reading unsigned — never sign-extend; CAN u…
metadata:
  type: project
tags: [smanet, encoding, signed, offset-binary]
---

Investigated (July 2026) why `lib/smanet` "doesn't return signed values" — conclusion: **it does, and always did**.

SMANet wire encoding for analog channels is **offset-binary (excess-32767)**, not two's complement. The device's own channel list supplies `offset = -32767 * gain` (e.g. -3276.7 @ gain 0.1) on every signed-capable channel; `_getval()` in `lib/smanet/value.c` applies `raw_unsigned * gain + offset`, which restores the sign. Verified live: TotBatCur -33.3, TotLodPwr -1.1 decode correctly in the unmodified library.

**Do NOT sign-extend the raw values** (tried it 2026-07-02: `(int16_t)` cast broke the entire positive half of every offset channel — TotExtPwrAt +0.7 became -6552.9 because positive values sit at raw >= 0x8000 in offset-binary). Reverted; left a warning comment in `_getval()` and wrote `lib/smanet/docs/smanet.md`.

Contrast: the SAME SI firmware uses true two's complement on the CAN bus (0x300-range = signed int16 x 100; per-inverter 0x1xx/0x2xx frames show FF FF FF FF sign extension). Two conventions, one device.

FLOAT channels carry sign natively. BatVtg-style never-negative channels have offset 0. PARA channels skip gain/offset entirely in `_getval` — a negative-capable PARA channel would be the one place sign could still break (none observed).

**How to apply:** If SMANet values look wrong by ~3276.7 or ~6553, suspect offset handling, not signedness. When decoding raw SMANet packets by hand, remember positive = raw >= 0x8000. When correlating CAN vs SMANet values, apply the respective convention per bus. Related: [[project-si-writes-standby]], [[user-steve-smanet]].
