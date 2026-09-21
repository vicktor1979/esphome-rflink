# RFLink → ESPHome, v0.1 RX kísérleti alap

**GitHubról betölthető ESPHome külső komponens, változatlan `RFLink/Plugins` könyvtárral.**

Ez egy működőképes irányt demonstráló **vételi prototípus**, nem a teljes RFLink firmware kész átirata.
A helyi C++ tesztek lefutottak; valódi ESPHome firmware-fordítás és rádiós hardverteszt itt NEM történt.
Az első telepítés előtt olvasd el a korlátokat és őrizd meg a működő RFLink firmware-t.

## Mi maradt változatlan?

A feltöltött `RFLink-5.5wj(2).zip` teljes `RFLink/Plugins` könyvtárának **mind az 54 fájlja bájtról bájtra azonos**:
48 darab `Plugin_###.c`, 5 darab `.old` és `_Plugin_Config_01.h`.
Az eredeti konfiguráció 47 RX plugint engedélyez, a 083-at nem; TX oldalon a 004-et engedélyezi.
A prototípus **csak RX-et fordít**, az eredeti TX-beállítás nem aktivál adást.
A `.old` fájlok megmaradnak, de nem fordulnak le.

A `UPSTREAM_SHA256.json` ellenőrzőösszegeket tartalmaz. A `.gitattributes` megakadályozza az eredeti
fájlok sorvégeinek Git általi átírását. A könyvtárat a generátor csak olvassa.

## Felépítés

```text
esphome-rflink/
├── components/rflink/
│   ├── __init__.py             # YAML validálás, ESPHome kódgenerálás
│   ├── stage_sources.py        # automatikus pluginlista, változatlan fordítási másolatok
│   ├── rflink.h / rflink.cpp    # ESPHome remote_receiver listener, on_message esemény
│   └── rflink_engine.h / .cpp   # RawSignal és display_* kompatibilitási réteg
├── RFLink/
│   ├── Plugins/                # az eredeti teljes könyvtár
│   ├── 2_Signal.h, 4_Display.h, …
│   └── 7_Utils.cpp
├── examples/
│   ├── esp8266-d1-mini.yaml
│   ├── esp32-arduino.yaml
│   └── secrets.example.yaml
├── tests/
├── .github/workflows/build.yaml
├── UPSTREAM_SHA256.json
└── UPSTREAM_LICENSE.txt
```

Adatút: **RF vevő → ESPHome remote_receiver → RawSignal átalakítás → eredeti RFLink pluginok → JSON → ESPHome MQTT.**
Az eredeti blokkoló `ScanEvent/FetchSignal` helyett az ESPHome rögzíti az impulzusokat.
A Wi-Fi, MQTT, időszinkronizálás és OTA is az ESPHome feladata. Az eredeti
Wi-Fi/MQTT, AutoConnect, OLED és credentials részek nem kerültek át.

Az RFLink `.c` pluginjai valójában C++ fordítási egységbe beillesztett forrásrészletek.
A generátor a build `src/rflink_vendor/Plugins` könyvtárában `.c.inc` nevű, **azonos tartalmú** másolatokat
készít, és azokat egyetlen C++ fordítási egységbe illeszti. A repóban a fájlnevek is eredetiek maradnak.
Nem történik keresés-csere vagy pluginforrás-átírás. Minden `PLUGIN_###` definíció a 001 beillesztése ELŐTT
létrejön, így a protokollfüggő előfeldolgozás feltételes részei megmaradnak.

## GitHub és ESPHome beállítása

1. Hozz létre egy `esphome-rflink` nevű GitHub repót.
2. A kicsomagolt projekt **tartalmát** töltsd a repó gyökerébe. A gyökérben közvetlenül a
   `components`, `RFLink`, `examples` könyvtárak legyenek, ne egy további `esphome-rflink` szint alatt.
   A ZIP egyetlen fájlként történő feltöltése nem elegendő. Rejtett fájlokat is tölts fel.
