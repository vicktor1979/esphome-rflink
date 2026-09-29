# RFLink ESPHome v0.2.0.6 – Alecto vételi javítás

Alap: a csatolt `esphome-rflink-main(1).zip`, benne a v0.2.0.5 komponenssel.
Környezet: NodeMCU / ESP8266, SRX882 vevő, működő EV1527 vétel.

## Feltöltés és telepítés

1. A ZIP-et csomagold ki a számítógépen.
2. A benne lévő fájlokat és mappákat az `esphome-rflink` GitHub-repó **gyökerébe** töltsd fel, az azonos útvonalú fájlokat felülírva. A ZIP-et önmagában feltölteni nem elegendő. Nincs külön felső szintű csomagmappa.
3. Az ESPHome-konfiguráció továbbra is ezt a repót/ágat használja. A meglévő `@main` hivatkozás megtartható. Ha kiadási tagre rögzítetted, az új commitból hozz létre `v0.2.0.6` taget, és frissítsd a megfelelő hivatkozást. A ZIP feltöltése önmagában nem hoz létre GitHub-tag-et.
4. Friss forrásból végezz tiszta fordítást és telepítést. Ha a külső komponens régi példánya marad a gyorsítótárban, a GitHub `external_components` forrásnál ideiglenesen `refresh: 0s` használható. A tiszta build önmagában nem igazolja, hogy a külső forrás is frissült.
5. A telepített eszköz indulási naplójában vagy az RFLink build entitásban ellenőrizd: **v0.2.0.6**.

A GPIO, a vevő `filter`, `idle`, `buffer_size` beállításai és az EV1527 konfigurációja ehhez a javításhoz nem igényelnek módosítást. A működő API-/Wi-Fi-/OTA-beállításaid maradnak.

## Mit javít?

A régi helyreállító a 120 impulzusnál hosszabb blokkot még az Alecto zajszűrése előtt eldobta. Az új változat előbb a teljes blokkban összevonja a 220 µs-nál rövidebb belső zajimpulzusokat. Ez időtartamot és polaritást megőrző művelet.

Ha az összevonás során egy legalább 6500 µs-os **szünet** áll helyre, ott külön Alecto-ismétléseket dolgoz fel. Az egyes sorok továbbra is legfeljebb 120 impulzusosak lehetnek; a teljes blokk a meglévő legacy bemeneti korlát miatt legfeljebb 291 impulzus. Bizonytalan sorhatárnál nem osztja fel a blokkot pusztán impulzusszám szerint.

A teljesen helyreállt 36 bitet ellenőrzőösszeg- és tartományvizsgálat után az eredeti Plugin 030 ismét ellenőrzi. A többi sérült sor a korábbi bitenkénti többségi ellenőrzésen megy át. A hőmérséklet betanulásához továbbra is legalább három, legalább öt másodpercre elkülönülő, egymáshoz közeli mérés kell. Ugyanazon rádiós adás ismétlései nem számítanak három külön tanulási mintának.

Az EV1527 normál dekódere továbbra is megelőzi ezt a helyreállítást. A többletmunkát legfeljebb négy sor és sorok között vizsgált 8 ms-os keret korlátozza. Egy megkezdett illesztés tovább tarthat 8 ms-nál; ez nem szigorú teljes futásidő-garancia. A munkapuffer körülbelül 1,2 kB további statikus memóriát használ.

## Tesztelés a készüléken

Az Alecto maradjon az új helyén. A 030-as plugin legyen bekapcsolva. Első körben 10–15 perc normál napló elég a számlálók megfigyelésére; közben próbáld ki az EV távirányító rövid és hosszú gombnyomását is.

