# 0.1.7 – változtatási jegyzék

## Futási kód

- Új, kifejezett `plugin_profile: extended` választás; alapból backward-compatible `legacy`.
- Összesen 55 logikai RX-ID. Újak: 016,018,048,049,050,076,077. 048 az eredeti `.old` egy részének aktiválása.
- 001,037,072,083 külön javított példány; eredeti pluginmappa változatlan.
- Új bounded mikroszekundumos beolvasás az eredeti byte-os ABI átírása nélkül.
- Rövid CAME-keretek és hosszú ismétléssorozatok eljutnak az új dekóderhez.
- Független, fix nyolc bejegyzéses ismétlésszűrő; nincs EV1527-gesztus kitalálva más protokollból.
- CHAN és WINDIR_DEG; opcionális két diagnosztikai entitás; generátor --channel szűrővel.

## Beállítás

- Az új teljes példában `plugin_profile: extended`, `rx_plugins: all`.
- A LaCrosse 12 csomagos, 1058 elemes teszt miatt a példa `buffer_size: 1200b` értéket használ.
- Minden más bevált vételi beállítás változatlan: GPIO5, 100us filter, 5ms idle, capture boot OFF,
  `high_frequency: false`, Wi-Fi/API állapotfeliratkozás utáni 5s indítás.
- A receiver kódja, holdfix1, v0.1.6 YAML-os távirányító/tanuló és a saját konyhai gesztuslogika nem változott.

## Tudatosan kimaradt

Somfy017, Hyundai051, NOX087; teljes Oregon V1/szél/csapadék/UV; CRC-s Avantek; kétértelmű WH5/Rosenborg47bit;
TX/adás; új protokollok automatikus gesztuskezelése; forrásként nem elérhető FA21RF#73/Byron#67 teljes PR-diff.

## Bizonyíték

`TEST_RESULTS.txt`: elvégzett host-tesztek és a még nem elvégzett célfordítás/hardverpróba különválasztva.
`RFLink/Extensions/REVIEW_CHANGES.diff`: az eredetihez képest ténylegesen átírt meglévő pluginok.
`RFLink/Extensions/SOURCES.json`: források és adapterlenyomatok, kitalált Git-commit nélkül.
