# RFLink ESPHome 0.1.7 – javított és kibővített vételi pluginok

**Kiadási próba: 2026-09-23.** Ez forráskód-frissítés a működő **0.1.6 távirányító/tanuló + 0.1.5 motor + rxgate2 + holdfix1** rendszerhez. Nem gyári RFLink-frissítés, nem teljes RFLink32-port, és nem rádiós kompatibilitási tanúsítás. A meglévő eredeti `RFLink/Plugins` mind az 54 fájlja változatlan marad.

A bővített profil **55 választható RX-pluginazonosítót** tartalmaz: az eredeti 48, hét újonnan fordítható azonosítóval. A 048-as a régi ZIP-ben már `.old` fájl volt: az archivált Oregon-kód egy ellenőrzött részét aktiváljuk, nem számítjuk teljesen ismeretlen korábbi protokollnak. Hat teljesen új azonosító és egy ilyen aktiválás adja a hét bővítést.

**Ténylegesen végzett vizsgálatok:** számítógépes GNU C++20-fordítás, ASan/UBSan futási ellenőrzés, pontos jelminták és hibás üzenetek feldolgozása, meglévő gesztus/tanuló regresszió, YAML/Python-szerkezeti ellenőrzés és eredeti fájlok összehasonlítása. **Nem történt:** teljes ESPHome/Xtensa firmware-fordítás, eszközfeltöltés, valódi rádiós/Wi-Fi/HA próba, célgépi flash/RAM-mérés. A mellékelt GitHub Actions munkafolyamat nincs lefuttatva. Részletek: `TEST_RESULTS.txt` és `TEST_LOGS/`.

## 1. Gyors telepítés a meglévő rendszerre

Előbb mentsd el a működő készülék-YAML-t és a GitHub-repó jelenlegi állapotát. A csomagból azonos útvonalra másold:

```text
components/rflink/__init__.py
components/rflink/stage_sources.py
components/rflink/rflink.cpp
components/rflink/rflink_engine.cpp
components/rflink/rflink_engine.h
components/rflink/rflink_fields.h
RFLink/Extensions/                       # teljes új könyvtár, licencekkel együtt
packages/rflink-ha-extended-fields.yaml   # opcionális új diagnosztika
FIELD_MAP.json
FIELD_MAP.md
tools/make_rf_device.py
```

A **`components/remote_receiver/`, `components/rflink_remote/`, `components/rflink/rflink_gestures.h`, a meglévő adatcsomag és az eredeti `RFLink/Plugins/` nem cserélendő**. A frissítő ZIP nem is tartalmaz ezekből új futási változatot. A régi könyvtárakat ne töröld; ez nem önálló, üres repóba telepíthető teljes projekt.

A saját készülék-YAML meglévő `rflink:` szakaszában:

```yaml
rflink:
  id: rf_bridge
  receiver_id: rf_receiver
  rx_plugins: all
  plugin_profile: extended
  log_messages: true
  on_message:
    - then:
        - script.execute:
            id: rflink_api_process
            message: !lambda return x;
```

A hosszú LaCrosse-csomagokhoz a meglévő `remote_receiver:` blokkban még ezt módosítsd:

```yaml
  buffer_size: 1200b
```

Az új teljes példában már ez szerepel. A szimulált, 1058 időzítési elemes csomag 1000-es mérettel túlcsordult, 1200-assal átment. A 200 további 32 bites elem **800 bájt többlet a fő vételi tömbben**; ez számított foglalási különbség, nem célgépen mért teljes szabadmemória-változás. A többi vevőbeállítás, különösen `high_frequency: false`, változatlan. Csak a korábbi távirányítókkal 1000 is használható, de akkor a hosszú csomagú új időjárás-érzékelők vétele nem teljes.

**A profil lényegi új sora a `plugin_profile: extended`.** Az itt kiírt többi sor a meglévő működés példája. Ha saját további `on_message` kezelőd van, azt tartsd meg; ne hozz létre második felső szintű `rflink:` kulcsot.

