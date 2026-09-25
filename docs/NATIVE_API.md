# RFLink → natív Home Assistant-entitások

Kiegészítés a már működő **RFLink RX bridge v0.1.1** komponenshez. Készült: 2026-09-22.
Cél: elsődlegesen az ESPHome natív API-ján megjelenő entitások; MQTT csak igény szerint.

## Mi változik, és mi nem?

A működő `components/rflink` könyvtárhoz és az eredeti `RFLink/Plugins` fájljaihoz nem kell
hozzányúlni. A PROGMEM-javítás a meglévő v0.1.1-ben marad. Ez a csomag nem tartalmaz
új dekódert, nem állítja át a vevő lábát, tápengedélyezését, impulzusszűrését vagy
csomaghatár-időzítését. A `dump: raw` és a diagnosztikai `on_raw` naplózás maradjon kikapcsolva.
Az induló RFLink verziósor továbbra is v0.1.1 lehet: ez most YAML-bővítés, nem motorcsere.

A feldolgozás útja:

```text
Változatlan RFLink plugin → v0.1.1 dekóder → on_message(x)
                                               ├─ rflink_api_process → ESPHome-entitások → natív API → HA
                                               └─ mqtt.publish → RFLink/msg (opcionális)
```

## Az alapcsomag entitásai

A `packages/rflink-ha-api.yaml` **8 entitást** definiál:

| Név | Típus | Tartalom |
|---|---|---|
| RF 085372 távirányító | event | A most tesztelt EV1527 távirányító |
| Konyhai távirányító | event | A korábbi EV1527 / 01fac2 távirányító |
| RF utolsó protokoll | text_sensor / HA sensor | Legutóbbi NAME |
| RF utolsó azonosító | text_sensor / HA sensor | Legutóbbi ID |
| RF utolsó gomb | text_sensor / HA sensor | Legutóbbi SWITCH |
| RF utolsó parancs | text_sensor / HA sensor | Legutóbbi CMD |
| RF utolsó üzenet | text_sensor / HA sensor | Legutóbbi rövid dekóder-JSON |
| RF dekódolt csomagok | sensor | Indulás óta feldolgozott dekóderüzenetek száma |

A távirányítókon `button_00`–`button_0f` típusok vannak. Ezek az EV1527 négybites
`SWITCH` mezőjének lehetséges kódjai, nem annak állítása, hogy a fizikai távirányítón
16 külön gomb van. A bizonyított `085372 / 08 / ON` üzenet a megfelelő entitáson
`button_08` eseményt vált ki. A `00` vagy több bitet tartalmazó kódok csak valóban
ilyen dekódolt csomagnál kerülnek kiadásra.

A Home Assistant event-entitásának állapota a legutóbbi esemény időpontja, az
`event_type` attribútum az esemény típusa. A küldő oldalon minden elfogadott
eseményhez meghívjuk a `trigger()` függvényt, akkor is, ha a típusa nem változott.
Nem használunk mesterséges `ON` → `OFF` impulzust, és nem próbálunk felengedést
vagy hosszú nyomást kitalálni az egyszerű `ON` csomagból.

A diagnosztikai mezők változatlan értékeit nem küldjük újra feleslegesen. Hiányzó
`SWITCH` vagy `CMD` esetén az adott mező kiürül, nem marad benne egy másik eszköz
korábbi gombja/parancsa. A számláló 5 másodpercenként frissül; a gombesemények
nem várnak erre a frissítési időre. A számláló dekóderüzenetet, nem garantáltan
külön fizikai gombnyomást számol, és újrainduláskor nullázódik.

A `RF utolsó üzenet` kizárólag diagnosztikai megjelenítés. 250 bájtnál hosszabb JSON
helyett rövid tájékoztató szöveg jelenik meg; a teljes dekóderüzenet a meglévő
`log_messages: true` mellett DEBUG naplóban olvasható. Automatizmusokhoz ne ezt a
szöveges mezőt, hanem a megfelelő event-entitást használd.

