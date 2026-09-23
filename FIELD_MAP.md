# RFLink adatmezők – v0.1.7 (az eredeti 34 mező változatlan)

A feltöltött display-forrás mezőnevei és ábrázolásai az alap. A működő híd a hexadecimális értékeket idézőjeles JSON-szövegként továbbítja. A táblázat a HA-réteg új feldolgozását írja le. A `nyers` azt jelenti, hogy az egész értéket kiolvassuk a megadott számrendszerből, de nem találunk ki fizikai skálát.

| Mező | A híd JSON-típusa | Feldolgozás | Egység / korlát |
|---|---|---|---|
| `SET_LEVEL` | Decimális szám | Decimális érték | Szintkód; a forrás 0–15-öt ír, a nyers byte értéket megőrizzük |
| `TEMP` | Hex szöveg | 16 bites előjel–abszolútérték; 0x8000 az előjel; osztás 10-zel | °C |
| `HUM` | Decimális szám | Decimális érték | % |
| `BARO` | Hex szöveg | Hex → egész | A fizikai egység/skála nincs hozzárendelve |
| `HSTATUS` | Hex szöveg | Hex → egész | 0 normál, 1 komfortos, 2 száraz, 3 nedves; az eredeti kód is megmarad |
| `BFORECAST` | Hex szöveg | Hex → egész | 0 nincs információ, 1 napos, 2 részben felhős, 3 felhős, 4 eső |
| `UV` | Hex szöveg | Hex → egész | A fizikai egység/skála nincs hozzárendelve |
| `LUX` | Hex szöveg | Hex → egész | A fizikai egység/skála nincs hozzárendelve |
| `RAIN` | Hex szöveg | Hex → egész; osztás 10-zel | mm |
| `RAINRATE` | Hex szöveg | Hex → egész; osztás 10-zel | mm a forrás szerint; az időalap hiányzik, ezért nem mm/h-ként jelenik meg |
| `WINSP` | Hex szöveg | Hex → egész; osztás 10-zel | km/h |
| `AWINSP` | Hex szöveg | Hex → egész; osztás 10-zel | km/h |
| `WINGS` | Hex szöveg | Hex → egész | A komment km/h-t említ, de a WINSP-vel ellentétben nem ír ÷10-et. Az egységes fizikai skála igazolásáig nyers szám. |
| `WINDIR` | Decimális szám | 0–15 kód × 22,5 | ° |
| `WINCHL` | Hex szöveg | 16 bites előjel–abszolútérték; 0x8000 az előjel; osztás 10-zel | °C |
| `WINTMP` | Hex szöveg | 16 bites előjel–abszolútérték; 0x8000 az előjel; osztás 10-zel | °C |
| `CHIME` | Decimális szám | Decimális érték | Dallamsorszám |
| `CO2` | Decimális szám | Decimális érték | A fizikai egység/skála nincs hozzárendelve |
| `SOUND` | Decimális szám | Decimális érték | A fizikai egység/skála nincs hozzárendelve |
| `KWATT` | Hex szöveg | Hex → egész | Nincs kitalált kWh/Energy Dashboard besorolás; nyers érték |
| `WATT` | Hex szöveg | Hex → egész | W |
| `CURRENT` | Decimális szám | Decimális érték | A fizikai egység/skála nincs hozzárendelve |
| `DIST` | Decimális szám | Decimális érték | A fizikai egység/skála nincs hozzárendelve |
| `METER` | Decimális szám | Decimális érték | A fizikai egység/skála nincs hozzárendelve |
| `VOLT` | Decimális szám | Decimális érték | A fizikai egység/skála nincs hozzárendelve |
| `BAT` | `OK` / `LOW` szöveg | Szöveges állapot + alacsony-elem bináris szenzor | Nem százalék; hiány/hibás érték ismeretlen |
| `PIR` | `ON` / `OFF` szöveg | Mozgás bináris szenzor | Nincs mesterséges OFF; hiány/hibás érték ismeretlen |
| `SMOKEALERT` | `ON` / `OFF` szöveg | Füstjelzés bináris szenzor | Nem tanúsított biztonsági rendszer |
| `RGBW` | Hex szöveg | Eredeti kód megőrzése | A szín/fényerő bájtsorrendje itt nincs feltételezve |
| `PARAM` | Szöveg | Eredeti csomagsorszám | Nem globális eszközazonosító |
| `NAME` | Szöveg | Eredeti protokollnév | Rögzített eszköz szűrésének része |
| `ID` | Szöveg | Vezető nullák, eredeti írásmód megőrzése | A NAME-mel együtt azonosítjuk a forrást |
| `SWITCH` | Szöveg | Gomb/ház-egység kód megőrzése | EV1527 ismert eszköznél eseményt is indít |
| `CMD` | Szöveg | Parancs megőrzése | Nem kapcsolóállapot-visszajelzés és nem parancsküldés |

## Nem tettünk hozzá hiányzó adatot

A `BAT` kódból nem számítható valódi töltöttségi százalék vagy elemfeszültség. A `KWATT` név nem elég kWh-s számláló feltételezéséhez. A BARO/UV/LUX/CO2/SOUND/CURRENT/DIST/METER/VOLT mezők adott fizikai egységét és szorzóját a tényleges dekóder/eszköz alapján kell ellenőrizni; jelenleg nyers számként olvashatók. A hőmérsékleti előjelbit és a kifejezetten leírt ÷10 átszámítás viszont megvalósult.

A soros splash `RFLink_ESP` nem vett rádiós adat. A firmware információja az ESPHome eszközadatokban marad. Adás/TX továbbra sincs.

## Szintetikus ellenőrző példák

`TEMP="00ea"` → 23,4 °C; `WINCHL="8037"` → −5,5 °C; `RAIN="008d"` → 14,1 mm; `WINDIR=4` → 90°. Ezek tesztértékek, nem a felhasználó mért adatai.

A teljes formázótesztből származó 34 kulcsos példa: `examples/synthetic-all-fields.json`. A parser ennek minden definiált mezőjét kezeli; a példafájl a készüléken nem fut le és nem hoz létre mesterséges mérést.

## v0.1.7: két külön új mező

| Mező | JSON-típus | Értelmezés | Korlát |
|---|---|---|---|
| `CHAN` | Decimális egész | A dekóder eredeti csatornakódja; nem feltételezzük az 1-től számozást | 0–255 |
| `WINDIR_DEG` | Decimális egész | Már fokban közölt szélirány, nem szorozzuk 22,5-tel | 0–359° |

A LaCrosse-széladat `AWINSP` marad tized km/h formában; `WINDIR_DEG=315` pontosan 315°-ot jelent, míg a régi `WINDIR=4` változatlanul 90°-ot. A közös diagnosztikához opcionálisan töltsd be a `packages/rflink-ha-extended-fields.yaml` fájlt az eddigi `rflink-ha-data-only.yaml` mellé. Ezek az utolsó csomaghoz tartozó diagnosztikai mezők, nem összekevert állandó mérési sorozatok. A generátor `--channel` opciója pontos `NAME + ID + CHAN` illesztést készít; az alapból választható mérési mezőkben `CHAN,WINDIR_DEG` is használható. A csatornakód eszközfüggő (például LaCrosse 0..3; Oregonban eredeti kód), mindig a vett JSON alapján add meg.
