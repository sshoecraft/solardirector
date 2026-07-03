---
name: reference-si-can-docs
description: SI CAN cluster protocol reverse-engineering notes live on solardirector home dir
metadata: 
  node_type: memory
  type: reference
  originSessionId: fe056e56-968e-4b78-bf56-39f493a051c3
---

Steve's CAN reverse-engineering notes for the 2x2 SI multicluster live on solardirector, dated 2025-10-25:

- `~/CAN_CLUSTER_FINDINGS.md` — summary of findings, CAN ID map, test decoder results
- `~/CAN_CLUSTER_PROTOCOL.md` — detailed protocol doc, per-inverter CAN IDs, encoding patterns, TODO list
- `~/can_capture_10k.txt` — 10,000-frame capture used for analysis
- `~/sunny_island_can_map.txt` — initial ID map
- `~/test_cluster_decoder.c` — partial decoder for Cluster 1

**How to apply:** Read these before proposing changes to `agents/si/can.c` frame filtering or decoding. The 2Phase4 mismatch story is documented; don't rederive. See [[project_si_can_2phase4_missing]] for the summary.
