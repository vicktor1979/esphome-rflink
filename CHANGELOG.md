# Changelog

Az RFLink ESPHome komponens fontosabb verziói és változásai.

## v0.2.0.9 — választható eredeti jellegű vétel, próbaverzió

- A feltöltött RFLink-5.6wj `Plugin_030.c`, `Plugin_061.c` és `Plugin_001.c` forrása megegyezik a jelenlegi forrással; a fő különbség a GPIO polling és a megszakításos jelmérés.
- Új, kifejezetten választandó `remote_receiver.capture_mode: rflink_polling`. A régi YAML alapértelmezése `interrupt`; az IRQ-adatgyűjtés és a dekódolók változatlanok.
- Polling: legalább 400 µs LOW előtag, 100 µs alatti impulzusra eldobás, 5 ms lezárás; legfeljebb 25 ms keresés, 200 ms keret és 291 tárolt időzítés. A rendszermegszakítások engedélyezve maradnak. A vételi kapu polling esetén is működik.
- A polling mód saját gyors főciklust igényel; nem telepít GPIO ISR-t. Mért élek számlálója `edges` néven jelenik meg a korábbi `irq` helyett. Külön vételimód-/eldobásszámlálók és eredeti Alecto 74-impulzusos/elfogadott keretszámlálók kerültek a ritka diagnosztikába.
- GPIO-alapú host tesztek: Alecto, hibás checksum, EV ismétlések, rövid/dupla/hosszú nyomás, közbeiktatott Alecto, zaj, időkorlát, óraátfordulás és vételi kapu; a meglévő IRQ-regressziókkal együtt. A host teszt nem igazolja az ESP8266 Wi-Fi/API/RF viselkedését.
- Telepítés és visszaállítás: `UPDATE_v0.2.0.9_HU.md`. A név-/OTA-javítások és az eredeti Alecto-publikálás megmaradnak.

## v0.2.0.8

- Kérésre visszaállítva az Alecto V1 eredeti Plugin 030 feldolgozása. A pluginforrás bájtról bájtra változatlan; saját kerethossz-, checksum-, tartomány- és ismétlésvizsgálata megmarad.
- Eltávolítva a bridge egyedi Alecto-előszűrése, impulzusillesztése, bitkonszenzusa, szintetikus keretkészítése és összefolyó ismétlések feldarabolása, a hozzájuk tartozó pufferekkel és diagnosztikával együtt.
- Eltávolítva a többmintás betanulás, hőmérséklet/elemállapot-simítás, ID- és csatorna-átírás, háromadós elfogadási korlát, ötperces publikálási korlát és nagy hőmérséklet-ugrás szűrése. Az eredeti plugin üzenete azonnal, változatlan mezőértékekkel jut tovább.
- A külön Alecto-szenzorcsomag nyers pluginüzeneteket fogad; megtartott entitásazonosítók mellett legfeljebb három teljes RF ID-hez rendeli a kijelzési helyeket. A bridge összes elfogadott üzenete továbbra is elérhető. Kitalált csatornaérték helyett `Nincs adat` jelenik meg.
- Megmarad az EV1527 feldolgozása, a runtime pluginkapcsolók, az automatikus Wi-Fi/API indulás, az OTA-eseménykezelés és a v0.2.0.7 név-/OTA-példajavítása.
- A korábbi helyreállítási teszt fájlneve megmaradt, tartalma az eredeti működés regresszióit ellenőrzi. Új ellenőrzés a tényleges Alecto YAML-megjelenítési kódra. Telepítés: `UPDATE_v0.2.0.8_HU.md`.

## v0.2.0.7

- A generált pluginkapcsolók neveiben az ASCII `/` helyére már a sémaellenőrzés előtt `⁄` (U+2044) kerül. Ez megegyezik az ESPHome eddigi automatikus cseréjével; megszűnik a névre vonatkozó figyelmeztetés. Az összes perjeles pluginnévre érvényes, köztük a 061-es EV1527-re.
- A két fő példa az OTA `password:` helyett `encryption:` beállítást használ, a meglévő API-kulccsal. Az RFLink `on_begin` és `on_error` eseménykezelői megmaradnak. ESPHome 2026.9.0 vagy újabb szükséges; régi firmware-ről az átállási sorrendet az útmutató írja le.
- A két fő példa forráshivatkozása `main`, így kézi GitHub-feltöltés után új tag létrehozása nélkül eléri a javítást. A korábbi példák még a v0.2.0.1 tagre hivatkoztak.
- Az Alecto-helyreállítás a v0.2.0.6-tal azonos. Ez a csomag annak módosított fájljait is tartalmazza.
- Telepítés és a saját ESPHome YAML szükséges módosítása: `UPDATE_v0.2.0.7_HU.md`.

