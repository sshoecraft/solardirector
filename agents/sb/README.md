# SB Agent - SMA Sunny Boy PV Inverter

The SB agent monitors SMA Sunny Boy grid-tie PV inverters over their built-in web/JSON API (Webconnect / "SMA WebBox"-style endpoint). It logs in over HTTPS, polls the inverter's measurement objects, and publishes a standard `solard_pvinverter` data set to MQTT and InfluxDB.

**Agent role:** `SOLARD_ROLE_PVINVERTER`

## Features

- HTTPS communication with the inverter's JSON-RPC web API (via libcurl)
- Session-based login (user/installer password) with automatic re-login
- Object/measurement discovery driven by a JSON object map (`objects.json`)
- String-table resolution for labels, units, hierarchy, and event messages
- Configurable read/login/config field sets
- Min/max/sum/avg/count aggregation per object
- Optional parameter writes to the inverter
- Power logging to InfluxDB
- Optional use of internal (compiled-in) string tables

## How It Works

1. **Login** - POSTs credentials to the inverter endpoint and stores a session ID.
2. **Read** - Requests the configured measurement objects; values are decoded using the object map and string tables.
3. **Publish** - Maps results into a `solard_pvinverter` structure and publishes via MQTT, optionally logging power to InfluxDB.

Object definitions live in `objects.json` (installed as a data file). The string tables can be loaded from a file or compiled in (`en_strings.h`).

## Configuration

Standard SD INI/JSON format (see `sbtest.conf`):

```ini
[sb]
name=sbtest
endpoint=https://192.168.1.153
password=mypass
#objects=objects.json
#influx_database=power
#interval=10
```

| Parameter | Description |
|-----------|-------------|
| `name` | Instance name (e.g. `sb1`, `sb2`, `sb3`) |
| `endpoint` | Inverter base URL (`https://<ip>`) |
| `user` | Login user (installer/user) |
| `password` | Login password |
| `lang` | Language for string resolution |
| `objects` | Path to object definition JSON |
| `interval` | Poll interval in seconds |
| `log_power` | Log output power to InfluxDB |

## Files

- `main.c` - Agent entry point and main loop
- `sb.c` - Core HTTP/session logic (login, request/response, curl buffer handling)
- `results.c` - Result/value decoding and aggregation
- `info.c` - Agent info JSON (`agent_role` = pvinverter, version, author)
- `config.c` - Configuration properties and handlers
- `strings.c` / `en_strings.h` - String-table loading and compiled-in English strings
- `objects.c` / `objects.json` - Measurement object definitions
- `jsfuncs.c` - JavaScript bindings
- `mkstr1.c` - Helper to generate the compiled string table
- `sb.h` - Session, object, result, and value structs
- `read.js.no` - Optional JavaScript read logic (disabled by extension)
- `dump.js`, `dump_ivals.js`, `dump_parms.js` - Developer dump/diagnostic scripts
- `sbtest.conf` - Test configuration

## Notes

- Built with `CURL=yes`; requires libcurl with TLS support.
- This is the SMA **Sunny Boy** (PV/grid-tie) agent. The **Sunny Island** battery inverter is handled by the separate `si` agent over CAN/SMANet.

## See Also

- `agents/si/README.md` - SMA Sunny Island battery inverter agent
- `agents/pvc/README.md` - PV combiner that aggregates multiple PV inverter agents
- `lib/sd/pvinverter.h` - `solard_pvinverter_t` data structure
- `../../CLAUDE.md` - Overall system architecture
