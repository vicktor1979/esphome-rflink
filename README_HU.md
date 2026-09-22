# RFLink v0.1.5 – minden RX-plugin + API + adatok + EV1527-gesztusok

## Mit tartalmaz?

Összegző frissítőcsomag a már meglévő `vicktor1979/esphome-rflink` repóhoz.
A teljes, egymáshoz illő `components/rflink/` és két szükséges `packages/` fájl benne van.
Nem kell előbb külön feltenni a v0.1.4-et. A meglévő `RFLink/` vendor mappa szükséges:
a csomag azt NEM törli, NEM módosítja, és nem tartalmaz annak helyettesítésére új pluginokat.

A készülék fő konfigurációja: `rflink-all-plugins-proba.yaml`.
Alapértéke `rx_plugins: all`, azaz az eredeti forrásban lévő mind a 48 `.c` RX-része.
A kötelező 001-es már része a 48-nak. `configured` az eredeti fejlécben engedélyezett
47-et választja, a 083-as nincs közöttük. A `[61]` és `[34, 40, 61]` továbbra is használható.
Az öt `.old` fájl nem külön, aktuális plugin. TX nincs megvalósítva.

Megmarad: GPIO5, 100us szűrés, 5ms idle, 1000b puffer, titkosított API, az állapotokra
feliratkozott API-kliens utáni 5 másodperces indítás, kapcsoló, OTA alatti szüneteltetés,
a teljes RF-adatmezőcsomag és az EV1527 gesztusok. Nincs MQTT és nincs nyers dump.
A gesztuskezelés továbbra is csak a két beállított EV1527-jelmintához van kötve.
Az összes RX-plugin kiválasztása nem tesz automatikusan gesztusképessé más protokollokat.

## Reprodukált fordítási hiba és javítás

Az eredeti `Plugin_083.c` Brel Motor / Dooya dekóderében `char *commandstring` kapja
meg négy `PSTR(...)` értékét. Az ESP8266 PSTR-makrója `const char*` eredményt ad.
Ez valódi típusütközés, nem pusztán printf-figyelmeztetés.

Az új, a const típust megőrző host-próbában a javítás nélküli all-választás 4 darab
`invalid conversion from 'const char*' to 'char*'` hibával megállt. A korábbi tesztben
használt PSTR-helyettesítő nem őrizte meg helyesen ezt a const típust, ezért ezt nem fogta meg.
A régi teszt korlátját most kijavítottuk; a negatív reprodukció is a teszt része.

A `stage_sources.py` kizárólag a 083-as beillesztése körül ad szűkített PSTR
kompatibilitási burkolatot. Az ESP8266 PSTRN makrója, flash-elhelyezése és igazítása
megmarad; a read-only szöveghez a régi plugin által várt pointertípus készül.
A pointeren át nem ír a plugin, a szöveget továbbra is pgm_read_byte() olvassa.
A framework eredeti PSTR makrója közvetlenül az include után visszaáll.
Nem adtunk globális -fpermissive kapcsolót, nem némítottunk el általánosan hibákat.

Az eredeti Plugin_083.c, a többi plugin és még a generált .c.inc pluginmásolat is
bájtról bájtra ugyanaz marad. A generált registry.inc az illesztőréteg része, nem eredeti plugin.

## Telepítés

1. Mentsd el a jelenlegi működő YAML-t és a repó állapotát.
2. A ZIP-ből töltsd a repóba azonos útvonalon a TELJES `components/rflink/` tartalmát
   és a két `packages/` fájlt. A meglévő `RFLink/` könyvtár maradjon érintetlen.
   Ne egyetlen ZIP-fájlként töltsd fel: a könyvtárszerkezetet kell megtartani.
3. Az ESPHome készülék konfigurációjához használd a mellékelt
   `rflink-all-plugins-proba.yaml` fájlt. A titkok nevei a korábbival egyeznek,
   valós jelszót és kulcsot nem tartalmaz a csomag. A saját `secrets.yaml` marad helyben.
4. GitHub-forrás frissítéshez a YAML-ban `refresh: 0s` van. Clean build files, majd
   fordítás és feltöltés. Önmagában a GitHub-frissítés nem változtatja meg a készüléket.
5. Indulási ellenőrzés:

```text
RFLink RX compatibility bridge v0.1.5 (all RX plugins + API gate + EV1527 gestures):
  RX plugins compiled: 48
  Decode enabled: NO
```

