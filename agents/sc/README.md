# SC Agent - Signal / Event Utility Agent

The `sc` agent is a lightweight utility agent derived from the `template` agent. Its distinguishing capability is a JavaScript `signal()` function that emits agent events, allowing JavaScript logic (or other agents via MQTT) to raise named module/action events into the SD event system.

**Agent role:** `SOLARD_ROLE_UTILITY`

> **Note:** This is the in-process **`sc` agent**. It is distinct from the standalone **Service Controller** utility in `utils/sc/`, which is a separate program for discovering and monitoring agents.

## Capabilities

- Standard agent lifecycle (config, MQTT, InfluxDB, embedded JavaScript).
- Includes the `pa_client.js` library, so it can participate in power allocation (`pa`) as a client.
- Exposes a single native JavaScript function:

  ```javascript
  sc.signal(module, action);
  ```

  This calls `agent_event(ap, module, action)`, raising an event into the agent's event subsystem. Both arguments are strings.

Beyond `signal()`, the agent is essentially the template skeleton — most of its behavior is intended to be implemented in JavaScript.

## Configuration

Standard SD INI/JSON format. The shipped `test.json` is still the template default:

```json
{
    "template": {
        "script_dir": "."
    }
}
```

Set `name`, `mqtt_uri`, `interval`, etc. as needed for deployment.

## Files

- `main.c` - Agent entry point, config, info JSON (`agent_role` = utility), main loop
- `driver.c` - Driver interface (`sc_driver`)
- `jsfuncs.c` - JavaScript bindings, including `js_sc_signal` → `agent_event`
- `init.js` - JavaScript initialization (`init_main`); wires SD lib + `pa_client`, sets publish topic
- `sc.h` - Session struct and prototypes
- `test.json` - Test configuration

## Notes

- Built with `JS=yes`, `MQTT=yes`, `INFLUX=yes`.
- Scratch/working files may appear in this directory during development (`mcp-shell.log`, `wtf_is_this`); they are not part of the build.

## See Also

- `agents/template/README.md` - The skeleton this agent is derived from
- `utils/sc/` - The standalone Service Controller utility (different component, same short name)
- `lib/sd/pa_client.js` - Power-allocation client library
- `../../CLAUDE.md` - Overall system architecture
