# Changelog

Az RFLink ESPHome komponens fontosabb verziói és változásai.

## v0.1.8.2

- Plugin 254 ismeretlen RF jelek Home Assistant diagnosztikája
- `RFLink ismeretlen jel` text sensor
- `RFLink ismeretlen jel impulzusszám` sensor
- Plugin 254 runtime kapcsolható, alapértelmezetten kikapcsolva
- 24 impulzustól támogatott a 254-es debug fallback
- Runtime plugin kezelés megtartva

## v0.1.8.1

- Runtime plugin kapcsolók javítása
- Csak a YAML-ban felsorolt pluginok indulnak aktívan
- Plugin 001 mindig aktív
- Plugin 254 alapértelmezetten kikapcsolva
- Plugin 254 a normál dekóderek után fut
- Aktív pluginok listája Home Assistant text sensorban

## v0.1.8

- Pluginonkénti runtime engedélyezés és tiltás
- Home Assistant kapcsolók a kiválasztott RFLink pluginokhoz
- Plugin állapotok visszaállítása újraindítás után
- `RFLink aktív pluginok` text sensor

## v0.1.7

- `extended` plugin profil
- 55 fordítható RFLink plugin
- Új / aktivált pluginok:
  - 016 Silvercrest
  - 018 Louvolite
  - 048 Oregon V2/V3
  - 049 LaCrosse TX141/TX145
  - 050 FineOffset WH2
  - 076 CAME TOP-432
  - 077 Avantek
- Plugin javítások:
  - 001
  - 037
  - 072
  - 083
- CHAN és WINDIR_DEG mezők
- Eredeti RFLink pluginforrások változatlanul megőrizve

## v0.1.6

- YAML-ból konfigurálható RF távirányítók
- RF tanuló mód
- Home Assistant event entitások
- EV1527 gesztuskezelés megtartva

## v0.1.5

- EV1527 gesztuskezelés
- press / release / single / double / triple
- hold / hold_repeat / hold_release
- stabilabb RF receiver működés ESP8266-on

## Korábbi verziók

A projekt korábbi verziói az alap RFLink → ESPHome kompatibilitási réteget,
PROGMEM javításokat és Home Assistant API integrációt vezették be.
