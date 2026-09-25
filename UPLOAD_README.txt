RFLink ESPHome v0.1.9.2 - Alecto rolling-ID fix

Copy the contents of this ZIP over the repository root.
It contains the current v0.1.9.1 receiver/backlog fixes plus the updated
packages/rflink-alecto-006c.yaml.

Why the Alecto package changed:
RFLink Plugin_030 treats the Alecto V1 sensor ID as a rolling code. A battery
replacement/reset can change the previous 006C ID. The old package silently
ignored every Alecto V1 frame whose ID was not exactly 006C.

The updated package:
- accepts any decoded NAME="Alecto V1" ID;
- preserves the existing Alecto 006C temperature/battery entity names;
- adds humidity;
- adds "Alecto V1 aktuális RF ID" diagnostic text sensor;
- logs only when the received Alecto RF ID changes;
- does not modify RFLink/Plugins/Plugin_030.c.

IMPORTANT: Runtime Plugin 030 still has to be ON.
If the RFLink active plugins entity does not contain 030, use the existing
"RF pluginok alaphelyzet" button or turn on the Plugin 030 switch.
