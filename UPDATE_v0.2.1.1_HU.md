# v0.2.1.1 – szenzor fordítási javítás

A v0.2.1.0 automatikus state_class alapértéke szövegként került a generált C++-ba:

```cpp
mebus_temp->set_state_class("measurement");
```

Az ESPHome saját enumkonverziójával ez most:

```cpp
mebus_temp->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
```

A javítás TEMP, HUM, BARO, WINCHL és WINTMP mezőkre, valamint az elhagyott field (alapértelmezett TEMP) esetére is érvényes. A kézzel megadott state_class opciót nem írja felül.

## Telepítés

1. Bontsd ki a ZIP-et, és a mappaszerkezet megtartásával töltsd fel a repó gyökerébe.
2. A meglévő external_components bejegyzésben az első újrafordításhoz használd a `refresh: 0s` beállítást, hogy a friss Python-séma töltődjön le. Utána visszaállítható a szokásos frissítési időre.
3. Fordítsd újra és telepítsd. A szenzor YAML-ját nem kell módosítani; a meglévő rflink_sensor komponenslistát és a capture_mode: rflink_polling beállítást tartsd meg.

A csomag az előző új szenzorplatform összes fájlját is tartalmazza; v0.2.0.9-ről közvetlenül is telepíthető a `UPDATE_v0.2.1.0_HU.md` beállítási lépéseivel.

## Ellenőrzés

ESPHome 2026.9.0 valódi sémaellenőrzés és kódgenerálás: hat automatikus eset és két explicit felülbírálás (total, üres/none). A kimenet mindenhol enumot tartalmaz; nincs szöveges set_state_class hívás.
Regresszió: `python tests/test_rflink_sensor_codegen.py` ESPHome-ot tartalmazó Python-környezetben.

A módosítás csak a konfigurációs alapérték típuskonverzióját javítja. A C++ vételi, dekódolási és szenzor-publikálási kód változatlan (a bridge verziófeliratán kívül).
Teljes ESP8266 target fordítás nincs igazolva; a korábbi fordítói csomagletöltés checksumhibát jelzett. Készülékes rádiós/OTA teszt nem történt.