Az új csatorna/fokban mért szélirány diagnosztikájához a meglévő adatcsomag-lista:

```yaml
packages:
  rflink_api:
    url: https://github.com/vicktor1979/esphome-rflink
    ref: main
    refresh: 0s
    files:
      - packages/rflink-ha-data-only.yaml
      - packages/rflink-ha-extended-fields.yaml
```

Más, saját csomagbejegyzéseid maradjanak meg. Az új fájl nem helyettesíti a `data-only` csomagot; a régi `rflink-ha-api`, `all-data` vagy régi `gestures` YAML-csomagot ne tedd vissza mellé. A 0.1.6-os `rflink_remote` kezeli a saját távirányítókat.

A külső komponensek továbbra is **`[rflink, remote_receiver, rflink_remote]`**. Nem kell új ESPHome-komponenst hozzáadni. Az `Extensions` nem külön komponens, a fordítási előkészítő a repóból emeli be. A GitHubra a kicsomagolt könyvtárszerkezet kerüljön, ne a ZIP egyetlen fájlként.

Forrás- és csomagfrissítés után **konfigurációellenőrzés → Clean build files → fordítás → feltöltés**. A `refresh: 0s` a Git-forrást frissíti, nem az eszköz firmware-ét. A korábbi cache-hiba tanulsága miatt a fordító kimenetében is ellenőrizd, hogy az új opció és az `extended` profil valóban betöltődött. Ne telepíts hibával megállt fordításból származó régi binárist.

OTA előtt a működő RFLink figyelést kapcsold ki. Az új OTA-kezelés nem változott; ha nincs stabil kapcsolat, USB-s telepítés/naplózás szükséges. Valódi jelszó vagy API-kulcs nincs a frissítő ZIP-ben, a saját `secrets.yaml` maradjon helyben.

A teljes `examples/rflink-extended.yaml` a korábban kiadott, konyhai lámpakezelést tartalmazó mintára épül. A saját közben módosított konfigurációdat ne írd felül ellenőrzés nélkül: az új profil és az opcionális csomagsor külön is átvezethető.

## 2. Profilok, számok, visszaállítás

| Beállítás | Eredmény |
|---|---|
| `plugin_profile: legacy`, `rx_plugins: all` | A korábbi 48, a korábbi plugintörzsekkel. Az új profil nélkül ez az alapérték. |
| `plugin_profile: extended`, `rx_plugins: all` | 55 azonosító: 48 régi + 7 új; a kijelölt régi dekódereknél javított példány fut. |
| `plugin_profile: extended`, `rx_plugins: configured` | Az eredeti konfiguráció 47 azonosítója, a javításokkal; nem kapcsolja be automatikusan a hét újat vagy a 083-at. |
| `plugin_profile: extended`, `rx_plugins: [48, 49, 50, 61]` | 001 + a négy megadott dekóder, összesen 5. |

A **001-es már benne van a számokban**. A javított és eredeti 072 nem két külön plugin; ugyanaz az azonosító, a hosszú csomagokhoz külön feldolgozási belépési ponttal. TX/adás egyik profilban sincs.

Visszaváltás a régi dekóderekre: `plugin_profile: legacy` és `rx_plugins: all`, majd új fordítás/feltöltés. A 037-es régi figyelmeztetés és a régi Brel-kimenet ekkor szándékosan visszatérhet. Az új mezőcsomag megmaradhat, az új csatorna/széliránymezők értéke ilyenkor többnyire ismeretlen lesz. Az `extended` profil az eredeti javítandó fájlok SHA-256 lenyomatát is ellenőrzi: ismeretlenül módosult alapot nem ír felül hallgatólagosan.

Indulási ellenőrzés:

```text
RFLink RX compatibility bridge v0.1.7 (optional extended RX plugins; receiver/gestures unchanged):
  Plugin profile: extended
  RX plugins compiled: 55
```

