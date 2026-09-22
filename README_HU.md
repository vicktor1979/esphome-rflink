# RFLink v0.1.3 – automatikus API-indítás és teljes adatmező-kezelés

Ez KIEGÉSZÍTÉS a már működő v0.1.2-diag projekthez. Nem teljes forráscsomag.
A távirányító vételi/dekódolási kódja, a PROGMEM-javítás és az eredeti RFLink/Plugins fájlok nem változnak.

## Az első próba – három repófájl és egy eszköz-YAML

Mentsd el a jelenlegi működő firmware-konfigurációt és a GitHub-commit azonosítóját.
A meglévő `vicktor1979/esphome-rflink` repó `main` ágába azonos útvonalon tedd fel:

```text
components/rflink/rflink.h              lecserélendő
components/rflink/rflink_fields.h       új fájl
packages/rflink-ha-all-data.yaml        új fájl
```

A `rflink.cpp` maradjon a működő v0.1.2-diag változat. A `rflink_engine.cpp`,
`__init__.py`, `stage_sources.py` és az `RFLink/Plugins` mappa maradjon érintetlen.
A régi `packages/rflink-ha-api.yaml` fájlt nem kell törölni vagy átírni.

A készülék konfigurációját a mellékelt `rflink-auto-api-proba.yaml` tartalmára cseréld.
Megőrzi a nodemcuv2 célt, a GPIO5 bemenetet, 100 us szűrést, 5 ms idle-t,
1000b puffert, a titkok neveit és a két meglévő távirányító-entitást.

FONTOS: a régi és az új API-entitáscsomagot NE töltsd be együtt! A `packages`
bejegyzés az új fájlra mutasson. A forrásfrissítés mindkét Git-forrásnál 0s,
mert most még új fájlokat és módosításokat tesztelünk. A fájlokat előbb töltsd
fel a repóba, csak azután indítsd az ESPHome ellenőrzést/fordítást.

Az ESPHome Device Builderben ellenőrzés, Clean build files, fordítás és tényleges
firmware-feltöltés következzen. Az első indulást USB-n figyeld.
A secrets.yaml-t ne töltsd GitHubra. Nem szükséges új Wi-Fi-, OTA- vagy API-kulcs.

## Automatikus indítás

Induláskor a dekódolás KI. Az RF-impulzusgyűjtés továbbra is aktív.
A program másodpercenként ellenőrzi a Wi-Fit és az `api.connected` feltételt,
`state_subscription_only: true` beállítással. A puszta naplóolvasó kapcsolat nem elég.
Öt másodpercnyi folyamatos kész állapot után a dekódolás automatikusan BE.
Nincs 60 másodperces automatikus leállítás.

Az észlelt Wi-Fi-/API-állapotfeliratkozás-vesztéskor a dekódolás szünetel,
majd újbóli kapcsolódás után ismét kivárja az öt másodpercet.
A kapcsolatvesztés felismerése függ a hálózati/API-időzítésektől;
a másodperces ellenőrzés nem jelenti a fizikai hálózati hiba azonnali felismerését.

A régi „RFLink dekódolás próba” kapcsoló neve és ID-ja megmarad. Bekapcsolva most
AZ AUTOMATIKUS DEKÓDOLÁS ENGEDÉLYÉT jelzi. A tényleges állapotot az új
„RFLink dekódolás aktív” bináris szenzor mutatja. A kapcsoló induláskor engedélyezett;
a korábbi kikapcsolás nem íródik flashmemóriába és nem marad meg újraindítás után.
OTA indulásakor a dekóder szünetel, OTA-hiba után ismét engedélyezhető.

API nélkül szándékosan nem dekódol, és a szünetben vett csomagokat nem játssza
vissza később. Nem készült offline eseményarchívum. MQTT nincs ebben a próbában.

Elvárt új naplósor:

```text
[rflink.auto]: v0.1.3: API-first auto gate; full RF fields; no 60 s cutoff
[rflink.auto]: DECODE=ON; api_states=YES; wifi=CONNECTED
```

A dekóder saját verziósora továbbra is v0.1.2-diag lehet: a dekódermagot most
nem cseréljük. Az első próbában `RX plugins compiled: 2` várható.

## Minden adatmező – de csak amit a rádiós eszköz ténylegesen elküld

A mellékelt feltöltött display-forrás 34 RF-adat/metaadat-kulcsát kezeli a csomag:
25 numerikus mező, BAT/PIR/SMOKEALERT, továbbá PARAM/NAME/ID/SWITCH/CMD/RGBW.
A bootkori `RFLink_ESP` splash nem rádiós mérés; a firmware-információt az ESPHome adja.
A mezőlista és konverziók a FIELD_MAP.md dokumentumban találhatók.

