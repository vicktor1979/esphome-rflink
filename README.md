# RFLink ESPHome

**RF-jelek vétele változatlan RFLink-pluginokkal, natív Home Assistant-entitások, YAML-ban megadott távirányítók, gesztusfelismerés és tanuló nézet.**

Dokumentáció: **2026. szeptember 25.**  
Jelenlegi stabil fejlesztési alap: **v0.1.9 · ESP8266 rxgate2 · runtime plugin gate · rflink_remote**.

Ez a README a projekt jelenlegi, közösen kipróbált felépítését foglalja össze. **Nem új firmware-verzió és nem teljes forráscsomag:** a mellékelt konfigurációs példák a GitHub-repóban már meglévő komponenseket használják. A dokumentációfrissítés nem módosítja a rádiós dekódereket, a vételi időzítéseket vagy a gesztusfelismerő C++ kódot.

A **v0.1.9** fő célja a gyorsabb és önjavító vételi út: csak az aktív dekódereket járja be, az extended előfeldolgozás csak akkor fut, ha kell, a single-click pedig nem vár fölöslegesen multi-click időablakra. A korábbi YAML-os 1 s/10 s régi indítási és diagnosztikai `interval` logika a komponensbe került (`auto_start: true`). `high_frequency: false` mellett a vevő egy főciklusban korlátozottan több már lezárt keretet is leürít, így a lassú backlog nem tölti fel a ring buffert; overflow vagy tartósan lezáratlan impulzussor után saját maga újraszinkronizál, a legacy ismétlésszűrő állapot pedig hosszabb RF-csend után automatikusan ürül. A Home Assistantban megjelenő tanuló/utolsó RF szövegek rövidek; a teljes JSON hibakereséshez továbbra is a naplóban elérhető. A fő példákban a részletes RF üzenetnapló alapból ki van kapcsolva a kisebb futásidejű terhelésért; szükség esetén a **RFLink részletes napló** kapcsolóval ideiglenesen bekapcsolható.