A **vevő továbbra is rxgate2**, a távirányító-kezelő továbbra is 0.1.6. Ez nem kevert/hibás firmware: ezeket most nem módosítottuk. Sikeres API-állapotfeliratkozás és az 5 másodperces várakozás után:

```text
CAPTURE=ON; DECODE=ON; api_states=YES; wifi=CONNECTED; fast_loop=OFF
```

## 3. Javított meglévő pluginok

### 001 – hosszú csomagok előfeldolgozása

Az Auriol/Atlantic harmadik ismétlésének vizsgálatában egy hozzáférés a tömbhatár mögé kerülhetett. Külön célzott bemenettel az eredeti kód határsértése reprodukálható, a javított példány határellenőrzéssel elutasítja ezt a hozzáférést. A Byron-ismétlési ellenőrzésnél is a valóban olvasott későbbi indexhez igazítottuk a korlátot. Ez memóriabiztonsági javítás, **nem utólagos állítás arról, hogy ez okozta a korábbi Wi-Fi-hibákat**.

### 037 – AcuRite 986

A korábbi `%x`/`unsigned long` figyelmeztetést típushelyes argumentumokkal és korlátos `snprintf` hívással javítja. A hibás CRC továbbra is elutasítást okoz, de az eredeti, feltétel nélküli soros CRC-hibakiírás csak külön debug módban marad. A Fahrenheit→Celsius átszámítás a tizedes eredményt nem csonkolja le egész Celsiusra az utolsó szorzás előtt: például a tesztben 78°F → 25,6°C, 30°F → −1,1°C. Ez a kód átszámításának javítása, nem érzékelőkalibrálás.

### 072 – Byron SX

Az eredeti `072` C/C++ egész literál oktális 58-at jelentett; az előfeldolgozó viszont 72-es jelölőt ad át. Az override decimális 72-t használ. A pulzushatárok az aktuális `RawSignal.Multiply` értékből számolódnak, nem fix mintavételi osztóból. Külön, korlátos hosszúcsomag-feldolgozó legalább két azonos, helyesen határolt Byron-keretből tud üzenetet készíteni. Így a régi 291 körüli globális pulzushatár feletti ismétléssorozat is feldolgozható a bővített nézetben.

Ez a megnyitott donorral összevetett, célzott helyi javítás. **Nem állítjuk, hogy a teljes `couin3/RFLink#67` változtatási javaslatot átvettük:** annak teljes diffje nem volt hozzáférhető.

### 083 – Brel Motor / Dooya

Típushelyes konstans parancsmutató és külön `CMD` mező. A korábbi kódban a parancs második `NAME` mezőbe került `CMD=UP` formában. Az új kimenet például:

```json
{"PARAM":"20;00","NAME":"BrelMotor","ID":"123456","SWITCH":"08","CMD":"UP"}
```

Ez szintetikus tesztpélda. `UP`, `DOWN`, `STOP`, `SETUP` ágak ellenőrizve; nincs új RF-adás. Meglévő, a hibás Brel-névhez kötött saját szűrőt az új helyes `NAME + CMD` szerint kell átállítani. Az EV1527-nevek és távirányító-szűrők nem változnak.

## 4. Újonnan fordítható dekóderek – a pontos lefedettség

