# RFLink v0.1.8.2 – Plugin 254 ismeretlen jel a Home Assistantban

Ez a javítás a v0.1.8.1 managed runtime pluginrendszerre épül. Az eredeti `RFLink/Plugins` fájlokat, az rxgate2 vevőt, a gesztuskezelést és a v0.1.7 extension dekódereket nem módosítja.

## Mi új?

Ha a `plugin_switches.plugins` listában szerepel a **254**, két új diagnosztikai entitás automatikusan létrejön:

- **RFLink ismeretlen jel** – az utolsó olyan RF-csomag impulzuslistája, amelyet az aktív normál dekóderek nem ismertek fel, de a 254-es fallback elfogadott.
- **RFLink ismeretlen jel impulzusszám** – ugyanennek a csomagnak a pontos `RawSignal.Number` értéke.

A meglévő `RF utolsó ...` entitások változatlanok. A 254 továbbra is `NAME=DEBUG` üzenetet készít, de a nyers impulzusok most külön HA-entitásban is megjelennek.

## YAML

Ha már ez van:

```yaml
rflink:
  id: rf_bridge
  receiver_id: rf_receiver
  rx_plugins: all
  plugin_profile: extended
  plugin_switches:
    restore: true
    plugins: [30, 61, 83, 254]
    active_plugins:
      name: "RFLink aktív pluginok"
```

akkor **nem kell új sort hozzáadni**. A két diagnosztikai entitás alapértelmezett névvel automatikusan létrejön, mert a 254 szerepel a kapcsolható pluginok között.

Ha át akarod nevezni őket:

```yaml
  plugin_switches:
    restore: true
    plugins: [30, 61, 83, 254]
    active_plugins:
      name: "RFLink aktív pluginok"
    unsupported_signal:
      signal:
        name: "RFLink ismeretlen jel"
      pulse_count:
        name: "RFLink ismeretlen jel impulzusszám"
```

## Példa HA-érték

Egy ismeretlen csomagnál például:

```text
RFLink ismeretlen jel:
Pulses=40; Pulses(uSec)=672,1088,800,1216,...

RFLink ismeretlen jel impulzusszám:
40
```

A szöveges állapot legfeljebb 240 karakterre van korlátozva. Ha a teljes impulzussor hosszabb, a végén `,...` jelenik meg. Az impulzusszám ettől függetlenül pontos marad.

A szöveg az RFLink-kompatibilitási réteg 32 µs felbontású `RawSignal.Pulses[]` adataiból készül, tehát ugyanazt a kvantált időalapot használja, mint az eredeti Plugin 254 `Pulses(uSec)` soros kimenete.

## 24–35 impulzusos ismeretlen csomagok

A régi Plugin 254 saját minimuma 24 impulzus volt, miközben a normál RFLink legacy dispatcher 36 impulzus alatt nem indította a dekódereket. A v0.1.8.2 ezt a különbséget kezeli:

- 24–35 impulzus között csak a 254 futhat, és csak akkor, ha runtime ON;
- a normál legacy pluginok továbbra is a 36 impulzusos minimumot használják;
- 24 impulzus alatt a csomag továbbra is elutasításra kerül.

Így a 254 közelebb viselkedik az eredeti debug plugin szándékához, de a normál dekóderek kapuja nem lazul.

## Runtime viselkedés

A v0.1.8.1 szabályai változatlanok:

- mind az 55 plugin belefordítható az `extended/all` firmware-be;
- 001 mindig ON;
- `plugin_switches` használatakor a listában nem szereplő pluginok runtime OFF;
- `restore: true` visszaállítja a kapcsolók állapotát reboot után;
- 254 első állapota OFF, később a mentett állapot áll vissza;
- 254 továbbra is utolsó fallback/debug plugin.

A két új HA-entitás csak akkor frissül, amikor a 254 ténylegesen elfogad egy ismeretlen csomagot. Az előző értéket megtartják a következő 254-es találatig; nincs timeout.

## Napló

254-es találatkor a normál `NAME=DEBUG` RFLink üzenet mellett új sor is készül:

```text
[D][rflink]: Plugin 254 unsupported RF: Pulses=40; Pulses(uSec)=672,1088,...
```

Ha a HA-s összefoglaló csonkolt:

```text
[HA summary truncated]
```

jelölés kerül a naplóba.

Az eredeti Plugin 254 saját `Serial.print()` kimenete megmarad.

## Frissítendő fájlok

A v0.1.8.1 fölé ezeket kell felülírni a GitHub repóban:

```text
components/rflink/__init__.py
components/rflink/rflink.h
components/rflink/rflink.cpp
components/rflink/rflink_engine.h
components/rflink/rflink_engine.cpp
```

A `stage_sources.py`, `RFLink/Plugins`, `RFLink/Extensions`, `remote_receiver`, `rflink_remote`, package-ek és a saját fő YAML nem igényel kötelező módosítást.

GitHub frissítés után: **Clean Build Files → Validate → Compile → Upload**.

## Ellenőrzési határ

Host C++20/ASan/UBSan tesztekkel ellenőrizve lett a 254 OFF/ON/OFF állapot, a 40 impulzusos csomag, a 24 impulzusos alsó debug-határ, a 290 impulzusos hosszú csomag csonkolása, az 55 pluginos szintaxisfordítás, a managed runtime állapot, EV1527 061 és CAME 076 regresszió, valamint az eredeti 54 RFLink fájl integritása.

Nem történt ebben a környezetben valódi ESPHome 2026.9.0 schema/codegen, Xtensa firmware-link, fizikai ESP8266 flash vagy Home Assistant end-to-end teszt. Az első saját Validate/Compile és élő 254-es vétel a céloldali ellenőrzés.
