# RFLink v0.1.5-rxgate1 – az RF-impulzusgyűjtés szüneteltetése

## Cél és korlát

Ez a teljes v0.1.5 RFLink-rendszer kiegészítése, nem teljes projekt és nem új firmware-bináris.
A Wi-Fi-hiba oka a rendelkezésre álló naplóból még nem bizonyított. A dekóderhívások
száma nulla volt, de a remote_receiver közben tovább gyűjtötte az impulzusokat.
Ez a változat a GPIO megszakításkezelőjét és a vevő nagyfrekvenciás főciklus-kérését
is szünetelteti, nem csak az RFLink dekódolását.

**Csak ESP8266 / Arduino céleszközre készült**, az ESPHome 2026.9.0 forrásának alapján.
A konfiguráció elutasítja az ESP32-t és más célokat. Más ESPHome-verzióval nincs
igazolt kompatibilitás. Ne frissíts egyidejűleg keretrendszer-verziót is ehhez a próbához.

A host C++20 tesztek sikeresek. Nem történt teljes ESPHome-konfigurációvalidálás,
kódgenerálás, Xtensa-firmware-fordítás, rádiós, Wi-Fi-, API- vagy OTA-hardverpróba.
A tesztek nem bizonyítják, hogy ezzel a konkrét Wi-Fi-hiba megoldódik.

## Telepítés a már meglévő v0.1.5 mellé

1. Mentsd el a jelenlegi készülék-YAML-t és a repó működő állapotát.
2. A GitHub-repóba másold az új könyvtárat azonos útvonalra:

   ```text
   components/remote_receiver/__init__.py
   components/remote_receiver/remote_receiver.h
   components/remote_receiver/remote_receiver.cpp
   components/remote_receiver/LICENSE_GPLv3.txt
   components/remote_receiver/LICENSE_MIT.txt
   ```

   A könyvtár legyen a meglévő `components/rflink` TESTVÉRE, ne azon belül.
   A ZIP-et ne egyetlen fájlként töltsd fel. Ez innen még nincs a GitHubra publikálva.
3. A meglévő `components/rflink/`, `packages/` és `RFLink/` tartalmát ne cseréld le.
   Mind az 54 eredeti plugin/config/old fájl változatlan marad.
4. A készülékhez használd a teljes `rflink-rxgate1-proba.yaml` fájlt. A saját
   `secrets.yaml` és a kulcsok maradjanak helyben; ne kerüljenek GitHubra.
5. Az új YAML az `external_components` bejegyzésben ezt kéri:

   ```yaml
   components: [rflink, remote_receiver]
   ```

   Ez tudatosan a beépített vevő helyett a repóban lévő változatot tölti be.
6. Forrásfrissítés, konfigurációellenőrzés, Clean build files, majd fordítás.
   A jelenleg Wi-Fi nélküli eszközre **USB-n telepíts**, és USB-n figyeld az indulást.
   Ez a csomag nem javítja meg a még futó firmware-t a telepítés előtt.

## Fontos: ez nem beépített ESPHome-opció

A `capture_enabled: false` és a C++ `set_capture_enabled()`, `is_capture_enabled()`,
`get_edge_count()` az itt mellékelt külső komponens új lehetőségei.
A három komponensfájl telepítése nélkül az új YAML nem használható.
Az ismeretlen `capture_enabled` opcióra vagy hiányzó `set_capture_enabled` tagra
utaló hiba azt jelzi, hogy még a beépített/régi vevő töltődik be.

## Mi marad meg?

- `rx_plugins: all`: mind a 48 elérhető RX-plugin kiválasztása.
- Mindkét csomag: `rflink-ha-all-data.yaml`, `rflink-ha-gestures.yaml`.
- Hőmérséklet-, elem-, páratartalom- és további RF-adatmezők, EV1527-gesztusok.
- GPIO5, `filter: 100us`, `idle: 5ms`, `buffer_size: 1000b`.
- Titkosított API és a régi titoknevek.
- Nincs MQTT és nincs nyers `dump`.
- Az eredeti 083-as kompatibilitási javítás és a PROGMEM-javítás is megmarad.
- TX továbbra sincs megvalósítva. A teljes készlet rádiós helyessége és hosszú távú
  stabilitása ezzel a kiegészítéssel sincs igazolva.

## Működés

Az induló `capture_enabled: false` már a setup előtt érvényesül: a vevő
megszakításkezelője az inicializáláskor sem kerül rövid időre bekapcsolásra.
A puffer egyszer lefoglalódik; kapcsolódásonként nem újra és újra foglaljuk.

A bekapcsolás feltétele: Wi-Fi kapcsolódva, API állapotfeliratkozás jelen,
a HA engedélyező kapcsoló ON, nincs OTA, és `rf_capture_permitted` igaz.
Öt másodperc folyamatos kész állapot után indul a vétel és a dekódolás.
A készenléti feltételt a fő YAML másodpercenként ellenőrzi; a hálózati szakadás
észlelése nem feltétlenül azonos a fizikai megszakadás pillanatával.

