# RFLink ESPHome v0.1.9 – rövid magyar útmutató

A részletes, aktuális dokumentáció a gyökérben lévő [`README.md`](README.md). Ez a fájl a napi használathoz szükséges rövid összefoglaló.

## Stabil verzió használata

A `main` fejlesztési ág. Működő eszközön rögzített taget használj:

```yaml
packages:
  rflink_api:
    url: https://github.com/vicktor1979/esphome-rflink
    ref: v0.1.9
    refresh: 5min
    files:
      - packages/rflink-ha-data-only.yaml

external_components:
  - source: github://vicktor1979/esphome-rflink@v0.1.9
    components: [rflink, remote_receiver, rflink_remote]
    refresh: 5min
```

Teljes ESP8266 példák:

- `examples/rflink.yaml` – legacy/original profil;
- `examples/rflink-extended.yaml` – 55 pluginos extended profil;
- `examples/fragments/` – távirányító, MQTT, időjárás és GitHub package részletek.

## v0.1.9 fontos változásai

- Csak az aktív runtime legacy dekóderek kerülnek bejárásra.
- Extended profilnál a kiegészítő pulse-view feldolgozás nem fut, ha nincs aktív extension dekóder.
- Ha egy EV1527 binding nem használ double/triple/click_N eseményt, a `single` a release után azonnal megjelenik; nem várja ki a `multi_click_timeout` idejét.
- A tanuló jel, tanuló gesztus és az utolsó RF üzenet rövid, Home Assistant-barát szövegként jelenik meg.
- A teljes dekódolt JSON szükség esetén a `RFLink részletes napló` kapcsolóval tehető láthatóvá a logban.
- Plugin 254-hez van `RF debug 60 másodperc` gomb, hogy a nagy terhelésű debug ne maradjon véletlenül bekapcsolva.
- `RF pluginok alaphelyzet` és `RF tanulás indítása` gomb könnyíti a napi használatot.
- Receiver overflow esetén a pontos számláló megmarad, de a warning napló aggregált, így zajos RF környezetben nincs logvihar.

## Bevált ESP8266 RX alap

```yaml
remote_receiver:
  id: rf_receiver
  capture_enabled: false
  high_frequency: false
  pin:
    number: GPIO5
    inverted: false
    mode: INPUT
  filter: 100us
  idle: 5ms
  buffer_size: 1200b
```

A vétel csak Wi-Fi + Home Assistant API kapcsolat után, 5 másodperces stabilizálási idővel indul el a teljes példákban.

## Pluginok

`plugin_switches` használatakor csak az ott felsorolt pluginok kapcsolhatók runtime. Plugin 001 mindig aktív. Plugin 254 debug fallback első induláskor OFF, normál használatban maradjon kikapcsolva.

A lehető legkisebb ténylegesen szükséges pluginlistát érdemes használni. Például Alecto V1 + EV1527 esetén a 030 és 061 elég; a 254-et csak hibakereséskor kapcsold be.

## Tanuló mód

A tanuló entitások rövid formátuma például:

```text
EV1527 · 085372 · 08 · ON · gestures
EV1527 · 085372 · 08 · ON · single
```

A pontos machine-readable minta tanulás közben a logban továbbra is megjelenik, így YAML-ba másolható.

## Alecto V1

Ha egy korábban működő Alecto V1/Plugin 030 szenzor rádiós aktivitást mutat, de nem dekódol stabilan, ellenőrizd az elemet. A projektben az Alecto 006C gyenge elemmel hibás/hiányos vételt adott; elemcsere után a dekódolás ismét stabil lett. Emiatt a v0.1.9 nem lazította fel a Plugin 030 érvényességi feltételeit.

## Visszalépés

A korábbi stabil tagek megmaradnak, például:

```text
v0.1.7
v0.1.8.2
v0.1.9
```

Probléma esetén a YAML-ban elég mind a `packages` `ref:` értékét, mind az `external_components` `@tag` részét ugyanarra a korábbi verzióra visszaállítani, majd újrafordítani.

## Tesztek

A host tesztek leírása: `tests/README.md`. Az eredeti RFLink plugin/config/old fájlok SHA256 manifesttel védettek; a projekt célja továbbra is az, hogy az upstream pluginforrások ne módosuljanak.
