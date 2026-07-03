---
name: user-steve-smanet
description: Steve wrote lib/smanet and utils/siutil himself (pre-AI) — authoritative on SMA RS485 protocol; his smanet lib bypasses SMA security
metadata:
  type: user
tags: [steve, smanet, siutil]
---

Steve personally wrote (pre-AI era):
- `lib/smanet/` — his own implementation of SMA's proprietary SMANet RS485 protocol for Sunny Island/Boy, reverse-engineered. It bypasses SMA's security/auth (his words), so parameter access levels are not the reason writes fail.
- `utils/siutil/` — the SI CLI on top of it. siutil compiles the SI agent's own can.c/smanet.c/driver.c via Makefile vpath — same code, which makes it the no-downtime test harness for agent changes.

**How to apply:** Treat Steve as the authority on SMANet framing and SI parameters — don't explain basics. When something in lib/smanet or siutil looks odd, ask before assuming a bug (see [[project-smanet-offset-binary]] for a cautionary tale: the "unsigned" reads were correct all along). Related: [[project-si-writes-standby]].
