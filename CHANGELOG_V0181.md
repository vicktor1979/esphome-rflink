# RFLink v0.1.8.1 – változások

- Javítva: `plugin_switches` használatakor a listán kívüli lefordított pluginok már nem maradnak véletlenül aktívak.
- Managed runtime induláskor csak a kötelező Plugin 001 aktív; a kapcsolható pluginok restore setupja állítja be a további ON állapotokat.
- Plugin 001 runtime API-ból sem kapcsolható OFF-ra.
- Plugin 254 felvehető a `plugin_switches.plugins` listába.
- Plugin 254 ON/OFF a legacy `RFUDebug` kaput is vezérli, ezért a debug plugin ténylegesen működik.
- Plugin 254 `restore: true` esetén első bootkor OFF, utána a felhasználói állapotot állítja vissza.
- Plugin 254 alapból OFF marad akkor is, ha nincs `plugin_switches`, megfelelve a régi RFLink alapértelmezett debug állapotának.
- `RFLink aktív pluginok` most a tényleges futásidejű listát mutatja.
- Eredeti `RFLink/Plugins` és extension dekóderek nem módosultak.
