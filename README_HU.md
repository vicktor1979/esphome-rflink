# RFLink v0.1.5-rxgate2 – vétel gyorsított főciklus nélkül

Kiegészítés a már telepített v0.1.5 + rxgate1 rendszerhez, ESP8266 / nodemcuv2 célra.
Nem teljes RFLink-repó. Nem módosítja az RFLink/Plugins könyvtárat vagy az RFLink-motort.

## Miért készült?

A felhasználó 2026-09-23-i naplója kikapcsolt vétellel 08:57:15–09:03:35 között
(6 perc 20 másodperc) folyamatos Wi-Fi-kapcsolatot és API-állapotfeliratkozást mutat.
Az előző próba bekapcsolt vételnél Beacon Timeout hibákat tartalmazott. Ez még nem
bizonyítja, hogy a gyorsított főciklus a hibás: a kapcsoló eddig egyszerre állította
le a megszakítást, a gyorsított főciklust és a dekódolást.

Az rxgate2 próbában a vétel alatt a GPIO-megszakítás és mind a 48 dekóder megmarad,
csak a vevő saját HighFrequencyLoopRequester-kérését kapcsoljuk ki. A normál
ESPHome-főciklus továbbra is kiolvassa a megszakításban gyűjtött időzítéseket.
Nem teszünk várakozást a megszakításkezelőbe, és nem tiltjuk le globálisan az IRQ-kat.
Az ISR, az impulzusszűrő és a keret-összeállítási algoritmus változatlan.

Ez diagnosztikai változat, nem bizonyított hardveres Wi-Fi-javítás.

## Telepítés

1. Mentsd el a jelenlegi rxgate1 állapotot és a készülék YAML-ját.
2. A GitHub-repóban azonos útvonalon cseréld ezt a három fájlt:
   - components/remote_receiver/__init__.py
   - components/remote_receiver/remote_receiver.h
   - components/remote_receiver/remote_receiver.cpp
3. A components/rflink/, packages/ és RFLink/ könyvtárak változatlanul maradjanak.
4. Használd a mellékelt rflink-rxgate2-proba.yaml-t. Megőrzi a korábbi titokneveket,
   repócímeket, az API-first kaput, az összes RX-plugint, adatmezőt és gesztust.
   A saját secrets.yaml fájlt ne cseréld le, és ne töltsd GitHubra.
5. Frissítsd a külső forrást (a mintában refresh: 0s), ellenőrizd a konfigurációt,
   Clean build files után fordíts és tölts fel. Az új kód feltöltés előtt nem hat
   a jelenlegi firmware-re. A frissítés idejére a mostani HA-kapcsolót hagyd OFF-on.

A lényeges új opció a remote_receiver alatt:

```yaml
remote_receiver:
  id: rf_receiver
  capture_enabled: false
  high_frequency: false
  # Az eddigi pin, filter, idle, buffer_size és on_raw változatlan.
```

A `high_frequency` ennek a külső komponensnek az új opciója, nem a gyári
remote_receiver általános beállítása. Új komponensfájlok nélkül érvénytelen lehet.
Az alapértéke true a korábbi konfigurációkkal való kompatibilitás miatt; a próba
YAML kifejezetten false-ra állítja. A capture_enabled: false csak az induláskori
állapot: API-feliratkozás + 5 s stabil kapcsolat után a korábbi kapu engedélyezi.

A mintában nincs MQTT. Az eredeti pluginok továbbra is befordulnak (`rx_plugins: all`).
A 037-es örökölt formátumfigyelmeztetést nem némítjuk el.

## Ellenőrzés

Induláskor:

```text
Remote Receiver rxgate2 (ESP8266 / based on 2026.9.0):
  Capture enabled: NO
  High frequency configured: NO
```

HA-feliratkozás és 5 s után:

```text
RX gate: capture=ON; irq=ON; fast_loop=OFF
```

A diagnosztika új mezői:
- fast_loop: kizárólag ennek a vevőnek az aktív gyorsított-főciklus-kérése.
- rx_loop_calls: a bekapcsolt vevő loop() hívásainak összesített száma.
- overflow_reports: a loop() által észlelt túlcsordulásjelzések összesített száma;
  NEM az elvesztett élek vagy csomagok pontos darabszáma.

Az irq_total és frames számlálónak bekapcsolt vételnél tovább kell emelkednie.
A `fast_loop=OFF` tehát ebben a próbában nem a rádió kikapcsolását jelenti.
Más komponens saját gyorsított-főciklus-kérését ez nem kapcsolja ki; egyéb ilyen
komponens hozzáadásakor ezt figyelembe kell venni.

Hagyd legalább 5 percig bekapcsolva a vételt gombnyomás nélkül; utána rövid, dupla,
tripla és hosszú gombnyomás következhet. A kapcsoló kapcsolgatása helyett most a
bekapcsolt vétel melletti folytonos hálózatot vizsgáljuk. A mérési idő javaslat,
nem valamely garantált stabilitási határ.

A ritkább pufferkiolvasás növelheti a kézbesítési késleltetést és nagy forgalomnál
adatvesztést okozhat. A valós hardveren a túlcsordulásokat és a gombvételt is
ellenőrizni kell. A puffer értékét most nem növeljük, a szűrést nem szigorítjuk.

## Visszaállítás

A három korábbi rxgate1 fájl és a korábbi készülék-YAML visszaállításával a régi
állapot reprodukálható. Az rxgate2 fájlokkal a `high_frequency: true` szintén
visszakapcsolja az eredeti gyorsított-kérést; ezt csak külön összehasonlító próbaként
használd, ne keverd más beállításmódosítással.

## Tesztek és korlátok

A TEST_RESULTS.txt és TEST_LOGS tartalmazza az ebben a körben ténylegesen lefuttatott
ellenőrzést. Host GNU C++20 + ASan/UBSan fordítás és szimulált GPIO-időzítés történt,
a valódi módosított vevővel és mind a 48 eredeti RX-pluginnal. A 16 ms-os szimulált
loop() kiolvasás 20 külön EV1527-keretet felismert túlcsordulás nélkül.
Nem történt ESPHome-konfigurációvalidálás/kódgenerálás, teljes Xtensa-firmware-fordítás,
valódi Wi-Fi/API-hálózati teszt, OTA vagy rádiós hardverpróba.
A sikeres host teszt nem bizonyítja a Wi-Fi-hiba okát vagy a hardver stabilitását.

A tesztekhez egy helyi, teljes 0.1.5-ös repó szükséges az eredeti RFLink/ tartalommal:

```sh
python3 tests/rx_gate/run_tests.py --repo /path/to/esphome-rflink --out /tmp/rxgate2-results
```

Források a technikai háttérhez:
- ESPHome HighFrequencyLoopRequester: https://raw.githubusercontent.com/esphome/esphome/2026.9.0/esphome/core/helpers.h
- ESPHome főciklus: https://raw.githubusercontent.com/esphome/esphome/2026.9.0/esphome/core/application.h
- ESP8266 időzítés/hálózati együttfutás: https://arduino-esp8266.readthedocs.io/en/latest/reference.html

A csomag a korábbi licencfájlokat megőrzi.
