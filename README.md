# RFLink v0.1.2-diag – futás közben kapcsolható dekóder

Ez diagnosztikai kiegészítés a korábbi v0.1.1 forráshoz, NEM bizonyított javítása az API-kapcsolódási hibának.
Az eredeti pluginokat és a PROGMEM-javítást tartalmazó `rflink_engine.cpp`-t nem módosítja.

## Mit mutatott a 13:00–13:08 közötti eszköznapló?

A Wi-Fi felállt. Az API 13:04:11-kor TCP-kapcsolatot fogadott a 192.168.1.111 címről, és 13:04:33-kor egy `aioesphomeapi` nevű klienshez `connected` üzenetet írt. Ez nem bizonyítja a Home Assistant entitásainak sikeres feliratkozását. Az első kapcsolat később megszakadt. A 13:04:58-kor fogadott új kapcsolat 13:05:58-kor kézfogási időtúllépéssel megszakadt. A köztes diagnosztikák Wi-Fi-kapcsolatot és 35 kB körüli szabad memóriát mutattak. A 13:07 körüli uptime-visszaesés új indulást jelez, de a kiváltó ok nem állapítható meg a megszakadt USB-naplóból.

A dekóder nélküli korábbi firmware-rel a HA kapcsolódott. Eddig viszont az RFLink hozzáadása/kivétele új fordítást is jelentett. Ezzel a csomaggal ugyanabban a firmware-ben kapcsolható az RFLink-feldolgozás: a linkelt kód és a rádiós puffer változatlan marad.

## Telepítés

1. Készíts másolatot a jelenlegi eszköz-YAML-ról és a két eredeti komponensfájlról.
2. A GitHub-repóban cseréld le, azonos útvonalon:
   - `components/rflink/rflink.h`
   - `components/rflink/rflink.cpp`
3. A többi komponensfájl, az `RFLink/Plugins` és a `packages/rflink-ha-api.yaml` maradjon meg.
4. A készülékhez használd a mellékelt `rflink-api-kapcsolhato-proba.yaml` teljes konfigurációt.
   Az eszköz neve, a GitHub-hivatkozások és a titkok nevei a beszélgetésben megadottak.
   A saját `secrets.yaml` változatlan maradjon, és ne töltsd fel GitHubra.
5. Frissített `main` ággal fordíts és töltsd fel. A YAML `refresh: 0s` beállítása megmarad.
   A korábbi build törlése elvégezhető, de a forrás Git-frissítése és a firmware feltöltése külön szükséges lépés.
6. USB-n ellenőrizd: `v0.1.2-diag`, `RX plugins compiled: 2`, `Decode enabled: NO`.

A két új C++ fájl önmagában nem kapcsolja ki a korábbi konfiguráció vételét: alapértelmezésben a dekóder továbbra is ON. A mellékelt teszt-YAML a kapcsoló inicializálásával és `on_boot` művelettel állítja OFF-ra.

## Próba

Induláskor a rádiós impulzusgyűjtés aktív, a dekóder és az RFLink `on_message` visszahívása viszont nem fut.
A kihagyott rádiós csomagok eldobódnak, később sem kerülnek lejátszásra. A csomagszámláló ekkor nullán marad.

Először töltsd újra a HA meglévő ESPHome-bejegyzését, és várd meg, hogy az `RF dekódolt csomagok` szenzor elérhető legyen. Ne változtass API-kulcsot, portot, Wi-Fi-jelszót, pufferbeállítást vagy GPIO-t.

A készülékhez egy új, kizárólag szoftveres kapcsoló tartozik: **RFLink dekódolás próba**.
Ha az API felállt, kapcsold be. Néhány rövid gombnyomással ellenőrizd az RF-eseményt és a kapcsolatot.
**60 másodperc után a dekóder automatikusan kikapcsol**, újraindulás nélkül. Kézzel hamarabb is kikapcsolható.
A 60 másodperc nem API-timeout-módosítás, hanem a teszt kapcsolójának helyi védelmi időzítése.
A script a főciklus működésétől függ: teljes processzorfagyás esetén nem tud végrehajtódni.

