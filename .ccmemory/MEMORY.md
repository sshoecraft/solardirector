## user
- [steve_profile](user_steve.md) — Steve is the developer of the SolarDirector system - works on C agents with embedded JS for solar/battery/HVAC management
- [user-steve-smanet](user_steve_smanet.md) — Steve wrote the SMANet library and siutil from scratch (pre-AI) — deep firsthand knowledge of SMA's RS485 protocol

## feedback
- [feedback-dyngrid-meaning](feedback_dyngrid_meaning.md) — dyngrid in agents/si throttles charge amps to cap grid import — it does NOT add battery discharge to support loads
- [never_restart_services](feedback_no_restart.md) — NEVER restart any SD agent service - always ask the user to do it. Use sdconfig for error clearing.

## project
- [buildroot_local_build](project_buildroot.md) — BUILDROOT feature for NFS-shared source - redirects build artifacts to local disk per architecture
- [project-si-can-2phase4-missing](project_si_can_2phase4_missing.md) — 2Phase4 is ONE production cluster (master+3 slaves) powering the house; CAN aggregate 0x300-0x30A incomplete; can.c +t/3 fudge makes input_power unre…
- [project-si-cluster](project_si_cluster.md) — Sunny Island cluster topology — 4x SI 6048-US in 2-phase-4 config on solardirector host
- [project-si-topology](project_si_topology.md) — Steve's site is AC-coupled — 4x SI 6048-US cluster + 3x Sunny Boy PV inverters (sb1/sb2/sb3) combined at pvcombiner
- [project-si-writes-standby](project_si_writes_standby.md) — Some SI parameters (e.g. GdRvPwr) require the cluster to be in standby to accept writes — siutil shows protocol timeout if attempted while running

## reference
- [ac_host_info](reference_ac_host.md) — The "ac" host is the embedded target running the AC agent - 906MB RAM, accessible via ssh ac
- [reference-ac-log](reference_ac_log.md) — AC agent log location on the "ac" host — check here for runtime diagnostics, NOT journalctl
- [reference-si-can-docs](reference_si_can_docs.md) — SI CAN cluster protocol reverse-engineering notes live on solardirector home dir
- [reference_sdconfig_no_jsexec](reference_sdconfig_no_jsexec.md) — sdconfig dispatches to registered agent functions; there is no jsexec and remote JS exec is intentionally disabled
