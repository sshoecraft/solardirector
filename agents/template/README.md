# Template Agent - Reference Skeleton for New Agents

The `template` agent is a minimal, working reference agent used as the starting point for creating new SD agents. It is not deployed itself; instead it is copied and renamed by the `mkagent` script to bootstrap a new agent with the standard structure, build configuration, and JavaScript integration already wired up.

**Agent role:** `SOLARD_ROLE_UTILITY`

## Creating a New Agent

Use the `mkagent` helper in the `agents/` directory:

```bash
cd agents
./mkagent myagent "My Agent Description"
```

This:
1. Copies `template/` to `myagent/`.
2. Renames `template.*` files to `myagent.*`.
3. Substitutes identifiers in all text files: `template_` → `myagent_`, `TEMPLATE_` → `MYAGENT_`, `"template"` → `"myagent"`, `template.h` → `myagent.h`, and the generic `template` token.
4. Substitutes `+DESC+` in the Makefile with the provided description.
5. Enables the program build target in the Makefile.

After generation, add your hardware/business logic to `driver.c`, expose any C functions to JavaScript in `jsfuncs.c`, and put control logic in the `.js` files.

## What the Template Provides

- Standard agent `main.c` with `agent_init`, config-property registration, info JSON (`agent_name`, `agent_role`, `agent_description`, `agent_version`, `agent_author`), and the main loop.
- A `driver.c` stub implementing the `solard_driver_t` interface.
- A `jsfuncs.c` stub for exposing native functions to the embedded SpiderMonkey runtime.
- An `init.js` that wires up the SD JavaScript library (`init.js`, `pa_client.js`), registers properties, sets up the MQTT publish topic, and enables message purging/republish.
- A `test.json` test configuration.
- A Makefile configured with `JS=yes`, `MQTT=yes`, `INFLUX=yes`.

## Files

- `main.c` - Agent entry point, config, info JSON, main loop
- `driver.c` - Driver interface stub (`template_driver`)
- `jsfuncs.c` - JavaScript binding stub (`jsinit`)
- `init.js` - JavaScript initialization (`init_main`)
- `template.h` - Session struct and prototypes
- `test.json` - Test configuration

## Standard Agent Structure

Every SD agent follows this layout (see the main `CLAUDE.md`):

1. `main.c` - CLI, configuration loading, main loop
2. `driver.c` - Hardware-specific communication (CAN, serial, SMANet, HTTP, etc.)
3. `jsfuncs.c` - C functions exposed to JavaScript
4. JavaScript files - Business logic (`init.js`, `read.js`, `write.js`, `pub.js`)
5. `test.json` - Test configuration with virtual interfaces
6. `Makefile` - Agent-specific build configuration

## See Also

- `mkagent` - Agent generation script (in this directory)
- `agents/sc/README.md` - A minimal utility agent derived from this template
- `agents/pa/README.md`, `agents/ac/README.md` - Fuller agent examples
- `../../CLAUDE.md` - Overall system architecture and agent development pattern
