# v0.2.0.9 — az eredeti RFLink-vétel összehasonlító próbája

Ez a csomag **választható vételi módot** ad a feltöltött RFLink-5.6wj projekt alapján. A hardveres javulás még nincs igazolva. Az Alecto és EV1527 eredeti dekódolója, a checksum-ellenőrzés és az EV gombkezelés megmarad. A jelenlegi megszakításos vétel egy YAML-beállítással visszaállítható.

## Mit mutat a forrás és az utolsó napló?

- Az RFLink-5.6wj `Plugin_030.c`, `Plugin_061.c` és `Plugin_001.c` forrása bájtra megegyezik a jelenlegivel. A Plugin 030 visszaállítása tehát nem állította vissza az eredeti jelgyűjtést.
- Az eredeti `FetchSignal()` közvetlen GPIO-lekérdezéssel mér. A mostani ESPHome-vevő a GPIO-megszakításokból gyűjtött időzítéseket dolgozza fel. Eltér az indulás és a zajos, túl rövid impulzusok kezelése is.
- A `rflink-logs (3)(1).txt` biztosan v0.2.0.8-at mutat. Az utolsó sorban 114616 átadott keret, nulla jelzett túlcsordulás és nulla vevő-újraszinkronizálás szerepel; a vételi kapu aktív. Ez kizárja a naplóban jelzett pufferhibát, de nem bizonyítja, hogy minden rádiós él helyesen lett megmérve.
- Az EV `ok=0` és `observed=0`; nem tudjuk, volt-e a felvétel alatt tényleges EV gombnyomás. A `decoded=839` a 254-es diagnosztikai plugin elfogadásait is számolja, ezért nem jelent 839 értelmes hőmérséklet- vagy EV-üzenetet.
- A napló Alectóra jellemző kb. 480 / 1920 / 4480 µs szakaszok között szabálytalan impulzusokat mutat. Az ilyen megjelenített hosszú sorok `...` jellel csonkoltak; a hiányzó végét nem lehet megbízhatóan visszaállítani.
- A friss indulási napló **4 lefordított plugint** jelez, a későbbi aktív lista 001/030/061/254. Az `extended` profil neve önmagában nem bizonyít 55 lefordított plugint.

Ezekből nem dönthető el biztosan, hogy a hibát a jelgyűjtés, rádiós zavar, a jel alakja vagy más ok okozza. A polling mód célja az eredeti és az új jelgyűjtés elkülönített összehasonlítása ugyanazon a hardveren. A régi, már sérülten megmért adat offline újrajátszása nem teszteli az új jelmérési módot.

## Mit változtat az új mód?

`capture_mode: rflink_polling` esetén:

- Aktív vételnél közvetlenül méri a GPIO5 logikai szintjeinek időtartamát; nem telepít GPIO-vevőmegszakítást. A rendszer/Wi-Fi megszakításokat nem tiltja le.
- Az eredetihez hasonlóan legalább 400 µs LOW előtag után kezdi a mérést. A 100 µs-nál rövidebb mért impulzus eldobást okoz; nincs bitpótlás, impulzusösszeragasztás vagy hőmérséklet-kitalálás.
- A lezárás 5 ms. A keresés legfeljebb 25 ms, egy keret mérése legfeljebb 200 ms, a tárolás legfeljebb 291 időzítési elem. A kód időkorlátja nem valós idejű garancia rendszermegszakítások alatt.
- Az elkészült keret ugyanazon a bridge-en, eredeti pluginokon és EV képkocka-callbacken halad át, mint eddig. A Plugin 061 által elnyomott ismétlések továbbra is táplálják az EV gombállapot-kezelést.
- A vételi kapu, a Wi-Fi/API indulási feltétel és az OTA-pihenő megmarad. A polling mód aktív vételkor saját gyors főciklust kér, a `high_frequency: false` mellett is. Ezért itt a `fast=ON` és `irq=OFF` helyes.
- A polling a főciklus más feladatait egy keret idejére késleltetheti. Ez **nem általános csere minden protokollhoz**: a 200 ms-nál hosszabb csomagokra nem való. A cél az Alecto V1 + EV1527 telepítés összehasonlító próbája.

Az alapértelmezés továbbra is `capture_mode: interrupt`. A fájlok puszta feltöltése nem váltja át a saját YAML-edet pollingra.

## Feltöltés és a saját YAML módosítása

1. A ZIP tartalmát a GitHub-repó **gyökerébe**, a könyvtárszerkezetet megtartva töltsd fel. A csomag a v0.2.0.5 óta szükséges új/módosított fájlokat tartalmazza; v0.2.0.8-ra is rátehető.
2. A saját konfigurációban a forrás mutasson a frissített ágra, például `github://vicktor1979/esphome-rflink@main`, és a komponenslista tartalmazza a `remote_receiver` komponenst is. A régi tag nem fogja átvenni a feltöltést.
3. Frissítsd a külső komponensek és a használt GitHub package-ek cache-ét. Átmenetileg `refresh: 0s` használható a megfelelő Git-forrásbejegyzésekben; utána visszaállítható a korábbi érték. Majd végezz tiszta újrafordítást. A fordítási fájlok törlése önmagában nem bizonyítja a Git-forrás frissülését.
4. A meglévő `remote_receiver:` blokkba add hozzá a `capture_mode` sort; ne hozz létre második azonos blokkot:

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

