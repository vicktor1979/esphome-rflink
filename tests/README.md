# Host regressziós tesztek – v0.1.9

A `tests/` alatti stubbok kizárólag Linux/host tesztsegédek; firmware-be nem kerülnek.
A tesztekhez Python 3.10+, PyYAML és GNU g++ szükséges.

A repó gyökeréből a fő ellenőrzések:

```bash
python3 tests/run_tests.py
python3 tests/remote_config/run_tests.py --repo . --out /tmp/rflink-remote
python3 tests/rx_gate/run_tests.py --repo . --out /tmp/rflink-rxgate
python3 tests/holdfix/run_tests.py --repo . --out /tmp/rflink-holdfix
python3 tests/all_plugins/test_all_plugins.py --repo . --out /tmp/rflink-all
python3 tests/test_v013.py
python3 tests/test_native.py
```

A suite ellenőrzi többek között:

- az eredeti 54 RFLink plugin/config/old fájl SHA256 azonosságát;
- legacy pluginválasztásokat és host C++ fordítást;
- EV1527 gesztusokat, holdfix működést és v0.1.9 immediate-single viselkedést;
- `rflink_remote` minták/tanulás runtime útvonalát;
- rxgate2 capture gate-et, pause/resume és overflow recovery viselkedést;
- adatmezőket, generátort és YAML szerkezeti regressziókat.

Ezek **nem** helyettesítik a valódi ESPHome/Xtensa fordítást és hardvertesztet.
A host környezet nem emulálja az ESP8266 Wi-Fi/API/OTA időzítéseit vagy a tényleges 433 MHz-es rádiót.
