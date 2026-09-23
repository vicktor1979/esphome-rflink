# RFLink v0.1.5-holdfix1 – külön felengedési idő a felismert tartáshoz

Kiindulás: RFLink v0.1.5 + remote_receiver rxgate2, `high_frequency: false`.
Ez kiegészítés, nem teljes repó, és nem helyettesíti az rxgate2 vevőt.

## Megfigyelés és korlát
A felhasználó 09:32:04–09:32:10 között folyamatos tartásként megjelölt próbájában
ismételt hold/release/hold_release/press sorok jelennek meg. A futó kód az
összes gombállapothoz 180 ms-os felengedési időt használt. Az eddigi `max_gap_ms`
csak egy felismert szakaszon belüli keretközöket tartalmazta, a lezáró kimaradást nem.
A logból nem ismert minden sikeres keret pontos ideje; ez a javítás és a teszt nem
az eredeti rádiójel visszajátszása. A 450 ms próbaérték, nem a naplóból kimért maximum.

## Telepítés – pontosan két fájl
A GitHub-repóban cserélendő:

- `components/rflink/rflink_gestures.h`
- `packages/rflink-ha-gestures.yaml`

A készülék most működő rxgate2 YAML-ja maradhat. A meglévő csomaghivatkozás az új
fájlt tölti be. Ne adj hozzá még egy gesztuscsomagot. Ne másold vissza a régebbi
all-plugins fő YAML-t: abból hiányoznának az rxgate2 beállításai.
A `components/remote_receiver/`, a dekódermotor, az all-data csomag és az eredeti
`RFLink/Plugins/` könyvtár nem módosul. Marad `rx_plugins: all`, `high_frequency: false`,
`buffer_size: 1000b`, a jelenlegi GPIO, a vételi szűrés és a csomaglezárási idő.

A meglévő 0s frissítési beállítások mellett frissítsd a külső forrást/csomagot,
majd Clean build files, ellenőrzés, fordítás és telepítés következik.
Ha korábban a fő YAML-ban már felülírtál gesztusváltozókat, azokat ellenőrizd.
A valódi jelszavakat/API-kulcsot sem módosítani, sem GitHubra feltölteni nem kell.

## Időzítések
Az új csomag alapértékei:

```yaml
substitutions:
  rflink_gesture_release_ms: "180"
  rflink_gesture_hold_release_ms: "450"
  rflink_gesture_repeat_fresh_ms: "180"
  rflink_gesture_multi_ms: "350"
  rflink_gesture_hold_ms: "700"
  rflink_gesture_repeat_ms: "250"
  rflink_gesture_max_press_ms: "30000"
```

Ez nem önálló új `substitutions:` blokk a meglévő mellé; az értékek a meglévőbe
illeszthetők, ha helyileg szeretnéd felülírni a csomag alapértékeit.

- Rövid nyomásnál továbbra is 180 ms csend jelenti az elengedést. Ezután 350 ms
  a többkattintás várakozása. Az egyszeres kattintás a legutóbbi matching keret
  után kb. 530 ms + ütemezési idő alatt véglegesedik, nem lett lassabb.
- A legalább három valós matching kerettel, legalább 700 ms-on át felismert HOLD
  állapotnál külön 450 ms-os csendidő van. A 180 és 450 ms közti vételi kimaradás
  nem indít új holdot. A 450 ms-ot ELÉRŐ kimaradás lezárja a tartást.
- A `hold_repeat` NEM használja a teljes 450 ms türelmi időt: csak 180 ms-nál
  frissebb matching keret alapján, és az előző hold/hold_repeat óta legalább egy
  új matching kerettel mehet ki. Kimaradáskor szünetel, visszatérő jelnél folytatódhat.
  A 250 ms célidőköz minimum a tényleges kiadások között; nincs bepótló eseményáradat.
- Elengedéskor a bináris nyomva állapot és a hold_release az utolsó elfogadott
  matching keret után kb. 450 ms + ütemezési idő múlva frissül. Ez nem a fizikai
  elengedés pontos ideje. Az utolsó keret után a rövidebb frissességi időn belül
  még egy hold_repeat előfordulhat.
- A 30 másodperces tartáskorlát megmarad. API/OTA/dekóder-szüneteltetésnél a cancel
  azonnali helyi törlést végez, nem vár 450 ms-ot, és nem véglegesít kattintást.

## Amit ez nem tud megoldani
A felengedési bit nélküli jel esetén a kimaradás és a fizikai elengedés lehet
azonos megfigyelés. HOLD után 450 ms-nál rövidebb tényleges elengedés/újranyomás
egyben maradhat. A HOLD felismerése ELŐTT a 180 ms-os küszöb megmaradt, ezért az
ott fellépő nagy vételi szünet még szétszedhet egy nyomást. Ez tudatos kompromisszum,
hogy a már működő rövid dupla/tripla felismerés ne változzon meg észrevétlenül.
450 ms-nál hosszabb matching-jelhiány továbbra is felengedés. A hiányzó rádióadatot
nem gyártjuk újra, más ID-t/protokollt nem fogadunk el folytatásként.
A hosszabb türelmi idő nem teszi megbízhatóvá a sérült RF-kereteket.

## Napló és HA
Várt új indulási sor:

```text
[rflink.gesture]: v0.1.5-holdfix1; patterns=2; release=180; hold_release=450; fresh=180; multi=350; hold=700; repeat=250 ms
```

Az RFLink motor továbbra is v0.1.5, a vevő rxgate2 marad. A HA-entitások neve,
belső ID-ja és eseménytípusai változatlanok. Új HA-automatizmust a csomag nem hoz létre.

Új diagnosztikai mezők:

- `t_ms`: a helyi állapotgép időpontja ms-ban (nem falióra).
- `silence_ms`: hány ms telt el az utolsó elfogadott, pontosan megfelelő keret óta.
- `gap_ms`: az utolsó két elfogadott megfelelő keret távolsága. Új press-nél is
  megmarad, így a korábbi szakasz lezárása utáni kimaradás látható.
- `bridged`: felismert HOLD során a rövid küszöböt elérő, de a hosszút el nem érő,
  áthidalt keretközök száma. Nem elveszett csomagok darabszáma.
- `max_gap_ms`: a jelenlegi szakaszon belüli legnagyobb matching keretköz; a záró
  csendet továbbra is külön, a `silence_ms` mutatja.

A fényerő állításához a hold/hold_repeat eseményekhez érdemes egy-egy korlátozott
lépést rendelni, nem a türelmi időben is ON állapotú bináris szenzorra végtelen ciklust.
Ne fusson párhuzamosan a régi button_08 és az új gesztus ugyanarra a műveletre.

## Próba
Először 5–10 másodperc folyamatos nyomás, majd elengedés és 2 másodperc szünet.
Ezután rövid egyszeres, dupla és tripla nyomás külön próbákkal. A várt tartás:
press → hold → hold_repeat… → release → hold_release; a tartás közben nincs új
press/hold, ha a kimaradások 450 ms alattiak és a HOLD már felismert.
Nyers dump nem kell. A wifi=CONNECTED/api_states=YES/fast_loop=OFF állapotok is
maradjanak meg a tesztben. A részletes teszteredmények a TEST_RESULTS.txt-ben vannak.
