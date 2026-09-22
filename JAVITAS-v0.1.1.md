# RFLink ESPHome v0.1.1 – ESP8266 PROGMEM-szövegkezelés javítása

## Mi a hiba?

A v0.1 JSON-kiírójának `escaped()` függvénye közvetlen `*p` byte-olvasással
járta be a pluginoktól kapott `const char*` szövegeket. A változatlan pluginok
azonban rendszeresen `PSTR(...)` szöveget adnak át, például:

```cpp
display_Name(PSTR("EV1527"));
```

Az ESP8266-on ez flashben tárolt adat. A közvetlen byte-olvasás LoadStoreErr
kivételt okozhat. A v0.1.1 az illesztőrétegben `pgm_read_byte()` segítségével
olvassa a karaktereket. A célzott Arduino ESP8266 és ESP32 környezetben ez
RAM-ból származó szöveggel is használható.

Ez konkrétan kimutatott v0.1 forráshiba. A felhasználói logban szereplő
LoadStoreErr hibával összhangban van; az ottani pontos PC/stack forrássorhoz
rendeléséhez a hibát produkáló buildhez tartozó `firmware.elf` szükséges.
A hardveres hiba megszűnését az új firmware-rel külön ellenőrizni kell.

## Minimális frissítés a meglévő repóban

A futó firmware forrásai közül csak ez a két fájl változott:

```text
components/rflink/rflink_engine.cpp
components/rflink/rflink.cpp
```

Az első a tényleges javítás, a második a verziójelzést frissíti.
A kis javító-ZIP ezt a két fájlt és ezt az útmutatót tartalmazza.

1. Cseréld le ezt a két fájlt a saját GitHub-repódban, azonos útvonalon,
   azon az ágon, amelyre az ESPHome konfigurációd hivatkozik.
2. Az ESPHome YAML meglévő `external_components` RFLink-bejegyzésében
   ideiglenesen állítsd `refresh: 0s` értékre a frissítésellenőrzést.
   Rögzített régi tag/commit esetén a forráshivatkozást is át kell állítani
   a javított változatra. A `refresh` nem módosít egy rögzített régi commitot.
3. A Device Builderben a készülék menüjében Clean build files, majd Install.
   Új fordítás ÉS feltöltés kell. Önmagában a GitHubra feltöltés nem telepít.
4. Az indulási naplóban ezt keresd:

```text
RFLink RX compatibility bridge v0.1.1 (PROGMEM fix):
```

Sikeres frissítés után a `refresh` visszaállítható például `5min` értékre.

**A saját YAML-t ne cseréld le a csomag példakonfigurációjára!**
A pin, filter, idle, MQTT, on_message és API beállításokat ez a javítás nem
módosítja. A példák továbbra is a korábbi MQTT-alapú kiinduló konfigurációk;
natív Home Assistant API-t ez a javítócsomag nem kapcsol be automatikusan.
A nyers impulzusnaplózás maradhat az első próbánál.

**Az RFLink/Plugins könyvtár egyik fájlja sem változott.** A teljes projektben
mind az 54 pluginmappabeli fájl egyezik az eredeti RFLink-5.5wj(2).zip tartalmával.
A 47/48-as pluginválasztás, az impulzuskezelés és az ismétlésszűrés is változatlan.
A teljes készlet korábban megfigyelt ismétléses korlátja nincs javítva ebben a patchben.

## Elvégzett ellenőrzések

- 54/54 eredeti pluginfájl SHA-256 és közvetlen bájtazonossági ellenőrzése.
- A normál host C++ tesztek sikeresek: 2, 4, 47 és 48 RX pluginos összeállítás.
- Védett PSTR-teszt: a hostban a PSTR-címek közvetlenül nem olvasható memórialapokra
  mutatnak; a mögöttes karakterek a teszt `pgm_read_byte()` segédjén át érhetők el.
  Az eredeti v0.1 engine ugyanazon EV1527 tesztnél SIGSEGV hibával megállt,
  a javított engine mind a négy összeállítással átment.
- RAM/PSTR szöveg, JSON-escape, UTF-8, null/üres szöveg és kimeneti méretkorlát tesztelve.

A védett PSTR-teszt célzott Linuxos regressziós teszt, **nem ESP8266-emulátor**.
Ez nem az eredeti ESP stack cím szerinti megfejtése, és nem helyettesíti a
konkrét firmware hardveres kipróbálását. Új ESPHome firmware-build és
ESP8266/ESP32 rádiós hardverteszt itt nem futott.

A tesztek a teljes csomagban vannak:

```bash
python3 tests/run_tests.py
python3 tests/run_progmem_tests.py
```

Az eredeti engine-nel történő összehasonlítás opcionális:

```bash
python3 tests/run_progmem_tests.py --legacy-engine /eleresi/ut/v0.1/rflink_engine.cpp
```

## Források

- ESP8266 Arduino Core PROGMEM útmutató:
  https://arduino-esp8266.readthedocs.io/en/latest/PROGMEM.html
- ESP8266 `pgm_read_byte()` megvalósítás:
  https://github.com/earlephilhower/newlib-xtensa/blob/xtensa-4_0_0-lock-arduino/newlib/libc/sys/xtensa/sys/pgmspace.h
- ESPHome külső komponensek és forrásfrissítés:
  https://esphome.io/components/external_components/

Kiadás: 2026-09-22. A csomag forráskód; nem előre fordított firmware.