A jelenlegi EV1527 távirányítód üzenetében nincs hőmérséklet vagy elemállapot.
Ezért attól, hogy a megfelelő entitások elkészülnek, ezek a gombnyomástól nem
kapnak értéket. Nincs kitalált 100%-os töltöttség, 0 °C vagy hamis „OK” állapot.

A hiányzó/hibás numerikus mező ismeretlen (NaN). A hiányzó/hibás bináris érték
ismeretlen, nem OFF. BAT=LOW jelenti az alacsony elemet, BAT=OK annak hiányát;
a BAT nem százalékos töltöttség. PIR/SMOKEALERT értéknél nincs kitalált visszaállítás.
Ez kísérleti vételi híd; a füstjelzés megjelenítése nem helyettesít tanúsított riasztót.

A hexadecimális mezőket a meglévő javított híd SZÖVEGKÉNT küldi a JSON-ban.
Ez a parser ehhez a JSON-hoz készült, nem a régi formázó esetleges hibás
idézőjelezésű/vezető nullás soros szövegéhez. A HUM már a hídban át van alakítva
BCD-ből decimális számmá, ha ezt a plugin kéri; itt nem alakítjuk át másodszor.

## Diagnosztikai pillanatkép és állandó szenzor nem ugyanaz

Az „RF utolsó ...” entitások EGY legutóbbi dekódolt csomag mezőit mutatják.
Ha másik eszköz küld adatot, a hiányzó mérési mezők ismeretlenre váltanak.
Így az egyik eszköz azonosítója nem jelenik meg egy másik eszköz korábbi hőmérsékletével.
Ugyanez történik, ha egy időjárási csomagot távirányítóüzenet követ.
Ez nem a szenzor meghibásodása; ez a közös diagnosztikai nézet működése.
A mezők API-frissítése nem atomikus tranzakció. Automatizmushoz és tartós grafikonhoz
rögzített NAME+ID szerinti entitást használj, ne ezt a közös állapotképet.

A távirányító eseményei azonnal feldolgozódnak, és minden elfogadott ismétlés
külön esemény marad. A sok diagnosztikai mezőt legfeljebb másodpercenként egyszer
frissítjük, az időközben érkezett LEGUTOLSÓ csomagból. Ez tehermentesíti az API-t;
a köztes csomagok nem alkotnak diagnosztikai archívumot. A számláló minden érvényes
NAME+ID-t tartalmazó dekóderüzenetet számol, függetlenül a pillanatkép frissítésétől.

A csomag 48 entitást definiál; ebből az öt teljes-JSON rész alapból letiltott.
A fő YAML ehhez egy kapcsolót és egy aktív-dekódolás szenzort ad. Ez több entitás,
mint a korábbi próba, ezért a hardveres memória/API-stabilitás új ellenőrzést igényel.
A két meglévő esemény-entitás és az eredeti alapdiagnosztikák ID-ja/neve megmaradt.

### Állandó entitások egy konkrét RF-eszközhöz

Ehhez a rádiós eszköz valódi NAME és ID mezője szükséges. Az új
`tools/make_rf_device.py` bármely támogatott mezőkészlethez generál kis csomagot.
Csak a kiválasztott mezők kerülnek bele, nem automatikusan ötven entitás minden adóhoz.

Példa (a protocol és rf-id helyére a ténylegesen dekódolt adatot írd):

```bash
python -m pip install pyyaml
python tools/make_rf_device.py --prefix kert_rf --name "Kert" --protocol "Cresta" --rf-id "CSERELD_A_SAJAT_IDRA" --fields TEMP,HUM,BAT --output packages/rflink-kert.yaml
```

A generatorhoz a ZIP-ben lévő FIELD_MAP.json is kell az eredeti helyén.
A kész YAML-t töltsd a saját repó `packages/rflink-kert.yaml` útvonalára, majd a
fő YAML packages / rflink_api / files listáját egészítsd ki:

```yaml
    files:
      - packages/rflink-ha-all-data.yaml
      - packages/rflink-kert.yaml
```

A generált csomag saját `rflink.on_message` listabejegyzést ad hozzá. Nem kell
kézzel kiegészíteni a központi feldolgozó lambdát. A fő YAML-ban ezért listás
az on_message formája: az ESPHome a csomagok listáit összefűzi.

A csomag kizárólag a megadott NAME+ID párost fogadja. Hiányzó mező esetén az adott
eszköz korábbi értékét tartja meg az egyedi lejáratig; más eszköz sosem írhatja felül.
Azonos értékű új mérés is frissíti a lejáratot. Alapesetben 60 perc után ismeretlen
lesz az a numerikus/bináris mező, amely nem frissült. `--stale-after 10min` átállítja.
A szöveges mezők utolsó ismert értéket tartanak meg, nincs automatikus lejáratuk.
Érvénytelen, de jelen levő mezőnél nem várunk a lejáratig: a mérés ismeretlenné válik.