Ez nem végleges üzem: minden induláskor OFF, minden ON-próba csak 60 másodperces. A működő beállítás később e korlátozás nélkül használható; előbb az OFF→ON→OFF eredményt kell rögzíteni.

## Napló

10 másodpercenként egy összefoglaló sor:

- `DECODE=OFF/061`: a valódi futási engedély, nem pusztán a lefordított pluginlista.
- `api_any`: az ESPHome `api.connected` feltétele. Önmagában nem bizonyít entitásfeliratkozást.
- `api_states`: az `api.connected: state_subscription_only: true` feltétele. Az állapotokra feliratkozott klienst jelzi, nem pusztán a hálózati naplóolvasót. Nem kizárólag Home Assistant lehet ilyen kliens.
- `frames`: a nyers fogadó által átadott impulzussorok száma, zajt is tartalmazhat.
- `calls`: RFLink-dekóderhívások száma.
- `skipped`: kikapcsolt dekóder miatt át nem adott impulzussorok száma.
- `decoded`: a HA-csomag feldolgozója által elfogadott üzenetek száma.
- `decode_max_us`: a dekóderhívás eddigi legnagyobb mért időtartama mikroszekundumban.
- `callback_max_us`: az `on_message` visszahívás eddigi legnagyobb időtartama mikroszekundumban; a külön RFLink JSON-log kiírását nem tartalmazza.

A futásidők falióra-időt mérnek, a közben kiszolgált megszakításokat is tartalmazhatják; nem CPU-kihasználtság-százalékok.
A mérések nem kerülnek nullázásra a kapcsolóváltásokkal.

A YAML szándékosan az API-feliratkozás hivatalos YAML-feltételét használja, nem egy verziófüggő C++ `is_connected(true)` túlterhelést.

## Az eredmény értelmezése

- Ha DECODE=OFF mellett is fennáll a hiba: a dekóder aktuális futása és az RF-esemény visszahívása nem szükséges a hibához ebben a firmware-ben. Ne állítsd a kapcsolót ON-ra. Ekkor a HA-oldali hibanapló és az eszköz azonos idejű USB-naplója kell.
- Ha OFF mellett feláll, ON mellett megszakad, majd OFF mellett visszatér: a feldolgozás futásával összefüggő eltérés mérhető ugyanabban a firmware-ben. A futásidő-mérések segítenek a dekóder és a visszahívás különválasztásában.
- Ha OFF és ON mellett is stabil és az esemény-entitás frissül: a kapcsolat felépítése előtti szünet ebben a próbában segített. Ettől még nem ismert minden korábbi hiba oka, és a teljes 47 pluginos készlet nincs ellenőrizve.

## Ellenőrzések és korlátok

Lásd `TEST_RESULTS.txt`. Lefutott a módosított komponens és a változatlan EV1527-dekóder C++17/UBSan host tesztje minimális ESPHome-helyettesítő fejlécekkel. Ellenőriztem a kapcsolóágakat, az ismétlésszűrés megmaradását és a diagnosztikai számlálókat. A tesztben az óra szimulált: ez nem ESP8266-teljesítménymérés.
Mind az 54 pluginfájl bájtról bájtra egyezik a feltöltött eredeti RFLink-archívummal.

**Nem futott itt valódi ESPHome konfiguráció-ellenőrzés/kódgenerálás, ESP8266-firmware-fordítás, hálózati API-próba vagy hardveres teszt.** A GitHub aktuális tartalmát nem sikerült beolvasni; a csomag alapja a beszélgetésben kiadott v0.1.1 archívum és a megadott YAML.

Elsődleges referenciaforrások:
- ESPHome API: https://esphome.io/components/api/
- ESPHome 2026.9.0 API szerver és feltételek: https://raw.githubusercontent.com/esphome/esphome/2026.9.0/esphome/components/api/api_server.h
- ESPHome 2026.9.0 API kapcsolat: https://raw.githubusercontent.com/esphome/esphome/2026.9.0/esphome/components/api/api_connection.cpp
- Template switch: https://esphome.io/components/switch/template/
