# JK Agent - JiKong (JK) BMS Control

The JK agent provides monitoring and control for JiKong (JK / "JKBMS") battery management systems. It reads pack and per-cell telemetry — including per-cell internal resistance — manages balancing and charge/discharge MOSFETs, and publishes a standard `solard_battery` data set to MQTT and InfluxDB.

**Agent role:** `SOLARD_ROLE_BATTERY`

## Features

- Per-cell voltage monitoring (up to 32 cells) with min/max/diff/avg/total derived values
- Per-cell internal resistance readings (JK-specific)
- Multiple temperature sensor readings (up to 8 channels)
- Pack voltage, current, capacity, and computed power
- Cell balancing control (off / on / charge-only)
- Charge and discharge MOSFET control
- Hardware info reporting (manufacturer, device, model, manufacture date, hardware/software versions)
- EEPROM read/write for configuration
- Power logging to InfluxDB

## Transports

Selected at build time / by configuration:

- **CAN bus** - native CAN frame format
- **IP** - networked devices / `rdevserver`
- **Serial** - direct UART connection
- **Bluetooth** - BLE (when compiled with `BLUETOOTH=yes`; disabled by default in the Makefile via `BLUETOOTH=no`)

## Configuration

Standard SD INI/JSON format (see `jktest.conf`):

```ini
[jk]
name=pack_02
transport=ip
target=pack_02
script_dir=.
mqtt_host=192.168.1.5
influx_database=power
```

| Parameter | Description |
|-----------|-------------|
| `name` | Instance/pack name (e.g. `pack_02`) |
| `transport` | Transport type: `can`, `ip`, `serial`, `bt` |
| `target` | Transport target (device, host, or BT MAC) |
| `topts` | Transport options (e.g. `ffe1` BT characteristic) |
| `start_at_one` | Number cells starting at 1 instead of 0 |
| `balancing` | Balance mode: 0=off, 1=on, 2=only when charging |
| `log_power` | Log pack power to InfluxDB |

Example BT configuration (commented in `jktest.conf`):
```ini
transport=bt
target=3C:A5:4A:E4:09:31
topts=ffe1
```

## Files

- `main.c` - Agent entry point and main loop
- `driver.c` - JK protocol I/O (open/close, transport init, packet build/verify)
- `io.c` - Low-level read/write and CAN helpers
- `config.c` - Configuration properties and `config` handler
- `info.c` - Hardware info query and agent info JSON (`agent_role`, version, author)
- `jsfuncs.c` - JavaScript bindings
- `jk.h` - Session struct, hardware-info struct, protocol/state constants
- `jktest.conf` - Test configuration

## See Also

- `agents/jbd/README.md` - JBD BMS agent (sibling BMS implementation)
- `lib/sd/battery.h` - `solard_battery_t` data structure
- `../../CLAUDE.md` - Overall system architecture
