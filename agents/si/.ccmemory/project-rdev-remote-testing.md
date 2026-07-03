---
name: project-rdev-remote-testing
description: Remote SI agent testing via rdevserver works (readonly + sidev_full.json from Mac); gotchas: SOLARD_TOPTS_LEN=64 truncation, old binary ID restrictio…
metadata:
  type: project
tags: [rdev, testing, si, gotchas]
---

Verified July 2026: the SI agent can be tested from the Mac against the PRODUCTION cluster with zero risk using rdevserver on solardirector + `agents/si/sidev_full.json` (readonly: true, can_info: "rdev,192.168.1.168,can0", input_current_source: calculated). Run: `~/.build/sd/agents/si/si -c sidev_full.json -d 2`. readonly blocks all CAN/SMANet writes (si_can_write returns early with banner; also the config DEFAULT is readonly=yes — production must explicitly disable). **Steve: an un-readonly agent would fight the production agent and take his power down. NEVER run without readonly.** Also never run the agent binary with guessed flags (unknown flag = full agent start).

Gotchas discovered:
1. **SOLARD_TOPTS_LEN = 64** (lib/sd/common.h): device topts strings silently truncate. Long CAN filter specs in rdevserver.conf must use compact ranges: `topts=500000,0x180-0x230+0x300-0x30F,no` (34 chars) not 24 individual ids (~190 chars).
2. **Installed /opt/sd/bin/rdevserver (2026-01-18) restricts CAN ids to 0x300-0x30F** — built when the RDEV_SI_FRAMES restriction code existed (flag still in utils/rdevserver/Makefile but no code uses it now). Requests outside the range WEDGE the connection (child blocks forever, all subsequent reads -1). Fresh build from current source serves any id.
3. **can_start_buffer bitmap alloc bug FIXED** (lib/sd/can.c ~1370): was `calloc(num/32, 1)` — uint32-count-as-bytes, heap overflow for >32-id buffered ranges. Now `((num+31)/32)*sizeof(uint32_t)` bytes.
4. rdevserver forks per client but the CAN buffer thread lives in the parent — buffered mode (`yes`) serves fork-frozen snapshots to children. Use `no` (direct reads) for correctness.
5. solardirector: eth0 (wired) = 192.168.1.168, wlan0 = .164, mDNS resolves to wifi — use .168 for streaming. Mac = .193.

State as of 2026-07-02: rdevserver systemd service STOPPED; fresh test build running manually (`/home/steve/.build/sd/utils/rdevserver/rdevserver -c /opt/sd/etc/rdevserver.conf -l /tmp/rdevserver_test.log`); conf modified (filter widened + buffer=no), backup at /opt/sd/etc/rdevserver.conf.backup. Steve to decide: install new rdevserver binary or revert. Related: [[project-si-can-fix-205]].
