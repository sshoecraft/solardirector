# InfluxDB HVAC Database Schema

**Database:** `hvac`
**Retention Policy:** `autogen` (infinite duration, 7-day shard groups)

---

## Measurements

### `ac`
Main AC system state and environmental readings.

| Field | Type | Description |
|-------|------|-------------|
| cycle_time | string | |
| humidity | float | Relative humidity (%) |
| mode | string | Operating mode (e.g., Heat) |
| outside_temp | float | Outside temperature |
| pressure | float | Barometric pressure |
| pump_temp | float | Pump temperature |
| temp | float | Temperature reading |
| water_temp | float | Water temperature |
| water_temp_time | float | |

---

### `air_handler`
Air handler unit status and temperatures.

| Field | Type | Description |
|-------|------|-------------|
| air_in | float | Inlet air temperature |
| air_out | float | Outlet air temperature |
| cool_state | string | Cooling state (on/off) |
| fan_state | string | Fan state (on/off) |
| heat_state | string | Heating state (on/off) |
| mode | string | Operating mode |
| name | string | Unit identifier (e.g., ah1, ah2) |
| water_in | float | Water inlet temperature |
| water_out | float | Water outlet temperature |

---

### `directs`
Direct zone control units.

| Field | Type | Description |
|-------|------|-------------|
| active | float | Active flag (0/1) |
| enabled | float | Enabled flag (0/1) |
| fan | string | Associated fan/air handler |
| mode | string | Operating mode (e.g., Heat) |
| name | string | Unit identifier (e.g., dg1) |
| pin | float | GPIO pin number |
| pin_state | float | Pin state (0/1) |
| state | string | Running state (e.g., Running, Stopped) |
| unit | string | Associated AC unit |

---

### `errors`
System error logging.

| Field | Type | Description |
|-------|------|-------------|
| value | string | Error message |

---

### `events`
System event logging.

| Field | Type | Description |
|-------|------|-------------|
| action | string | Action taken |
| agent | string | Agent/component that triggered event |
| module | string | Module name |

---

### `fans`
Fan unit status and temperatures.

| Field | Type | Description |
|-------|------|-------------|
| enabled | float | Enabled flag (0/1) |
| error | float | Error flag (0/1) |
| mode | string | Operating mode |
| name | string | Fan identifier (e.g., ah1, ah2) |
| running | float | Running flag (0/1) |
| state | string | State (e.g., Running, Stopped) |
| temp_in | float | Inlet temperature |
| temp_out | float | Outlet temperature |

---

### `pumps`
Circulation pump status.

| Field | Type | Description |
|-------|------|-------------|
| enabled | float | Enabled flag (0/1) |
| error | float | Error flag (0/1) |
| flow_rate | float | Flow rate |
| name | string | Pump identifier |
| running | float | Running flag (0/1) |
| state | string | State (e.g., Running, Stopped) |
| temp_in | float | Inlet temperature |
| temp_out | float | Outlet temperature |

---

### `units`
HVAC unit status (likely heat pumps or compressors).

| Field | Type | Description |
|-------|------|-------------|
| enabled | float | Enabled flag (0/1) |
| error | float | Error flag (0/1) |
| liquid_temp | float | Liquid line temperature |
| mode | string | Operating mode |
| name | string | Unit identifier |
| running | float | Running flag (0/1) |
| state | string | State (e.g., Running, Stopped) |
| vapor_temp | float | Vapor line temperature |

---

## Notes

- No tag keys are defined; all series are identified by measurement name only
- 8 measurements total, 8 series
- Outside temperature is stored in the `ac` measurement as the `temp` field
- Appears to be a hydronic HVAC system with water-based heat distribution