**Nincs automatikus, futás közbeni eszköztanítás.** Az entitásokat a YAML definiálja.
Más RF-azonosító továbbra is látható a közös diagnosztikában, de nem hoz létre
magától új távirányító- vagy mérési entitást. Ezek az RFLink ESPHome-eszköz alatti
entitások, nem automatikusan létrehozott külön HA-eszközök.

## Telepítés a meglévő GitHub-projektbe

A két YAML kerüljön az eddigi repó gyökeréből induló `packages` könyvtárba:

```text
components/rflink/                 # meglévő, változatlan
RFLink/Plugins/                    # meglévő, változatlan
packages/rflink-ha-api.yaml        # új, ez kell az első próbához
packages/rflink-ha-weather.yaml    # új, opcionális mérési sablon
```

A ZIP egy **kiegészítő csomag**, nem a teljes RFLink-repó pótlása. A `packages`
könyvtár feltöltése elég a használathoz. Az `examples`, `tests`, a külön nevű README,
tesztnapló és workflow segédanyag; az eredeti fájljaidat nem kell lecserélni.
A ZIP-et ne egyetlen tömörített fájlként töltsd fel a repóba.

A saját, meglévő ESPHome-eszköz YAML-jában:

```yaml
packages:
  rflink_api: github://YOUR_GITHUB_USER/esphome-rflink/packages/rflink-ha-api.yaml@v0.1.9
```

A felhasználónév, repónév és ág a saját repódhoz igazítandó. A korábbi
`external_components` bejegyzést hagyd meg változatlanul. A csomag nem tartalmaz
jelszót vagy `!secret` hivatkozást, így közvetlenül GitHubról is betölthető.
Meglévő `packages:` blokk esetén az új bejegyzést abba illeszd, ne legyen két
azonos felső szintű YAML-kulcs.

Helyi betöltés is használható. A `packages` könyvtárat ilyenkor a készülék YAML-ja
mellé tedd, és a bejegyzés:

```yaml
packages:
  rflink_api: !include packages/rflink-ha-api.yaml
```

## API és on_message

A működő, titkosított API-beállításod maradjon meg. Amennyiben még hiányzik:

```yaml
api:
  encryption:
    key: !secret rflink_api_key
  reboot_timeout: 0s
```

A saját kulcsod a helyi `secrets.yaml`-ban legyen; ne töltsd GitHubra. Ne hozz létre
második `api` blokkot és ne cseréld le a már használt kulcsodat. A csomag maga
nem módosítja az API beállításait.

A meglévő **`rflink:` blokkon belül csak az `on_message` részt** cseréld erre:

```yaml
  on_message:
    then:
      - script.execute:
          id: rflink_api_process
          message: !lambda return x;
```

Az `id`, `receiver_id`, `rx_plugins`, `log_messages` és minden működő rádiós
beállítás maradjon. Az `x` az eredeti dekóder JSON-kimenete; a script ebből
állítja az entitásokat az ESPHome JSON-komponensével. Ehhez nincs szükség a
Home Assistant eseménybuszára történő `homeassistant.event` küldésre.

API-only használatnál a korábbi `mqtt:` blokk és az MQTT-publikáló műveletek
kivehetők. A meglévő Wi-Fi/API/OTA működését ellenőrizd, mielőtt az MQTT-naplót
megszünteted. A többi, korábban MQTT-metaadatokhoz használt belső szenzor maradhat;
nem kell feleslegesen újabb működő beállítást átszerkeszteni.

Mentés után az ESPHome konfigurációját ellenőrizni, lefordítani és a firmware-t
feltölteni kell. A csomag feltöltése a GitHubra önmagában nem módosítja a készüléket.

## MQTT megtartása opcióként

Az API-feldolgozás után külön műveletként az eredeti MQTT-publikálás megmaradhat:

```yaml
  on_message:
    then:
      - script.execute:
          id: rflink_api_process
          message: !lambda return x;
      - mqtt.publish:
          topic: RFLink/msg
          qos: 0
          retain: false
          payload: !lambda return x;
```

Ez az egyszerű MQTT-példa csak a dekóder eredeti mezőit küldi. A TIME/IP/MAC/SIGNAL
kiegészítés megtartásához az utolsó `payload` helyére a korábbi, működő saját lambda
kerüljön. Az API-nak ezek nem kellenek, és nem az MQTT-ről olvassa vissza az adatot.

