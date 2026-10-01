# [L4D2] Pause

Pause package for Versus servers:

| Plugin | Description |
|---|---|
| `pause` | Adds `!pause` / `!unpause` without breaking the game, and prevents SI from spawning because of the pause |
| `autopause` | Pauses the game automatically when a player crashes. Fork adapted for public servers (see below) |
| `l4d_pause_message` | Blocks pause/unpause console spam when the server doesn't support pausing |

`pause` and `l4d_pause_message` are unchanged from the originals. Requires [left4dhooks](https://forums.alliedmods.net/showthread.php?t=321696) and `colors`. `autopause` also requires this repo's [readyup](../readyup).

Original: https://github.com/SirPlease/L4D2-Competitive-Rework

Autopause rules
------
- When a survivor or infected player crashes (*"timed out"* or *"No Steam logon"*), the game is paused and the crash is announced in chat. This only happens in matches started by a mix (`IsMixMatch()` from [readyup](../readyup)). In public games, a crash doesn't pause anything.
- No auto-pause during ready-up or after the round has ended.
- Manual pause (`!pause` / `!unpause`) is not affected.
- When an infected player rejoins after a crash, they get their remaining spawn timer back. This doesn't pause anything, so it also works in public games.
- If `readyup` isn't loaded, or is an older version without `IsMixMatch()`, auto-pause never triggers.

Autopause convars
------
| ConVar | Default | Description |
|---|---|---|
| `autopause_enable` | 1 | Pause automatically when a player crashes (only in mix matches) |
| `autopause_force` | 0 | Use a force pause (`sm_forcepause`) instead of a regular pause |
| `autopause_forceunpause` | 0 | Force unpause once every crashed player has rejoined |
| `autopause_apdebug` | 0 | 0 = off, 1 = SourceMod logs, 2 = chat, 3 = both |

### autopause v2.4.2

------
- Auto-pause now only happens in matches started by a mix (requires this repo's `readyup` with `IsMixMatch()`).
