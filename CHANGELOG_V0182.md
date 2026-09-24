# v0.1.8.2

- Plugin 254 találatból külön HA diagnosztika:
  - `RFLink ismeretlen jel` text sensor
  - `RFLink ismeretlen jel impulzusszám` sensor
- 240 karakterre korlátozott impulzus-összefoglaló, explicit `,...` csonkolásjelzéssel.
- Pontos impulzusszám külön numerikus entitásban.
- A 254 eredeti 24 impulzusos minimuma visszaállítva debug-only útvonalon; a normál legacy pluginok 36 impulzusos minimuma változatlan.
- A 254 `NAME=DEBUG` üzenete, soros debugja, runtime kapcsolója és restore működése megmarad.
- Változatlan: rxgate2, holdfix1, rflink_remote, eredeti `RFLink/Plugins`, v0.1.7 extension dekóderek.
