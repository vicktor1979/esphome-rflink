# v0.2.1.2 – numerikus elemállapot és frissített példák

## A tényleges elemállapotok

A meglévő RFLink formatter `display_BAT(boolean)` függvénye két szöveget küld:

| RFLink BAT | Új numerikus szenzor | Jelentés |
|---|---:|---|
| LOW | 0 | Az adó alacsony elemállapotot jelez. |
| OK | 100 | Az adó nem jelez alacsony elemállapotot. |
| hiányzó, ismeretlen vagy hibás | nincs frissítés | Az utolsó érvényes érték megmarad. |

Ez kétállapotú jelzés, nem mért töltöttségi százalék. Az OK nem igazolja, hogy új vagy teljesen feltöltött az elem. A battery eszközosztályhoz % egységet használunk, de az értékek csak állapotkódok. NORMAL/HIGH állapotot a jelenlegi formatter nem bocsát ki, így nincs kitalált 50-es köztes érték.
Újraindítás után az első érvényes BAT-üzenetig ismeretlen az állapot. Azonos értékű új jelentés is eljut az ESPHome szűrőihez, ezért az opcionális timeout működik. Ismeretlen BAT nem frissíti a timeoutot.

Alecto V1 Plugin 030: van BAT kimenet. Mebus Plugin 040: nincs BAT kimenet; hozzá elemállapot-szenzort létrehozva nem lesz adat. A rádiós pluginokat nem módosítottuk.

## Használat

A meglévő sensor listához add hozzá, a tényleges adóazonosítóval:

```yaml
sensor:
  - platform: rflink_sensor
    id: alecto_006c_battery
    name: "Alecto 006C elemállapot"
    rflink_id: rf_bridge
    protocol: "Alecto V1"
    rf_id: "006c"
    field: BAT
```

A battery device_class, % egység, 0 tizedes, measurement state_class és diagnostic kategória automatikus. Az external_components listájában szerepeljen rflink_sensor.
A régi binary_sensor helyett most sensor keletkezik: a rá hivatkozó HA-automatizációkat ennek megfelelően módosítsd. A 0 jelent LOW állapotot; az ismeretlen értéket ne kezeld automatikusan 0-ként.

## Frissített example YAML-ek

- `examples/rflink.yaml`, `rflink-extended.yaml`: közvetlen Mebus TEMP és Alecto TEMP/BAT szenzorok. Mebus 040 szerepel a választható pluginok között. A bevált polling mód aktív a példákban. A csomagkor diagnosztikai szenzor 5 percenként frissül.
- `examples/rflink-sensors.yaml`: egyszerű háromszenzoros minta, külön process script nélkül.
- `examples/fragments/03-optional-weather-device.yaml`: Cresta TEMP/HUM/BAT példa, opcionális 60 perces timeouttal.
- `examples/generated/fixed-sensor-example.yaml`: a régi fájlút megmaradt, a tartalom már az új platformot mutatja. A régi make_rf_device.py generátor továbbra is a korábbi template/script formátumot állítja elő; ennek a friss példának már nem az a generátora.
- `examples/fragments/01-existing-device-API-only.yaml`, `05-github-packages.yaml`: a jelenlegi data-only diagnosztika és a rflink_sensor komponens betöltése. Régi időjárás-package és eszközönkénti script-hívás nem kell.
- A fő példák logger-konfigurációjában javítottuk a korábbi INFO globális / DEBUG tag érvénytelen kombinációt: fordítási DEBUG, kezdeti futásidejű INFO, külön rflink tag DEBUG. A részletes RF-üzeneteket továbbra is a meglévő kapcsoló engedélyezi.

A rflink_api_process script megmarad a közös diagnosztikához; a hőmérséklet és elemállapot közvetlenül az új platformhoz érkezik. A régi package fájlokat nem töröltük: aki még más entitásukra támaszkodik, megtarthatja őket.
A teljes példák minták; meglévő eszközön ne írd felül velük a saját Wi-Fi-, API-, GPIO- és távirányító-beállításaidat. A szenzorblokkok egyenként átvehetők. Azonos id ne szerepeljen egyszerre régi és új entitásnál.

## Telepítés és ellenőrzés

A ZIP módosított/új fájljait töltsd a repó gyökerébe a mappaszerkezet megtartásával. Az első újrafordításhoz az external_components refresh értéke legyen 0s, majd visszaállítható 5min-ra. A saját szenzorlistádba fel kell venni a BAT érzékelőt; a kódfrissítés önmagában nem hoz létre új entitást.

Ellenőrzések:
- Tényleges C++ szenzorkód host tesztje ASan/UBSan mellett: LOW/OK, visszaállás OK-ra, hiányzó/hibás/idegen adat, adók elkülönítése, ismétlés, Mebus hiányzó BAT, hőmérséklet/páratartalom regresszió.
- 10 000 TEMP/HUM/BAT nélküli EV-üzenet nem vált ki JSON-parse műveletet az új szenzorokban.
- ESPHome 2026.9.0 valódi sémaellenőrzés és C++ kódgenerálás: BAT setter, metaadatok, enumértékek; mindkét teljes példa és az opcionális időjárás-példák.
- A rádiós jelbegyűjtés és EV-gesztuskezelés forrása változatlan.

Teljes ESP8266 target fordítás és készülékes/OTA teszt nem történt ebben a javításban. Feltöltés után ellenőrizd a BAT mezőt a tényleges adó üzenetében, valamint a megszokott EV-gombokat.
