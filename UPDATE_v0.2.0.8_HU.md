# RFLink v0.2.0.8 – eredeti Alecto Plugin 030 feldolgozás

Ez a változat a kért visszaállítás. Felülírja a v0.2.0.5–v0.2.0.7 egyedi Alecto-helyreállítását és értékszűrését. A korábbi útmutatók ilyen funkciókra vonatkozó leírásai történeti állapotot mutatnak; ehhez a verzióhoz ezt az útmutatót kövesd.

## Visszaállított működés

Az eredeti `RFLink/Plugins/Plugin_030.c` dönti el, érvényes-e az Alecto-csomag. A fájl tartalma változatlan. A bridge továbbra is a közös ESPHome → RFLink időzítésátalakítást végzi, ugyanúgy, mint a többi pluginhoz.

Kikerült az egyedi Alecto-zajösszevonás, a sérült jelek és bitek helyreállítása, az ismétlésekből történő új csomag összeállítása és a hosszú blokkok speciális bontása. Az eredeti dekóder által visszautasított sorok nem kapnak külön Alecto-javítási kísérletet.

A plugin által kiadott üzenet azonnal továbbjut. Nincs hárommintás betanulás, öt másodperces mintaköz, ötperces publikálási korlát, hőmérséklet-/elemállapot-átírás, külön nagyugrás-szűrés vagy csatornaalapú elutasítás. Az eredeti plugin ellenőrzőösszeg-, tartomány- és ismétlésvizsgálata érvényes marad. Az ismétlésként felismert, kimeneti üzenet nélküli keret továbbra sem új mérés.

A `rflink.alecto.rx`, `rflink.alecto.gate` és a külön Alecto nyers naplózása megszűnik. A szokásos diagnosztikában `ALECTO=original` jelzi a visszaállított útvonalat. A 254-es plugin általános ismeretlenjel-kijelzése továbbra is bekapcsolható.

## Feltöltés

1. Csomagold ki a ZIP-et, és a fájlokat az `esphome-rflink` GitHub-repó gyökerébe töltsd fel, azonos útvonalon felülírva a régieket. A csomag a csatolt v0.2.0.5 alaphoz képest módosult/új fájlokat tartalmazza; v0.2.0.6 vagy v0.2.0.7 után is alkalmazható. Nem kell fájlokat kézzel törölni.
2. A saját YAML külső komponensének forrása az új commitot tartalmazó ágra mutasson, például `github://vicktor1979/esphome-rflink@main`. Régi tagről a visszaállítás nem töltődik le. Ez a ZIP nem hoz létre kiadási taget.
3. Ha használod a külön `packages/rflink-alecto-006c.yaml` csomagot, azt is friss forrásból töltsd le. A régi példány a már eltávolított `SLOT`/`CHANNEL` mezőkre várna, így nem frissítené a hőmérsékletet. GitHubról betöltött `packages` esetén a forrás ága/tagje és gyorsítótára is számít.
4. Szükség esetén a GitHub `external_components` és `packages` forrásnál ideiglenesen `refresh: 0s` használható. Ezután tiszta fordítás és telepítés. Induláskor az RFLink build verziója **v0.2.0.8** legyen.
5. Ha az előző összehasonlító próba miatt már átírtad az `idle` értékét 7 ms-ra, a projekt korábbi alapbeállításához állítsd vissza **`idle: 5ms`**-ra. A `filter: 100us`, `buffer_size: 1200b`, `capture_enabled: false`, `high_frequency: false` és a GPIO5 marad. A 030-as plugint kapcsold be; a 254 normál használatkor legyen kikapcsolva.

A v0.2.0.7 perjeles névjavítása és titkosított OTA-példái megmaradnak. A már működő saját OTA-beállítást emiatt nem kell újból átállítani. Az OTA-átállás részletes leírása a `UPDATE_v0.2.0.7_HU.md` fájlban található.

## A külön Home Assistant Alecto-szenzorok

A meglévő hőmérséklet-, elem- és RF ID-entitások azonosítói megmaradnak. A csomag az első három hőmérséklet-adó teljes, eredeti RF ID-jéhez rendel egy-egy kijelzési helyet, az első elfogadott üzenettől kezdve. A hozzárendelés újraindulásig él; sorrendjét az első üzenetek érkezése határozza meg. RF ID-váltás új kijelzési azonosítónak számít. Negyedik ID nem írja felül az első három kijelzését, de a bridge az üzenetét is továbbítja az általános `on_message` kezelőknek.

A plugin nem ad külön fizikai csatornamezőt. A megtartott csatorna-diagnosztikák ezért `Nincs adat` értéket mutatnak. Az RF-üzenetbe nem kerül mesterséges `SLOT`, `CHANNEL` vagy `RFBASE` mező. Hiányzó hőmérséklet-/elemmező nem törli a korábbi értéket a külön szenzoron.

## Ellenőrzés

- Eredeti Alecto-kimenet és ismétlésszűrés; hibás checksum és tartomány visszautasítása; sérült vagy összefolyó ismétlések helyreállításának megszűnése.
- A teljes komponensen át az első mérés azonnal továbbjut; öt percen belüli új mérés, nagy ugrás, új ID és nem hőmérséklet típusú eredeti Alecto-üzenet sem kap külön szűrést.
- EV1527 dekódolás és ismétlések megfigyelése; runtime pluginkapcsolók és 254-es fallback; legacy és 55 plugint fordító extended profil.
- A tényleges Alecto YAML-lambda ellenőrzése helyettesítő JSON-/szenzorkörnyezettel: első mérés, közvetlen frissítés, három külön ID, korábbi érték megtartása. Az eredeti 54 RFLink-fájl hash-ellenőrzése.
- AddressSanitizer és UndefinedBehaviorSanitizer a célzott teszteken; a környezet korlátozása miatt a LeakSanitizer ki volt kapcsolva. Az ESP8266 auto-start C++ ág host fordítási ellenőrzést kapott.

Valódi ESPHome/Xtensa firmware-fordítás, rádiós hardverteszt és OTA-feltöltés itt nem történt. A legutóbbi napló 90 mentett nyers blokkjából az eredeti dekóder is **0** Alecto-üzenetet adott. Ez a kiadás a kért alapműködést állítja vissza; a rádiós vétel javulását nem állítja igazoltnak.

Tesztparancsok a repó gyökeréből (a régi tesztfájlnév szándékosan megmaradt):

```bash
python3 tests/run_tests.py
ASAN_OPTIONS=detect_leaks=0 python3 tests/test_alecto_burst.py
ASAN_OPTIONS=detect_leaks=0 python3 tests/test_alecto_package.py
```
