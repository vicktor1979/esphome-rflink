# RFLink v0.1.6 – YAML-ban megadható távirányítók és tanuló nézet

## Mire épül?

A működő **v0.1.5 + rxgate2 + holdfix1** összeállítás kiegészítése.
Nem teljes repó. A meglévő RFLink-motor, a pluginmappa, a PROGMEM/083 javítás,
a holdfix1 állapotgépe és a remote_receiver rxgate2 kódja változatlan.
Az új rész a feldolgozott jelekből készít natív ESPHome event entitásokat,
és időkorlátos diagnosztikai tanuló nézetet ad.

**Az új platform neve `rflink_remote`. Nem az ESPHome beépített platformja:**
a mellékelt külső komponens adja. A kód és az összes példakonfiguráció
telepítése szükséges. A változat nem igényel MQTT-t.

A távirányító fizikai azonosítóját nem nevezi meg és nem tanulja be a hardverbe;
a tanuló nézet egy dekódolt azonosítómintát mutat meg a saját konfiguráció elkészítéséhez.
Nem írja át automatikusan a YAML-t, nem hoz létre futás közben új HA-entitásokat,
és nem ad hozzá véletlen közelben lévő rádiós eszközöket a vezérléshez.

## Telepítés

1. Mentsd el a jelenlegi működő fő YAML-t és a GitHub-repó állapotát.
2. Másold a repó gyökeréhez képest azonos útvonalra:

```
components/rflink_remote/__init__.py
components/rflink_remote/event.py
components/rflink_remote/validation.py
components/rflink_remote/rflink_remote.h
components/rflink_remote/rflink_remote.cpp
packages/rflink-ha-data-only.yaml
```

3. A saját készülék konfigurációjához használd a mellékelt
   `rflink-yaml-tanulas-proba.yaml` fájlt, vagy az alábbi változtatásokat vezesd át.
   A saját titkokat tartalmazó `secrets.yaml` maradjon meg, ne töltsd fel GitHubra.
4. A régi `packages/rflink-ha-all-data.yaml` és `packages/rflink-ha-gestures.yaml`
   helyett most **csak a `packages/rflink-ha-data-only.yaml`** legyen betöltve.
   Az elavult `rflink-ha-api.yaml` se maradjon mellette.
   A régi fájlok a repóban megmaradhatnak visszaállításhoz, csak ne töltsd be őket.
5. Az `external_components` listája: `[rflink, remote_receiver, rflink_remote]`.
6. Frissítsd a Git-forrást és a package cache-t; a példában mindkét `refresh: 0s`.
   Ezután ellenőrzés, Clean build files, fordítás és feltöltés következzen.
   Stabil verziónál rögzített commit/tag célszerűbb, mint mindig a változó main.
7. OTA előtt a már működő figyelési kapcsolót ideiglenesen kapcsold ki.
   Ez a csomag nem módosítja az OTA mechanizmusát.

Nem kell cserélni a `components/rflink/`, `components/remote_receiver/` és
`RFLink/Plugins/` könyvtárak egyetlen fájlját sem.
A távirányítók neve és a két gesztus-entitás belső ID-je a teljes példában megmaradt.
A két régi, pusztán `button_08` típusú esemény-entitást a data-only csomag már nem
hozza létre. Ha a HA megőrizte a régi bejegyzésüket, azok elérhetetlenné válhatnak;
a már használt automatizmusokat ellenőrizd a gesztus-entitások tényleges entity_id-jával.
A szenzorok, adatmezők, frissítési idő és átszámítás változatlan.

## 1. Megadott távirányító az `event:` szakaszban

A közös kezelő egyszer szerepel a készülék YAML-jában:

```yaml
rflink_remote:
  id: rf_remotes
  rflink_id: rf_bridge
```

Egy konkrét gomb definíciója:

```yaml
event:
  - platform: rflink_remote
    id: konyha_08
    name: "Konyhai távirányító 08"
    remote_id: rf_remotes
    protocol: "EV1527"
    rf_id: "01fac2"
    button: "08"
    command: "ON"
    mode: gestures
    event_types: [single, double, triple, hold, hold_repeat, hold_release]
    pressed:
      name: "Konyhai 08 nyomva"
```