`--fields ALL` minden mezőt felvesz egy konkrét eszközhöz, de ESP8266-on érdemes
csak a valóban létező mezőket választani. Ez konfigurációs csomaggenerátor,
nem futás közbeni eszközfelderítés. Új entitásokhoz új firmware-fordítás kell.
A mappában lévő `rogzitett-erzekelo-PELDA.yaml` szándékos helykitöltő ID-t tartalmaz,
és nincs automatikusan betöltve.

## Melyik RF-protokollt engedélyezzük?

Az első próbában marad `[61]`: EV1527 + az automatikus 001 előfeldolgozó.
Ez teszteli az automatikus indulást és a bővebb API-konfigurációt.
A parser minden mezőt ismer, ettől még nem fordul be minden rádiós protokoll.

Ha az első próba több percig stabil, második lépésben:

```yaml
rflink:
  rx_plugins: [34, 40, 61]
```

Ez az eredeti pluginfájlok szerint a Cresta, Mebus és EV1527 kiválasztása,
a 001-essel együtt 4 plugin. Újabb fordítás és feltöltés kell.
A konkrét időjárási eszköz más dekódert is igényelhet. Az ismeretlen/át nem vett
eszközből a program nem tud adatot előállítani. A `configured` (47) és `all` (48)
a híd korábbi funkciójaként megmarad, de a teljes készlet ESP8266-os hálózati
stabilitása nincs igazolva; ne kapcsoljuk vissza ezt az első automatikus próbával együtt.

## Teljes JSON és pontos nyers számok

Az „RF utolsó üzenet” 250 bájtig teljes JSON-t mutat; hosszabbnál rövid összefoglalót.
A mezők ettől még mind feldolgozódnak. A pontos, akár 1024 bájtos eredeti JSON-hoz:

```yaml
substitutions:
  rflink_full_json_parts: "true"
```

Fordítás/feltöltés után a HA-ban engedélyezd az RF teljes JSON 1 ... 5 entitásokat.
A részeket sorrendben, elválasztó nélkül összeillesztve az eredeti JSON áll elő.
A darabolás nem vág szét UTF-8 karaktert. Ez továbbra sem nyers RF-impulzus dump.
Az ESPHome számértékei float pontosságúak; nagy mérőállás pontos eredeti egészszáma
az eredeti JSON-ból olvasható ki. Ismeretlen új kulcsok is megmaradnak ebben a JSON-ban,
de külön HA-entitás nem keletkezik automatikusan egy új, nem definiált kulcshoz.

## Diagnosztika és elfogadási próba

USB-n várd meg az API állapotfeliratkozását, majd az automatikus DECODE=ON sort.
Kézi bekapcsolás nélkül működjön a button_08 esemény. Várj legalább 2–3 percet:
az eddigi 60 másodperces próbakorlát már nincs benne.
Ezután próbálj egy HA-integráció-újratöltést is; az újrafeliratkozást újabb öt
másodperces várakozás és automatikus újraindulás kövesse.
A 10 másodperces logban heap, legnagyobb szabad blokk és fragmentáció is szerepel.
A decode_max_us a dekóder, callback_max_us a közvetlen üzenetkezelés,
snapshot_max_us a késleltetett teljes diagnosztikai feldolgozás mért maximuma.
Ezek helyi futási idők, nem a rádiós vételtől a HA megjelenítéséig tartó késleltetések.

## Tesztek és határok

A TEST_RESULTS.txt felsorolja a ténylegesen lefuttatott ellenőrzéseket.
G++-szal teszteltük az aktuális YAML-lambdákat json-c alapú mockkal,
AddressSanitizer/UndefinedBehaviorSanitizer mellett; továbbá az eredeti híd
szintetikus EV1527 tesztjeit és a 34 mezőt kiíró formázót.
A json-c tesztadapter NEM az ESPHome tényleges ArduinoJson-függősége.
Ebben a környezetben nincs ESPHome telepítve, és a függőségek letöltése nem volt
elérhető, ezért teljes ESPHome-validálás, firmware-fordítás, flashméret- és
hardveres API-próba NEM történt. Ez a következő tesztváltozat, nem készre
minősített firmware. A korábbi hálózati hiba belső okát sem tekintjük bizonyítottnak.

## Forrásalap

A felhasználó által feltöltött `Beillesztve text(20260922-134720).txt-be`
display-függvényei határozzák meg a mezőneveket/formátumokat és az ott kifejezetten
leírt átszámításokat. A hiányos egység-/skálaleírásokat nem pótoltuk találomra.
Az eredeti RFLink-5.5wj(2).zip szolgált a pluginok bájtszintű összehasonlításához.
Az ESPHome API-, packages-, sensor- és binary_sensor-dokumentáció a használt
interfészek alapja. A frissítés semmilyen valódi titkot nem tartalmaz.
