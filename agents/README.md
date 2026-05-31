# SD Agents

This directory contains the hardware and service agents for the SolarDirector (SD) system. Each agent is a standalone C program with an embedded SpiderMonkey JavaScript runtime. Agents communicate over MQTT, optionally log time-series data to InfluxDB, and are configured via JSON/INI with hot-reload.

## What Is an Agent?

An agent is an independent process that monitors and/or controls one class of hardware (a BMS, an inverter, an A/C unit, an air handler, …) or performs a coordination/aggregation role. All agents share a common framework from `lib/sd`:

- **Config system** - hierarchical: compile-time defaults → JSON/INI files → MQTT updates, with hot-reload.
- **MQTT** - pub/sub on `solar/agents/<name>/<func>` topics (`Info`, `Data`, control functions).
- **InfluxDB** - optional time-series logging.
- **Embedded JavaScript** - business logic in `.js` files, with native C functions exposed via `jsfuncs.c`.

Each agent advertises an `agent_role` in its `Info` message (e.g. `battery`, `pvinverter`, `utility`), which lets coordinating agents discover and consume it.

## Agents in This Directory

| Agent | Role | Description | README |
|-------|------|-------------|--------|
| `ac` | controller | Air-conditioning / mini-split control, power-aware via `pa` | [ac/README.md](ac/README.md) |
| `ah` | utility | Air handler: fan/cool/heat control and air-temp sensing | [ah/README.md](ah/README.md) |
| `btc` | utility | Battery combiner — aggregates multiple BMS agents into one battery view | [btc/README.md](btc/README.md) |
| `jbd` | battery | JBD / Xiaoxiang BMS monitoring and control | [jbd/README.md](jbd/README.md) |
| `jk` | battery | JiKong (JK) BMS monitoring and control | [jk/README.md](jk/README.md) |
| `pa` | utility | Power allocator — grants/revokes power reservations across agents | [pa/README.md](pa/README.md) |
| `pvc` | utility | PV combiner — aggregates multiple PV inverter agents | [pvc/README.md](pvc/README.md) |
| `sb` | pvinverter | SMA Sunny Boy grid-tie PV inverter (HTTP/JSON API) | [sb/README.md](sb/README.md) |
| `sc` | utility | Signal/event utility agent (template-derived); see note re: `utils/sc` | [sc/README.md](sc/README.md) |
| `si` | Inverter | SMA Sunny Island battery inverter (CAN / SMANet) | [si/README.md](si/README.md) |
| `template` | utility | Reference skeleton; copied by `mkagent` to create new agents | [template/README.md](template/README.md) |

## Standard Agent Structure

Each agent directory follows the same layout:

1. **`main.c`** - command-line interface, configuration loading, config-property registration, main loop
2. **`driver.c`** - hardware-specific communication (CAN, serial, Bluetooth, SMANet, HTTP, GPIO, …)
3. **`jsfuncs.c`** - native C functions exposed to the embedded JavaScript runtime
4. **JavaScript files** - business logic (`init.js`, `read.js`, `write.js`, `pub.js`)
5. **`test.json`** (or `<name>test.conf`) - test configuration with virtual/local interfaces
6. **`Makefile`** - agent-specific build options (`JS`, `MQTT`, `INFLUX`, `BLUETOOTH`, `CURL`, …)

Larger agents may split functionality into additional files (`config.c`, `info.c`, `io.c`, `results.c`, etc.).

## Building

```bash
# Build all agents (from this directory)
make

# Build a single agent
make -C si

# Clean / install
make clean
make install        # installs to PREFIX (/opt/sd or ~/)
```

The top-level `Makefile` here recurses into every subdirectory. See the repository `CLAUDE.md` for `BUILDROOT` / NFS shared-source build caching across architectures.

## Creating a New Agent

```bash
./mkagent newagent "New Agent Description"
```

This copies the `template/` agent, renames its files, and substitutes identifiers and the description. Then implement your logic in `driver.c`, `jsfuncs.c`, and the `.js` files. See [template/README.md](template/README.md).

## Running / Testing

```bash
# Run an agent with a test config in verbose debug mode
./si/si -c si/test.json -v -d

# Monitor MQTT traffic
mosquitto_sub -t 'solar/agents/+/+' -v

# Inspect / change config of a running agent
sdconfig -n si -L              # list variables
sdconfig si get <param>
sdconfig si set <param> <value>
```

## Roles and Data Flow

```
   BMS agents (jbd, jk) ──Data──▶ btc (battery combiner) ─┐
                                                          ├─▶ si (inverter) ◀── battery/PV data
   PV agents   (sb)     ──Data──▶ pvc (PV combiner)     ──┘
                                                          
   pa (power allocator) ◀──reserve/release──▶ ac, ah, … (power-consuming agents)
```

All inter-agent communication is via MQTT; combiners discover sources by their advertised `agent_role`.

## See Also

- `../CLAUDE.md` - SD architecture, build system, and development patterns
- `../lib/sd/` - Core agent framework (config, MQTT, CAN, JavaScript glue)
- `../utils/sc/` - Standalone Service Controller for agent discovery/monitoring