Ezt nem kell a teljes példában meglévő ugyanilyen gomb mellé is beilleszteni:
a meglévő bejegyzést nevezd/módosítsd, vagy új gombhoz új listatagot készíts.

Egy bejegyzés **egy pontos protokoll + RF ID + gomb + parancs** kombináció.
Másik gomb ugyanazon távirányítón külön bejegyzés, eltérő `button` és saját `id`/`name`.
Az RF-kódok idézőjelben legyenek. `rf_id` nem azonos a YAML belső `id` mezőjével.
EV1527 gesztusmódban a kódok normalizálódnak: `1FAC2` -> `01fac2`, `8` -> `08`.
Más protokoll üzenetmódjában a tanuló nézetben látott karakterlánc pontos egyezése kell.
Nincs wildcard. Az üres button/command az adott mező hiányát/üres szövegét jelenti,
nem tetszőleges gombot/parancsot.

Az `event_types` opcionális; elhagyva gesztusmódban minden alábbi típus megjelenik:

```
press, release, single, double, triple,
click_4, click_5, click_6, click_7, click_8, click_9, click_10,
hold, hold_repeat, hold_release, cancel, multi_overflow
```

A lista a HA-ba/ESPHome on_event automatizmushoz továbbított eseménytípusokat szűri.
A belső felismerés és az opcionális nyomva bináris szenzor működése ettől nem változik.
A `log_events: false` kikapcsolja az adott bejegyzés diagnosztikai gesztussorait.

Egy `single` nem azonnali: ki kell várni, hogy jön-e dupla/tripla nyomás.
A `press` azonnali, de egy többkattintásos sorozat minden nyomásánál is jelentkezik.
A `cancel` nem `hold_release`: API-/OTA-/kézi leállításkor nem igazolunk kattintást.

### ESPHome-on belüli automatizmus

A natív `on_event` is használható:

```yaml
    on_event:
      then:
        - if:
            condition:
              lambda: return event_type == "double";
            then:
              - logger.log: "Konyhai dupla nyomás"
```

Az `event_type` itt a natív ESPHome event komponens változója.
Konkrét relét/lámpát ez a csomag nem vezérel automatikusan.

## 2. Általános tanuló jel és gesztus

A teljes példában van:

- `RF tanuló mód` kapcsoló: alapból OFF, bekapcsolás után 60 másodpercre aktív.
- `RF tanuló jel` szöveges szenzor: a legutóbb bemutatott pontos szűrőminta.
- `RF tanuló gesztus` szöveges szenzor: teljes azonosítóminta és gesztus együtt.

A saját konfigurációjú távirányítók a tanuló mód kikapcsolt állapotában is működnek.
Az ismeretlen EV1527-kódok tanulásához nincs előzetes ID-lista. A tanulás
legfeljebb négy (állíthatóan 1–8) egyidejű/jelenleg még aktív mintát követ.
Nem foglal egyre több memóriát a véletlenül vett különböző azonosítókhoz.
Ha mindegyik foglalt, az új ismeretlen minta kimarad, számláló/log jelzi;
a konfigurált távirányítók nem maradnak ki emiatt.

Kapcsold be a tanulást a Wi-Fi/API/vétel felállása után; nyomj meg egy új gombot,
majd próbáld röviden, duplán és hosszan. A szenzorok az ESPHome eszköz
diagnosztikai entitásai között is megjelenhetnek.

Példa a **formátumra**, nem egy új, valódi felismert eszköz:

```json
{"protocol":"EV1527","rf_id":"012345","button":"04","command":"ON","mode":"gestures"}
```

```json
{"protocol":"EV1527","rf_id":"012345","button":"04","command":"ON","mode":"gestures","gesture":"double","seq":7}
```

Az azonosító és gesztus **egyetlen szöveges állapotban együtt** van.
A `seq` növekedése miatt ugyanannak a gesztusnak az ismétlése is új állapotfrissítés.
Ne társíts egy általános eseményt egy külön, később átíródó „utolsó ID” szenzorhoz;
az egységes JSON elkerüli az ilyen téves párosítást.

