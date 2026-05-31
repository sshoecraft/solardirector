# AH Agent - Air Handler

The AH (air handler) agent monitors and controls a forced-air HVAC air handler. It reads supply/return air temperatures from thermistors via an ADC, tracks fan/cool/heat states, drives the fan via a GPIO relay, and publishes telemetry to MQTT and InfluxDB. A built-in simulation mode allows running without hardware.

**Agent role:** `SOLARD_ROLE_UTILITY`

## Features

- Thermistor temperature sensing via ADC channels (Steinhart/Beta conversion)
- Supply ("air in") and return ("air out") air temperature monitoring
- Optional water in/out temperatures (when compiled with `-DWATER`)
- Fan control and state sensing via GPIO pins (WiringPi / `wpi`)
- Cool / heat / emergency-heat state sensing
- Operating-mode reporting (None / Cool / Heat / Emergency Heat)
- Manual fan override
- Simulation mode for development without GPIO/sensors

## Temperature Conversion

Thermistor readings are converted to temperature using the Beta equation with a voltage divider:

```
T = 1 / ( ln(R / refres) / beta + 1 / (reftemp + 273.15) ) - 273.15
```

Where `refres`, `reftemp`, `beta`, and `divres` are configurable.

## Configuration

Standard SD INI/JSON format (see `test.json`). GPIO pins are WiringPi pin numbers; `-1` disables a pin.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `fan_control_pin` | int | -1 | Fan control (relay) GPIO pin |
| `fan_state_pin` | int | -1 | Fan state-sense GPIO pin |
| `cool_state_pin` | int | -1 | Cool state-sense GPIO pin |
| `heat_state_pin` | int | -1 | Heat state-sense GPIO pin |
| `air_in_ch` | int | -1 | ADC channel for supply air thermistor |
| `air_out_ch` | int | -1 | ADC channel for return air thermistor |
| `refres` | int | 10 | Reference resistance |
| `reftemp` | int | 25 | Reference temperature (°C) |
| `beta` | int | 3950 | Thermistor beta value |
| `divres` | int | 10 | Voltage-divider resistance |
| `sim_enable` | bool | 0 | Enable simulation mode (no hardware) |

Example (`test.json`):
```json
{
    "ah": {
        "sim_enable": 1,
        "name": "ahtest",
        "interval": 10,
        "script_dir": ".",
        "mqtt_uri": "tcp://localhost:1883",
        "influx_endpoint": "localhost:8086"
    }
}
```

## Service

Installs as a systemd service (`ah.service`, "Air Handler Agent"):

```
ExecStart=/opt/sd/bin/ah -ic /opt/sd/etc/ah.json -l /opt/sd/log/ah.log -a
```

## Files

- `main.c` - Agent entry point, config properties, fan on/off control, mode strings
- `driver.c` - Temperature sensing, state reading, and the read callback
- `ah.h` - Session struct, mode constants (`AH_MODE_*`), ADC base
- `ah.service` - systemd unit
- `test.json` - Test configuration (simulation enabled)

## Notes

- Built with `JS=yes`, `MQTT=yes`, `INFLUX=yes`; uses the `wpi` hardware-interface library for GPIO/ADC.
- Water-loop sensing (`water_in`/`water_out`) is conditional on the `WATER` compile flag.

## See Also

- `agents/ac/README.md` - Air-conditioning control agent (related HVAC agent)
- `lib/wpi/` - GPIO / hardware interface library
- `../../CLAUDE.md` - Overall system architecture
