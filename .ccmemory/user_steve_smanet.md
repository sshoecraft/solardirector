---
name: user-steve-smanet
description: "Steve wrote the SMANet library and siutil from scratch (pre-AI) — deep firsthand knowledge of SMA's RS485 protocol"
metadata: 
  node_type: memory
  type: user
  originSessionId: fe056e56-968e-4b78-bf56-39f493a051c3
---

Steve personally wrote (pre-AI era):
- `lib/smanet/` — the SMANet library, his own implementation of SMA's RS485 protocol for Sunny Island / Sunny Boy
- `utils/siutil/` — the SI command-line utility that uses it

**How to apply:** When discussing SMA protocol, SI parameter names (GdManStr, GdRvPwr, etc.), or RS485 framing in this repo, treat Steve as the authoritative source — he reverse-engineered the protocol himself. Don't explain SMANet basics to him. When something in `lib/smanet/` or `utils/siutil/` looks wrong or odd, ask before assuming it's a bug; it's likely intentional. Related: [[project_si_cluster]].
