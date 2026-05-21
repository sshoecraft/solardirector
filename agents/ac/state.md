# AC Agent — Session State
**Saved**: 2026-04-25
**Session Goal**: Diagnose and fix the 2026-04-25 01:35 runaway-pumps incident in cycle.js, then build a JS unit-test harness covering all AC business-logic modules so this class of bug surfaces before reaching production.

## What Was Accomplished

### 1. Diagnosed the 2026-04-25 01:35 → 09:30 runaway-pumps incident
Pulled `/opt/sd/log/ac.log` from `ac` host (over SSH). Sequence was:
- **01:35:42** — `cycle_main()` fired anti-freeze cycling. All five pumps (primer, ac1, ac2, ah1, ah2) started.
- **04:01:06** — User noticed during a storm + battery-mode switch and manually disabled `units[ac1] enabled = false` and `fans[ah2] enabled = false`. ac1/ac2/ah2 pumps remained running.
- **04:01:31 → 09:30:03** — 5.5h log silence. Pumps kept running.
- **09:30:03** — User force-stopped ah2/ac1.
- **09:33:54** — User restarted ac.service.

Total runaway: ~8 hours.

### 2. Root-cause analysis of cycle.js
Three issues compounded:
- **Typo** (cycle.js:108): `pump.cycle_state = SAMPLE_STATE_RUNNING;` should have been `CYCLE_STATE_RUNNING`. Worked by coincidence — both enums equal 2 in their respective `*_init()` orderings.
- **Stop-gate bug**: original line 121 had `if (diff >= ac.cycle_duration && cycle_interval > adj)` where `adj = cycle_duration + wait_time`. When `m = ac.temp / ac.cycle_start` was small (cold), the m-scaling collapsed `cycle_interval` → 0, so `cycle_interval > adj` was false, so the stop never fired. Pumps stayed in `CYCLE_STATE_RUNNING` forever.
- **No recovery path**: when `ac.temp` rose back above `cycle_start` after the bad reading, `cycle_main()` early-returned at line 74 without checking whether any pumps were still mid-cycle. The runaway then ran until manual intervention.

The trigger was a transient bad/cold BME280 reading. Current humidity reading is 100% (real — fog, confirmed by user), so the BME isn't broken. The cycle_main logic was the failure mode.

### 3. Fixed cycle.js (`agents/ac/cycle.js`)
- Line 108: `SAMPLE_STATE_RUNNING` → `CYCLE_STATE_RUNNING` (typo fix)
- Lines 74-87: added recovery block — when `ac.temp >= ac.cycle_start`, iterate pumps and stop any still in `CYCLE_STATE_WAIT_PUMP` or `CYCLE_STATE_RUNNING`, reset to `CYCLE_STATE_STOPPED`, log "Cycle exit: stopping pump %s".
- Lines 96-99: added `continuous = (cycle_interval < ac.cycle_duration * 2)` mode. When the m-scaled off-time would be less than 2× on-time, skip the stop entirely so pumps run continuously rather than churn (start/stop every 3 min on a compressor is brutal).
- Line 135: stop check now `if (!continuous && diff >= ac.cycle_duration)` — clean stop after duration unless we're in continuous mode.

Net behavior with defaults (cycle_start=30, cycle_interval=1800, cycle_duration=180):
- temp ≥ 30°F: any cycling pump stopped on next cycle_main pass
- 30°F → ~6°F: normal cycle (180s on, scaled off-time)
- < ~6°F: continuous run, no churn
- INVALID_TEMP: no action

### 4. Fixed pump.js missing-pump guards (`agents/ac/pump.js`)
Latent bug surfaced by tests: `pump_stop` and `pump_force_stop` both dereferenced `pumps[name].state` / `pumps[name].primer` before checking existence — they threw `TypeError` on a missing pump instead of returning 1 with `config.errmsg`. Added guards at the top of both functions: check `pumps[name]`, set errmsg, return 1 if missing.

