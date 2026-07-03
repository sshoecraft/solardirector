---
name: reference-ac-log
description: "AC agent log location on the \"ac\" host — check here for runtime diagnostics, NOT journalctl"
metadata: 
  node_type: memory
  type: reference
  originSessionId: d4e82aa8-a782-431c-9a1c-968c9c0f84ab
---

The AC agent's runtime log is at `/opt/sd/log/ac.log` on the [[reference-ac-host|ac host]].

**Why:** SD agents write to their own log files under `/opt/sd/log/<agent>.log`, not journald. `journalctl -u ac.service` will be empty/useless.

**How to apply:** When user reports an AC issue and asks to check the log, run:
```
ssh ac "tail -200 /opt/sd/log/ac.log"
```
or grep it for relevant unit/fan/pump names. Same pattern likely applies for other SD agents (si, jbd, jk, etc.) — check `/opt/sd/log/<agent>.log` first.
