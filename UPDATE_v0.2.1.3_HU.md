# v0.2.1.3 – az rx_plugins hozza létre a kapcsolókat is

Az `rx_plugins` az egyetlen pluginlista: a felsorolt pluginokat fordítja bele a firmware-be, és automatikusan létrehozza a Home Assistant-kapcsolóikat.

## Átállás

A meglévő `rflink:` blokkban ezt:

```yaml
rx_plugins: all
plugin_switches: [30, 40, 61, 254]
```

cseréld erre:

```yaml
rx_plugins: [30, 40, 61, 254]
```

A meglévő `id`, `receiver_id`, `plugin_profile`, `auto_start` és más beállítások megmaradnak. A régi `plugin_switches` kulcsot törölni kell; a komponens átállási útmutatót tartalmazó validálási hibával jelzi, ha még szerepel.

| Plugin | Automatikus kapcsoló | Induló működés |
|---|---|---|
| 001 előfeldolgozó | nincs | Automatikusan bekerül és mindig aktív. |
| 030 Alecto V1 | igen | Utolsó kapcsolóállapot; mentett állapot nélkül ON. |
| 040 Mebus | igen | Utolsó kapcsolóállapot; mentett állapot nélkül ON. |
| 061 EV1527 | igen | Utolsó kapcsolóállapot; mentett állapot nélkül ON. |
| 254 hibakeresés | igen | Minden reboot/OTA után OFF. |

A kapcsolók neve és névnormalizálása változatlan. A kapcsoló kikapcsolása futás közben leállítja az adott plugin használatát; a lefordított kód méretét nem csökkenti. A lista módosításához újrafordítás és feltöltés kell. A listából eltávolított pluginhoz kapcsoló sem készül.

## all, configured és plugin_profile

- `rx_plugins: all`: minden, a profilban elérhető plugin lefordul. Legacy esetén 48 plugin és 47 kapcsoló, extended esetén 55 plugin és 54 kapcsoló készül. A különbség a kapcsoló nélküli 001.
- Az összes új normál kapcsoló első alkalommal ON állapotú. Az `all` tehát több aktív dekódert és több entitást jelenthet, mint a korábbi `all` + rövid `plugin_switches` lista. ESP8266-on az ismert eszközökhöz célszerű konkrét listát használni.
- `rx_plugins: configured`: az eredeti `_Plugin_Config_01.h` által kijelölt lista, nem a régi kapcsolólista. Ez maradt az alapértelmezés, ha az `rx_plugins` hiányzik. A kiválasztott pluginokhoz itt is automatikusan kapcsolók készülnek.
- `plugin_profile: legacy` / `extended`: továbbra is a rendelkezésre álló pluginforrásokat választja ki. Ez külön beállítás marad.
- A nem elérhető plugin száma validálási hibát ad; az ismételt számokból egyetlen plugin és kapcsoló készül. A `[1]` vagy `[]` csak a kötelező előfeldolgozót tartja meg, kapcsoló nélkül.

## Példák

- `examples/rflink.yaml`: `[30, 40, 61, 254]`.
- `examples/rflink-extended.yaml`: `[30, 40, 61, 83, 254]`.
- A Cresta időjárás-fragment használatakor a 34-et is add a listához. Egy szenzor `protocol:` mezője önmagában nem választ ki fordítandó plugint.

A korábbi natív hőmérséklet-/elemállapot-szenzorok, a polling vétel, az 5 perces csomagkor-diagnosztika és az EV-gesztusok megmaradnak.

## Telepítés

A ZIP a v0.2.1.2-höz képest módosított és új fájlokat tartalmazza. Töltsd fel őket a repó gyökerébe, a mappaszerkezet megtartásával. Korábbi verziónál előbb a v0.2.1.2 javításai is szükségesek.

A saját YAML pluginlistáját is módosítsd a fenti módon. A forrást `main` ágról betöltve az első fordítás előtt az `external_components` `refresh` értéke legyen `0s`; utána visszaállítható `5min`-ra. Ez a csomag önmagában nem hoz létre GitHub taget.

## Ellenőrzések

- ESPHome 2026.9.0 valódi sémaellenőrzés és C++ kódgenerálás: explicit, all (mindkét profil), configured, alapértelmezett, 001-only, üres és 254-only lista; darabszám, kapcsolónév, restore mód, debug entitások és átállási hibaüzenetek.
- Mindkét teljes YAML-példa és a Cresta-fragmentek összevont kódgenerálása; a szenzorok enumértékeinek és BAT-metaadatainak regressziója.
- A natív szenzorfeldolgozás host C++ tesztje ASan/UBSan mellett.

Teljes ESP8266 target fordítás és készülékes/OTA teszt ebben a javításban nem történt. Feltöltés után az `RFLink build` v0.2.1.3-at mutasson; ellenőrizd a kapcsolókat és az `RFLink aktív pluginok` értékét.
