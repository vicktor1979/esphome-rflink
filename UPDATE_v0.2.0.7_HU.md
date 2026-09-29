# RFLink ESPHome v0.2.0.7 – Alecto-javítás és ESPHome-figyelmeztetések

A csomag a csatolt v0.2.0.5 forráshoz képest módosult vagy új fájlokat tartalmazza, a v0.2.0.6 Alecto-javításával együtt. Az előző javítócsomagot nem kell előbb feltölteni. Ha már feltöltötted, ez rá is alkalmazható.

## GitHub-feltöltés

1. Csomagold ki a ZIP-et, majd a tartalmát töltsd az `esphome-rflink` repó gyökerébe, az azonos útvonalú fájlokat felülírva. A ZIP önmagában történő feltöltése nem frissíti a komponenst.
2. A saját ESPHome YAML külső komponensének forrása az új commitot tartalmazó ágra mutasson, például `github://vicktor1979/esphome-rflink@main`. Régi verziótag használata esetén a javítás nem töltődik le. Az új csomag nem hoz létre automatikusan `v0.2.0.7` GitHub-taget; ha rögzített verziót szeretnél, létrehozott tagre vagy a feltöltés commitjára hivatkozz.
3. Fordíts friss külső forrásból. Szükség esetén az `external_components` forrásnál ideiglenesen `refresh: 0s` kényszeríti a frissítést. A telepített készülék RFLink build entitásában/indulási naplójában **v0.2.0.7** szerepeljen.

## 1. Perjeles pluginnév

A `plugin_switches: [30, 61, 254]` által generált EV1527-kapcsoló neve:

`RFLink 061 · EV1527 ⁄ Chinese sensors`

A komponens már eleve `⁄` (U+2044, FRACTION SLASH) karaktert ad át a névellenőrzésnek. Ugyanezt a cserét alkalmazza minden perjeles pluginnévre. Ez az a megjelenített név, amelyre az ESPHome eddig figyelmeztetés mellett automatikusan átírta a nevet. A plugin száma, protokollja és kapcsoló-visszaállítási beállítása nem változik.

Ehhez a generált névhez nem kell kézi `name:` sort hozzáadnod. Ha a saját YAML-edben külön, kézzel megadott entitásnév is tartalmaz ASCII perjelet, abban is végezd el a cserét.

## 2. OTA-jelszó helyett a meglévő API-titkosítás

**A repó példafájljainak feltöltése nem módosítja automatikusan az ESPHome Device Builderben tárolt saját készülék-YAML-edet.** Abban is cseréld le az ESPHome OTA-platform `password: !secret ota_password` sorát egy ugyanúgy behúzott, üres `encryption:` sorra. A meglévő `api.encryption.key` értékét tartsd meg. Az OTA alá nem kell külön kulcs.

A repó `rf_bridge` komponensazonosítójával a teljes OTA-blokk:

```yaml
ota:
  - platform: esphome
    encryption:
    on_begin:
      then:
        - lambda: id(rf_bridge).set_ota_active(true);
    on_error:
      then:
        - lambda: id(rf_bridge).set_ota_active(false);
```

Az `on_begin` és `on_error` részek az RF-vevő OTA alatti leállításához és hiba utáni visszaindításához kellenek. Ha nálad más komponensazonosító vagy további OTA-eseménykezelő van, azokat őrizd meg. Ne hozz létre második `ota:` blokkot, a meglévőt módosítsd. A Wi-Fi/MQTT `password:` beállításai nem OTA-jelszavak, azokat ne töröld. A már nem hivatkozott `ota_password` titok a secrets fájlban maradhat; a figyelmeztetést az OTA-beállítás okozza.

### Már telepített készülék átállítása

Az új beállításhoz ESPHome **2026.9.0 vagy újabb** kell. OTA-feltöltés előtt a készüléken futó firmware-nek is támogatnia kell a titkosított OTA-t:

- Ha a készülék OTA-indulási naplójában `Encryption: offered, plaintext accepted` látszik, alkalmazhatod a fenti cserét és feltöltheted a firmware-t. Ha már `Encryption: required` szerepel, a titkosított OTA aktív.
- Régebbi firmware esetén előbb a meglévő API-kulccsal és **még a régi OTA-jelszóval** telepíts ESPHome 2026.9.0 vagy újabb firmware-t. Ezután cseréld a jelszó sort `encryption:` sorra és telepíts újra. USB-s telepítésnél rögtön az új konfiguráció használható.

Sikeres átállás után az OTA naplójában `Encryption: required` várható. A kapott fordítási figyelmeztetés önmagában nem igazolja, melyik firmware fut jelenleg a készüléken.

Hivatalos forrás: [ESPHome OTA – Encryption és Enabling Encryption on an Existing Device](https://esphome.io/components/ota/esphome/#encryption).

## Ellenőrzés és Alecto-próba

A Python-fájl szintaxisa, a generált pluginkapcsolók nevei, valamint a két példa YAML szerkezete és az OTA-eseménykezelők megőrzése ellenőrizve. Teljes ESPHome-firmware-fordítás és valódi OTA-feltöltés itt nem történt.

Az Alecto/EV1527 dekódolási kód ebben a kiegészítésben nem változott. A korábban lefutott host regressziókat és a rádiós próba lépéseit az `UPDATE_v0.2.0.6_HU.md` tartalmazza. Annak telepítési verziószáma és OTA-megjegyzése helyett ezt a v0.2.0.7 útmutatót kövesd. A csatolt régi napló sérült mintáiból továbbra sem igazolt a helyszíni Alecto-vétel helyreállása; ezt a készüléken kell kipróbálni.
