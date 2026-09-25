ESPHome RFLink v0.1.9.1 - Alecto/backlog javitas

EZ NEM PATCH.
A ZIP a tenylegesen modositott, feltoltesre kesz allomanyokat tartalmazza.

Hasznalat:
1. Csomagold ki a GitHub RFLink repository gyokerbe.
2. Engedd a harom fajl felulirasat:
   components/remote_receiver/remote_receiver.cpp
   components/remote_receiver/remote_receiver.h
   components/rflink/rflink.cpp
3. Az RFLink/Plugins konyvtarat NE modositsd.
4. ESPHome-ban forditsd ujra es toltsd fel a firmware-t.

Fontos:
- remote_receiver high_frequency: false maradjon.
- A YAML interval-alapu Wi-Fi/API/capture/decode kapu tovabbra sem kell;
  ezt az RFLink komponens auto_start logikaja kezeli.

Mi valtozott az elozo rx-backlog-fix verzihoz kepest:
- kivettem a tobb RF frame feldolgozasat egyetlen receiver loop() alatt;
- ujra maximum 1 RF frame kerul a legacy RFLink decoderhez egy loop() alatt;
- backlog eseten rovid adaptiv scheduler-gyorsitas indul, nem multi-frame decode;
- boost legfeljebb 8 ms, utana legalabb 20 ms cooldown;
- buffer overflow es 2.5 s beragadt resz-frame self-heal megmaradt;
- diagnosztika: backlog_boosts, backlog_max.
