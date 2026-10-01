# [L4D2] Ready-Up

Holds the start of each round until players are ready. During ready-up, survivors stay in the saferoom, the director's timers are paused, god mode and infinite ammo are on, and a panel lists every player with their ready state. When everyone is ready, a countdown runs and the round goes live.

Fork adapted for public servers: ready-up stays off in public games and only runs in campaigns started by a mix (`l4d2_mix`). Requires [left4dhooks](https://forums.alliedmods.net/showthread.php?t=321696), [builtinvotes](https://github.com/L4D-Community/builtinvotes) and `colors`.

Original: https://github.com/Target5150/MoYu_Server_Stupid_Plugins

Mix plugin used (`l4d2_mix`, from l4d2-zone-server): https://github.com/altair-sossai/l4d2-zone-server/blob/master/addons/sourcemod/scripting/l4d2_mix.sp

Rules
------
- When a map starts, the plugin does nothing: no panel, no freeze, no countdown.
- When a mix starts, ready-up kicks in immediately in the current round, as long as no survivor has left the saferoom yet. The panel stays hidden while captains pick, and `l4d2_mix` shows it again when picking ends.
- Once a mix has started, every following round and chapter of the same campaign has ready-up. This includes campaigns chained by `l4d2_map_transitions`, which carries the scores over.
- Ready-up goes back to off until a new mix when the campaign changes:
  - the map changes in any way other than a normal chapter transition (vote, admin `changelevel`, map restart);
  - the new map is the first map of a campaign and the score was reset to 0–0 (finale ended, `l4d2_early_victory`, vote on the scoreboard). This is checked 3 seconds after the new map loads, after `l4d2_map_transitions` has restored the scores.
- While off, the plugin still fires `OnRoundLiveCountdown` / `OnRoundIsLive` when a survivor leaves the saferoom (or the round is force-started). Plugins that rely on them keep working: `l4d2_mix` locks `!mix` once the round starts, `l4d2_alltalk_before_round_start` turns all-talk off, `starting_items` hands out items, etc.
- While off, footer lines that other plugins add at round start (boss percents, panel text...) are kept. They show up if a mix activates ready-up later in that round.

Convars
------
`cfg/sourcemod/readyup.cfg` is created on first load with every ConVar, and runs on every map.

| ConVar | Default | Description |
|---|---|---|
| `l4d_ready_enabled` | 1 | 0 = disabled, 1 = manual ready, 2 = auto start, 3 = team ready. Even when enabled, ready-up only runs in campaigns started by a mix |
| `l4d_ready_cfg_name` | "" | Config name shown on the panel |
| `l4d_ready_server_cvar` | sn_main_name | ConVar holding the server name shown on the panel (falls back to `hostname`) |
| `l4d_ready_max_players` | 12 | Maximum number of players listed on the panel |
| `l4d_ready_disable_spawns` | 0 | Prevent SI from spawning during ready-up |
| `l4d_ready_survivor_freeze` | 1 | Freeze survivors during ready-up (0 = they move freely but can't leave the saferoom) |
| `l4d_ready_enable_sound` | 1 | Enable sounds |
| `l4d_ready_notify_sound` | buttons/button14.wav | Sound when a player readies/unreadies |
| `l4d_ready_countdown_sound` | weapons/hegrenade/beep.wav | Sound on each countdown tick |
| `l4d_ready_live_sound` | ui/survival_medal.wav | Sound when the round goes live |
| `l4d_ready_autostart_sound` | ui/buttonrollover.wav | Sound during the auto-start countdown |
| `l4d_ready_chuckle` | 0 | Random Moustachio chuckle when the round goes live |
| `l4d_ready_secret` | 1 | Easter egg when a survivor readies up |
| `l4d_ready_delay` | 3 | Countdown seconds before going live |
| `l4d_ready_force_extra` | 2 | Extra countdown seconds on a force start |
| `l4d_ready_autostart_delay` | 5 | Auto-start countdown seconds (mode 2) |
| `l4d_ready_autostart_wait` | 20 | Seconds to wait for loading players before auto-start is forced (mode 2) |
| `l4d_ready_autostart_min` | 0.25 | Fraction of max players required for auto-start (mode 2) |
| `l4d_ready_unbalanced_start` | 0 | Allow going live with incomplete teams |
| `l4d_ready_unbalanced_min` | 2 | Minimum players per team for an unbalanced start |
| `l4d_ready_autoready` | 0 | Auto-ready active (non-AFK) players after N seconds (0 = disabled). When enabled, `!unready` is disabled |

If a future version adds ConVars, delete `readyup.cfg` so it's created again with them.

Commands
------
| Command | Access | Description |
|---|---|---|
| `sm_ready` / `sm_r` | all | Mark yourself as ready (also F1) |
| `sm_unready` / `sm_nr` | all | Mark yourself as not ready, cancelling the countdown (also F2) |
| `sm_toggleready` | all | Toggle your ready state |
| `sm_hide` / `sm_show` | all | Hide / show the ready-up panel |
| `sm_return` | all | Return to the saferoom if stuck (unfrozen ready-up) |
| `sm_forcestart` / `sm_fs` | ADMFLAG_BAN | Force the round to start. An admin `!unready` aborts it |

API
------
Include `readyup.inc` (library `readyup`). Same natives and forwards as the original (`IsInReady`, `IsReady`, `ToggleReadyPanel`, footer natives, `OnRoundIsLive`...), plus:

| API | Description |
|---|---|
| `bool IsMixMatch()` | `true` if the current campaign was started by a mix. `false` in public games and during the 3-second check after a map change |

### v10.2.9

------
- Ready-up is off by default and only runs in campaigns started by a mix (`l4d2_mix`). It stays on for the rest of the campaign and resets when the campaign changes.
- While off, the countdown/live forwards still fire when the round starts, so other plugins keep working.
- Footer lines added while off are kept when a mix activates ready-up.
- New native `IsMixMatch()`.
- Config file `cfg/sourcemod/readyup.cfg` is now created automatically.
- Fixed: panel timers could be created twice when ready-up was started again in the same round.
- Fixed: footer natives with a negative index threw an error.
