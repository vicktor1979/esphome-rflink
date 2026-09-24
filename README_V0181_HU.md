# RFLink v0.1.8.1 – javított pluginonkénti runtime kapcsolás

Ez a v0.1.8 javítókiadása. Az `extended + all` profil továbbra is **55 RX pluginazonosítót fordít a firmware-be**, de ha van `plugin_switches:` blokk, akkor a futásidejű engedélyezés már valóban a HA-kapcsolók által kezelt készletre szűkül.

Az eredeti `RFLink/Plugins` könyvtár, az `RFLink/Extensions`, a vevő (`rxgate2`), `high_frequency: false`, a holdfix1 gesztuskezelés és a tanuló mód nem változik.

## Új működés

```yaml
rflink:
  id: rf_bridge
  receiver_id: rf_receiver
  rx_plugins: all
  plugin_profile: extended

  plugin_switches:
    restore: true
    plugins:
      - 30
      - 61
      - 83
      - 254
    active_plugins:
      name: "RFLink aktív pluginok"
```

A fenti konfigurációban mind az 55 plugin benne van a firmware-ben, de induláskor:

- **001 mindig ON** – ez a kötelező RFLink csomag-előfeldolgozó;
- a `plugin_switches.plugins` alatt felsorolt pluginok a saját HA-kapcsolójuk állapota szerint működnek;
- a listában nem szereplő összes többi lefordított plugin **runtime OFF**;
- a 254-es debug plugin külön kapcsolható.

Ha a 30, 61 és 83 kapcsoló ON, a 254 OFF, akkor az aktív lista:

```text
001,030,061,083
```

Ha a 83-at kikapcsolod:

```text
001,030,061
```

Ez javítja a v0.1.8 hibáját, ahol a kapcsoló nélkül maradó, de lefordított pluginok továbbra is aktívak voltak.

## `restore: true`

```yaml
plugin_switches:
  restore: true
```

A kapcsolók utolsó ON/OFF állapota reboot után visszaáll.

- A normál, még sosem mentett plugin-kapcsolók első állapota **ON**.
- A **254-es debug plugin első állapota OFF**, hogy egy friss telepítés ne kezdjen azonnal nagy mennyiségű ismeretlen-csomag debug kimenetet írni.
- Ha egyszer kézzel bekapcsolod a 254-et, `restore: true` mellett az az állapot is megmarad reboot után, amíg újra ki nem kapcsolod.

`restore: false` esetén a listázott plugin-kapcsolók minden bootkor ON állapotból indulnak; ezért 254 esetén a `restore: true` ajánlott.

## Plugin 254 – unsupported packet debug

A régi RFLinkben ez volt:

```cpp
#define PLUGIN_254 // Debug to show unsupported packets
```

A v0.1.8.1-ben a 254 ugyanúgy felvehető a kapcsolható pluginok közé:

```yaml
plugin_switches:
  restore: true
  plugins:
    - 30
    - 61
    - 83
    - 254
```

A HA-ban megjelenik:

```text
RFLink 254 · Unsupported packet debug
```

Bekapcsoláskor a runtime maszk mellett az eredeti RFLink `RFUDebug` kapuja is bekapcsol, ezért a 254-es plugin ténylegesen feldolgozza azokat a megfelelő hosszúságú csomagokat, amelyeket az előtte futó aktív dekóderek nem ismertek fel. Kikapcsoláskor az `RFUDebug` is leáll.

A 254 továbbra is a normál legacy pluginlista végén fut. Ez fontos: fallback/debug dekóder, nem normál protokollfelismerő.

**Figyelem:** a 254 sok zajos vagy ismeretlen RF-forgalomnál jelentős soros debug kimenetet készíthet. A korábbi ESP8266 Wi-Fi érzékenység miatt célszerű csak hibakereséskor bekapcsolni, majd kikapcsolni.

## Aktív pluginok text sensor

A meglévő:

```yaml
active_plugins:
  name: "RFLink aktív pluginok"
```

most ténylegesen csak a futásidőben engedélyezett pluginokat mutatja. Nem a lefordított listát mutatja.

Példa:

```text
001,030,061,254
```

A `001` mindig szerepel. A 254 csak akkor, ha valóban ON.

## Plugin 001

A 001 nem tehető HA-kapcsolóvá. A YAML validátor már korábban is elutasította, most pedig a C++ runtime API sem engedi kikapcsolni. Így egy másik kódrészlet sem tudja véletlenül letiltani.

## Ha nincs `plugin_switches:` blokk

A régi használat kompatibilitása megmarad: a normál lefordított dekóderek bekapcsolva indulnak. A 254-es debug kivétel: az továbbra is opt-in, alapból OFF, mert a régi plugin saját `RFUDebug` kapuja is alapból kikapcsolt volt.

## Feltöltendő fájlok

A v0.1.8 fölé ezeket kell felülírni a GitHub repóban:

```text
components/rflink/__init__.py
components/rflink/rflink.h
components/rflink/rflink.cpp
components/rflink/rflink_engine.h
components/rflink/rflink_engine.cpp
```

A `stage_sources.py` ehhez a javításhoz nem változott. Az example fájl csak minta.

Ezután: GitHub commit → ESPHome forrás/cache frissítés → Clean Build Files → Validate → Compile → Upload.

## Várt indulási napló

```text
RFLink RX compatibility bridge v0.1.8.1 (managed runtime plugin gates; receiver/gestures unchanged):
  Plugin profile: extended
  RX plugins compiled: 55
```

A `RX plugins enabled` értéket a komponens `dump_config()` időzítése miatt még befolyásolhatja, hogy a plugin-switch komponensek restore setupja pontosan mikor fut le. A mérvadó futásidejű állapot a **RFLink aktív pluginok** text sensor és a `Plugin NNN runtime ON/OFF` naplósor.

## Host ellenőrzés

A javításhoz célzott host teszteket futtattam:

- extended/all: 55 plugin továbbra is lefordítható;
- managed indulás: csak `001` aktív;
- `001` C++ API-n keresztül sem tiltható le;
- 30/61/83 runtime engedélyezés után a lista pontosan `001,030,061,083`;
- 254 ON bekapcsolja az eredeti `RFUDebug` kaput, OFF kikapcsolja;
- 254-es izolált buildben egy támogatás nélküli tesztkeretet OFF állapotban elutasít, ON állapotban `NAME=DEBUG` üzenettel elfogad, majd OFF után újra elutasít;
- EV1527 061 és CAME 076 korábbi ON/OFF/ON runtime tesztje továbbra is sikeres;
- az eredeti RFLink plugin/config/archive készlet **54/54 fájlja változatlan**.

Nem történt itt valódi ESPHome 2026.9.0 schema/codegen futtatás, Xtensa firmware link, fizikai ESP8266 flash vagy HA hardverteszt. Az első saját Validate/Compile és az indulási log lesz a következő céloldali ellenőrzés.
