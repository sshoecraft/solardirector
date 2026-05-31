# PVC Agent - PV Inverter Combiner

The PVC (PV combiner) agent aggregates data from multiple PV inverter agents into a single combined `solard_pvinverter` view. It listens to the data published by each PV agent over MQTT, merges them, and republishes a unified PV data set (and optionally logs combined power to InfluxDB).

**Agent role:** aggregator/utility (consumes agents with role `SOLARD_ROLE_PVINVERTER`)

## What It Does

1. Discovers PV inverter agents by watching their `Info` messages and recording any agent whose `agent_role` is `pvinverter`.
2. Collects each known PV agent's `Data` messages.
3. Combines the per-inverter values into a single aggregate PV inverter reading.
4. Republishes the combined result and optionally logs total output power to InfluxDB.

In an AC-coupled site this provides one combined "PV combiner" reading (e.g. summing several Sunny Boy `sb` agents) for downstream consumers such as the `pa` power allocator and dashboards.

## Configuration

Standard SD INI/JSON format (see `test.json`):

```json
{
    "pvc": {
        "log_power": 1,
        "data_source": "192.168.1.164",
        "name": "pvctest",
        "mqtt_uri": "tcp://localhost:1883",
        "influx_endpoint": "localhost:8086",
        "influx_database": "test",
        "script_dir": "."
    }
}
```

| Parameter | Description |
|-----------|-------------|
| `name` | Instance name |
| `data_source` | MQTT broker/host to read source PV agents from |
| `log_power` | Log combined output power to InfluxDB |
| `mqtt_uri` | MQTT broker URI |
| `influx_endpoint` / `influx_database` | InfluxDB logging target |

## Files

- `main.c` - Agent entry point, message processing, and PV combination logic
- `driver.c` - Driver/config glue (`pvc_driver`, properties, info)
- `pvc.h` - Session struct and per-agent info struct (`pvc_agentinfo_t`)
- `test.json` - Test configuration

## Notes

- Built with `JS=no` (pure C; no embedded JavaScript logic).
- This is the PV analog of the `btc` battery-combiner agent.

## See Also

- `agents/btc/README.md` - Battery combiner (sibling aggregator)
- `agents/sb/README.md` - SMA Sunny Boy PV inverter source agent
- `lib/sd/pvinverter.h` - `solard_pvinverter_t` data structure
- `../../CLAUDE.md` - Overall system architecture