| ID | Támogatott illesztés | Kimenet és korlát |
|---|---|---|
| **016** | Silvercrest rádiós dugaljak távirányítói | Ismert parancstáblák, remote/gomb, ON/OFF; legalább két egyező logikai keret a sorozatban. Nem a 075-ös SilverCrest csengő. |
| **018** | Louvolite R1492-6CH-WH | Ellenőrzött AC-fejléc, 65 bit és checksum; UP/STOP/DOWN. Egy ID + SWITCH, nem két ID. A RELEASE üzenet eldobása megmarad; nincs gesztusígéret. A projektben a név `Louvolite` (a donor rövid `LOUVO` neve helyett). |
| **048** | Oregon V2/V3 hőmérős/páratartalmas ágak | EA4C, CA48, 0A4D hőmérő; FA28, 1A2D, 1A3D, CA2C, FAB8 és xACC TH-ág. Hőmérséklet, modellfüggően HUM/BAT, csatorna/ID. **Nem a teljes Oregon V1/V2/V3 csomag:** nincs itt V1, Oregon szél/csapadék/UV/OWL támogatás. |
| **049** | LaCrosse TX141/TX145 formátumcsalád | 32/33/37 bites hőmérő-kereteknél három azonos ismétlés; 40/41 bites TH-nál LFSR; 64/65 bites TX141W/TX145-nél CRC8. TEMP/HUM/BAT/CHAN és megfelelő ágnál szélsebesség/fokban szélirány. Nem minden márkaváltozat hardvertesztje. |
| **050** | Fine Offset WH2/WH2A, Telldus/Proove és TFA 30.3225 formátum | 48/49/55 bites ágak, type/CRC, 55 bites extraösszeg-ellenőrzés. TEMP, HUM ahol tényleg van; BAT csak az ellenőrzött TFA-ágban. A kétértelmű 47 bites WH5/Rosenborg ág kimarad. |
| **076** | CAME TOP-432 | A 26 impulzusos keret a régi 36-os minimum előtt kerül feldolgozásra. ID/SWITCH/CMD. Nem ad kapuállapot-visszajelzést és nem vezérli a kaput. |
| **077** | Avantek csengő, CRC nélküli alapváltozat | PCM szinkron + 36 bites PWM adat, teljes 32 bites ID és 4 bites gomb. **Nincs kitalált `CMD=ON`**. A donor CRC-s ága nem ellenőrzi ténylegesen a CRC-t; ezt a változatot most nem fogadjuk el ellenőrzöttnek. |

A 049 és 050 a megnyitott rtl_433 források protokollja alapján készült, kis önálló impulzusfeldolgozóval. Nem került be a teljes rtl_433 vagy RFLink32 runtime, és nem történik hibás 8→16 bites mutatóátértelmezés. Az eredeti `RawSignal` ABI változatlan; a bővítések a már meglévő, előjeles mikroszekundumos impulzusvektort olvassák külön, korlátos nézetként.

**Külön hardveres korlát:** a forrásformátum támogatása nem teszi képessé a vevőt más frekvencia vagy moduláció vételére. Ez a konfiguráció a meglévő OOK/ASK vevővel vett impulzusokra épül. Somfy 017 (433,42 MHz), Hyundai 051 és NOX 087 most nincs benne. A 051 teljes forrása és a FA21RF #73 javítás teljes diffje nem volt ellenőrizhető; a 087 adatkiadása hiányos. Nem szerepelnek rejtetten vagy félkészen az `all` készletben.

## 5. Új mezők és a mérőeszköz-generátor

- `CHAN`: az eredeti csatornakód, decimális szám. Nem feltételezzük, hogy 1-től számozott.
- `WINDIR_DEG`: 0–359 közötti, már fokban megadott irány. A régi `WINDIR` továbbra is 0–15 szektorkód ×22,5°. Nem keverjük a kettőt, és 315° nem csonkolódik nyolc bitre.

Az opcionális `rflink-ha-extended-fields.yaml` két diagnosztikai szenzort ad: **RF utolsó csatorna**, **RF utolsó szélirány fokban**. Ezek az utolsó dekódolt csomag mezői, hiány esetén ismeretlenre váltanak; nem állandó, több érzékelő adatait összekeverő idősorok. A fő adatcsomag összes korábbi mezője megmarad.

A generátor új, opcionális `--channel` szűrést kapott. Egy adott mérőeszköz rögzített entitásaihoz például:

