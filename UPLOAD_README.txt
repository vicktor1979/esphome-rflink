ESPHome RFLink v0.1.9.3 - Alecto V1 74-pulse diagnostic

Cumulative upload-ready files. Extract/copy into the repository root, preserving paths.
This package includes the v0.1.9.1 receiver backlog fix and v0.1.9.2 rolling-ID package fix.

Changed/cumulative files:
  components/rflink/rflink.cpp
  components/rflink/rflink_engine.cpp
  components/rflink/rflink_engine.h
  components/remote_receiver/remote_receiver.cpp
  components/remote_receiver/remote_receiver.h
  packages/rflink-alecto-006c.yaml

What is new in v0.1.9.3:
- Original RFLink/Plugins/Plugin_030.c is NOT modified.
- When Plugin 254 receives an unsupported 74-pulse frame, the bridge mirrors
  Plugin_030's checks and reports the exact first reject reason.
- Diagnostic examples:
    AlectoV1 candidate: ID=0074; reject=checksum got=... expected=...; plugin030=ON
    AlectoV1 candidate: ... checksum=OK; would pass Plugin_030; plugin030=OFF
- The Home Assistant "RFLink ismeretlen jel" entity shows the Alecto diagnostic
  instead of a truncated raw pulse list for 74-pulse candidates.
- All other unsupported frames retain the old raw pulse summary.

For field diagnosis enable Plugin 254 temporarily (RF debug 60 masodperc) and
ensure RFLink aktiv pluginok contains 030 while the debug capture runs.
