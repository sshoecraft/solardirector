## feedback
- [steve-prefers-direct-action](steve-prefers-direct-action.md) — Steve wants direct investigation/verification over theorizing, and obvious fixes done — not offered as multiple-choice menus

## project
- [ac-direct-bothpumps-rework](ac-direct-bothpumps-rework.md) — AC direct both-pumps rework DEPLOYED+verified live 2026-07-02; fan-pump ref via dg.direct_fan_pump_ref; pub.js valve1/2=Direct/Storage inline from dg…
- [ac-direct-init-enabled-bug](ac-direct-init-enabled-bug.md) — Fixed: direct.js _direct_init hardcoded dg.enabled=true, re-enabling direct mode on every load despite saved enabled:false
- [ac-direct-valve-rework](ac-direct-valve-rework.md) — AC direct-mode valves rewired to two-relay (power+select) 24V model; new config valve{1,2}_{power,select}+valve_time; deploy pending 2026-06-10
- [home-warden-monitor-design](home-warden-monitor-design.md) — Design direction for a persistent AI that monitors/heals house control systems, modeled on /src/aitrader; three-layer docs + local-disk invariant

## reference
- [ac-deploys-from-opt-sd](ac-deploys-from-opt-sd.md) — ac agent loads JS from /opt/sd/lib/agents/ac/ (root-owned, sudo to deploy); hot-reload updates inline logic only, NOT sdconfig-callable funcs
- [ac-direct-topology](ac-direct-topology.md) — AC direct mode = closed loop between AC BPHX and air handler, 4 three-way valves on 2 relay sets, BOTH pumps run in series
- [ac-unit-reserve-required](ac-unit-reserve-required.md) — AC units need reserve set (ac1=6500W); reserve=0 orphaned PA reservations → revoke storm. Config fixed + unit_revoke now acks PA on reserve=0
- [agent-loop-cadence](agent-loop-cadence.md) — SD agent loop: run.js every 1s, read.js/write.js + direct_main every interval secs; timed work belongs in run.js
- [pythonuserbase-noninteractive-shells](pythonuserbase-noninteractive-shells.md) — ~/.local python tools (ccmemory, ccloop, ccusage-statusline) need PYTHONUSERBASE=$HOME/.local; set it in ~/.zshenv not ~/.zshrc
- [sd-config-funcs-frozen-at-init](sd-config-funcs-frozen-at-init.md) — config.add_funcs JS funcs were frozen at init (captured+rooted); FIXED in source via late-binding-by-name in js_config_call_func - pending build/depl…