A meglévő `mqtt:` blokkba az API-first használathoz:

```yaml
  log_topic: null
  discovery: false
  reboot_timeout: 0s
```

A `discovery: false` az MQTT-entitások automatikus HA-felderítését kapcsolja ki,
nem az API-s entitásokat. A `log_topic: null` az MQTT-naplót tiltja le, nem az
`RFLink/msg` publikálást. A broker hiánya miatti újraindítást a MQTT saját
`reboot_timeout` beállítása tiltja. Ez külön van az API azonos nevű beállításától.

**Átálláskor ne legyen egyszerre aktív ugyanarra a lámpára a régi MQTT-s és az új
API-s automatizmus**, mert ugyanaz a jel mindkét útvonalon megérkezhet.

## Az első Home Assistant-próba

A firmware-frissítés és az API-csatlakozás után a Beállítások → Eszközök és
szolgáltatások → ESPHome → RFLink eszköznél keresd az új entitásokat. A diagnosztikai
szenzorok a diagnosztikai részben lehetnek. Minden létrehozott entitásnak van neve
és `internal: false` beállítása. Az API-hoz valódi HA-kapcsolat szükséges;
MQTT Explorerben való megjelenés ehhez nem elég.

Az `085372` távirányító ismert gombját megnyomva az `RF 085372 távirányító`
esemény-entitásának időpontja frissül, az eseménytípusa `button_08` lesz. A későbbi,
újra megnyomott azonos gomb is új esemény lehet, amennyiben a dekóder újra átadja.
Az offline időben érkező gombnyomásokat nem tároljuk későbbi visszajátszáshoz.

A `examples/04-home-assistant-event-test.yaml` a jelenlegi HA `event.received`
indítójával ad egy értesítéses tesztet. Ez **Home Assistant-automatizmus**, nem
ESPHome-konfiguráció. Az abban lévő `entity_id` példa: a saját HA-d tényleges
entitásazonosítójára kell cserélni. A konyhai automatizmus átállításakor a második,
`01fac2`-höz tartozó entitást kell választani; a két RF-azonosítót nem vonjuk össze.

## Nevek, azonosítók és ismétlések

A fő ESPHome-YAML meglévő `substitutions` blokkjában felülírhatók:

```yaml
substitutions:
  rflink_api_remote_1_id: "085372"
  rflink_api_remote_1_name: "RF 085372 távirányító"
  rflink_api_remote_2_id: "01fac2"
  rflink_api_remote_2_name: "Konyhai távirányító"
  rflink_api_repeat_ms: "0"
```

A két ID legyen különböző. Az ID-ket idézőjeles szövegként kezeld, a vezető nullák
megtartásával. Az alapcsomagban a két event-entitás EV1527 protokollra van szűrve;
más protokollhoz a megfelelő parancs- és mezőkezelést külön kell megadni.

A `0` érték nem ad hozzá új ismétlésszűrőt a régi plugin mellé. Például `200`
másodperc helyett **200 milliszekundumos** sebességkorlátot jelent: távirányítónként
ugyanazt a legutóbbi gombtípust legfeljebb 200 ms-onként továbbítja. A megváltozó
PARAM mező ezt nem kerüli meg; másik távirányító vagy másik gomb külön kezelődik.
Ez nem tesz különbséget egy hosszabb rádiós ismétléssorozat és két valódi közeli
gombnyomás között. Nem garantál egy eseményt fizikai lenyomásonként, és nem
állítja vissza a dekóder által már elnyomott ismétléseket.

## Opcionális hőmérő / páratartalom / elem

A `rflink-ha-weather.yaml` egy **adott NAME + ID pároshoz** hoz létre hőmérséklet-,
páratartalom- és alacsony-elem entitást. A tényleges mérőeszközök ID-ja ebben a
beszélgetésben még nem szerepelt, ezért az alapcsomagban nincs hozzájuk találomra
aktivált mérési entitás. A `03` és `05` példák mutatják a helyi, illetve GitHubos
betöltést. A példában szereplő Cresta-protokoll és az ID-helykitöltő nem valós mérés.

