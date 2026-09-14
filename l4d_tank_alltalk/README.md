Enables `sv_alltalk` for a while (survivors and infected can hear each other), reverting it back to whatever it was set to before once the timer runs out. Also announces it in chat, crediting whoever landed the killing blow when the Tank dies mid-round. All chat output is handled through translations (English, Portuguese and Spanish included).

Rules
------
- If the Tank dies, all talk is enabled once the death is confirmed (not a pass to another player/bot).
- Whenever the round ends, all talk is enabled — a Versus round only ends one of two ways (the Tank wins or the survivors win), and both should trigger it, so there's no need to work out which one happened.
- All talk is cut short the moment the next round actually starts, even if the configured duration hasn't run out yet — it's only meant for the break between rounds, not to leak into live gameplay of the next one (e.g. a same-map round swap between teams with little/no delay).

Convars
------
- `l4d_tankalltalk_enabled` - Enable the plugin. Default: `"1"`
- `l4d_tankalltalk_duration` - How long (in seconds) `sv_alltalk` stays enabled after the Tank dies. Default: `"10.0"`
- `l4d_tankalltalk_announce` - Announce in chat when all talk is enabled/disabled by this plugin. Default: `"1"`
- `l4d_tankalltalk_sound` - Play a sound when all talk gets enabled by this plugin. Default: `"0"`

Notes
------
- If an admin (or another plugin) manually changes `sv_alltalk` while this plugin is holding it on, the plugin backs off instead of overriding that change when its timer expires.