A `rflink:` blokkod, a `plugin_switches: [30, 61, 254]`, az `auto_start: true` és az `on_message` maradhat. Az OTA titkosítási és a perjeles entitásnév-javítások benne vannak. A saját OTA YAML korábbi átállási útmutatója: `UPDATE_v0.2.0.7_HU.md`.

Az első próba alatt 030 és 061 legyen ON, 254 OFF. Ne legyen `dump: raw`. Az általános naplószint legyen INFO; az eddigi `rflink: DEBUG` tagbeállítás maradhat. Ezzel a sűrű szenzor-/nyersnaplózás kevésbé terheli a mérést.

## Mit ellenőrizz a feltöltés után?

Induláskor v0.2.0.9 és a következő mód legyen látható:

```text
RX gate: capture=ON; mode=rflink_polling; irq=OFF; fast_loop=ON
rflink.capture: mode=rflink_polling; poll_short=...; poll_limit=...
rflink.rfdiag: ALECTO=original exact74=... ok=...; EV near=... exact50=... ok=... last=...
```

- Alecto `exact74`: ennyiszer kapott az eredeti 030-as plugin pontosan 74 időzítési elemet. Ettől még lehet hibás a keret.
- Alecto `ok`: az eredeti plugin elfogadta a keretet; az ismétlések is beleszámítanak.
- EV `ok`: az eredeti 061-es plugin elfogadta; szintén keretszám, nem gombnyomásszám.
- `poll_short`: túl rövid impulzus miatt félbehagyott mérés. `poll_limit`: idő-/mérethatár miatt félbehagyott mérés. Nem az elveszett rádiós csomagok pontos száma.
- Az eddigi `irq` helyett a periodikus diagnosztikában `edges` jelenik meg: IRQ módban megszakítások, polling módban megfigyelt szintváltások száma. A háttérfeladatok alatt történt, pollinggal nem látott élek nem szerepelnek benne.

Először próbálj néhány rövid EV-nyomást, egy dupla kattintást és 2–3 másodperces nyomva tartást. Ezután várj több Alecto-adást, és ellenőrizd a gyári vevővel a hőmérsékletet. Ha API-szakadás, EV-romlás vagy ismétlődő hosszú főciklus-probléma jelentkezik, válts vissza. A hosszabb polling-mérés ESPHome főciklus-időfigyelmeztetést is kiválthat; ez önmagában nem checksum-hiba, de a hálózati működést hardveren ellenőrizni kell.

Ha nincs javulás, a következő napló tartalmazza az indulást, a fenti diagnosztikát, a kipróbált EV-gombnyomások idejét és több Alecto-adást. Az adó pontos típusa és a gyári vevőn látható hőmérséklet is szükséges a protokoll biztos azonosításához; a márkanév önmagában kevés.

## Visszaállítás

Egy sor módosítása és újrafordítás:

```yaml
  capture_mode: interrupt
```

A sor törlése ugyanezt az alapértelmezést adja. A GPIO, a `filter: 100us`, az `idle: 5ms`, az `1200b` puffer és az EV eseménybeállítások maradjanak.

## Ellenőrzés és korlátok

- A tényleges C++ vevő, bridge és pluginok GPIO-szimulációs próbái legacy és extended összeállításban. A polling próba nem kész dekódolt adatot ad a vevőnek, hanem időzített GPIO-szinteket.
- Alecto 22,5 °C elfogadás és hibás checksum elutasítás; négy EV-ismétlés külön megfigyelése az eredeti JSON-ismétlésszűrés mellett; egyes/dupla kattintás, hold/hold_repeat/hold_release; EV tartás közé tett Alecto-keret.
- Zaj utáni újraszinkronizálás, folyamatos jel idő-/mérethatára, mikroszekundumos óra átfordulása, vételi kapu, újraindított vétel és leállítás. A teszt a vevőhívások között 1,5 ms más munkát is modellez.
- A meglévő IRQ vételi, ütemezett EV-, indulási-, újracsatlakozási- és OTA-kaputesztek is megmaradnak.
- AddressSanitizer és UndefinedBehaviorSanitizer aktív. A firmware-élettartamú pufferek miatt a LeakSanitizer nincs használatban.
- Nem történt tényleges ESPHome séma-/kódgenerálási ellenőrzés, Xtensa firmware-fordítás vagy NodeMCU/SRX882 rádiós, Wi-Fi/API/OTA próba. A szimuláció nem modellezi a rádiófrekvenciás zavarást vagy az ESP8266 rendszermegszakításainak késleltetését. **A hardveres EV működés változatlansága és az Alecto javulása ezért még nem garantálható.**

Futtatás a teljes repóból: `python3 tests/test_polling_capture.py` és `ASAN_OPTIONS=detect_leaks=0 python3 tests/test_alecto_burst.py`.