A tanuló nézet nem további végleges event-entitás: az új távirányítóhoz a fenti
minta alapján te adsz meg egy `event:` bejegyzést. A bemutatott JSON érvényes YAML
flow mappingként is, de a `gesture` és `seq` nem a végleges konfiguráció része.
Másold át a `protocol/rf_id/button/command/mode` értékeket, add meg a saját nevet,
majd válaszd ki az `event_types` listát. A log `YAML match:` sora ugyanezt tartalmazza.

Az EV1527 tanuláshoz alapból legalább **3 megfelelő keret** szükséges egy
felismert nyomási szakaszban, hogy egy elszórt hibás vétel ne legyen rögtön tanítási
javaslat. Emiatt a bemutatás rövid nyomásnál annak lezárásakor, tartásnál a hold
felismerésekor történik. Ha egy másik EV1527 távirányító csak 1–2 keretet ad,
a `min_frames` csökkenthető, de ilyenkor még fontosabb többször ellenőrizni a mintát.
Ez a küszöb nem módosítja a már megadott távirányítók felismerését.

A tanuló módot újra bekapcsolva új 60 másodperces mérés kezdődik. Reboot után OFF.
A kikapcsolás nem törli a HA-ban látható legutóbbi mintát, csak a követést állítja le.
Nem készül flashbe mentett tanulteszköz-adatbázis.

## 3. Más protokollok és a fontos korlát

Az **összes 48 lefordított RX-plugin dekódolt JSON-ja** eljut a jelnézethez.
A teljes keretszintű gesztusfigyelő viszont továbbra is **EV1527**-hez van meg.
Nem változtattuk meg a pluginok belső ismétlésszűrését és sorrendjét.

Más protokollnál az általános jelnézet `mode: message` értéket ad.
Ilyen mintához a konfigurált event entitás `received` eseményt küld:

```yaml
  - platform: rflink_remote
    id: egyeb_rf_parancs
    name: "Egyéb RF parancs"
    remote_id: rf_remotes
    protocol: "A_TANULO_NEZETBEN_LATOTT_NEV"
    rf_id: "A_LATOTT_AZONOSITO"
    button: "A_LATOTT_GOMBKOD"
    command: "A_LATOTT_PARANCS"
    mode: message
    event_types: [received]
    message_cooldown: 250ms
```

Az alapértelmezett cooldown 0 ms; 250 ms esetén az utolsó továbbított ugyanezen
bejegyzéshez tartozó üzenet után 250 ms-on belül érkező újabb üzenet nem jut tovább.
Ez **nem** dupla-/tartásfelismerés. Más protokollokhoz nem találunk ki holdot
olyan JSON-adatfolyamból, amelyből a plugin már kidobhatta az ismétléseket.
A `mode: gestures` más protokollnévvel egyértelmű konfigurációs hibát jelez.
A `mode: auto` EV1527-hez gestures-t, más névhez message-et választ.

A tanulás nem univerzális nyers rádióanalizátor: amit egyik RFLink-plugin sem
ismer fel, abból nem állít elő megbízható protokollt és azonosítót.
A fizikai távirányítónak ismételnie kell a jelet a tartás alatt; a felengedés
becslés, a beállított csendidővel.

A tesztben előfordult protokoll-átfedés is: egy szintetikus 300/900 us jel,
amelyet csak 001+061 mellett EV1527 085372/04-ként olvas a motor, a teljes
készlettel az előrébb sorolt Eurodomest dekóderhez került. Ez a régi motor
viselkedése, nem az új event réteg változtatása. A teszt kifejezetten ellenőrzi,
hogy ilyenkor az új réteg ne találjon ki EV1527-gesztust. Valódi eszköz
felvételekor ezért a megismételt tényleges tanuló kimenetet használd.

## 4. Külön YAML / másik ESPHome eszköz

Egy távirányító `event:` blokkja külön fájlba is kerülhet:

```yaml
packages:
  sajat_konyha: !include taviranyitok/konyha.yaml
```

A GitHubos adatcsomag bejegyzése mellé tedd, ne hozd létre kétszer a `packages:`
felső szintű kulcsot. Az `examples/uj-taviranyito.yaml` ilyen külön fájl példája.
Új saját távirányító felvételéhez ezután nincs szükség C++ vagy közös package
szerkesztésére; csak új YAML-bejegyzésre és új fordítás/feltöltésre.

