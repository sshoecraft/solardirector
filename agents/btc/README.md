# BTC Agent - Battery Combiner

The BTC (battery combiner) agent aggregates data from multiple individual battery (BMS) agents into a single combined `solard_battery` view. It listens to the data published by each battery agent over MQTT, sums/merges the packs, and republishes a unified battery data set (and optionally logs combined power to InfluxDB).

**Agent role:** aggregator/utility (consumes agents with role `SOLARD_ROLE_BATTERY`)

## What It Does

1. Discovers battery agents by watching their `Info` messages and recording any agent whose `agent_role` is `battery`.
2. Collects each known battery agent's `Data` messages.
3. Combines the per-pack values into a single aggregate battery (total/series view).
4. Republishes the combined result and optionally logs total power to InfluxDB.

This gives downstream consumers (e.g. the `si` inverter agent, dashboards, or the `pa` power allocator) one combined battery reading instead of many individual packs.

## Configuration

Standard SD INI/JSON format (see `test.json`):

```json
{
    "btc": {
        "log_power": 1,
        "data_source": "192.168.1.142",
        "name": "btctest",
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
| `data_source` | MQTT broker/host to read source battery agents from |
| `log_power` | Log combined battery power to InfluxDB |
| `mqtt_uri` | MQTT broker URI |
| `influx_endpoint` / `influx_database` | InfluxDB logging target |

## Files

- `main.c` - Agent entry point, message processing, and battery combination logic
- `driver.c` - Driver/config glue (`btc_driver`, properties, info)
- `btc.h` - Session struct and per-agent info struct (`btc_agentinfo_t`)
- `test.json` - Test configuration

## Notes

- Built with `JS=no` (pure C; no embedded JavaScript logic).
- This is the battery analog of the `pvc` PV-combiner agent.

## See Also

- `agents/pvc/README.md` - PV inverter combiner (sibling aggregator)
- `agents/jbd/README.md`, `agents/jk/README.md` - source BMS agents
- `lib/sd/battery.h` - `solard_battery_t` data structure
- `../../CLAUDE.md` - Overall system architecture