### 5. Built JS unit-test harness for the AC agent
**Files created:**
- `agents/ac/test/harness.js` — agent-runtime stubs, controllable `time()` (`harness.now` + `harness.advance(s)`), hardware fakes (`digitalRead/Write`, `pinMode`, `readbme` driven by `harness.bme`, `can_get_sensor` from `harness.can`), spies for cross-module functions (`pump_start/stop/force_stop`, `fan_*`, `unit_*`, `direct_on/off`), object factories (`harness.add_pump/fan/unit`), runner (`harness.run()` discovers `test_*` globals, runs each in try/catch, exits via `abort()` because sdjs `exit()` doesn't propagate codes).
- `agents/ac/test/assert.js` — `assert.eq/neq/truthy/falsy/near/called/not_called/call_count/called_with`. Throws on failure.
- `agents/ac/test/test_cycle.js` — 15 tests (Phase 1)
- `agents/ac/test/test_pump.js` — 14 tests (Phase 2)
- `agents/ac/test/test_fan.js` — 16 tests (Phase 3)
- `agents/ac/test/test_unit.js` — 17 tests (Phase 4)
- `agents/ac/test/test_mode.js` — 13 tests (Phase 5)
- `agents/ac/test/test_charge.js` — 16 tests (Phase 6)
- `agents/ac/test/test_direct.js` — 23 tests (Phase 7) — covers entry validation, valve helpers, query helpers, direct_off no-op. NOT the full 16-state lifecycle.
- `agents/ac/test/test_sample.js` — 13 tests (Phase 7)

**Total: 127 tests across 8 modules. All passing.**

**Build integration:** added `test` target to `agents/ac/Makefile` — runs every `test/test_*.js` under `sdjs -U512K`, exits non-zero on any failure. Run with `cd agents/ac && make test`.

### 6. Verified the harness catches regressions
Reverted the cycle.js recovery-stop block, ran `make test`, confirmed the 3 relevant regression tests fail with useful messages (`test_temp_recovery_stops_running_pump`, `test_temp_recovery_stops_wait_pump`, `test_recovery_with_multiple_pumps`). Restored the fix; all green again.

## Current State of the Code

- **All 127 tests pass.** `cd agents/ac && make test` exits 0.
- **No commits made.** All changes are uncommitted on `main` branch.
- **Modified files:**
  - `agents/ac/cycle.js` — typo fix + recovery-stop + continuous-mode threshold
  - `agents/ac/pump.js` — missing-pump guards in pump_stop / pump_force_stop
  - `agents/ac/Makefile` — added `test` target
- **New files:**
  - `agents/ac/test/harness.js`
  - `agents/ac/test/assert.js`
  - `agents/ac/test/test_cycle.js` through `test_sample.js` (8 files)
- **AC service NOT restarted** — per standing rule, never restart agent services without user direction. cycle.js fix won't take effect on the live agent until next restart.

## What We Were Actively Working On

User asked for a recommendation on whether to keep deepening test coverage (function-by-function trace + multi-function integration tests) or stop. I gave the recommendation but no further code was written.

## Known Issues / Bugs

- **Live agent still runs old cycle.js.** Service was restarted at 09:33:54 today but with the old code. The fix is uncommitted and the agent must be restarted again after install for the fix to apply. **Tell the user before they restart.**
- **Tests cover 8 modules; do NOT cover** `run.js` (730 lines — the main loop driver), `pub.js`, `read.js`, `write.js`, `start.js`, `init.js`, `errors.js`, or the full direct.js 16-state lifecycle. The same *shape* of bug as today's (a state with no exit path under some condition) could exist in any of these. run.js is the highest-leverage gap.
- **Coverage caveat:** all tests reflect my reading of each module. A bug that fits my misunderstanding of intent will be enshrined alongside the code. Treat the 127-test floor as a useful baseline, not a proof of correctness.

## Recommendations (In Priority Order)

### Phase 8 — Test run.js's main loop (HIGHEST LEVERAGE)
run.js is the conductor. Every state machine tested in isolation in Phases 1-7 only matters when run.js drives transitions forward. Today's 8-hour runaway was exactly this shape: a state that wasn't triggerable by normal flow but became reachable, and run.js never knocked the pumps loose.

Cover the start sequences end-to-end (~30 tests):
- pump start chain: STOPPED → RESERVE → START_PRIMER → WAIT_PRIMER → START → WAIT_PUMP → FLOW → WARMUP → RUNNING → COOLDOWN → STOPPED. With and without primer chain, with/without flow_sensor.
- unit start lifecycle: STOPPED → START_PUMP → WAIT_PUMP → RESERVE → START → RUNNING → RELEASE → STOPPED. Including PA reservation handshake.
- fan start with direct handoff: normal start, mode-mismatch direct start, water-depleted direct start, direct disabled fallback.
- The run.js flow-check logic (recently fixed in 2026-03-12 session) — assert mode is taken from `unit.mode` not `ac.mode`.

### Phase 9 — Runtime watchdog instrumentation (DURABLE INSURANCE)
Cheaper than exhaustive tests for the long tail. Add `agents/ac/watchdog.js` called from `read.js` once a minute. Logs (or auto-clears) invariant violations:
- any pump in PUMP_STATE_RUNNING > 4h with no upstream demand (no fan/unit refs, no cycle, no sample) — would have caught today's runaway at the 5-minute mark, not 8 hours
- any unit in UNIT_STATE_RESERVE > 5min without a PA grant arriving
- any cycle_state non-STOPPED while ac.temp >= cycle_start (defense-in-depth alongside the recovery-stop fix)
- any refcount > sane max (e.g. 10) — likely a leak
- any direct group in DIRECT_STATE_ERROR for > 1h

This catches "didn't think to test" bugs in production before they cause hardware/comfort impact.

### What NOT to do
- Don't try to test every cross-module pair (8C2 = 28 combinations, most cold paths). Cover specific real-world *paths* instead: charge_main → unit_start → run.js → unit RUNNING → charge_main RUNNING; direct_on full lifecycle.
- Don't trace every function exhaustively. Diminishing returns; the cold paths that haven't burned us in production aren't where the next bug is.

### Quick wins worth doing alongside Phase 8
- Mine `/opt/sd/log/ac.log` (19MB) for "Unknown" state strings, multi-hour gaps without state transitions, "force" stops that shouldn't have been needed. Each anomaly = candidate test or watchdog rule.
- File a sdjs upstream issue: `exit(N)` doesn't propagate to shell exit codes; `abort(N)` does. Confusing for anyone writing CLI scripts. The harness uses `abort()` as workaround.

## Key Decisions Made This Session

- **Continuous-mode crossover at off-time < 2× on-time**, not 1×. With defaults this kicks in at temp < ~6°F. User explicitly worried about 3-min start/stop churn on compressors at freezing temps; 2× threshold avoids the worst case. Tunable via `cycle_duration` / `cycle_interval` config.
- **Hardcoded enum values in harness.js** (PUMP_STATE_*, FAN_STATE_*, UNIT_STATE_*, AC_MODE_*, CHARGE_STATE_*, SAMPLE_STATE_*) matching each module's `*_init()` ordering. If a test includes the source module, init will reassign these to identical values. Avoids dragging in jconfig/common dependencies for tests that don't need them.
- **Spies vs real functions resolution by include order.** Harness installs spies in `harness.init()`; if a test then `include`s the production module, the module's own function definitions overwrite the spies. Means cycle.js tests get spy `pump_start`, pump.js tests get real `pump_start` — automatic, no flag needed.
- **Latent bug surfacing → fix immediately.** When tests caught the missing-pump TypeError in pump.js, fixed in code rather than leaving as documented-failing tests. Tests flipped to assert correct behavior.
- **Skipped Plan-agent in plan-mode workflow.** Exploration agents had already validated structural decisions; running another agent just to validate would have added cycles without insight.

## Important Context the Next Session Needs

- **AC agent runs on `ac` host via SSH.** 906MB RAM. Source NFS-mounted from Mac at `~/src/sd/agents/ac` ↔ `/opt/sd/` on target. Run Claude Code locally, SSH for log/sdconfig commands.
- **Logs at `/opt/sd/log/ac.log`** (NOT journald — journald only shows systemd lifecycle events). Tail with `ssh ac tail -f /opt/sd/log/ac.log`.
- **NEVER restart the ac.service yourself.** Always ask the user. Real hardware (compressors, pumps, valves). To clear errors: `ssh ac /opt/sd/bin/sdconfig ac clear_error all` then `ssh ac /opt/sd/bin/sdconfig ac enable_unit ac1`.
- **sdjs runtime quirks** — discovered this session:
  - `time()`, `log_info`, `printf`, `sprintf`, `delay`, `dirname`, `include`, `script_name`, `window`, `exit` are all available standalone.
  - `readbme`, `digitalRead`, `digitalWrite`, `dumpobj`, `config`, `agent`, `event` are NOT available standalone — harness stubs them.
  - C-bound globals can be reassigned by JS (verified — `time = function(){...}` works).
  - `include()` resolves paths against CWD, not the calling script's directory. Tests use `dirname(script_name)+"/..."` for sibling/parent paths.
  - `exit(N)` always returns 0 to the shell; `abort(N)` propagates. Harness runner uses `abort()`.
- **Running tests:** `cd agents/ac && make test`. `make test` exits non-zero on failure; `sdjs path/to/test_*.js` runs one file directly.
- **Plan file:** `/Users/steve/.claude/plans/memoized-zooming-fountain.md` has the original Phase-1 plan. Phases 2-7 were added inline during execution after user said "do all 7 phases."
- **User preferences:** terse and direct; no platitudes; never restore from git unless explicitly asked; no underscore prefixes/suffixes on variable names; no Claude attribution in commits/PRs; document module changes in `docs/<module>.md` when applicable.
- **The Phase 7 direct.js tests intentionally don't cover the 16-state lifecycle** — only entry validation, valve helpers, queries, and direct_off short-circuits. The full lifecycle is integration-test territory, better suited for Phase 8 or a dedicated direct integration test.
