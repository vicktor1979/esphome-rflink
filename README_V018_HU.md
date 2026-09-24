# RFLink v0.1.8 – pluginonkénti futásidejű kapcsolók

Ez a frissítés a v0.1.7 kiegészítése. Az `extended + all` profil továbbra is 55 RX pluginazonosítót fordít a firmware-be, de csak a YAML `plugin_switches.plugins` listájában felsorolt pluginok kapnak Home Assistant kapcsolót.

Az eredeti `RFLink/Plugins` könyvtár nem módosul. A vevő (`rxgate2`), `high_frequency: false`, a holdfix1 gesztuskezelés, a tanuló mód és a v0.1.7 új/javított dekóderei változatlanok.

## Használat

A meglévő `rflink:` blokkban:

```yaml
rflink:
  id: rf_bridge
  receiver_id: rf_receiver
  rx_plugins: all
  plugin_profile: extended

  plugin_switches:
    restore: true
    plugins: [16, 18, 30, 37, 48, 49, 50, 61, 76, 77, 83]
    active_plugins:
      name: "RFLink aktív pluginok"

  log_messages: true
  on_message:
    - then:
        - script.execute:
            id: rflink_api_process
            message: !lambda return x;
```

A fenti konfigurációban mind az 55 plugin le van fordítva, de csak a felsorolt 11 plugin kap külön HA switch entitást. A listából kihagyott, lefordított pluginok továbbra is engedélyezve maradnak és normálisan dekódolnak.

A plugin 001 nem tehető kapcsolhatóvá. Ez az RFLink csomag-előfeldolgozója, ezért a validátor hibát ad, ha bekerül a `plugin_switches.plugins` listába.

## Restore

```yaml
plugin_switches:
  restore: true
```

A kapcsolható pluginok utolsó ON/OFF állapota reboot után visszaáll. Az alapértelmezett, még sosem mentett állapot ON.

`restore: false` esetén a kapcsolható pluginok is minden bootkor ON állapotból indulnak.

## HA entitások

A pluginazonosítóból automatikusan készül a név, például:

- `RFLink 030 · Alecto V1`
- `RFLink 061 · EV1527 / Chinese sensors`
- `RFLink 076 · CAME TOP-432`
- `RFLink 083 · Brel / Dooya`

A `RFLink aktív pluginok` text sensor az összes pillanatnyilag engedélyezett, lefordított plugin ID-ját mutatja, például:

```text
001,002,003,...,061,...,083,254
```

Ha a 061 kapcsolót kikapcsolod, a `061` eltűnik a listából. A lista nem csak a HA-kapcsolóval rendelkező pluginokat tartalmazza, hanem minden aktuálisan aktív, lefordított plugint.

## Futásidejű működés

A kapcsoló nem fordítja ki a plugint és nem indítja újra az ESP-t. A plugin a firmware-ben marad, de a dekóder-dispatcher átugorja, amíg OFF állapotú.

A globális `RFLink figyelés` továbbra is master kapcsoló. Ha az OFF, egyetlen plugin sem dekódol, de az egyedi pluginállapotok megmaradnak.

Fontos RFLink sajátosság: a dekóderek sorrendben futnak. Ha egy plugint kikapcsolsz, ugyanazt a rádiókeretet egy későbbi plugin még felismerheti más protokollként. Ez nem a runtime kapcsoló hibája, hanem a több dekóder közötti protokollátfedés természetes következménye. Az EV1527 gesztus-side-channel viszont nem fut, ha a 061 OFF.

## Telepítendő fájlok

A v0.1.7 fölé ezeket a fájlokat kell azonos útvonalon felülírni:

```text
components/rflink/__init__.py
components/rflink/rflink.h
components/rflink/rflink.cpp
components/rflink/rflink_engine.h
components/rflink/rflink_engine.cpp
components/rflink/stage_sources.py
```

Az `examples/rflink-extended.yaml` csak minta; a saját fő YAML-odban elég a `plugin_switches:` blokkot átvezetni.

A `RFLink/Extensions`, `RFLink/Plugins`, `components/remote_receiver`, `components/rflink_remote` és a package fájlok ehhez a funkcióhoz nem változnak.

Frissítés után: forrásfrissítés / cache törlés, Clean build files, konfigurációellenőrzés, fordítás, feltöltés.

## Megvalósítás

A runtime állapot egy 256 bites maszkkal tárolódik a dekóder motorban (32 byte RAM a maszkhoz). Minden dekóderhívás előtt a motor ellenőrzi az adott plugin bitjét. Ez a régi pluginokra és a v0.1.7 új extension dekódereire is vonatkozik.

A HA kapcsolók száma csak a `plugin_switches.plugins` lista hosszától függ. Ezért nem szükséges mind az 55 switch entitást létrehozni ESP8266-on.

## Ellenőrzés

Host oldalon ellenőrizve:

- extended/all továbbra is 55 plugint állít össze;
- a 54 eredeti RFLink plugin/config/archive fájl SHA-256 szerint változatlan;
- runtime OFF állapotban az izolált EV1527 061 dekóder nem fut, ON után újra dekódol;
- runtime OFF állapotban az izolált CAME 076 extension dekóder nem fut, ON után újra dekódol;
- all-55 összeállításban a 061 kikapcsolásakor az EV1527 frame observer nem jelez 061-es keretet, visszakapcsolás után újra jelez;
- az aktív pluginlista követi a runtime bitmaskot.

Korlát: itt nem történt valódi ESPHome 2026.9.0 schema/codegen + Xtensa firmware fordítás, fizikai ESP8266 flash, Home Assistant entitásregisztráció vagy rádiós hardverteszt. Ezeket az első saját fordítás után kell ellenőrizni.