## v0.2.0.6

- Alecto: az előszűrés már a teljes, a legacy motor által elfogadott blokkot kezeli, legfeljebb 291 impulzusig. A 120 impulzusos sorhatár csak a rövid zajimpulzusok összevonása és az ismétlések szétválasztása után érvényesül.
- A legalább 6500 µs hosszú, ténylegesen megfigyelt vagy helyreállított szüneteknél külön, egymást nem átfedő Alecto-sorokat dolgoz fel. Bizonytalan határnál nem darabol önkényesen.
- A teljesen helyreállított, ellenőrzőösszeg-helyes sorok gyors úton kerülnek az eredeti Plugin 030 elé; a további sérült sorokat a meglévő, ismétléseken alapuló bitellenőrzés kezeli.
- Egy blokkból legfeljebb négy sort vizsgál. Újabb sort csak a blokkszintű 8 ms-os időkereten belül kezd el; egy már megkezdett illesztést nem szakít félbe. Az EV1527 normál dekódolása továbbra is az Alecto-helyreállítás előtt történik.
- Új `rflink.alecto.rx` diagnosztika, valamint a betanulási/értékellenőrzési kapu `received`, `samples`, `last_id`, `reason` adatai. A megerősítésre váró mérések °C-ban megjelennek a naplóban, a HA-ba csak az elfogadott érték jut el.
- Plugin 254 mellett a nem pontosan 74 impulzusos Alecto-jelöltek teljes nyers blokkja is naplózható; legfeljebb egy blokk öt másodpercenként, számozott részekben.
- A hárommintás tanulás, az ötperces hőmérséklet-közzétételi korlát, a nagy ugrások ellenőrzése, a közös vevőbeállítások és az eredeti RFLink-pluginok változatlanok.
- Új host regressziók: hosszú/sérült/összefolyó Alecto-jelek, hibás checksum, zaj, időkeret, plugin-kikapcsolás, EV1527 és a teljes kapun át történő publikálás. A meglévő teszt-entitáshelyettesítők kiegészítése a már használt ESPHome-metódusokkal.
- Részletes telepítés és a mérés korlátai: `UPDATE_v0.2.0.6_HU.md`.

## v0.2.0.1

- Egyszerűsített runtime plugin konfiguráció: `plugin_switches: [30, 61, 254]`; nincs kézi `plugin_id`, kapcsoló `id`, név vagy külön restore beállítás.
- A felsorolt normál pluginok `RESTORE_DEFAULT_ON` módban indulnak és visszaállítják az utolsó állapotukat; Plugin 254 mindig OFF-ról indul.
- Kikerült a plugin-alaphelyzet gomb. Az `RFLink aktív pluginok` text sensor megmaradt, és minden runtime ki-/bekapcsoláskor frissül. A 254 közvetlenül a saját kapcsolójával kapcsolható hibakereséshez.
- Capability-aware diagnosztika: az eredeti, változatlan pluginforrások `display_*` használatából build közben készül mezőképesség-tábla. A konfigurált pluginok által nem támogatott diagnosztikai mezők már setup alatt internal státuszt kapnak, ezért nem kerülnek a Home Assistant entitáslistájába.
- Plugin kikapcsolásakor a hozzá tartozó runtime diagnosztikai állapotok érvénytelenednek / `Kikapcsolva` állapotot kapnak. ESPHome 2026.9.0 alatt a már regisztrált natív API entitások futás közbeni biztonságos eltávolítása és újbóli felvétele nem támogatott, ezért a kapcsolgatás nem használ setup utáni `set_internal()` trükköt.
- Az Alecto saját hőmérséklet/elem/RF-ID entitásai Plugin 030-hoz kötött diagnosztikai életciklust kapnak; páratartalom nincs létrehozva ennél a készülékcsomagnál.
- Az eredeti `RFLink/Plugins` fájlok változatlanok.

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