```bash
python tools/make_rf_device.py --prefix kert_lacrosse --name "Kert" --protocol "LaCrosse-TX141THBv2" --rf-id "A_TENYLEGES_ID" --channel 0 --fields TEMP,HUM,BAT,CHAN --stale-after 60min --output packages/rflink-kert.yaml
```

Ez **helykitöltős példa**: az ID-t és csatornát a valós dekódolt JSON-ból vedd, az aktuális csatorna nem szükségszerűen 0. A generátor nem tudja, hogy a beírt ID létezik-e. Csatornaszűrés nélkül a régi NAME+ID működés változatlan. Szélmérőnél `AWINSP,WINDIR_DEG,BAT,CHAN` választható. A kész csomagot a saját csomaglistádhoz kell adni, majd új firmware-t fordítani.

BAT hiányából nem készül 100% vagy OK állapot. A TFA-ág fix 0xFF HUM jelölőjéből nem készül 255% páratartalom. A rövid JSON-formátum és az API közötti átszámítás továbbra is a közös, típusellenőrző mezőkezelőn át történik.

## 6. Tanulás, gesztusok, konyhai vezérlés

A működő 0.1.6 tanuló nézet és a YAML-os távirányító-felvétel megmarad. Új dekódolt protokoll megjelenhet az RF tanuló jelben, de **teljes kattintás/tartás felismerés továbbra is csak EV1527-hez van**. Más protokollhoz `mode: message` és `event_types: [received]` használható. Avanteknél a hiányzó parancsot üresen hagyd (`command: ""`), ne találj ki `ON` értéket. A tanuló nézetben látott teljes mintát használd.

A konyhai `EV1527 / 01fac2 / 08 / ON` és `event_types: [single, hold_repeat]` változatlan. A `single` +50-es, a `hold_repeat` +20-as fényerőkezelést nem írtuk át. A 180/450/180/350/700/250 ms gesztusidőzítések, az API-késleltetett vételindítás, `high_frequency: false` és OTA-leállítás megmaradnak. A teljes új példában egyetlen vételi kapacitásváltozás van: 1000 helyett 1200.

Az új protokollok bővítik a lehetséges egyezéseket. Egyes rádióformátumok eleve átfednek; nincs általános tévesfelismerés-mentességi garancia. A régi motor saját plugin-sorrendje és EV1527-megfigyelése nem változott, az új dekóderek azonban előbb vizsgálják a nyers mikroszekundumos adatot. Az ismert 085372/08 és 01fac2/08 minták, valamint a régi átfedéses teszt a bővített készlettel is regressziós tesztet kaptak. A teljes készletben a régi JSON-ismétlési viselkedés továbbra is eltérhet a csak [61] készlettől; a vezérléshez az esemény-entitást használd.

## 7. Memória, puffer, hálózat

A receiver C++/Python forrása nem változott. A teljes új példában a vételi puffer beállítása 1200; a korábbi 1000-hez képest a fő tömb 800 bájttal nagyobb. A további dinamikus tárolás és az API teljes memóriaigényét célgépen kell ellenőrizni. A kiegészítő nézet legfeljebb 2048 időzítési elemet vizsgál; ez **nem új 2048-as pufferfoglalás**, és nem jelenti, hogy a vevő automatikusan ennyit képes veszteség nélkül összegyűjteni. Az új ismétlésszűrés nyolc fix helyet használ; nem korlátlan eszközadatbázis.

A LaCrosse hosszú, sokszor ismételt csomagjai meghaladják az 1000-es vételi beállítást. A forrás szerinti 12×88 időzítés 1056 elem, a végződéssel a tesztben 1058. A tényleges rxgate2 kóddal, szimulált GPIO-élekkel és 16 ms-os feldolgozással ezt az 1000-es beállítás eldobta, túlcsordulást jelezve; 1200 mellett mind a 41 pozitív vételi próba átment. Ez indokolja a célzott méretnövelést. A vevőben már elveszett adatot a dekóder nem állítja vissza. **A gyorsított főciklust ne kapcsold vissza**, a puffert pedig ne növeld tovább találomra: a valós `overflow_reports`, a jelkimenet és a szabad memória alapján kell dönteni.

