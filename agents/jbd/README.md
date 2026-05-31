# JBD Agent - JBD/Xiaoxiang BMS Control

The JBD agent provides monitoring and control for JBD (Jiabaida / "Xiaoxiang") battery management systems. It reads pack and per-cell telemetry, manages cell balancing and charge/discharge MOSFETs, and publishes a standard `solard_battery` data set to MQTT and InfluxDB.

**Agent role:** `SOLARD_ROLE_BATTERY`

## Features

- Per-cell voltage monitoring (up to 32 cells) with min/max/diff/avg/total derived values
- Multiple temperature sensor readings (up to 8 NTC channels)
- Pack voltage, current, capacity, and computed power
- Cell balancing control (off / on / charge-only)
- Charge and discharge MOSFET control
- Hardware info reporting (manufacturer, model, manufacture date, version)
- Protection-status decoding (over/under voltage, over/under temp, over-current, short, IC/MOS faults)
- EEPROM register read/write for configuration
- Power logging to InfluxDB

## Transports

The JBD protocol is supported over several transports (selected at build time, see `jbd_transports[]` in `driver.c`):

- **CAN bus** (`can_driver`) - native CAN frame format
- **IP** (`ip_driver`) - networked devices / `rdevserver`
- **Serial** (`serial_driver`) - direct UART connection
- **Bluetooth** (`bt_driver`) - BLE (when compiled with `BLUETOOTH=yes`)

The standard (serial/BT) reader and the CAN reader are selected automatically based on the configured transport.

## Configuration

Configuration uses the standard SD INI/JSON format. Key parameters (see `jbdtest.conf`):

```ini
[jbd]
name=pack_01
transport=ip
target=pack_01
influx_database=power
interval=15
```

| Parameter | Description |
|-----------|-------------|
| `name` | Instance/pack name (e.g. `pack_01`) |
| `transport` | Transport type: `can`, `ip`, `serial`, `bt` |
| `target` | Transport target (device, host, or BT MAC) |
| `topts` | Transport options (e.g. CAN interface, BT characteristic) |
| `interval` | Read interval in seconds |
| `start_at_one` | Number cells starting at 1 instead of 0 |
| `balancing` | Balance mode: 0=off, 1=on, 2=only when charging |
| `log_power` | Log pack power to InfluxDB |

## Files

- `main.c` - Agent entry point and main loop
- `driver.c` - JBD protocol I/O (CAN and serial/BT readers, packet build/verify, MOSFET and balance control, EEPROM access)
- `config.c` - Configuration properties and `get`/`config` handlers
- `info.c` - Hardware info query and agent info JSON (`agent_role`, version, author)
- `jsfuncs.c` - JavaScript bindings
- `jbd.h` - Session struct, protocol constants, command/function codes
- `jbd_regs.h` - JBD EEPROM/register definitions
- `jbdtest.conf` - Test configuration

## Protocol Notes

- Packets framed by `0xDD` (start) / `0x77` (end), with read (`0xA5`) and write (`0x5A`) commands.
- Multi-byte values are big-endian (`-DTARGET_ENDIAN=BIG_ENDIAN`); helpers `jbd_getshort`/`jbd_putshort` map to `_gets16`/`_puts16`.
- Hardware info: `JBD_CMD_HWINFO` (0x03), cell info `0x04`, hardware version `0x05`, MOSFET control `JBD_CMD_MOS` (0xE1).

## See Also

- `agents/jk/README.md` - JiKong (JK) BMS agent (sibling BMS implementation)
- `lib/sd/battery.h` - `solard_battery_t` data structure
- `../../CLAUDE.md` - Overall system architecture