Másik **RF-vevős ESPHome eszköz** saját YAML-jába is átmásolható ugyanaz a minta.
Annak is szüksége van a vevőre, az RFLink-komponensre és a rflink_remote kezelőre.
Másik fizikai ESP eszköznek adj más `esphome.name`-et. Ettől a kiegészítéstől
nem lesz rádióvevő nélküli ESP-n automatikus RF-vétel vagy hálózati továbbítás.
A mellékelt fő hardverkonfiguráció továbbra is NodeMCU/ESP8266 és az egyedi rxgate2.

## 5. Megmaradó időzítések és korlátok

Közös alapértékek: release 180 ms, hold release 450 ms, repeat freshness 180 ms,
multi 350 ms, hold 700 ms, repeat 250 ms, felső tartáskorlát 30 s.
A közös `rflink_remote.timing` blokk állítja ezeket. Egy event saját `timing`
blokkja külön beállításkészletet használ, a nem megadott mezők **a beépített
180/450/180/350/700/250/30000 értékek**, nem a módosított közös blokk értékei.
Ez elkerüli a fordítási sorrendtől függő öröklést.

A tanuló gesztus a közös időzítéseket használja. Egyedi timingú event ennél
eltérő időpontban/gesztussal dönthet; ez tudatos konfigurációs különbség.

A 450 ms-nál rövidebb valódi újranyomás egy már felismert hold után továbbra is
összeolvadhat. Az új réteg ezt a bevált holdfix1 kompromisszumot nem módosítja.

## 6. Ellenőrzés

Új sor:

```
[rflink.remote]: v0.1.6: YAML-configured remotes; holdfix1 unchanged; learning slots=4
```

Az RFLink motor verziója továbbra is v0.1.5, a vevőé rxgate2. Az automatikus
indítás diagnosztikájában továbbra is `CAPTURE=ON; DECODE=ON; fast_loop=OFF`
kell legyen sikeres API-feliratkozás és öt másodperc stabil kapcsolat után.
A tanuló kapcsoló ettől különálló funkció, a figyelési kapcsolót nem helyettesíti.

A konfigurált gomb eseménye a saját HA event entitáson látható. Az esemény állapota
az utolsó esemény időpontja, az `event_type` a single/double/hold/... érték.
A nyomva szenzor opcionális. A csak tanuló nézetben látott ismeretlen távirányító
nem kap automatikusan saját event entitást.

## Tesztek

`TEST_RESULTS.txt` és `TEST_LOGS/` tartalmazza a tényleges host-tesztek eredményét.
A natív ESPHome entitásokat és JSON könyvtárat a futási host-tesztben kis
helyettesítők adják; ez **nem** valódi ESPHome konfigurációvalidálás, Xtensa
fordítás vagy HA/hardverteszt. A környezetben nincs telepített ESPHome/PlatformIO
és célfordító, és a csomagok hálózati beszerzése itt nem volt lehetséges.

Az opcionális `.github/workflows/rflink-remote-events.yml` és
`.ci/prepare_remote_firmware.py` a repóban lévő valódi ESPHome konfigurációt
fordítja all/configured/[61] változatban, helyi komponensforrásokkal és
jelölt, nyilvános áladatokkal. A GitHub-munkafolyamatot itt nem futtattam.
Nem igényli a jelszavaidat, nem tölt fel eszközre, és nem ad ki áladatos firmware-t.
A használathoz ezeket a fájlokat és a fő próba-YAML-t is a repóba kell tenni.

## Elsődleges dokumentáció (külső háttér, nem helyi hardverteszt)

- ESPHome event komponens: https://esphome.io/components/event/
- ESPHome külső komponensek: https://esphome.io/components/external_components/
- ESPHome packages és !include: https://esphome.io/components/packages/
- Natív event regisztráció forrása: https://raw.githubusercontent.com/esphome/esphome/dev/esphome/components/event/__init__.py

A komponens saját `protocol/rf_id/button/command/mode/learning/timing` opcióit
nem ezek a beépített oldalak vezetik be, hanem a mellékelt rflink_remote kód.