A nagyobb kód flash/RAM-igénye célfordítás után mérhető. A hosttesztekből nem adunk megtévesztő ESP8266 memória- vagy OTA-méretszámot. A korábbi Wi-Fi-javítás megmaradása forrásszinten ellenőrzött, az új teljes készlet rádiós hálózati stabilitása még hardveres próbát igényel.

## 8. Ellenőrzés és GitHub CI

A `tests/extensions/` tartalmazza a reprodukálható új teszteket; a `tests/remote_config` és a holdfix tesztek a korábbi működés regresszióját adják. Példák:

```bash
python tests/extensions/run_tests.py --out /tmp/rflink-v017-tests
python tests/extensions/run_remote_integration.py --out /tmp/rflink-v017-remotes
python tests/extensions/test_configuration.py --out /tmp/rflink-v017-config
python tests/extensions/run_capture.py --out /tmp/rflink-v017-capture
```

A tesztbemenetek többsége szintetikus; a CAME és Louvolite példák donorforrás-kommentben közzétett impulzussorok, nem most a felhasználó eszközéről rögzített mérések. Az Oregon három bájtmintája szintén a forrás kommentjéből származik, ezekhez generáltuk a Manchester-időzítéseket. A hibás eredeti 001 és 037 tesztek **szándékosan buknak**, ez a regresszió bizonyítása; a végső tesztprogram csak akkor sikeres, ha az új változat ugyanott átmegy.

A `.github/workflows/rflink-extensions.yml` és `.ci/prepare_extensions_firmware.py` négy tényleges ESPHome 2026.9.0/NodeMCU fordítást definiál: extended/all, extended/configured, extended/[61], legacy/all. Az Actions oldalon kézzel indítható. **Ezt itt nem futtattuk.** Csak a fordítási naplót teszi közzé, semmit nem tölt az eszközre. Az áladatos CI-firmware-t tilos saját konfigurációnak tekinteni. A szokásos firmware-t a saját titkaiddal fordítsd.

## 9. Források, licencek, terjesztés

A konkrét URL-ek, helyi SHA-256 értékek és az el nem készült jelöltek listája: `RFLink/Extensions/SOURCES.json`. A donor Git-commitok nem voltak ellenőrizhetően letölthetők, ezért nincsenek kitalált commit-SHA-k. A kiadott, helyben tárolt adapterek tartalmát lenyomat rögzíti; fordításkor nem töltünk le tetszőleges friss donorforrást.

A fontosabb külső források:

- https://github.com/cpainchaud/RFLink32/tree/master/RFLink/Plugins
- https://raw.githubusercontent.com/merbanan/rtl_433/master/src/devices/lacrosse_tx141x.c
- https://raw.githubusercontent.com/merbanan/rtl_433/master/src/devices/fineoffset.c
- https://esphome.io/components/external_components/
- https://esphome.io/components/packages/

Az eredeti RFLink és a donorok feltételei nem egységesek; GPL-2.0-or-later és kereskedelmi korlátozást tartalmazó RFLink-feltételek is szerepelnek. A szerzői/engedélyszövegek mellékelve vannak. Ez nem egységesen MIT-licencű csomag és nem jogi engedély a kereskedelmi terjesztésre. Különösen teljes összelinkelt firmware terjesztése előtt ellenőrizendő az alkalmazott licencek együttese. Lásd `RFLink/Extensions/THIRD_PARTY_NOTICES.md`.

A bővítés csak vételi otthonautomatizálási segéd. Nem elsődleges riasztórendszer, nem kapu vagy redőny biztonsági vezérlése, és nem biztosít parancsvégrehajtási visszaigazolást.
