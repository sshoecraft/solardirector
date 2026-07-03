---
name: buildroot_local_build
description: BUILDROOT feature for NFS-shared source - redirects build artifacts to local disk per architecture
type: project
---

Added BUILDROOT support to the SD build system (2026-03-12) to solve the NFS multi-architecture problem.

**Why:** Source tree is NFS-shared between Mac M4 (arm64), x86 Linux, RPi4 (aarch64), RPi3 (armv7l). Previously required full clean+rebuild when switching machines because object files were in the source tree.

**How to apply:** Set `BUILDROOT=$(HOME)/.build/sd` in `~/.sd_make_opts` on each machine. Build artifacts go to `~/.sd/build/<target>-<arch>/` (e.g., `macos-arm64/`, `linux-aarch64/`). Without BUILDROOT, behavior is unchanged (in-tree build). Key files modified: Makefile.sys, Makefile.sd, lib/js/Makefile, lib/{sd,smanet,wpi}/Makefile.
