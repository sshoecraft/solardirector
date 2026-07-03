---
name: ac_host_info
description: The "ac" host is the embedded target running the AC agent - 906MB RAM, accessible via ssh ac
type: reference
---

The AC agent runs on a host called "ac" (accessible via `ssh ac`). It has only 906MB RAM + 2.5GB swap. Too small for Claude Code - run CC locally and SSH in. Source at `/opt/sd/` on the target, NFS-mounted from Mac at `~/src/sd/agents/ac`. Steve plans to replace it with a Raspberry Pi 4 (more RAM). Services disabled to free memory: ModemManager, postfix-mta-sts-resolver. Extra 2GB swap file added at /swapfile.
