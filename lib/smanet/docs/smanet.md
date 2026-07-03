# lib/smanet — SMANet (RS485) Protocol Library

Steve's from-scratch implementation of SMA's proprietary SMANet RS485 protocol for
Sunny Island / Sunny Boy inverters. Used by `agents/si` and `utils/siutil` (which
compiles the agent's sources directly via vpath).

## Architecture

| File | Role |
|---|---|
| `session.c` | Session lifecycle, transport open/close, locking |
| `frame.c` | Low-level frame send/recv (0x68-delimited, FCS via `fcstab.h`) |
| `packet.c` | Packet assembly/parsing on top of frames |
| `command.c` | Command exchange (`smanet_command`), retry/timeout, NET_START, CFG_NETADR |
| `channel.c` | Channel-list download/parse, JSON channel-file load/save |
| `value.c` | Value read (`_read_values`, `_getval`) and write (`_write_value`, `smanet_set_value`) |
| `jsfuncs.c` | JavaScript bindings |

Channels are self-describing: the device supplies name, format, gain, and offset in
its channel list. Channel files are cached as JSON (e.g.
`/opt/sd/lib/agents/si/SI6048UM_channels.json`).

## Wire encoding — IMPORTANT (July 2026 finding)

**Analog values are offset-binary (excess-32767), NOT two's complement.**

A signed quantity is transmitted as an unsigned word, and the device-supplied channel
offset restores the sign during conversion in `_getval()`:

```
value = raw_unsigned * gain + offset      where offset = -32767 * gain (e.g. -3276.7 @ gain 0.1)
```

- raw 32756 @ gain 0.1, offset −3276.7 → **−1.1** (negative values sit BELOW 0x7FFF)
- raw 32774 (0x8006) → **+0.7** (positive values sit AT/ABOVE 0x8000)

Consequences:
- Reading raw values as unsigned in `value.c` is CORRECT. Do NOT "fix" the unsigned
  getters/storage by sign-extending — a two's-complement cast destroys the entire
  positive half of every offset-carrying channel (verified live 2026-07-02: casting
  turned TotExtPwrAt +0.7 into −6552.9).
- Channels that never go negative (e.g. BatVtg) have offset 0.
- FLOAT channels carry sign natively.
- The SAME firmware uses true two's complement on the CAN bus (0x300-range frames are
  signed int16 ×100). Two conventions, one device — don't assume one from the other.

## Write-path quirk — standby-locked parameters

Some safety-relevant parameters (observed: `GdRvPwr`) silently refuse writes while
the inverter/cluster is running: `CMD_SET_DATA` gets no response at all (timeout,
r=2 after retries), not a NAK. The parameter must be changed with the cluster in
standby. Others (e.g. `GdVtgMin`) accept writes while running. There is no
protocol-level error code distinguishing these cases — a set that times out
repeatedly on an otherwise-healthy session is the signature.

## Known quirks

- `_read_values` (value.c): SI6048 will not return a single channel value — only a
  whole group (request index 0); the code requests the group and walks all channels
  of the matching mask.
- PARA channels skip gain/offset entirely in `_getval()` (`mask & CH_PARA`) — raw
  values are used as-is. Any negative-capable PARA channel would not get offset
  correction (none observed so far).
- Optimized for 19200 baud: `smanet_command` sleeps 550 ms between send and recv.

## History

- 2021: initial implementation (Stephen Shoecraft)
- 2026-07-02: investigated "no signed values" question; determined encoding is
  offset-binary and the library was already correct. Added explanatory comment in
  `_getval()` and this doc. siutil rev'd to 1.0.1 (comment/doc only, no functional
  change).
