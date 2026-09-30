# v0.2.1.0 – RFLink szenzor közvetlenül YAML-ből

Ez a módosított/új fájlokat tartalmazó csomag a v0.2.0.9-re épül.
Bontsd ki és töltsd a repó gyökerébe a mappaszerkezet megtartásával.

## Külső komponens

A meglévő external_components bejegyzésben egészítsd ki a listát:

```yaml
external_components:
  - source: github://vicktor1979/esphome-rflink@main
    components: [rflink, remote_receiver, rflink_remote, rflink_sensor]
    refresh: 0s
```

A `refresh: 0s` az első újrafordításhoz biztosít friss forrást; utána visszaállítható 5min-ra.
A bevált `capture_mode: rflink_polling` maradjon. A plugin továbbra is legyen lefordítva és bekapcsolva: Mebus 040, Alecto V1 030, távirányító 061.
Az új szenzor nem kapcsolja be automatikusan a pluginokat.

## Egy hőmérséklet

A meglévő sensor listához add hozzá (ne hozz létre második sensor: főkulcsot):

```yaml
sensor:
  - platform: rflink_sensor
    id: mebus_temp
    name: "Mebus hőmérséklet"
    rflink_id: rf_bridge
    protocol: "Mebus"
    rf_id: "e501"
    field: TEMP
```

A `rflink_id` a meglévő `rflink:` komponens azonosítója, nem a távirányító-hub `remote_id` értéke.
A protokoll a JSON NAME mezőjének pontos neve. Az rf_id teljes szöveg legyen idézőjelben, a vezető nullákkal együtt; kis-/nagybetű nem számít az ID-ban. Másik adóhoz új listabejegyzés, egyedi ESPHome id és név kell.
TEMP esetén a °C, temperature device_class, measurement state_class és 1 tizedes automatikus. `field: TEMP` elhagyható.
A normál ESPHome sensor opciók (például filters, on_value) használhatók.

Alecto példa és páratartalom: `examples/rflink-sensors.yaml`. Ezek csak akkor frissülnek, ha a tényleges protokoll/ID és mező egyezik.

## Régi package kiváltása

Ha minden onnan használt érzékelőt átvittél, vedd ki a régi `packages/rflink-alecto-006c.yaml` vagy Mebus package hivatkozását, valamint az ahhoz tartozó `script.execute: alecto_process` / `mebus_process` hívást.
Más API- és távirányító-scriptek maradjanak. Egy id csak egyszer szerepelhet: a régi template szenzor és az új platform ne maradjon bent egyszerre azonos id-val.
Ha a régi csomag elemszintet, csatornát vagy ID-szöveget is megjelenített, azok külön entitások; ez az új platform numerikus mérési mezőket kezel.
A régi entitások neve és id-ja átvehető. HA-ban ellenőrizd a hivatkozásokat az átállítás után.

## Értékkezelés

Az RFLink meglévő mezőkonverzióját használja: a TEMP hexadecimális előjeles tizedfok (pl. 010a → 26,6 °C, 8037 → −5,5 °C).
Hiányzó vagy hibás érték nem írja felül az utolsó jó mérést; alapértelmezésben nincs lejárat.
Újraindítás után az első érvényes vételig ismeretlen az állapot, nincs flashbe mentett mérés.
Azonos értékű új mérések is eljutnak a szenzorszűrőkhöz. A szokásos ESPHome szűrők opcionálisan alkalmazhatók.
Nem hoz létre elemtöltöttséget vagy más adatot olyan üzenetből, amely azt nem tartalmazza.

Támogatott numerikus mezők: SET_LEVEL, TEMP, HUM, BARO, HSTATUS, BFORECAST, UV, LUX, RAIN, RAINRATE, WINSP, AWINSP, WINGS, WINDIR, WINCHL, WINTMP, CHIME, CO2, SOUND, KWATT, WATT, CURRENT, DIST, METER, VOLT, CHAN, WINDIR_DEG.
TEMP/WINCHL/WINTMP, HUM és BARO metaadatai automatikusak. Más mezőknél a mértékegységet és a megjelenítési beállításokat a protokollhoz igazítsd (FIELD_MAP.md).

## Ellenőrzés

- ESPHome 2026.9.0: konfiguráció és C++ kódgenerálás Mebus/Alecto szenzorokkal és EV-gesztuseseménnyel.
- Tényleges új C++ szenzorkód host tesztje: adók/protokollok elkülönítése, ID, negatív és nulla hőmérséklet, páratartalom, hibás/hiányzó adat, ismételt érték. 10 000 EV-üzenet nem indít új JSON-parse műveletet a hőmérséklet/páratartalom-szenzorokban.
- Polling és interrupt host regressziók; EV egyszeri/dupla/hosszú nyomás közbeiktatott Alectóval, legacy és extended profilban.
- A host teszt szimuláció, nem készülékes rádiós, OTA vagy HA-próba. Feltöltés után ellenőrizd a hőmérsékletet és az EV rövid/hosszú nyomását.

A teljes ESP8266 firmware-fordítás kísérlete a fordítói függőségek előzetes letöltése során le lett állítva; sikeres target fordítás nem igazolt. Az ESPHome sémaellenőrzés és C++ kódgenerálás sikeres.
