# A mellékelt tesztek futtatása

Ez a ZIP a meglévő 0.1.6-os projekt frissítése, nem teljes, önálló repó.
A saját repóval összemásolva az eredeti `RFLink/` headerek és `Plugins`, a változatlan
`components/rflink/rflink.h`, `rflink_gestures.h`, `remote_receiver` és `rflink_remote`,
valamint a `packages/rflink-ha-data-only.yaml` is rendelkezésre kell álljon.
A tesztstubbok csak a hostfordítás segédei; az ESPHome nem tölti be őket.

Linux, GNU g++, Python 3.10+ és PyYAML szükséges. A repó gyökeréből:

```bash
python tests/extensions/run_tests.py --out /tmp/rflink-v017-tests
python tests/extensions/run_remote_integration.py --out /tmp/rflink-v017-remotes
python tests/extensions/test_configuration.py --out /tmp/rflink-v017-config
python tests/extensions/run_capture.py --out /tmp/rflink-v017-capture
```

A korábbi 29/34 állapotgép-teszt forrása is mellékelve van; futtatható például:

```bash
g++ -std=gnu++20 -fsanitize=address,undefined -Icomponents/rflink tests/holdfix/legacy_gesture_test.cpp -o /tmp/rf-gestures
/tmp/rf-gestures
g++ -std=gnu++20 -fsanitize=address,undefined -Icomponents/rflink tests/holdfix/holdfix_test.cpp -o /tmp/rf-holdfix
/tmp/rf-holdfix
```

A régi, teljes `tests/remote_config/run_tests.py` korábbi kiadási fájlokat is ellenőriz: azt nem szükséges újra telepíteni ehhez a csomaghoz. A v0.1.7-es wrapper a tényleges változatlan távirányító C++ tesztet futtatja mindhárom extended kiválasztással.

A `test_configuration.py` kizárólag szerkezeti YAML-ellenőrzést végez. A valódi ESPHome fordítást a mellékelt GitHub Actions definiálja; a csomag elkészítésekor ez **nem futott**.
