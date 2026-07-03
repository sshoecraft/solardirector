# lib/sd — CAN Transport (can.c)

SocketCAN (Linux) transport driver used by agents via the solard_driver interface.
Serves both direct device access and the rdevserver remote-device path.

## Filtering

- `CAN_CONFIG_SET_RANGE start,end` — builds ONE `struct can_filter` kernel entry per id
  in the range (not a mask/value pair). Fine for small ranges; a 400-id range means 400
  kernel filter entries (kernel cap 512).
- `CAN_CONFIG_SET_FILTER` — explicit id list (`struct can_config_filter {id*, count}`).
- `can_parse_filter()` — string spec used by transport topts: ranges with `-`, unions
  with `+`, e.g. `0x180-0x230+0x300-0x30F`. Values parse via strtol base 0 (hex ok).
- **Gotcha:** transport topts strings are stored in `SOLARD_TOPTS_LEN = 64` byte fields
  (common.h) and silently truncate. Long filter specs must use compact ranges — 24
  individual `+0x...` ids will NOT fit (see agents/si/docs/can.md history, 2026-07).

## Buffered mode (`can_start_buffer` / topts 3rd field = yes)

A background thread reads all matching frames into a direct-mapped frame array
(`id - start`) with a presence bitmap; `can_read_buffer()` serves cached frames.

- **Fixed 2026-07-02:** the bitmap was allocated as `calloc(num/32, 1)` — a uint32
  count used as a BYTE count (4x too small, and truncating instead of rounding up).
  Any buffered span >32 ids overflowed the heap on bitmap writes. Now
  `((num+31)/32)*sizeof(uint32_t)` bytes. `bitmaps_size` is in BYTES (memset in the
  thread loop relies on that).
- **Design caveat:** the buffer-fill thread lives in the process that called
  `can_start_buffer`. rdevserver forks a child per client — the child inherits a
  frozen snapshot of the frame buffer and NO fill thread, so buffered devices served
  over rdevserver return fork-moment data forever. Use buffer=no (direct reads) for
  rdevserver device configs; `can_read_direct()` loops on the live socket until the
  requested id arrives, which is correct per-child.

## History

- 2026-07-02: bitmap allocation fix (above), found while widening the SI agent's CAN
  filter for per-slave telemetry (agents/si/docs/can.md).