Az új, felhasználói felületen használt kapcsolónév: **RFLink figyelés**. A korábbi neve **RFLink dekódolás próba** volt. Meglévő HA-entitás átnevezéséhez először olvasd el a [névkezelési részt](#4-a-figyelési-kapcsoló-átnevezése).

## Tartalom

- [1. Mire használható?](#1-mire-használható)
- [2. Verziók és a működő alap](#2-verziók-és-a-működő-alap)
- [3. Könyvtárszerkezet](#3-könyvtárszerkezet)
- [4. A figyelési kapcsoló átnevezése](#4-a-figyelési-kapcsoló-átnevezése)
- [5. Hardver és vételi beállítások](#5-hardver-és-vételi-beállítások)
- [6. Telepítés és konfigurációfrissítés](#6-telepítés-és-konfigurációfrissítés)
- [7. Indulás, kapcsolatvédelem és a kapcsolók](#7-indulás-kapcsolatvédelem-és-a-kapcsolók)
- [8. Pluginválasztás](#8-pluginválasztás)
- [9. Tanuló mód és a JSON értelmezése](#9-tanuló-mód-és-a-json-értelmezése)
- [10. Távirányító felvétele YAML-ból](#10-távirányító-felvétele-yaml-ból)
- [11. Gesztusok és időzítések](#11-gesztusok-és-időzítések)
- [12. Konyhai lámpa: single és hold_repeat](#12-konyhai-lámpa-single-és-hold_repeat)
- [13. Szenzoradatok és átszámítások](#13-szenzoradatok-és-átszámítások)
- [14. Állandó entitások egy mérőeszköznek](#14-állandó-entitások-egy-mérőeszköznek)
- [15. MQTT mint opcionális kimenet](#15-mqtt-mint-opcionális-kimenet)
- [16. OTA és biztonsági mód](#16-ota-és-biztonsági-mód)
- [17. Naplózás és hibakeresés](#17-naplózás-és-hibakeresés)
- [18. Ellenőrzött működés és korlátok](#18-ellenőrzött-működés-és-korlátok)
- [19. Források és karbantartás](#19-források-és-karbantartás)

## 1. Mire használható?

A projekt a régi RFLink rádiós pluginjait illeszti ESPHome alá. Az **`RFLink/Plugins` eredeti fájljai változatlanok** maradnak; a szükséges környezetet a köréjük épített kompatibilitási réteg biztosítja.

A jelenlegi használat középpontjában az **ESPHome natív API és a Home Assistant** áll. MQTT nem szükséges. A rádiós adatokból kétféle kimenet készül:

- **Saját entitások:** a YAML-ban pontosan megadott távirányító/gomb kombináció esemény-entitása és opcionális „nyomva” bináris szenzora; külön generált csomaggal egy adott mérőeszköz állandó szenzorai.
- **Diagnosztika:** a legutóbbi dekódolt üzenet mezői, számlálók, valamint bekapcsolható tanuló jel- és gesztusnézet.

Az adatút:

```text
RF-vevő DATA kimenete
  → remote_receiver rxgate2: impulzusgyűjtés
  → rflink: eredeti pluginokkal végzett dekódolás
      → dekódolt JSON → data-only csomag → diagnosztikai szenzorok
      → elfogadott EV1527-keretek → holdfix1 → rflink_remote események
      → tanuló nézet, amikor külön engedélyezett
  → ESPHome natív API → Home Assistant
```

**A teljes gesztusfelismerés jelenleg EV1527-re készült.** Más felismert protokollhoz pontos jelmintán alapuló `received` esemény rendelhető. Az összes plugin bekapcsolása nem jelent minden protokollra automatikus tartásfelismerést.

Ez **vételi, RX-megoldás**. RF-adás/TX, automatikus kód-visszajátszás, futás közbeni YAML-átírás és automatikusan létrehozott új HA-eszközadatbázis nincs benne.

## 2. Verziók és a működő alap

A különböző naplósorok eltérő verziói szándékosak: nem minden javítás cserélte le az összes komponenst.

| Rész | Jelenlegi alap | Feladat |
|---|---|---|
| `components/rflink` | v0.1.9 | Aktív dekóder-dispatch, JSON-formázás, runtime plugin gate, EV1527-keretmegfigyelés |
| `components/remote_receiver` | rxgate2 | ESP8266-specifikus vételkapcsolás; kikapcsolható gyorsított főciklus |
| `rflink_gestures.h` | holdfix1 | Rövid nyomások, többkattintás, tartás és kimaradástűrés |
| `components/rflink_remote` | v0.1.9 | YAML-os távirányító-entitások, gyors single-click és kompakt tanulás |
| `packages/rflink-ha-data-only.yaml` | v0.1.9 | Adatmezők és kompakt legutóbbi RF-üzenet, beégetett távirányítólista nélkül |
| Fő készülék-YAML | v0.1.9 | API utáni indulás, runtime plugin kapcsolók, tanulás, diagnosztikai segédgombok |

A feltöltött hardveres naplókban **ESPHome 2026.9.0**, **Home Assistant 2026.9.3** és `nodemcuv2` szerepelt. Ez az összeállítás dokumentált kiindulópontja, **nem általános kompatibilitási ígéret minden későbbi kiadáshoz**.

A jelenlegi `remote_receiver` felülírás **csak ESP8266 + Arduino célra készült**. A korábbi projektben volt ESP32-minta is, de ezt az rxgate2 fő konfigurációt nem lehet pusztán az `esp8266:` rész `esp32:`-re cserélésével átvinni.

A régi javítócsomagok egymásra épültek. Egy korábbi ZIP teljes visszamásolása felülírhatja a későbbi javítást. A README nem helyettesíti a már összeállított, működő forráskódot.

## 3. Könyvtárszerkezet

A GitHub-repó gyökeréhez képest:

```text
RFLink/
  Plugins/                         # eredeti pluginok; ne szerkeszd
  ...                              # eredeti, a híd által használt fejlécek/segédfájlok
components/
  rflink/
    __init__.py
    stage_sources.py
    rflink.h
    rflink.cpp
    rflink_engine.h
    rflink_engine.cpp
    rflink_fields.h
    rflink_gestures.h
  remote_receiver/                 # rxgate2, nem a gyári vevő változtatás nélkül
    __init__.py
    remote_receiver.h
    remote_receiver.cpp
    ...                            # a korábban mellékelt licencek
  rflink_remote/
    __init__.py
    event.py
    validation.py
    rflink_remote.h
    rflink_remote.cpp
packages/
  rflink-ha-data-only.yaml
examples/
  rflink.yaml                      # teljes legacy/original profilú példa
  rflink-extended.yaml             # teljes 55 pluginos extended példa
  fragments/                       # beilleszthető távirányító/MQTT/szenzor részletek
  generated/                       # generátor által készíthető példa
FIELD_MAP.json                     # az érzékelő-generátorhoz
FIELD_MAP.md
README.md
tools/
  make_rf_device.py                # a korábbi generátor változatlan másolata
```

A `.ci/`, `.github/` és `tests/` könyvtárak fejlesztési/ellenőrzési célúak; az eszköz hétköznapi futásához nem szükségesek. A v0.1.9 tesztjei már a rendezett `examples/` struktúrát használják.

A saját valódi `secrets.yaml` **az ESPHome konfigurációs környezetében maradjon**, ne kerüljön nyilvános repóba. A dokumentációs ZIP nem tartalmaz valódi jelszót, firmware-binárist vagy komponenskód-cserét.

## 4. RFLink figyelés és beépített automatikus indítás

A v0.1.9-ben a fő vételi engedélyt már maga az `rflink` komponens hozza létre. A teljes példákban elég:

```yaml
rflink:
  id: rf_bridge
  receiver_id: rf_receiver
  auto_start: true
```

Ez létrehozza a **RFLink figyelés** konfigurációs kapcsolót, a **RFLink dekódolás aktív** bináris diagnosztikát, valamint az **RFLink állapot** és **RFLink build** szöveges diagnosztikát. Nincs külön `rf_decode_test` template kapcsoló, `rf_start_gate` global vagy 1 másodperces YAML `interval` lambda.

Az alap viselkedés:

- `RFLink figyelés` induláskor ON (`ALWAYS_ON`);
- vétel indulás előtt Wi-Fi és HA API állapotfeliratkozás szükséges;
- ezek stabil fennállása után 5 másodperc múlva indul a capture + decode;
- figyelés OFF, kapcsolatvesztés vagy OTA alatt capture + decode leáll;
- visszatéréskor újra kivárja az 5 másodperces stabilizációt.

Haladó beállításnál az `auto_start` mappingként is megadható, például:

```yaml
rflink:
  auto_start:
    settle_time: 5s
    diagnostics_interval: 30s
    require_network: true
    require_api: true
```

A napi használathoz az egyszerű `auto_start: true` ajánlott.

## 5. Hardver és vételi beállítások

A jelenlegi készülék-YAML:

```yaml
esp8266:
  board: nodemcuv2

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

A `capture_enabled` és `high_frequency` **a saját rxgate2 komponens opciói**. A gyári `remote_receiver` nem feltétlenül ismeri őket. A megadott GPIO és polaritás a közös próbákban használt bekötéshez tartozik, nem tetszőleges vevő automatikus bekötési útmutatója.

A korábbi forrás ESP8266-os alapkapcsolásában a DATA bemenet D1/GPIO5 volt, és külön D5/GPIO14-es vevőengedélyezés is szerepelt. A most működő konfigurációba emiatt **nem kell utólag találomra tápvezérlő kapcsolót betenni**. Másik hardverre telepítésnél a tényleges tápot, adatvezetéket, engedélyezést és jelszinteket külön ellenőrizni kell.

A bevált `high_frequency: false` mellett az éleket továbbra is a GPIO-megszakítás gyűjti; csak a vevő saját folyamatosan gyorsított főciklus-kérése nincs engedélyezve. A v0.1.9 backlog-drain egy főciklusban legfeljebb 4 már lezárt keretet dolgoz fel, maximum 6 ms extra munkakerettel, ezért a normál ~16 ms-os ütemnél kissé gyorsabb keretáram sem gyűlik fel lassan a ringben. Az `overflow_reports`, `extra_drained` és `max_drain_batch` továbbra is figyelendő.

Az `1200b` az örökölt konfigurációs jelölés: ebben az ESP8266-os megvalósításban 1200 darab 32 bites időzítési elem tárolására kér helyet, vagyis körülbelül **4800 bájt** fő ring buffert. Erre a hosszabb támogatott impulzussorok miatt van szükség.

A v0.1.9 fő mintáiban nincs `on_raw` csomagszámláló és nincs `dump: raw`. A `remote_receiver` saját belső `frames`, `irq_total`, `overflow_reports` és `recoveries` számlálókat tart fenn, így a diagnosztika nem igényel minden nyers keretre YAML automationt.

## 6. Telepítés és konfigurációfrissítés

Kiindulásként a már működő repó és a hozzá tartozó mentett YAML szükséges. Az új teljes példa: [examples/rflink.yaml](examples/rflink.yaml).

**Meglévő készüléknél ne írj felül vakon saját távirányítókat, hálózati beállításokat vagy egyedi automatizmusokat.** A fő példák nem tartalmaznak konkrét saját RF ID-t vagy Home Assistant lámpaazonosítót; ezekhez az `examples/fragments/` mintákat használd.

A közös forrásbetöltés:

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

A `packages` a YAML-entitásokat/feldolgozást, az `external_components` a külső Python/C++ komponenseket tölti be. Mindkettő kell. A repó gyökerében közvetlenül legyen `components/` és `RFLink/`, ne egy újabb becsomagolt almappában.

**A következő régi csomagok ne legyenek az új `data-only` mellett betöltve:** `rflink-ha-api.yaml`, `rflink-ha-all-data.yaml`, `rflink-ha-gestures.yaml`. A fájlok a repóban megmaradhatnak, de a régi, beégetett távirányító-feldolgozás ne fusson párhuzamosan az új `rflink_remote` réteggel.

A készüléken az API, a Wi-Fi és az OTA a meglévő titkokat használja:

```yaml
api:
  id: api_main
  encryption:
    key: !secret rflink_api_key
  reboot_timeout: 0s

wifi:
  id: wifi_main
  ssid: !secret wifi_ssid
  password: !secret wifi_password
```

Ez csak részlet; a teljes mintában az OTA-kezelők, kapcsolatvesztési műveletek és az indítási vezérlés is szerepelnek. Az API-kulcs legyen érvényes, 32 bájtos Base64-kulcs; ne a README-ben látható példa vagy egy régi tesztkulcs.

A frissítés menete: mentés → szükséges repófájlok feltöltése → helyi YAML módosítása → forrásfrissítés → konfigurációellenőrzés → szükség esetén **Clean build files** → fordítás → tényleges telepítés. A fordítás vagy a GitHub-commit önmagában nem frissíti az eszközt.

A `refresh: 0s` fejlesztés közben segít új forrást lekérni, nem OTA-ütemezés. Az éles használatra elfogadott változatnál rögzített commit/tag célszerű, **a csomagoknál és a komponenseknél összeillő verzióval**. [Külső háttér: külső komponensek és csomagok][external] [packages]

A Home Assistantban az ESPHome-integrációhoz az eszköz aktuális IP-címe és az API-kulcs kell. Az API alapportja **6053**. Az ESPHome Device Builder és a HA-integráció kapcsolata nem ugyanaz: a Device Builder sikeres naplóolvasása még nem bizonyítja a HA állapotfeliratkozását. [Külső háttér: HA ESPHome-integráció][ha-esphome]

## 7. Indulás, kapcsolatvédelem és a kapcsolók

A fő YAML szándékosan ezt végzi:

```text
Bekapcsolás / újraindítás
  CAPTURE=OFF, DECODE=OFF
    → Wi-Fi csatlakozás
    → HA API-kliens állapotfeliratkozása
    → 5 másodperc folyamatosan teljesülő engedélyezési feltétel
    → CAPTURE=ON, DECODE=ON, fast_loop=OFF
```

A feltételeket a komponens belső, könnyű állapotgépe ellenőrzi (tipikusan 100 ms-os kapuzással); nincs hozzá YAML `interval`. Az öt másodperc nem a táp bekapcsolásakor indul, hanem attól, hogy a hálózat és a HA API állapotfeliratkozás folyamatosan készen áll. Egy csak naplót olvasó kliens nem elég hozzá. [Külső háttér: natív API][api]

| HA-kezelőszerv | Mit jelent? |
|---|---|
| **RFLink figyelés** | A rádiós feldolgozás engedélye. Kikapcsolva leállítja a saját GPIO-megszakítást, a gyűjtést és a dekódolást. |
| **RFLink dekódolás aktív** | A ténylegesen engedélyezett dekódolás diagnosztikai visszajelzése. |
| **RF tanuló mód** | A tanulási javaslatok és tanuló gesztusok átmeneti követése; nem a fő vételi kapcsoló. |
| **RFLink OTA helyreállítás** | Biztonsági módba újraindító gomb, nem hétköznapi figyeléskapcsoló. |

A fő kapcsoló `restore_mode: ALWAYS_ON`: **minden újraindításkor újra engedélyez**, a korábbi kézi OFF nem marad meg tartósan. Az indítási feltételeket ekkor is kivárja. A tanulás a mintában `enabled: false`, újraindítás után kikapcsolt.

Wi-Fi-/API-kapcsolatvesztés észlelésekor, kézi leállításkor és OTA-kezdéskor a feldolgozás szünetel. A fizikailag bekövetkezett kapcsolatvesztés észlelése nem feltétlenül azonnali. Újracsatlakozás után ismét teljesíteni kell az indítási feltételeket.

**Nincs 60 másodperces dekóderleállítás.** A 60 másodperc most a tanuló mód alapidőtartama. A safe mode „Boot seems successful” visszajelzése külön helyreállítási mechanizmus része.

A szünet alatt érkező jelek nem kerülnek későbbi visszajátszásra. API-feliratkozás nélkül ez a fő konfiguráció szándékosan nem végez offline rádiós vezérlést.

## 8. Pluginválasztás

```yaml
rflink:
  id: rf_bridge
  receiver_id: rf_receiver
  rx_plugins: all
```

| Választás | Eredmény az eredeti feltöltött készletből |
|---|---|
| `all` | 48 aktuális `.c` plugin RX-része, a 001-essel együtt |
| `configured` | Az eredeti `_Plugin_Config_01.h` által engedélyezett 47 RX-plugin |
| `[61]` | EV1527 + automatikusan a 001-es, összesen 2 |
| `[34, 40, 61]` | Cresta + Mebus + EV1527 + a 001-es, összesen 4 |

Az eredeti könyvtárban **48 `.c`, 5 `.old` és 1 konfigurációs fejléc**, összesen 54 fájl volt. Az `.old` állományok megmaradnak, de nem külön aktuális dekóderek. A 083-as az `all` része, az eredeti `configured` választásé nem.

A `stage_sources.py` generálja a kiválasztott pluginok regiszterét és az azonos tartalmú `.c.inc` másolatokat. A pluginokat így C++ környezetbe illeszti; nem indul mellettük külön, téves C-fordítás.

A PROGMEM-kezelés és a 083-as `PSTR`/mutatótípus-illesztése a kompatibilitási rétegben történt. **Nem kell az eredeti pluginfájlokban javítgatni a sorokat**, és a generált buildmásolatot sem érdemes kézzel módosítani.

A 037-es plugin `%x` / `unsigned long` formátumfigyelmeztetése ismert, nem elnémított típuseltérés. A korábbi próbafordításokban warningként megmaradhatott; a teljes fordítás eredményét a végső siker/hiba dönti el. A warning nem igazolja az adott plugin minden futási ágának helyességét.

A teljes készletnél előfordulhat protokoll-átfedés és a hagyományos JSON-kimenet eltérő ismétlésszűrése. Nem ígérhető, hogy egy korlátozott készletben EV1527-ként vett tetszőleges jel minden más dekóder mellett is ugyanoda kerül. Tanításkor ezért a **tényleges, többször megismételt vételt** használd.

## 9. Tanuló mód és a JSON értelmezése

### Bekapcsolás

Várd meg az aktív vételt, majd kapcsold be az **RF tanuló mód** kapcsolót. Ezután nyomd meg a beazonosítandó gombot. A már beállított távirányítók tanulás alatt és tanulás nélkül is működnek.

A közös konfiguráció:

```yaml
rflink_remote:
  id: rf_remotes
  rflink_id: rf_bridge
  learning:
    enabled: false
    duration: 60s
    max_signals: 4
    min_frames: 3
    log_events: true
    signal:
      id: rf_learning_signal
      name: "RF tanuló jel"
    gesture:
      id: rf_learning_gesture
      name: "RF tanuló gesztus"
```

Ez a meglévő `rflink_remote` blokk kiegészítése, nem második hub. A teljes mintában a közös `timing` is mellette szerepel.

### A mostani képen látható minta

**RF tanuló jel:**

```json
{"protocol":"EV1527","rf_id":"085372","button":"08","command":"ON","mode":"gestures"}
```

**RF tanuló gesztus:**

```json
{"protocol":"EV1527","rf_id":"085372","button":"08","command":"ON","mode":"gestures","gesture":"single","seq":1}
```

Ez a tanuló nézet megfelelő kimenete: a második JSON önmagában tartalmazza az azonosítómintát és a felismert egyszeres nyomást.

| Kulcs | Jelentés |
|---|---|
| `protocol` | A dekóder által közölt protokoll, itt EV1527 |
| `rf_id` | A rádiós azonosító, itt `085372` |
| `button` | Gombkód, itt `08`; nem gesztustípus |
| `command` | A dekóder parancsa, itt `ON` |
| `mode` | Ehhez a mintához `gestures` használható |
| `gesture` | Az adott tanulási esemény, itt `single` |
| `seq` | A tanuló gesztuskimenet sorszáma; nem kattintásszám és nem RF-azonosító |

**A `085372` a tesztelt távirányító mintája. A korábban felvett konyhai minta `01fac2`.** Ne nevezd át automatikusan az egyiket a másikra; a konfigurációban a ténylegesen használt távirányító értéke szerepeljen.

A képen látható **3 db dekódolt csomag nem tripla kattintást jelent**. A dekóderüzenetek számlálója, a rádiós keretszám és a felhasználói gesztus három külön fogalom.

A hosszú, egysoros JSON diagnosztikai szöveg, nem entitásnév. A pontos értéket az entitás részleteinél vagy a HA Fejlesztői eszközök → Állapotok nézetében másold ki. A `name:` mezőbe saját rövid nevet írj, ne ezt a JSON-t.

### Mit tanul és mit nem?

A tanuló nézet **nem hoz létre magától új távirányító-entitást**, nem párosít rádióhardvert és nem módosítja a YAML-t. A `protocol`, `rf_id`, `button`, `command`, `mode` mezőket másolod át egy saját `event:` bejegyzésbe. A `gesture` és `seq` nem az eseményplatform konfigurációs mezői.

A jelnézet csak új mintánál frissül, az ismétlődő gesztus JSON-ja viszont új `seq` értéket kap. A két tanulószenzor nem atomikus pár: például más protokoll új jelének megjelenésekor az utolsó EV1527-gesztus még régi maradhat. **A gesztust mindig a saját JSON-jában szereplő azonosítókkal együtt értelmezd.**

EV1527-nél a tanulás alapból legalább három megfelelő keretet vár, mielőtt elfogadja a mintát. Más protokollok jelnézete a dekódolt JSON-t használja; arra ez a háromkeretes gesztusmegerősítés nem általános garancia.

A tanuló gesztusnézet nem ír ki minden nyers `press`/`release` jelzést; a végleges kattintásokat, tartást és egyéb támogatott gesztusokat mutatja. A konfigurált esemény-entitás ezzel szemben az `event_types` listája szerint ezeket is kiadhatja.

A tanulás beállítható időtartama 1–300 másodperc, alapból 60 s; a követett aktív EV1527-minták száma 1–8, alapból 4. A `min_frames` 1–20 lehet. Ezek a kiegészítés saját korlátai. Ha minden tanulóhely foglalt, egy új ismeretlen jel kimaradhat; a már konfigurált távirányítók nem ettől a tanulóhely-készlettől függenek.

Az időkorlát után a tanulás kikapcsol, a legutóbbi szövegek azonban láthatók maradhatnak. Nincs flashbe mentett tanulteszköz-adatbázis. A tanulás újraindítása új időablakot kezd, a gesztussorszám nem kattintásszámláló.

## 10. Távirányító felvétele YAML-ból

A közös `rflink_remote` hub egyszer legyen jelen. Egy eseménybejegyzés **egy pontos protokoll + RF-ID + gomb + parancs kombinációja**:

```yaml
event:
  - platform: rflink_remote
    id: uj_taviranyito_08
    name: "Új távirányító 08"
    remote_id: rf_remotes
    protocol: "EV1527"
    rf_id: "085372"
    button: "08"
    command: "ON"
    mode: gestures
    event_types:
      - single
      - double
      - triple
      - hold
      - hold_repeat
      - hold_release
      - cancel
    pressed:
      name: "Új távirányító 08 nyomva"
```

**Ez a példa a már ismert `085372/08` jelet használja.** A teljes mintában ez már benne van: ne vedd fel másodszor ugyanazt a gombot, hacsak nem szándékosan akarsz párhuzamos feldolgozást. Új gombnál a valóban megtanult értékeket és új belső ID-t használd.

Másik távirányítóhoz új listatag, ugyanazon távirányító másik gombjához másik `button` és saját entitás kell. A `pressed` rész opcionális. A rádiós kódok maradjanak idézőjelezett szövegek; a kezdő nullák számítanak.

Az `event_types` elhagyása gesztusmódban minden támogatott eseményt engedélyez. A lista szűri az entitásra és a helyi `on_event` műveletre továbbított eseményeket, **nem alakítja át a duplát két single-lé**, és nem kapcsolja ki a tartás belső felismerését. A diagnosztikai log ettől még mutathat kiszűrt gesztusokat; `log_events: false` kapcsolja ki az adott bejegyzés gesztusnaplóit.

EV1527 gesztusmódban `rf_id` 20 bites hexadecimális kód (`000000`–`0fffff`), a gomb `00`–`0f`, a jelenlegi parancs `ON`. A rövidebb hexadecimális beírás normalizálódik, például `1FAC2` → `01fac2`, `8` → `08`. Más protokolloknál a tanulóban látott szöveg pontos egyezése szükséges. Nincs helyettesítő karakteres, tetszőleges gombot jelentő illesztés.

### Más protokoll: message mód

```yaml
event:
  - platform: rflink_remote
    id: masik_rf_gomb
    name: "Más protokollú RF gomb"
    remote_id: rf_remotes
    protocol: "PROTOKOLL_NEVE"
    rf_id: "RADIOS_AZONOSITO"
    button: "GOMBKOD"
    command: "PARANCS"
    mode: message
    event_types: [received]
    message_cooldown: 250ms
```

A helykitöltők cserélendők. Üres `button: ""` vagy `command: ""` a hiányzó/üres mezővel egyezik, **nem bármely gombbal vagy paranccsal**. `message_cooldown` alapból 0 ms, állíthatóan 0–60000 ms; rövid ismétlésszűrés, nem gesztusfelismerés. Message módban nincs `pressed` vagy gesztus-`timing`.

A `mode: auto` pontos `EV1527` protokollnévnél gesztusmódot, más névnél message módot választ. EV1527 is használható szándékosan message módban, de ekkor a hagyományos, ismétlésszűrt JSON-ra reagál, nem minden keretre.

### Külön fájlban tárolás

Egy saját gomb `event:` blokkja például `taviranyitok/nappali.yaml` fájlba kerülhet. A fő YAML meglévő `packages:` blokkjába:

```yaml
packages:
  sajat_nappali: !include taviranyitok/nappali.yaml
```

Ez a GitHubos adatcsomag mellé kerül, nem a helyére. A fő és a külön fájl ne definiálja kétszer ugyanazt az ID-t. Új távirányító felvételéhez új fordítás és feltöltés szükséges, C++-módosítás nem.

Másik RF-vevős ESPHome-eszközre is átmásolható az eseménydefiníció, ha ott is rendelkezésre állnak a megfelelő komponensek és a vevő. Ettől nem keletkezik automatikus hálózati RF-továbbítás egy vevő nélküli ESP felé. Másik fizikai készüléknek külön `esphome.name` kell.

## 11. Gesztusok és időzítések

| Gesztus | Jelentés |
|---|---|
| `press` | Egy új lenyomási szakasz kezdete |
| `release` | A becsült felengedés |
| `single`, `double`, `triple` | Egy, két vagy három külön rövid nyomás végleges felismerése |
| `click_4` … `click_10` | Négy … tíz rövid nyomás |
| `hold` | A tartás első felismerése, egyszer |
| `hold_repeat` | Újabb tartáslépés megfelelően friss rádiókeretek mellett |
| `hold_release` | A tartás becsült vége |
| `cancel` | Megszakítás, például vételszünet vagy felső tartáskorlát miatt |
| `multi_overflow` | A támogatott többkattintási darabszám túllépése |
| `received` | Message módú, pontosan illeszkedő dekódolt üzenet |

A közös, bevált időzítés:

```yaml
rflink_remote:
  id: rf_remotes
  rflink_id: rf_bridge
  timing:
    release_timeout: 180ms
    hold_release_timeout: 450ms
    repeat_fresh_timeout: 180ms
    multi_click_timeout: 350ms
    hold_time: 700ms
    repeat_interval: 250ms
    max_press_time: 30s
```

| Beállítás | Hatása |
|---|---|
| `release_timeout` | Rövid nyomásnál ennyi megfelelő jel nélküli idő jelzi a felengedést |
| `hold_release_timeout` | Már felismert tartásnál megengedett hosszabb kimaradás |
| `repeat_fresh_timeout` | Ennyire friss jel kell az újabb tartásismétléshez |
| `multi_click_timeout` | A következő rövid nyomásra várakozó időablak |
| `hold_time` | Tartásfelismerési küszöb, valódi keretsorozat alapján |
| `repeat_interval` | A tartásismétlések célidőköze; nem garantált hálózati késleltetés |
| `max_press_time` | Biztonsági felső korlát egy nyomási szakaszra |

Az egyszeres nyomás végleges eldöntése nem azonnali: meg kell várni, érkezik-e újabb kattintás. Az alapértékekkel körülbelül 180 + 350 = **530 ms** telik el az utolsó megfelelő kerettől, az ütemezési késleltetésen felül. A `press` gyorsabb, de dupla/tripla sorozatban is jelentkezik.

A holdfix1 már felismert tartásnál áthidalhat 180–450 ms közötti keretkimaradást. Az ismétléshez azonban új keret és a frissességi feltétel is szükséges; **nem generál végig mesterséges fényerőparancsokat a teljes 450 ms-os csend alatt**. Nincs elmaradt lépések gyors utólagos bepótlása.

A felengedés becslés, mert a jelenlegi EV1527 megfigyelésben nincs külön OFF/release keret. A távirányítónak ismételnie kell a jelet, amíg nyomják. Egy már felismert tartás utáni, 450 ms-nál rövidebb valódi elengedés és újranyomás összemosódhat. A tartás felismerése előtti rövid határ változatlanul 180 ms.

Egy eseménybejegyzés saját `timing:` blokkal is kaphat eltérő beállítást. **Saját `timing` jelenlétében a hiányzó mezők a beépített alapértékeket kapják, nem a közös blokk felülírt értékeit öröklik.** A tanuló gesztus a közös időzítést használja, ezért az egyedileg hangolt eseménytől eltérően dönthet.

## 12. Konyhai lámpa: single és hold_repeat

A kért, már elkészített konfiguráció: [examples/fragments/ev1527-light-control.yaml](examples/fragments/ev1527-light-control.yaml). A teljes fő példa szándékosan nem tartalmaz saját RF-ID-t vagy Home Assistant lámpaazonosítót.

A kötés:

```yaml
protocol: "EV1527"
rf_id: "01fac2"
button: "08"
command: "ON"
mode: gestures
event_types: [single, hold_repeat]
```

Céllámpa: **`light.clt2_konyha_clt2_konyha`**.

| Esemény | Számítás |
|---|---|
| `single` | A HA által ismert fényerő +50; 255 felett 0. A kapott eredeti automatizmus logikája. |
| `hold_repeat` | +20, legfeljebb 255; 255 után a következő ismétlés 0, utána 20-tól folytatódik. |

Nulláról induló példa:

```text
single:      0 → 50 → 100 → 150 → 200 → 250 → 0 → 50 …
hold_repeat: 0 → 20 → 40 → … → 220 → 240 → 255 → 0 → 20 …
```

A `hold_repeat` ág kikapcsolt lámpánál külön is nullának veszi a kezdőértéket, és `transition: "0"` értéket küld. A `single` ág megtartja a felhasználótól kapott eredeti attribútumalapú viselkedést. A 0 a HA fényerőskáláján kikapcsolást jelent. [Külső háttér: lámpavezérlés][ha-light]

A tényleges ESPHome-részlet:

```yaml
on_event:
  then:
    - if:
        condition:
          lambda: return event_type == "single";
        then:
          - homeassistant.action:
              action: light.turn_on
              data:
                entity_id: light.clt2_konyha_clt2_konyha
              data_template:
                brightness: >-
                  {% set current = state_attr('light.clt2_konyha_clt2_konyha', 'brightness') %}
                  {% if current is none %}{% set current = 0 %}{% endif %}
                  {% set new_brightness = (current | float(0) + 50.0) | int %}
                  {{ 0 if new_brightness > 255 else new_brightness }}
    - if:
        condition:
          lambda: return event_type == "hold_repeat";
        then:
          - homeassistant.action:
              action: light.turn_on
              data:
                entity_id: light.clt2_konyha_clt2_konyha
                transition: "0"
              data_template:
                brightness: >-
                  {% set current = state_attr('light.clt2_konyha_clt2_konyha', 'brightness') | int(0) %}
                  {% if is_state('light.clt2_konyha_clt2_konyha', 'off') %}
                    {% set current = 0 %}
                  {% endif %}
                  {% if current >= 255 %}
                    0
                  {% else %}
                    {{ [current + 20, 255] | min }}
                  {% endif %}
```

Ezt az eseménybejegyzés alá kell illeszteni, nem felső szintű eszközbeállításként. A `data_template` Jinja-kifejezéseit a HA értékeli ki; az `event_type` az ESPHome helyi `on_event` változója.

**A HA ESPHome-integrációjánál engedélyezni kell:** „Allow the device to perform Home Assistant actions”. Enélkül az esemény-entitás működhet, miközben a lámpaparancs tiltott. Az engedély nem szükséges pusztán az esemény-entitások megjelenítéséhez. [Külső háttér: API-műveletek][api]

Nem indítunk külön végtelen HA-ciklust: egy `hold_repeat` egy parancs. A `hold`, `release`, `hold_release`, dupla és tripla nem vezérli a lámpát ebben a konfigurációban. A tanuló nézet ezek közül továbbra is mutathat más gesztusokat, mert az nem a konyhai entitás kimeneti szűrőjét használja.

A régi, azonos gombra reagáló MQTT-s vagy HA-eseményautomatizmus ne vezérelje párhuzamosan ugyanazt a lámpát. A HA-ba elküldött esemény és az `on_event` művelet egyszerre is létezhet, de a lámpavezérlést egy helyen végezd.

A célérték minden alkalommal a **HA legutóbb ismert fényerejéből** készül. Lassú visszajelzésnél ismétlődhet azonos célérték, ezért ez nem garantáltan minden 250 ms-ban pontosan egy új, fizikai +20 lépés. A `transition: 0` nem oldja meg önmagában az elavult állapot kérdését. A konkrét lámpa végponttól végpontig tartó próbája a tanulóképből nem igazolható.

## 13. Szenzoradatok és átszámítások

A `rflink-ha-data-only.yaml` a legutóbbi dekódolt JSON mezőiből frissíti az **RF utolsó…** entitásokat. A parser a korábbi formázóban definiált **34 mezőt** kezeli. A részletes mezőtérkép: [FIELD_MAP.md](FIELD_MAP.md).

| RF-mezők | Jelenlegi kezelés |
|---|---|
| `PARAM`, `NAME`, `ID`, `SWITCH`, `CMD` | Eredeti csomagsorszám, protokollnév, RF-ID, gomb és parancs |
| `TEMP`, `WINCHL`, `WINTMP` | Hexadecimális, előjel–abszolútérték formátum; 0x8000 előjelbit; osztás tízzel; °C |
| `HUM` | Decimális páratartalom, %; a BCD-kezelés a híd formázásánál történik |
| `BAT` | `OK`/`LOW` szöveg és alacsony-elem bináris szenzor |
| `RAIN` | Hexadecimális érték /10, mm |
| `RAINRATE` | Hexadecimális érték /10, a forrás szerinti mm; nincs kitalált mm/h időalap |
| `WINSP`, `AWINSP` | Hexadecimális érték /10, km/h |
| `WINGS` | Hexadecimális egész, nyers: a közös skála nem kellően meghatározott |
| `WINDIR` | 0–15 kód ×22,5, fok |
| `BARO`, `UV`, `LUX` | Hexadecimális egész, nyers fizikai egység hozzárendelése nélkül |
| `HSTATUS` | Eredeti kód és szöveg: 0 normál, 1 komfortos, 2 száraz, 3 nedves |
| `BFORECAST` | Eredeti kód és szöveg: 0 nincs információ, 1 napos, 2 részben felhős, 3 felhős, 4 eső |
| `PIR`, `SMOKEALERT` | ON/OFF alapú mozgás-, illetve füstjelzés |
| `SET_LEVEL` | Szintkód, nem automatikusan a HA 0–255 fényerőskálája |
| `CHIME` | Dallamsorszám |
| `CO2`, `SOUND` | Decimális szám, nyers egység/skála nélkül |
| `KWATT` | Hexadecimális egész, nyers; nem automatikusan kWh |
| `WATT` | Hexadecimális egész, W |
| `CURRENT`, `DIST`, `METER`, `VOLT` | Decimális szám, a konkrét eszköz szerinti skála ellenőrzendő |
| `RGBW` | Eredeti hexadecimális kód; nincs feltételezett bájtsorrendű RGB-konverzió |

Szintetikus ellenőrző példák, **nem valódi mérések**:

```text
TEMP="00ea" → 23,4 °C
TEMP="8037" → −5,5 °C
RAIN="008d" → 14,1 mm
WINDIR=4    → 90°
```

A `BAT` nem töltöttségi százalék és nem elemfeszültség. A `LOW` jelzi az alacsony elemet; hiányzó mezőből nem készül „jó elem”. A nyers értékek egységét a konkrét dekóder és érzékelő alapján kell igazolni, nem a mezőnévből kitalálni.

### Közös pillanatkép, nem állandó szobaszenzor

A közös diagnosztika alapból **legfeljebb másodpercenként**, az időközben érkezett legutolsó dekódolt üzenetből frissül. A távirányító-események külön adatúton mennek; nem erre a pillanatképre várnak.

Ha egy hőmérős üzenetet egy hőmérséklet nélküli távirányítóüzenet követ, a közös **RF utolsó hőmérséklet ismeretlenre vált**. Ez szándékos: nem hagyja az egyik eszköz ID-je mellett egy másik eszköz régi mérését. A mezők több külön API-állapotüzenetben frissülhetnek, nem atomikus tranzakcióként.

Automatizmushoz és hosszú távú grafikonhoz **protokoll + RF-ID szerint rögzített szenzort** használj. A közös pillanatkép és a tanuló szövegek nem teljes üzenetarchívumok.

Az **RF dekódolt csomagok** számláló az indulás óta a feldolgozó által elfogadott, `NAME` és `ID` mezős dekóderüzeneteket számolja, és 5 másodpercenként jelenik meg. Nem azonos a fizikai gombnyomások vagy gesztusok számával. Az utolsó csomag kora segít a megmaradt régi adat felismerésében.

### Hosszú JSON

Az **RF utolsó üzenet** 250 bájtig teljes JSON-t, hosszabb üzenetnél rövid összefoglalót mutat. A mezőfeldolgozás ettől még megtörténik.

Az opcionális teljes kimenethez a fő YAML meglévő `substitutions` blokkjában:

```yaml
substitutions:
  rflink_full_json_parts: "true"
```

Fordítás/feltöltés után a HA-ban is engedélyezd az **RF teljes JSON 1…5** alapból letiltott entitásokat. A részeket sorrendben, elválasztó nélkül összerakva áll elő a JSON. Ez nem nyers impulzus-dump. Nagy egész mérőállásoknál a pontos eredeti értékhez a JSON megbízhatóbb lehet a float típusú numerikus szenzornál.

## 14. Állandó entitások egy mérőeszköznek

A korábban készített `tools/make_rf_device.py` a kiválasztott mezőkből kis, konkrét **NAME + ID** pároshoz kötött YAML-csomagot generál. A README-hez a generátor és a szükséges `FIELD_MAP.json` változatlan másolata is jár.

A projekt gyökeréből, Python és PyYAML mellett:

```bash
python -m pip install pyyaml
python tools/make_rf_device.py --prefix kert_rf --name "Kert" --protocol "Cresta" --rf-id "CSERELD_A_TENYLEGES_IDRA" --fields TEMP,HUM,BAT --stale-after 60min --output packages/rflink-kert.yaml
```

**A protokoll és az RF-ID a saját valódi mérőeszköz adata legyen.** A parancs helykitöltős mintát tartalmaz; a generátor nem tudja, hogy a megadott ID valóban létezik-e. A `--prefix` legyen egyedi. Meglévő célfájlt a generátor nem ír felül automatikusan.

A kész fájlt töltsd a repóba, és a meglévő csomaglista bővüljön:

```yaml
files:
  - packages/rflink-ha-data-only.yaml
  - packages/rflink-kert.yaml
```

A generált csomag saját, **listás `rflink.on_message`** bejegyzést tesz hozzá. A fő konfigurációban maradjon a listás forma, hogy a csomagok kezelői összefűzhetők legyenek. Ne cseréld le az alap `rflink_api_process` hívást.

A numerikus és bináris adatok a választott ideig nem frissülve ismeretlenné válnak, alapból 60 perc után. Hiányzó mező ugyanazon érzékelő másik csomagjában nem törli azonnal az előző értéket; a mező saját lejárata működik. Jelen levő, de hibás adat viszont ismeretlen lehet. A szöveges mezők az utolsó ismert értéket tartják meg, automatikus lejárat nélkül.

A `--fields ALL` minden definiált mezőt felvesz az adott érzékelőhöz. ESP8266-on célszerű csak a ténylegesen létező adatokat kiválasztani. Ez **fordítás előtti csomaggenerátor**, nem dinamikus eszközfelderítés.

## 15. MQTT mint opcionális kimenet

A jelenlegi fő példa nem tartalmaz MQTT-t. A natív API-s entitások és a tanuló mód anélkül működnek.

Párhuzamos, opcionális JSON-kimenethez külön `mqtt:` blokk adható meg a saját brokeradatokkal:

```yaml
mqtt:
  broker: !secret mqtt_broker
  username: !secret mqtt_username
  password: !secret mqtt_password
  discovery: false
  log_topic: null
  reboot_timeout: 0s
```

A meglévő `rflink.on_message` listán belül az alap adatfeldolgozás után:

```yaml
on_message:
  - then:
      - script.execute:
          id: rflink_api_process
          message: !lambda return x;
      - mqtt.publish:
          topic: RFLink/msg
          qos: 0
          retain: false
          payload: !lambda return x;
```

Ez az eredeti dekóder-JSON-t küldi; a régi MQTT-rendszer egyedi `TIME`, `IP`, `MAC`, `SIGNAL` kiegészítéseit és LWT-topicját nem hozza vissza automatikusan. A `log_topic: null` csak az MQTT-naplót tiltja, az explicit publikálást nem. [Külső háttér: MQTT-komponens][mqtt]

**A jelenlegi API-függő indítási logika ettől nem változik meg.** Ha a HA állapotfeliratkozása hiányzik, nincs vétel/dekódolás, tehát új RF MQTT-kimenet sincs. Teljesen MQTT-only, HA-tól független használathoz külön indítási logikát kell készíteni; ez a README nem vezeti be azt észrevétlenül.

## 16. OTA és biztonsági mód

A fő konfiguráció OTA-kezdéskor csak `id(rf_bridge).set_ota_active(true)` hívást ad a komponensnek. A komponens leállítja a dekódolást és a saját RF GPIO-capture-t, majd OTA-hibánál/újraengedélyezésnél ismét a szokásos hálózati + API stabilizációs kapun keresztül indul. A hosszú leállító lambda már nincs a YAML-ban.

Frissítés előtt a **RFLink figyelés** kézzel is kikapcsolható. Ez már az OTA-kézfogás előtt szünetelteti a rádiós feldolgozást. A felesleges párhuzamos hálózati naplóolvasókat zárd be.

A feltöltés közbeni API-naplószakadás önmagában nem bizonyít sikertelen OTA-t. A feltöltőablak sikeres befejezése, majd az új program indulási naplója külön ellenőrzés. Az új firmware OTA-fogadási javítása csak annak telepítése után lép életbe: a folyamat elejét még a régi firmware fogadja.

A mintában szereplő **RFLink OTA helyreállítás** safe mode-ba indítja újra az eszközt. Ilyenkor a hálózat, soros naplózás és OTA marad használatban, a szokásos RF-/HA-entitásfunkciók nem. Ez helyreállítási lehetőség, nem rádiós kapcsoló. [Külső háttér: safe mode gomb][safe-mode]

Sikeres feltöltés után az 1–2 perces megfigyelés gyakorlati első próba, nem kötelező többperces indulási késleltetés. Tartós szakadozásnál ne a várakozást növeld korlátlanul: a teljes USB-indulási naplót nézd meg. Ne szakíts meg ismerten folyamatban lévő flashírást.

## 17. Naplózás és hibakeresés

### Alecto V1: látszó RF jel, de nincs stabil dekódolás

Ha egy korábban működő Alecto V1/Plugin 030 eszköz hirtelen csak szabálytalan impulzussorokat ad, az elemet is ellenőrizd. A projekt hardveres tesztjében gyenge elemmel még volt rádiós aktivitás, de nem állt össze stabilan érvényes keretté; elemcsere után az Alecto 006C adatai ismét rendesen érkeztek. Emiatt ehhez az esethez nem került lazább Plugin 030/framing kerülőmegoldás a v0.1.9-be.


### Elvárt verzió- és állapotjelzések

```text
RFLink RX compatibility bridge v0.1.9 (optimized active dispatch; self-healing RX)
RX plugins compiled: 48
Remote Receiver rxgate2 (ESP8266 / based on 2026.9.0)
High frequency configured: NO
[rflink.remote]: v0.1.9: YAML-configured remotes; fast single-click; compact learning; slots=4
```

Megfelelő HA-kapcsolat után:

```text
AUTO=ON; CAPTURE=ON; DECODE=ON; api_states=YES; network=CONNECTED; fast_loop=OFF
```

A fő diagnosztika `AUTO=ON` mezője a figyelési kapcsoló engedélyét tükrözi. Kikapcsoláskor a számlálók megállnak, nem kell nullázódniuk. A bináris aktív-dekódolás visszajelzés közvetlenül a komponens állapotváltásakor frissül.

| Naplómező | Jelentés |
|---|---|
| `uptime` | Indulás óta eltelt idő, másodperc |
| `heap`, `max_block`, `frag` | Szabad dinamikus memória, legnagyobb szabad blokk, töredezettség |
| `frames` | A vevő által átadott nyers impulzussorok száma; rádiós zaj is lehet |
| `calls` / `skipped` | Dekóderhívások / letiltás miatt át nem dolgozott sorok |
| `decoded` | Az adatcsomag-feldolgozó által elfogadott dekóderüzenetek |
| `observed` | Keretszinten megfigyelt, elfogadott EV1527-keretek, elnyomott ismétlésekkel együtt |
| `irq_total` | A saját RF GPIO-megszakításkezelő hívásszáma |
| `rx_loop_calls` | A bekapcsolt vevő főciklusbeli feldolgozásainak száma |
| `overflow_reports` | Észlelt ring-buffer túlcsordulások; v0.1.9-ben automatikus capture-resync követi |
| `recoveries` | Automatikus RX újraszinkronizálások száma (overflow vagy tartósan lezáratlan impulzussor) |
| `extra_drained` | Az elsőn felül ugyanabban a főciklusban feldolgozott backlog-keretek száma |
| `max_drain_batch` | A legnagyobb egy főciklusban feldolgozott keretszám; v0.1.9-ben legfeljebb 4 |
| `history_resets` | Legacy ismétlésszűrő állapot ürítéseinek száma; hosszabb RF-csend és RX-resync is növelheti |
| `decode_max_us` | Egy mért dekóderhívás legnagyobb ideje |
| `callback_max_us` / `frame_callback_max_us` | Üzenet- / keret-visszahívások mért legnagyobb ideje |

Az időmérések helyi feldolgozási időket mutatnak, **nem a teljes gombnyomás → Wi-Fi → HA → lámpa késleltetést**. Az `overflow_reports=0` nem jelenti, hogy minden rádiókeret hibátlanul megérkezett.

### Gyakoribb helyzetek

| Tünet | Első ellenőrzés |
|---|---|
| `high_frequency is an invalid option` | Valóban az rxgate2 `remote_receiver` töltődött-e be? Repófájlok, külső komponenslista, Git/package cache, majd buildtisztítás. Ne töröld megoldásként a szükséges beállítást. |
| `capture_enabled` ismeretlen | Ugyanígy a külső vevő hiányzik vagy régi forrás fut. |
| Wi-Fi jó, `api_states=NO`, vétel OFF | HA-integráció aktuális címe, API-kulcsa és állapotfeliratkozása; a naplóolvasó nem elegendő. |
| Wi-Fi `Beacon Timeout` | Indulási állapot, `fast_loop=OFF`, a vétel ki-/bekapcsolása és memóriaadatok összehasonlítása. Nem bizonyít automatikusan jelszóhibát. |
| `Restarting adapter` | A Wi-Fi-adapter újrapróbálkozása; önmagában nem teljes ESP-reset. |
| `Exception`, új boot és újra kis uptime | Valódi reset/összeomlás lehet; teljes USB-stack és a hozzá tartozó firmware szükséges. |
| Esemény-entitás `unknown` | Lehet, hogy még nincs esemény. `unavailable` ezzel nem azonos. |
| Tanuló bekapcsolva, nincs jel | Fő figyelés/aktív vétel, támogatott protokoll, megfelelő tanulási keretszám és valós ismételt vétel. |
| A tanulóban van single, a konyhai lámpa nem reagál | `085372` nem a konyhai `01fac2`; pontos kötés, `event_types`, HA műveletengedély és céllámpa ellenőrzése. |
| Hőmérséklet eltűnik egy gombnyomás után | A közös pillanatképben ez szándékos; állandó méréshez saját NAME+ID szenzor kell. |
| Tartás többször új press-szel indul | Valódi elengedések vagy túl hosszú megfelelőkeret-hiány. Előbb a rádiós naplót és a holdfix1 meglétét ellenőrizd. |
| Figyelés ON, tanuló OFF | Normál üzem: a felvett távirányítók továbbra is működnek. |
| Több perc csend után csak sokadik gombnyomásra ébred | v0.1.9 önjavítás: nézd a `recoveries`, `history_resets`, `frames`, `calls`, `observed` számlálókat. Overflow vagy beragadt részkeret automatikusan resyncel; az első hosszú csend utáni dekódolás előtt a legacy repeat history ürül. |
| Átnevezés után régi, nem elérhető kapcsoló is látszik | Firmware-beli névcsere HA-regisztrációs következménye lehet; hivatkozások ellenőrzése, nem teljes integrációtörlés. |

A nyers `dump` csak rövid, célzott hibakeresésre való. A korábbi próbákban a sok nyers naplózás mellett az MQTT használata is problémásnak tűnt. Normál üzemben ne küldj minden impulzussorból naplóüzenetet.

## 18. Ellenőrzött működés és korlátok

**Hardveres visszajelzések és a feltöltött naplók alapján:** az rxgate2 `fast_loop=OFF` mellett több bekapcsolt-vételi próba látható Wi-Fi/API-szakadás nélkül; az `085372/08` gombnál single, double, hold és hold_repeat is megjelent. A holdfix1 utáni naplóban több másodperces tartások egyben maradtak; az egyik sorozat ismétlései körülbelül 251–269 ms közönként készültek. A külön rövid tartások egy részéről a felhasználó megerősítette, hogy valóban külön nyomások voltak.

A legújabb tanulóképen a HA már megkapta az **EV1527 / 085372 / 08 / ON / single** tanulási eredményt. Ez igazolja ezt a jel- és tanuló kimenetet, **nem az összes protokollt, a konyhai lámpa fizikai reakcióját vagy a napokon át tartó stabilitást**.

**Korábban jelentett host-tesztek:** a v0.1.6 `TEST_RESULTS.txt` szerint a 48 és 47 pluginos motorral 85–85, a szűkített EV1527-készlettel 83 futási ellenőrzés, továbbá 29 alapgesztus- és 34 holdfix-eset futott le. Ezek helyettesített ESPHome/JSON környezetű számítógépes tesztek voltak. Az ottani jelentés szerint nem történt teljes Xtensa-fordítás, rádiós hardverszimuláció vagy az opcionális GitHub-workflow futtatása.

**A v0.1.9 regressziós ellenőrzése a `tests/` készlettel történik.** A hosttesztek nem helyettesítik az ESP8266 célfordítást és a hosszú, valódi rádiós hardvertesztet; kiadás előtt az eszközön is ellenőrizni kell az idle utáni első gombnyomást, overflow/recovery számlálókat és a Wi-Fi/API stabilitását.

Fontos határok:

- Az eredeti 48 RX-plugin lefordítása/bevonása nem igazolja minden eszköz valódi vételét. Protokoll-átfedések és régi pluginfigyelmeztetések megmaradhatnak.
- Gesztusok jelenleg EV1527-hez; más protokollhoz message-alapú esemény. Tanítás csak felismert, dekódolt jelhez, nem univerzális nyers RF-analízishez.
- A vétel API-állapotfeliratkozástól függ, kiesés alatt nincs archívum vagy utólagos visszajátszás. Az érzékelt `ON` nem a lámpa visszaigazolt állapota.
- A feltételezett jelazonosítás nem hitelesítési mechanizmus. A vételi híd nem tanúsított riasztó/biztonsági rendszer; füst- vagy mozgásadat megjelenítése nem helyettesíti az önálló biztonsági eszközt.

## 19. Források és karbantartás

### Projektforrások

A leírás elsődleges alapja a beszélgetésben kiadott v0.1.6 forráscsomag, annak `README_HU.md` és `TEST_RESULTS.txt` fájlja, a v0.1.5 motor, az rxgate2 vevő, a holdfix1 fejléc, a `FIELD_MAP.json`/`rflink_fields.h`, a korábbi érzékelőgenerátor és a konyhai YAML-részlet. A rádiós mezők nevei és a kifejezetten leírt skálák az eredetileg feltöltött `display_*` forrásból származnak. A képek és felhasználói naplók a hardveres visszajelzések alapjai.

A README a helyben rendelkezésre álló fájlokat írja le; **nem állítja, hogy a GitHub `main` ágának élő tartalmát vagy a készüléken futó aktuális binárist kiolvasta**. A dokumentációs csomag nem publikál változtatást a repóba.

A később módosított GPIO, szűrés, időzítés, hardver vagy helyi automatizmus a saját konfigurációban az elsődleges. Az eredeti pluginokat és a hozzájuk tartozó licenceket őrizd meg. A dokumentációs csomag nem változtatja meg a források licencét.

### Külső háttérdokumentáció

Ezek a beépített ESPHome/HA-interfészekről szólnak; a projekt egyedi `rflink_remote`, `capture_enabled` és `high_frequency` opcióit nem ezek vezetik be.

- [ESPHome natív API és Home Assistant-műveletek][api]
- [ESPHome esemény-entitások][event]
- [Külső komponensek][external]
- [YAML-csomagok és összefésülés][packages]
- [Home Assistant ESPHome-integráció][ha-esphome]
- [Home Assistant esemény-entitás][ha-event]
- [HA-entitások névkezelése][ha-names]
- [ESPHome 2026.9.0 entitáskulcsok – forrás][entity-base]
- [Home Assistant lámpaműveletek][ha-light]
- [ESPHome MQTT-kliens][mqtt]
- [ESPHome safe mode gomb][safe-mode]

[api]: https://esphome.io/components/api/
[event]: https://esphome.io/components/event/
[external]: https://esphome.io/components/external_components/
[packages]: https://esphome.io/components/packages/
[ha-esphome]: https://www.home-assistant.io/integrations/esphome/
[ha-event]: https://www.home-assistant.io/integrations/event/
[ha-names]: https://www.home-assistant.io/docs/configuration/customizing-devices/
[entity-base]: https://raw.githubusercontent.com/esphome/esphome/2026.9.0/esphome/core/entity_base.cpp
[ha-light]: https://www.home-assistant.io/integrations/light/
[mqtt]: https://esphome.io/components/mqtt/
[safe-mode]: https://esphome.io/components/button/safe_mode/
