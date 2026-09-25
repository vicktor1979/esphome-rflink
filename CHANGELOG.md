# Changelog

Az RFLink ESPHome komponens fontosabb verziói és változásai.

## v0.1.9

- A Wi-Fi + HA API indulási kapu, 5 s settle és alapból 30 s diagnosztika a komponensbe került (`auto_start: true`); a fő példákból eltűnt a nagy 1 s/10 s YAML `interval`, a hozzá tartozó globals és `on_raw` számláló.
- RX önjavítás: ring-buffer overflow után azonnali capture-resync; folyamatos, 2,5 s-nál tovább le nem záródó részkeret után automatikus újraszinkronizálás.
- Hosszabb RF-csend után a legacy ismétlésszűrő history ürül; receiver-resync és plugin ki/be kapcsolás szintén tiszta repeat state-t indít. Ez a több perc csend után nehezen ébredő távirányító esetét célozza.
- Új diagnosztika: `recoveries` és `history_resets`, valamint kompakt `RFLink állapot` önjavítás-számlálóval. A fő példák globális logger szintje INFO; csak az `rflink` tag enged DEBUG-ot, és a részletes RF üzeneteket külön HA kapcsoló engedi.
- Gyorsabb runtime dekóder-dispatch: csak az aktív legacy pluginok kerülnek bejárásra.
- `extended` profilnál az extension pulse-view előfeldolgozás kimarad, ha nincs aktív extension dekóder.
- EV1527 `single` esemény automatikusan azonnali a release után, ha az adott binding nem használ double/triple/click_N gesztust.
- A tanuló jel, tanuló gesztus és az utolsó RF üzenet rövid, Home Assistant-barát formátumot kapott; a teljes JSON továbbra is a logban marad.
- A `rflink_remote` csak a szükséges gesture/message binding listát járja be, a belső dekódolt üzenet const-reference útvonalon jut el hozzá.
- Újrafelhasznált dekódolási string buffer csökkenti a heap-allokációt.
- Receiver overflow napló 5 másodperces aggregálással védi az ESP8266-at logvihar ellen; a pontos overflow számláló megmarad.
- Példakonfigurációk: 60 másodperces Plugin 254 debug gomb, plugin-alaphelyzet, tanulás-indító gomb, build-/állapotdiagnosztika és alapból kikapcsolt, HA-ból kapcsolható részletes RF napló.
- A takarítás után elavult tesztútvonalak javítva; a host regressziós tesztek a jelenlegi `examples/` struktúrát használják.
- Az eredeti RFLink plugin/config/old fájlok továbbra is változatlanok.

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