3. Válaszd ki az `examples/esp8266-d1-mini.yaml` vagy `examples/esp32-arduino.yaml` mintát.
4. Írd át a `YOUR_GITHUB_USER` helyőrzőt. A `main` ág feleljen meg a tényleges ág nevének.
5. Ellenőrizd a vevő adatvezetékének GPIO-ját és a tápellátás/engedélyezés bekötését.
6. A Wi-Fi/MQTT/OTA adatokat csak a saját ESPHome `secrets.yaml` fájlodba írd.
7. Futtass **Validate**, majd **Install → Manual download / Compile** műveletet. A buildlogot vizsgáld meg
   a készülék felülírása előtt. A csomag még nem bizonyítottan fordul le az általad használt ESPHome kiadással.

```yaml
external_components:
  - source: github://YOUR_GITHUB_USER/esphome-rflink@main
    components: [rflink]
    refresh: 5min
```

Fejlesztés közben az 5 perces ellenőrzés kényelmes. A stabil, kipróbált állapotot később saját taghez/commit-hoz
rögzítsd. A GitHubra feltöltés nem frissíti magától a készüléket: új fordítás és firmware-feltöltés kell.
Privát repónál a teljes `type: git` forráskonfiguráció és a szükséges hitelesítés használható;
a token maradjon `!secret` hivatkozás mögött. GitHub-only korlátozás nincs: helyi és általános Git-forrás is támogatott.

A mellékelt GitHub Actions két ESPHome fordítási próbát és a host teszteket tartalmazza.
A workflow itt nem futott le. A `stable` konténer változó verziót jelent; sikeres teszt után célszerű rögzíteni.
A build fixture helyi komponenst használ, a GitHubos letöltési útvonalat külön a készülék YAML-jával kell ellenőrizni.

## Platform és bekötés

A feltöltött projekt `platformio.ini` fájlja ESP8266/D1 mini célt választ, és a régi forrásban
ESP8266-specifikus definíciók is vannak. A mintában GPIO5 (D1) az RX adatpin, az eredeti alapérték szerint.
Ez **nem bizonyítja a tényleges bekötésedet**: a régi webes konfiguráció ettől eltérhetett.

A külön ESP32 példa klasszikus `esp32dev`, GPIO27 példabemenettel. A szükséges keretrendszer:

```yaml
esp32:
  board: esp32dev
  framework:
    type: arduino
```

Ez a kompatibilitási réteg Arduino API-t használ, önálló ESP-IDF módban nem támogatott.
A példa nem ESP32-C6 vagy H2 konfiguráció, és nem helyettesíti a konkrét panel kiválasztását.

A régi projektben külön RX táp/engedélyező GPIO is szerepelhet. A prototípus nem kapcsol be
vakon egy ilyen lábat. **Csak ha a saját áramköröd tényleg aktív magas GPIO14-es NMOS vezérlést használ**, a D1 mini
YAML-ban például hozzáadható:

```yaml
switch:
  - platform: gpio
    id: rf_receiver_power
    internal: true
    pin: GPIO14
    restore_mode: ALWAYS_ON
```

Eltérő kapcsolásnál a pin és az invertálás is eltérhet. Ellenőrizd a közös földet és a vevő kimeneti jelszintjét;
a prototípus nem ad felhatalmazást 5 V-os jelek közvetlen ESP GPIO-ra kötésére.

## Pluginválasztás a források szerkesztése nélkül

```yaml
rflink:
  receiver_id: rf_receiver
  rx_plugins: configured
```

- `configured`: az eredeti `_Plugin_Config_01.h` aktív RX definíciói, jelenleg 47.
- `all`: az összes `Plugin_###.c` RX része, jelenleg 48. Ez sem kapcsolja be a TX kódot.
- `[34, 40, 61]`: kiválasztott RX pluginok; a kötelező 001 automatikusan hozzáadódik.

A 001 az előfeldolgozó, a 034 Cresta, a 040 Mebus, a 061 EV1527. Az első célzott próba:

