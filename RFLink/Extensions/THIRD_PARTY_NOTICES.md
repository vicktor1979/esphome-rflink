# Eredet és licencek – RFLink Extensions 0.1.7

Az eredeti `RFLink/Plugins` fájlok a felhasználó feltöltéséből származnak, változatlanul. A módosított példányok az `overrides/` alatt, az új illesztések a `decoders/` alatt találhatók. Minden módosítás helye az adott fájl elején jelölve van. A `SOURCES.json` tartalmazza a forráscímeket és a kiadott adapterek SHA-256 lenyomatait.

## RFLink-vonal

Az eredeti forrás RFLink Gateway License v1.0 szövege változatlanul mellékelve: `licenses/RFLink-Gateway-original.txt` (eredeti kódolás). Az RFLink32 projektben megnyitott licenc azonos című, kereskedelmi felhasználást korlátozó feltételeket tartalmaz: https://github.com/cpainchaud/RFLink32/blob/master/License.txt . A projekt egészét nem nevezzük MIT/GPL egységes licencűnek. A 016/018/077 forrásfájlban nem volt önálló licencfejléc; a projektszintű eredetre hivatkozunk, nem találunk ki új engedélyt.

A CAME 076 szerzője a donor fejlécében **2021 Christophe Painchaud**. A megnyitott fejléc szerint a kód a fejléc megtartásával használható nyílt forrású projektben; kereskedelmi alkalmazást tilt. A 048-as Oregon forrás eredeti RFLink-fejléce, szerzői, illetve a beágyazott OOK-dekóder engedélyszövege megmaradt az adapterben. Az ottani teljes régi modellfelsorolás nem támogatási ígéret: ebben a portban csak az útmutatóban felsorolt V2/V3 hőmérséklet/páratartalom ágak vannak engedélyezve.

## rtl_433-alapú protokollok

A 049-es LaCrosse protokoll forrása: `merbanan/rtl_433/src/devices/lacrosse_tx141x.c`. Copyright (C) 2017 Robert Fraczkiewicz; változtatások Andrew Rivettől. **GNU General Public License, version 2 vagy újabb**. A fejléc szerepel az adapterben, a GPL-2 teljes szövege a `licenses/GPL-2.0.txt` fájlban van.

Az 050-es Fine Offset WH2 ág forrása: `merbanan/rtl_433/src/devices/fineoffset.c`. Copyright (C) 2017 Tommy Vestermark; Enhanced (C) 2019 Christian W. Zuckschwerdt. Az eredeti fájl WH51-részének szerzője Marco Di Leo, de WH51-rész nem került ebbe az adapterbe. GPL-2.0-or-later; ugyanaz a mellékelt licencszöveg. A teljes rtl_433 futtatókörnyezet nincs beépítve.

## Közzétételi korlát

Ezek forrásszintű eredetjelzések, nem jogi megfelelőségi tanúsítás. A különböző feltételek összeegyeztethetőségét, különösen kereskedelmi használat vagy teljes összelinkelt firmware terjesztése előtt, külön ellenőrizni kell; a csomag nem ad új kereskedelmi jogosultságot. A jelen kiadás forráskód-kiegészítés, nem terjesztett kész firmware-bináris. Az eredeti szerzői megjegyzéseket és licenceket ne távolítsd el.

A webes forrásokat 2026-09-23-án vizsgáltuk. Donor commit-SHA nem volt ellenőrizhető; ezért a jegyzékben `null`, nem kitalált commit szerepel. A kiadott adaptált források tartalmát a helyi SHA-256 jegyzék rögzíti. A program nem tölt le tetszőlegesen változó donor-fájlokat fordításkor.