Kapcsolatvesztés észlelésekor, a HA-kapcsoló kikapcsolásakor és OTA-kezdéskor:
- leválasztja kizárólag az RF-adatláb saját megszakításkezelőjét;
- visszavonja ennek a vevőnek a nagyfrekvenciás főciklus-kérését;
- eldobja a félkész/függőben lévő RF-csomagot;
- szünetelteti a dekódert és újraindítja a készenléti várakozást.

A rendszer megszakításait nem tartja globálisan tiltva, és nem kapcsolgat táp-GPIO-t.
A szünet alatt elveszett jel nem kerül később visszajátszásra. Az `on_raw` és más
vevőhallgatók ilyenkor nem kapnak adatot. A vevő főciklusfüggvénye regisztrálva marad,
de kikapcsolt vételnél rögtön visszatér.

## Napló és próba

Induláskor ezt keresd:

```text
v0.1.5-rxgate1: CAPTURE and DECODE wait for API; all RX plugins retained
Remote Receiver rxgate1 (ESP8266 / based on 2026.9.0):
  Capture enabled: NO
RX plugins compiled: 48
```

A HA-csatlakozás előtti állapotban:

```text
CAPTURE=OFF; DECODE=OFF; ... frames=0; calls=0; observed=0; irq_total=0
```

Az `irq_total` a vevő saját megszakításkezelőjének hívásszáma (a szűrt élek is
beleszámítanak), nem a rádiós parancsok száma. A `frames` az átadott nyers
impulzussorok száma, nem az érvényes parancsoké. Mindkét számláló összegző:
induláskor nulla, későbbi kikapcsoláskor megáll, nem nullázódik.

Wi-Fi + HA API állapotfeliratkozás és a várakozás után:

```text
RX gate: capture=ON; irq=ON; fast_loop=ON
CAPTURE=ON; DECODE=ON; api_states=YES; wifi=CONNECTED
```

A változatlan RFLink-híd korábbi `RF capture remains active` szövege csak a híd
saját dekóderkapcsolójára vonatkozik. Ebben a próbában a tényleges vevőállapotot az
új `CAPTURE` és `RX gate` sorok mutatják. A híd verziósora változatlanul v0.1.5.

Először ne nyomj távirányítót. Várd meg a Wi-Fi- és HA-kapcsolódást, és csak az
ON állapot után próbálj rövid, dupla, tripla és tartott nyomást. Figyeld a kapcsolat
és a memória stabilitását. Az összes plugin bekapcsolása nem jelent automatikus
érzékelőfelismerést vagy minden protokollra kiterjedő gesztuskezelést.

Ha `CAPTURE=OFF`, `irq_total=0`, `calls=0` mellett is sikertelen a Wi-Fi, akkor ebben
az összeállításban már nem a ténylegesen futó RF-megszakítás vagy dekóder tartja
fel a kapcsolódást. Ez nem bizonyít routerhibát: a megmaradt szoftver és a hardver
egyéb tényezőit kell célzottan vizsgálni.
Ha OFF állapotban kapcsolódik, de ON után szakad meg, a különbség a vétel
engedélyezéséhez köthető, a pontos belső okhoz további mérés kell.

A `rf_capture_permitted: "false"` kapcsolóval külön diagnosztikai fordításban a
vétel a HA-csatlakozás után is tiltva tartható. Az első próbában maradjon `"true"`.

## OTA helyreállítási gomb

Az új YAML egy `RFLink OTA helyreállítás` safe-mode gombot is tartalmaz. Ez a
telepítés után, amikor már van HA-kapcsolat, biztonsági módba indíthatja az eszközt
hálózati frissítéshez. Ebben a módban az RF és a szokásos HA-entitások nem működnek.
Most ne nyomd meg a normál indulási próba közben. Nem segít visszamenőleg a már
futó, elérhetetlen firmware-en.

## Visszaállítás

A mentett 0.1.5 YAML visszaállításával a külső komponensek listája ismét `[rflink]`.
Az új fájlok maradhatnak a GitHubon, ha a konfiguráció nem tölti be őket.
Clean build és új telepítés szükséges. Ez a visszaállítás nem módosít eredeti plugint.

## Tesztek és forrás

A `TEST_RESULTS.txt` és `TEST_LOGS/` a tényleges host tesztek eredményeit tartalmazza.
Futtatás a teljes v0.1.5 repóval:

```bash
python3 tests/rx_gate/run_tests.py --repo /utvonal/esphome-rflink --out /tmp/rxgate-results
```

A teszt és a helyettesítő fejlécfájlok nem kerülnek a firmware-be.

Upstream alap (ESPHome 2026.9.0):
- https://raw.githubusercontent.com/esphome/esphome/2026.9.0/esphome/components/remote_receiver/remote_receiver.cpp
- https://raw.githubusercontent.com/esphome/esphome/2026.9.0/esphome/components/remote_receiver/remote_receiver.h
- https://raw.githubusercontent.com/esphome/esphome/2026.9.0/esphome/components/remote_receiver/__init__.py
- https://esphome.io/components/external_components/
- https://esphome.io/components/api/
- https://esphome.io/components/button/safe_mode/

A módosított C++ vevő a mellékelt GPLv3, a Python rész a mellékelt MIT licenc alatt
szerepel. A fájlok elején jelölt a változtatás és annak dátuma.
