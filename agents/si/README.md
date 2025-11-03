# SI Agent - SMA Sunny Island Inverter Control

The SI agent provides monitoring and control for SMA Sunny Island battery inverters via CAN bus and SMANet protocol.

## Features

- Real-time monitoring of inverter status, battery voltage, current, power
- Grid connection management (connect/disconnect)
- Battery management and State of Charge (SoC) tracking
- Charging control and optimization
- Generator start/stop coordination
- Multi-inverter synchronization support
- MQTT-based communication with other agents
- JavaScript-based business logic for flexible control algorithms

## Hardware Requirements

- SMA Sunny Island inverter (tested with SI6048)
- CAN bus interface (can0)
- Linux system with SocketCAN support

## Initial Setup

### 1. Download Channel Definitions

**IMPORTANT:** Before running the SI agent for the first time, you must download the channel definitions from your specific inverter model. The SI agent cannot function without this channel map.

```bash
# Stop the SI agent if running
sudo systemctl stop si

# Download channels from the inverter
# This creates a model-specific JSON file (e.g., SI6048UM_channels.json)
sudo siutil -u can,can0 -D /opt/sd/lib/agents/si

# Verify the channel file was created
ls -lh /opt/sd/lib/agents/si/*_channels.json
```

**Note:** The channel file name will be based on your inverter model (e.g., `SI6048UM_channels.json`, `SI8048_channels.json`). This file maps SMANet channel IDs to human-readable names and data types.

### 2. Configuration

Edit `/opt/sd/etc/sd.conf` or create a site-specific configuration:

```json
{
  "name": "si",
  "transport": "can,can0",
  "mqtt_uri": "tcp://localhost:1883",
  "interval": 10
}
```

### 3. Install and Start Service

```bash
# Install the agent
cd /home/steve/src/sd
make install

# Reload systemd
sudo systemctl daemon-reload

# Enable and start the service
sudo systemctl enable si
sudo systemctl start si

# Check status
sudo systemctl status si

# View logs
journalctl -u si -f
```

## Configuration Options

Key configuration properties (via `sdconfig`):

- **interval**: Read interval in seconds (default: 10)
- **smanet_auto_close**: Auto-close CAN connection when idle (default: yes)
- **smanet_auto_close_timeout**: Timeout in seconds (default: 30)
- **GdManStr**: Grid connection management (values: "Start", "Stop")
- **BatChaStt**: Battery charge state target

### Viewing Configuration

```bash
# List all SI agent parameters
sdconfig -n si -L

# Get specific parameter
sdconfig si get GdManStr

# Set parameter
sdconfig si set GdManStr Start
```

## Channel File Management

### Channel File Location

The channel definition file is stored at:
```
/opt/sd/lib/agents/si/<MODEL>_channels.json
```

For example: `/opt/sd/lib/agents/si/SI6048UM_channels.json`

### Type Encoding Migration

**IMPORTANT:** If you're upgrading from an older version or copying channel files between systems, you may encounter type encoding errors like:

```
error: _read_values: unhandled type: 16777217(UNKNOWN)
```

This happens because older channel files used a different type encoding system (bitfield-based: 0x1000001) while current builds use enum-based encoding (0, 1, 2...).

**Solution:** Re-download the channels using siutil:

```bash
sudo systemctl stop si
sudo rm /opt/sd/lib/agents/si/*_channels.json
sudo siutil -u can,can0 -D /opt/sd/lib/agents/si
sudo systemctl start si
```

### Backing Up Channels

Once downloaded, it's recommended to backup your channel file:

```bash
sudo cp /opt/sd/lib/agents/si/SI6048UM_channels.json ~/SI6048UM_channels.json.backup
```

## Usage

### Monitoring

```bash
# Monitor real-time data via MQTT
mosquitto_sub -t 'solar/si/data' -v

# View all current values
siutil -u can,can0 -a

# List all available channels
siutil -u can,can0 -l

# Monitor specific values
siutil -u can,can0 -m
```

### Control Operations

```bash
# Start grid connection
sdconfig si set GdManStr Start

# Stop grid connection
sdconfig si set GdManStr Stop

# View grid connection status
sdconfig si get GdManStr
```

### JavaScript Business Logic

The SI agent uses JavaScript files for control algorithms:

- `init.js` - Initialization and startup
- `read.js` - Data acquisition from inverter
- `write.js` - Writing commands to inverter
- `pub.js` - Publishing data to MQTT
- `charge.js` - Battery charging logic
- `grid.js` - Grid connection management
- `gen.js` - Generator control
- `soc.js` - State of Charge calculations
- `sync.js` - Multi-inverter synchronization

These can be modified without recompiling the C code.

## Troubleshooting

### Agent Won't Start

1. **Check CAN interface:**
   ```bash
   ip link show can0
   # Should show "UP" state
   ```

2. **Check channel file exists:**
   ```bash
   ls -l /opt/sd/lib/agents/si/*_channels.json
   ```

3. **Check logs:**
   ```bash
   journalctl -u si -n 100
   tail -f /opt/sd/log/si.log
   ```

### Type Encoding Errors

See "Type Encoding Migration" section above.

### Connection Timeouts

```
error: _read_values: retries exhausted
```

This can indicate:
- CAN bus issues (check wiring, termination)
- Inverter not responding (check power, status)
- Bus contention (other devices on CAN bus)

Try increasing the timeout or reducing polling frequency.

### Debug Mode

Run the agent manually with debug output:

```bash
sudo systemctl stop si
cd /opt/sd/lib/agents/si
sudo ./si -c test.json -v -d
```

## Architecture

The SI agent follows the standard SolarDirector agent pattern:

```
Hardware (SI via CAN) ←→ Driver (C) ←→ JavaScript ←→ MQTT ←→ Other Agents
                              ↓
                      SMANet Protocol Library
                              ↓
                      Channel Definitions (JSON)
```

### Key Components

- **main.c**: Entry point, command-line interface, main loop
- **driver.c**: CAN communication and connection management
- **smanet.c**: SMANet protocol implementation
- **can.c**: Low-level CAN bus operations
- **siconfig.c**: Configuration property definitions
- **jsfuncs.c**: C functions exposed to JavaScript

## Development

### Building

```bash
cd /home/steve/src/sd
make -C agents/si
```

### Testing

```bash
# Run with test configuration
./agents/si/si -c agents/si/test.json -v -d

# Test SMANet communication
./utils/siutil/siutil -u can,can0 -a
```

### Adding New Features

1. Modify JavaScript files for logic changes (no recompile needed)
2. Add C functions in `jsfuncs.c` for new native capabilities
3. Update `siconfig.c` for new configuration properties
4. Test thoroughly before deploying to production

## Related Tools

- **siutil**: Command-line utility for SI management and testing
- **sdconfig**: Configuration management across all agents
- **sc**: Service Controller for centralized agent monitoring

## References

- SMA SMANet Protocol Documentation
- SolarDirector Agent Framework: `/home/steve/src/sd/CLAUDE.md`
- SMANet Library: `/home/steve/src/sd/lib/smanet/`