| Naplóadat | Jelentés |
|---|---|
| `rflink.rfdiag` / `rebuilt` | Az eredeti dekódernek átadott helyreállított sorok száma; önmagában még nem HA-publikálás. |
| `rflink.alecto.rx` / `long` | A hosszú, Alecto-jelöltként kezelt blokkok száma. |
| `split` | A feldolgozás során megtalált hosszú szünetek száma. |
| `clean` | Előszűrés után teljesen helyreállított, checksum-helyes sorok száma. |
| `raw`, `normalized` | Az utolsó jelölt impulzusszáma előtte/utána. A `normalized` nem tartalmazza a végső idle lezárást, ezért egy tiszta sor itt 73. |
| `reason=unsplit_long` | A blokk hosszú maradt, és nem volt biztonságos bontási határ. |
| `reason=consensus` | Vannak használható bitek, de még nincs elfogadott helyreállított sor. |
| `reason=budget` | A további sorok feldolgozását az idő-/darabkorlát leállította. |
| `rflink.alecto.gate` / `received` | A Plugin 030 által dekódolt, az értékellenőrzési kapuig eljutott üzenetek. |
| `Pending ID=... temp=... C samples=...` | Valóban dekódolt, de még megerősítésre váró mérés. **Nem elfogadott szenzorérték.** |
| `reason=learning` / `pending_samples` | Még nincs három egyező minta. |
| `reason=spacing` | Az előző mintához képest öt másodpercen belüli ismétlés. |
| `reason=publish_interval` | Az ötperces közzétételi idő még nem telt le. |
| `reason=large_jump` | 10 °C-nál nagyobb ugrás; öt egyező mintát vár. |
| `learned`, `published` | Betanult adók, illetve elfogadott/közzétett Alecto-üzenetek száma. |

Ha nincs javulás, 2–3 percre kapcsold be a Plugin 254-et, és mentsd el a naplót. Az `rflink.alecto.raw` sorok teljes nyers blokkot írnak ki öt másodpercenként legfeljebb egyszer. Egy blokk részei a közös `capture=...` azonosító és `part=1/N ... N/N` alapján összerakhatók. Minden részt őrizz meg. A próba után kapcsold ki a 254-et.

## Elvégzett ellenőrzések és korlátok

- Host C++ regressziók legacy és 55 plugint fordító extended profillal: tiszta és 146 impulzusra szétesett Alecto-sor; zajjal megszakított szünetű összefolyó ismétlések; puha bitkonszenzus; hibás checksum; 1000 determinisztikus zajminta; plugin-kikapcsolás; időkeret; EV1527.
- A komponens teljes feldolgozási útján három külön rádiós adás után egy elfogadott hőmérséklet; nagy ugrásnál az ötmintás védelem; gyors ismétlések és lejárt jelöltek kezelése.
- Meglévő motor- és távirányító-regressziók; single/multi/hold gesztusok. Az eredeti 54 plugin/config/old fájl ellenőrzőösszege változatlan.
- AddressSanitizer és UndefinedBehaviorSanitizer ellenőrzés. A LeakSanitizer vizsgálata a futtatókörnyezet `/proc` korlátozása miatt ki volt kapcsolva.
- A csatolt napló 87 teljesen kiírt, sérült mintájának visszajátszása továbbra is **0 dekódolt Alecto-üzenet**. A hosszú blokkok a régi naplóban csonkoltak; azok valós ismétléseit nem lehetett teljesen visszajátszani. A javítás a kimutatott feldolgozási korlátot javítja, de a konkrét helyszín sikeres vételét még nem igazolja.
- Valódi ESPHome/Xtensa firmware-fordítás, rádiós hardverteszt, Wi-Fi/API/OTA terhelési teszt nem történt. Az ESP8266 auto-start/diagnosztikai C++ ág host helyettesítőkkel fordíthatósági ellenőrzést kapott.

Új célzott teszt futtatása a repó gyökeréből:

```bash
python3 tests/test_alecto_burst.py
```

Olyan Linux-környezetben, ahol a LeakSanitizer nem éri el a szükséges `/proc` adatokat:

```bash
ASAN_OPTIONS=detect_leaks=0 python3 tests/test_alecto_burst.py
```

Visszaállítás: a négy `components/rflink/rflink*.cpp/.h` fájlt együtt állítsd vissza a feltöltés előtti GitHub-commitból, majd fordíts és telepíts újra.