```yaml
rflink:
  receiver_id: rf_receiver
  rx_plugins: [34, 40, 61]
  on_message:
    - mqtt.publish:
        topic: RFLink/msg
        retain: false
        payload: !lambda return x;
```

Ez egy részlet, nem a teljes konfiguráció. Az `examples` mappában teljesebb minták vannak.
Új, **ugyanazt az RFLink interfészt használó** `Plugin_###.c` fájl bekerülhet a könyvtárba;
`all` vagy explicit ID-választás mellett a generátor automatikusan beilleszti újrafordításkor.
`configured` esetén csak a configban már engedélyezett ID-k kerülnek be. Ezt YAML-listával felülírhatod,
így a config fájlhoz sem kell hozzányúlni. Ismeretlen hardverfüggőségeket, más RFLink forkok eltérő
interfészeit vagy AVR regiszterkódot az illesztőréteg nem alakít át automatikusan.
A beépített hash-teszt az eredeti csomaghoz viszonyít; tudatos pluginfrissítés után a baseline-t is tudatosan frissíteni kell.
A `configured` parser egyszerű aktív `#define PLUGIN_###` sorokat kezel; nem teljes C-előfeldolgozó egy későbbi,
összetett `#if` logikával átírt konfigurációhoz.

## Meglévő MQTT automatizmusok

Az `on_message` minden tényleges dekódolt eseménynél lefut, nem egy szövegszenzor állapotváltozásához kötött.
A minták a meglévő `RFLink/msg` topicot használják, `retain: false` mellett.
A valódi, változatlan 061 dekóderrel futtatott szintetikus teszt kimenete:

```json
{"PARAM":"20;00","NAME":"EV1527","ID":"01fac2","SWITCH":"08","CMD":"ON"}
```

A `NAME == EV1527`, `ID == 01fac2`, `SWITCH == 08` feltételt használó automatizmus üzenetmezőit
nem kell átírni. Ez formátumellenőrzés, nem bizonyíték a tényleges RF vételre.
A példák `TIME`, `IP`, `MAC`, `SIGNAL` mezőket is hozzáadnak. `TIME` csak érvényes szinkronizált idővel jelenik meg;
`SIGNAL` Wi-Fi RSSI, nem RF jelerősség. A protokollból jövő `TEMP` továbbra is hexadecimális szöveg,
`HUM` JSON szám, `BAT` az `OK`/`LOW` szöveg. A negatív hőmérséklet RFLink előjelkódolása is a pluginé marad.

Az eredeti egyedi formatter néhány nem idézőjeles hexadecimális mezője hibás JSON-t eredményezhetett.
Ezeket az új formatter **idézőjeles hexadecimális szövegként** adja ki (pl. BARO, RAIN, UV).
Ezért az összes korábbi szenzor JSON-típus szerinti teljes kompatibilitása NEM garantált;
az érintett HA template-eket össze kell hasonlítani. Az EV1527 fenti kulcsmezői megegyeznek.

**Ne üzemeljen két vevő ugyanazon az eseménytopicon a próba alatt**, mert ugyanaz a gombnyomás kétszer futhat le.
Párhuzamos teszthez előbb állítsd `rflink_topic: RFLink/test/msg` értékre az új eszközt, és annak
LWT topicját is különítsd el (`RFLink/test/lwt`). A régi eszköz leállítása után válts vissza.

## Ismétlésszűrés: fontos tesztmegfigyelés

A pluginok eredeti ismétlésszűrő kódja megmaradt, nincs utólag ráerőltetett több másodperces tiltás.
A `[61]` és `[34, 40, 61]` összeállításban a teszt 100 ms utáni EV1527 ismétlése elnyomódott,
a későbbi csomag új eseményt adott. A teljes 47/48-as készlettel ugyanaz a host teszt ismételt eseményt is kapott.
A régi pluginok közös `SignalCRC` állapotot használnak, és más dekóder sikertelen próbája is módosíthatja azt.
Ezt a viselkedést itt nem írtam át. Ez a tesztkörnyezetben megfigyelt korlát, a hardveren is ellenőrizni kell.
Az első próba ezért célszerűen csak a ténylegesen használt pluginokkal történjen;
a meglévő HA oldali rövid RF-ismétlésszűrést ne töröld ki az első teszt előtt.

