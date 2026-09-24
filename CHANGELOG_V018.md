# v0.1.8

- Added per-plugin runtime enable/disable gates without changing original RFLink plugin files.
- `rx_plugins: all` + `plugin_profile: extended` still compiles 55 plugin IDs.
- Added optional `plugin_switches:` configuration; only listed plugins get HA switches.
- Added `plugin_switches.restore`, default/use case `true`: last individual plugin states restore after reboot; default first state ON.
- Added `RFLink aktív pluginok` text sensor containing all currently enabled compiled plugin IDs.
- Plugin 001 is deliberately not switchable.
- Runtime gate covers both legacy RFLink plugin table and v0.1.7 extension decoder table.
- No new `RF utolsó ...` diagnostic entities added; existing RF data diagnostics stay unchanged.