Az elején a NO szándékos. Az API-állapotfeliratkozás és az 5 másodperces várakozás után
DECODE=ON következik. A régi 60 másodperces tesztleállítás nincs benne.
A gesztuscsomag saját naplósora v0.1.4 maradhat: annak működése most nem változott.

## Mi lett itt ténylegesen tesztelve?

A `TEST_RESULTS.txt` és `TEST_LOGS/` tartalmazza az eredményeket.
GNU C++20 HOST fordítás, az ESP8266 const-PSTR típusának megőrzésével; az összes 48
RX-plugin együtt, mindegyik egyenként, valamint 47/4/2-es készletek. A valódi bridge
és dekóder C++-ja, a gesztus YAML-lambdái és védett flash-helyettesítés futott
ASan/UBSan ellenőrzéssel. Nem csak YAML-szintaxist néztünk.

FONTOS: ez még nem teljes ESPHome/Xtensa firmware-fordítás. Ebben a környezetben
nincs elérhető célfordító/ESPHome-telepítés; a függőségek beszerzése nem sikerült.
ESP8266 RAM/flash méretet, valódi Wi-Fi/API/OTA működést vagy a 48 protokoll valós
rádiós jelének vételét ezek a tesztek nem igazolják. A saját mostani pontos error:
sorodat nem kaptuk meg, tehát nem biztos, hogy csak a reprodukált hiba érintett.

## Valódi firmware-ellenőrzés GitHubon, feltöltés nélkül

Opcionálisan töltsd fel még a `.github/`, `.ci/`, `tests/` könyvtárakat,
`UPSTREAM_SHA256.json` és `rflink-all-plugins-proba.yaml` fájlt a repó gyökerébe.
A meglévő eredeti RFLink-fájlok kellenek a fordításhoz.

A workflow neve **RFLink all RX firmware**. Forrásmódosítás feltöltésekor futásra
van beállítva, és az Actions lapon kézzel is indítható (Run workflow).
ESPHome 2026.9.0 konténerben, `nodemcuv2` céllal három teljes fordítást készít:
`all`, `configured`, `[61]`. Minden változatban megmarad az API + összes adat + gesztus.
A forrásfájlokat közvetlenül az adott commitból olvassa, nem egy külön main-gyorsítótárból.

A workflow-t innen NEM indítottuk el. Csak a fájljait és a bemenetgenerálást ellenőriztük.
A CI-hez nyilvános, mesterséges jelszavak és API-kulcs készülnek elkülönített mappában;
valódi titkot nem kell a GitHubra tenni. Ezt a próbakonfigurációt NEM szabad a készülékre
feltölteni. Nincs hardverfeltöltési lépés; csak a fordítási naplókat teszi elérhetővé.
A zöld eredmény teljes fordítást igazol, de rádiós és hálózati hardverpróbát az sem.

Helyi host-ellenőrzés Linuxon (g++, Python, PyYAML szükséges):

```bash
python3 tests/all_plugins/test_all_plugins.py --repo . --out /tmp/rflink-host-results
```

## Megmaradó korlátok, nem elhallgatott ismert jelenségek

- A Plugin_037 eredeti `%x`/`unsigned long` format warningja megmaradt. A tesztekben
  nem állítja le a fordítást; nincs elnyomva. A 083-as const típushiba más jellegű.
- A teljes 47/48-as készlet megosztott CRC-állapotot használ. A szintetikus 41 keretes
  tartásból ilyenkor 41 hagyományos JSON, a 2/4-es készletből 1 JSON keletkezett.
  Az új gesztuskezelő mindegyik összeállításnál egy holdot, ismétléseket és egy
  hold_release-t készített; a double/triple tesztek is sikeresek. A régi button_08
  eseményre épített automatizmust ne futtasd párhuzamosan az új gesztusos vezérléssel.
- A Plugin_083 eredeti kódja a parancsát második NAME mezőként írja, például NAME=CMD=UP.
  A fordítási javítás ezt nem írja át és nem hitelesít normalizált Brel HA-parancskezelést.
  A különböző protokollok szemantikai sajátosságaihoz további illesztés szükséges lehet.
- Nem bizonyított a teljes pluginlista hosszú távú stabilitása az ESP8266-on.
  Az API-first indítás maradjon, a heap/max_block és futásidő naplót a teljes készlettel
  is ellenőrizni kell. Az egységes számláló nem jelenti minden protokoll valódi tesztjét.
- Pluginlicencek: az eredeti forrás licencei érvényesek; egyesek kereskedelmi használati
  korlátozást is tartalmaznak. Ez a kompatibilitási frissítés nem ad új felhasználási jogot.
