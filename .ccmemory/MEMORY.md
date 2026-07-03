# Memory Index

- [user_steve.md](user_steve.md) - Steve: SD system developer, solar/battery/HVAC management
- [reference_ac_host.md](reference_ac_host.md) - AC agent runs on "ac" host via SSH, 906MB RAM
- [reference_ac_log.md](reference_ac_log.md) - AC agent log is /opt/sd/log/ac.log on ac host (NOT journalctl)
- [project_buildroot.md](project_buildroot.md) - BUILDROOT feature for NFS local build cache per architecture
- [feedback_no_restart.md](feedback_no_restart.md) - Never restart AC service to clear errors; use sdconfig ac clear_error all
- [project_si_cluster.md](project_si_cluster.md) - SI deployment: 4x Sunny Island 6048-US 2-phase-4 cluster on solardirector, log at /opt/sd/log/si.log
- [user_steve_smanet.md](user_steve_smanet.md) - Steve wrote lib/sma SMANet library and utils/siutil himself; authoritative on SMA RS485 protocol
- [feedback_dyngrid_meaning.md](feedback_dyngrid_meaning.md) - dyngrid in agents/si throttles charge amps to cap grid import; does NOT discharge battery to offset loads
- [project_si_topology.md](project_si_topology.md) - AC-coupled site: 4x SI 6048-US + 3x Sunny Boy (sb1/sb2/sb3) combined at pvcombiner in InfluxDB
- [project_si_writes_standby.md](project_si_writes_standby.md) - Some SI params (e.g. GdRvPwr) only writable in standby mode; CMD_SET_DATA times out silently if cluster running
- [reference_sdconfig_no_jsexec.md](reference_sdconfig_no_jsexec.md) - sdconfig calls registered funcs only; no jsexec; remote JS exec disabled as security hole; opts are colon key=value
