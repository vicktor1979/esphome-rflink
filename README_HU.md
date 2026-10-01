# RFLink ESPHome v0.2.1.3 – rövid magyar útmutató

A részletes, aktuális dokumentáció a gyökérben lévő [`README.md`](README.md). Ez a fájl a napi használathoz szükséges rövid összefoglaló.

## Stabil verzió használata

Az alábbi példa a main ágról tölti be a kézzel feltöltött javításokat. Rögzített verzióhoz csak olyan taget használj, amelyet ténylegesen létrehoztál a repóban:

```yaml
packages:
  rflink_api:
    url: https://github.com/vicktor1979/esphome-rflink
    ref: main
    refresh: 5min
    files:
      - packages/rflink-ha-data-only.yaml

external_components:
  - source: github://vicktor1979/esphome-rflink@main
    components: [rflink, remote_receiver, rflink_remote, rflink_sensor]
    refresh: 5min
```

Teljes ESP8266 példák:

- `examples/rflink.yaml` – legacy/original profil;
- `examples/rflink-extended.yaml` – extended profil, konkrét kiválasztott pluginlistával;
- `examples/fragments/` – távirányító, MQTT, időjárás és GitHub package részletek.

## v0.2.1.3 fontos változásai

- Egyetlen lista: `rx_plugins: [30, 40, 61, 254]`. Ez vezérli a fordítást és az automatikus kapcsolókat. A régi plugin_switches kulcsot töröld.
- A normál felsorolt pluginok visszaállítják az előző kapcsolóállapotukat és első használatkor ON-ról indulnak; a 254 mindig OFF-ról indul.
- Nincs több plugin-alaphelyzet gomb; az `RFLink aktív pluginok` text sensor megmarad, és minden plugin ki-/bekapcsolásakor frissül.
- A diagnosztikai mezők build/setup során automatikusan a konfigurált pluginok képességeihez igazodnak. Amit egyik konfigurált plugin sem tud előállítani, az nem jelenik meg Home Assistantban.
- Runtime plugin OFF esetén az érintett diagnosztikai állapotok unavailable / `Kikapcsolva` állapotúak. ESPHome 2026.9.0 alatt a már regisztrált natív API entitások futás közbeni valódi eltávolítása/visszavétele nem biztonságosan támogatott.

## v0.1.9 fontos változásai

- Csak az aktív runtime legacy dekóderek kerülnek bejárásra.
- Extended profilnál a kiegészítő pulse-view feldolgozás nem fut, ha nincs aktív extension dekóder.
- Ha egy EV1527 binding nem használ double/triple/click_N eseményt, a `single` a release után azonnal megjelenik; nem várja ki a `multi_click_timeout` idejét.
- A tanuló jel, tanuló gesztus és az utolsó RF üzenet rövid, Home Assistant-barát szövegként jelenik meg.
- A teljes dekódolt JSON szükség esetén a `RFLink részletes napló` kapcsolóval tehető láthatóvá a logban.
- A korábbi YAML-os indítási/diagnosztikai `interval` blokk a komponensbe került: `auto_start: true` intézi a Wi-Fi + HA API kaput, az 5 s stabilizációt és a diagnosztikát.
- Receiver overflow esetén a vevő automatikusan újraszinkronizálja a ring buffert; tartósan lezáratlan részkeretnél 2,5 s után szintén önjavít. A warning napló korlátozott.
- Hosszabb RF-csend után a legacy ismétlésszűrő állapot automatikusan ürül, hogy az első új gombnyomást ne blokkolhassa beragadt history.

## Bevált ESP8266 RX alap

```yaml
remote_receiver:
  id: rf_receiver
  capture_mode: rflink_polling
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

A vétel csak Wi-Fi + Home Assistant API állapotfeliratkozás után, 5 másodperces stabilizálási idővel indul el a teljes példákban. Ezt már maga az `rflink` komponens végzi `auto_start: true` mellett; nincs hozzá külön YAML `interval`/global/on_raw logika.

## Pluginok

Az rx_plugins minden kiválasztott pluginjához automatikusan kapcsoló készül, kivéve a kötelező 001-et. Példa:

```yaml
rflink:
  plugin_profile: legacy
  rx_plugins: [30, 40, 61, 254]
```

Az `all` az egész profilt lefordítja és minden pluginhoz kapcsolót készít (legacy: 47, extended: 54). A `configured` az alapértelmezés, és az eredeti upstream konfigurációs fejléc választását követi; ehhez is automatikusan kapcsolók készülnek. Plugin 001 mindig aktív és nem kell felsorolni. A normál pluginok az utolsó kapcsolóállapotot visszaállítják (`RESTORE_DEFAULT_ON`), a 254 viszont minden reboot/OTA után OFF-ról indul.

A diagnosztikai mezők automatikusan a felsorolt pluginok képességeihez igazodnak. A statikusan nem támogatott mezők nem kerülnek ki HA felé; egy plugin futásidejű kikapcsolásakor a csak hozzá tartozó állapotok unavailable / `Kikapcsolva` állapotúak.

A lehető legkisebb ténylegesen szükséges pluginlistát érdemes használni. Például Alecto V1 + EV1527 esetén a 030 és 061 elég; a 254-et csak hibakereséskor kapcsold be.

## Tanuló mód

A tanuló entitások rövid formátuma például:

```text
EV1527 · 085372 · 08 · ON · gestures
EV1527 · 085372 · 08 · ON · single
```

A pontos machine-readable minta tanulás közben a logban továbbra is megjelenik, így YAML-ba másolható.

## Több perc csend utáni első gombnyomás

A v0.1.9 két önjavító védelmet tartalmaz: overflow vagy 2,5 s-nál tovább lezáratlan RF részkeret esetén automatikus receiver-resync történik, illetve 1 s-nál hosszabb felismert RF-csend után az első következő dekódolás előtt ürül a legacy repeat history. A `RFLink állapot` jelzi, ha RX-helyreállítás történt; a diagnosztikai logban `recoveries` és `history_resets` számláló is látható.

## Alecto V1

Az Alecto V1/Plugin 030 az eredeti feldolgozást használja, egyedi helyreállítás és többmintás értékszűrés nélkül. A rflink_sensor platform TEMP és BAT mezőket kezel; BAT: LOW=0, OK=100, kétállapotú jelzés. A Mebus plugin csak hőmérsékletet ad. Példák: examples/rflink-sensors.yaml.

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