Eszközönként külön `sensor_prefix` szükséges. Az `on_message`-ből minden felvett
eszköz saját `<sensor_prefix>_process` scriptjét is meg kell hívni. A protokoll és
az RF-azonosító egyezése kötelező; a szomszéd szenzorának adata nem keveredik bele.

A feltöltött forrás Cresta/Mebus hőmérséklet-formátumát használjuk:

```text
TEMP="00ea" → 0x00ea = 234 → 23,4 °C
TEMP="8037" → előjel: 0x8000; nagyság: 0x0037 = 55 → -5,5 °C
```

A TEMP nem egyszerű előjeles 16 bites kettős komplemens. A HUM a jelenlegi
formatterben decimális szám. A BAT csak `OK` vagy `LOW`; ebből nem gyártunk
százalékos elemtöltöttséget. `LOW` esetén az alacsony-elem binary_sensor igaz,
`OK` esetén hamis. Hiányzó mezőhöz nincs kitalált adat: egy csak hőmérsékletet
küldő Mebus eszköznél a többi entitás ismeretlen marad.

Az alap `stale_after: 60min` idő után a frissítés nélkül maradó mező állapota
érvénytelenedik. Ezt a valódi eszköz küldési gyakoriságához állítsd be. Az azonos
értékű, de újonnan érkezett mérés is frissíti a lejárati időt. Más RFLink-fork vagy
eltérő adatkonvenció esetén az átalakítást külön ellenőrizni kell. Szél-, csapadék-
és egyéb mezőknek ez a kiegészítés még nem készít külön mérési leképezést.

## Ellenőrzések és korlátok

A `tests/test_native.py` a YAML-ból kiemelt tényleges C++ lambdákat fordítja és
futtatja Linuxon, minimális entitás/JSON helyettesítőkkel. Futott: YAML-szintaxis,
eseményirányítás, azonos esemény többszöri kiadása, ismeretlen ID/protokoll,
hibás/hiányzó mezők, hosszú állapot védelme, opcionális ismétléskorlát és időszámláló
átfordulás, pozitív/negatív/nulla hőmérséklet, páratartalom és elem. A C++ teszt
AddressSanitizer és UndefinedBehaviorSanitizer mellett futott.

**Itt nem futott tényleges ESPHome-fordítás és Home Assistant API-s hardverteszt.**
A host-teszt JSON-C helyettesítőt használ, nem az igazi ArduinoJson/ESPHome
futásidejét. Az API-átvitel, a firmware memóriaigénye, az OTA-méret és a valós
időzített szűrők működése ezért még nem bizonyított ezzel a teszttel. A valós
készüléken már sikeresen dekódolt EV1527 jel a korábbi v0.1.1 eredménye.

A `TEST_RESULTS_NATIVE_API.txt` a tényleges host-kimenet. A mellékelt külön nevű
GitHub Actions workflow további ESPHome 2026.9.0-s fordítási ellenőrzésre használható,
de itt nem indítottam el. A `tests/compile-native-esp8266.yaml` kizárólag fordítási
próba: nincs RF-vevő beállítva, és nem a saját készülékedre telepítendő.

## Felhasznált elsődleges dokumentáció

ESPHome és Home Assistant dokumentáció, megtekintve 2026-09-22:

- https://esphome.io/components/event/
- https://esphome.io/components/event/template/
- https://esphome.io/components/text_sensor/template/
- https://esphome.io/components/json/
- https://esphome.io/components/script/
- https://esphome.io/components/packages/
- https://esphome.io/components/api/
- https://esphome.io/components/mqtt/
- https://esphome.io/components/sensor/filter/timeout/
- https://esphome.io/components/binary_sensor/
- https://www.home-assistant.io/integrations/esphome/
- https://www.home-assistant.io/integrations/event/

A dekóder és az eredeti pluginok technikai alapja a felhasználó által feltöltött
`RFLink-5.5wj(2).zip` és a korábban kiadott `esphome-rflink-v0.1.1-rx.zip`.