## Ellenőrzések és korlátok

Futtatás Linuxon/Python 3 + g++ mellett:

```bash
python3 tests/run_tests.py
```

Az elvégzett host teszt a teljes eredeti pluginmappa 54 hash-ét, a generált másolatok azonosságát,
a 47/48-as C++ RX készletek fordítását, az EV1527 szintetikus jelének dekódolását,
a célzott készlet ismétlésszűrését, valamint a hiányzó, túl rövid, túl hosszú és hibás impulzuslisták kezelését ellenőrzi.
A host Arduino shim és a számítógép adattípusai nem azonosak az ESP8266/ESP32 környezettel.
**A host teszt nem ESPHome YAML-validálás, nem firmware-build, nem rádiós hardverteszt.**

A csomaghoz mellékelt `TEST_RESULTS.txt` a tényleges futás kimenete.
A 083-as legacy plugin a host fordításban string-const figyelmeztetéseket ad; a `configured` készletben nincs bekapcsolva.

A vételi illesztés megőrzi az eredeti `RawSignal` struktúrát: byte impulzusok,
32 µs skála, 1-től indexelt adatok és 0-s marker. Nem növeli önkényesen a pluginok bufferét.
A túl hosszú csomagot eldobja, nem csonkítja érvényesnek látszó csomaggá.
Az ESPHome végső idle impulzusát normalizálja. A preambulum, a csomaghatár és az invertálás
minden ténylegesen használt protokollnál rádiós összehasonlítást igényel, különösen a hosszú csomagoknál.

Nem része ennek a verziónak: RF adás/TX és parancsfeldolgozás, az eredeti RFLink webes beállítófelület,
OLED, automatikus HA eszközlétrehozás minden ismeretlen RF eszközhöz, általános tetszőleges-fork kompatibilitás.
A debug pluginok régi `Serial.print` útvonalai nem mind jutnak az ESPHome hálózati naplójába;
nyers vételi hibakereséshez ideiglenesen `remote_receiver: dump: raw` használható.

## Következő műszaki lépés

Először valódi ESPHome fordítás, majd az EV1527 `01fac2 / 08` gomb és a használt hőmérők rádiós összehasonlítása
az eredeti firmware-rel. A teljes adási részhez külön `PluginTXCall`, parancsparszolás, `RawSendRF`/speciális
küldőrutinok, GPIO és időzítési illesztés, illetve MQTT/API akciók kellenek. Nem szabad működő TX-nek nevezni
azt, hogy a TX forrásfájlok már jelen vannak a repóban.

## Forrás és felhasználási feltételek

A `RFLink/` fájlok a felhasználó által feltöltött csomagból származnak; a szükséges kiegészítő headerek és
`7_Utils.cpp` is változatlanul maradtak. Az eredeti licencet `UPSTREAM_LICENSE.txt`, a szerzői megjegyzéseket
az eredeti fájlok őrzik. A csomag nem kapott egységes MIT/Apache átcímkézést.
Az eredeti fájlok licencmegjegyzései nem mindenhol egyformák; több plugin kereskedelmi alkalmazásra
külön korlátozást is ír. Továbbadás és üzleti felhasználás előtt ezeket tisztázni szükséges.
Valódi Wi-Fi/MQTT jelszavakat, eredeti credentials fájlt nem tettem a kiadott csomagba.

Hivatkozások az illesztéshez:
- https://esphome.io/components/external_components/
- https://esphome.io/components/remote_receiver/
- https://esphome.io/components/esp32/
- https://github.com/esphome/esphome/blob/dev/esphome/components/remote_base/remote_base.h
- https://github.com/esphome/esphome/blob/dev/esphome/components/remote_base/__init__.py
