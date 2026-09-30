# [L4D2] Rock Bonus

Versus plugin that gives the survivor team a score bonus for every Tank rock they break mid-air, and takes points away when a rock hits a survivor. The bonus never goes below 0, is added to the team's score at the end of the round, and is summarized in chat. Requires [left4dhooks](https://forums.alliedmods.net/showthread.php?t=321696).

## ConVars

| ConVar | Default | Description |
|---|---|---|
| `l4d2_rock_bonus_enable` | 1 | Enable the plugin (Versus only) |
| `l4d2_rock_bonus_skeet_points` | 25 | Points given when a survivor breaks a Tank rock (0 = disabled) |
| `l4d2_rock_bonus_hit_points` | 25 | Points taken when a rock hits a survivor (0 = disabled) |
| `l4d2_rock_bonus_max` | 400 | Maximum bonus per round (0 = no limit) |
| `l4d2_rock_bonus_count_bots` | 1 | Events involving bots also count |
| `l4d2_rock_bonus_require_safe` | 0 | Only give the bonus if the survivors reach the saferoom (or escape in the finale) |
| `l4d2_rock_bonus_summary` | 1 | End of round summary: 0 = off, 1 = always, 2 = only if events happened |

## API

Include `l4d2_rock_bonus.inc` (library `l4d2_rock_bonus`).

| API | Description |
|---|---|
| `Action RockBonus_OnEvent(int client, RockBonusEvent event, int &points)` | Before an event counts (`points` is negative for hits). Change `points` and return `Plugin_Changed`, or return `Plugin_Handled` to block it |
| `void RockBonus_OnEventPost(int client, RockBonusEvent event, int applied, int roundBonus)` | After an event changed the bonus. `applied` is what was actually added or removed |
| `int RockBonus_GetRoundBonus()` | Bonus accumulated this round, always within `[0, max]` |
| `int RockBonus_GetEventCount(RockBonusEvent event)` | How many times the event happened this round |

Events: `RockBonus_Skeet`, `RockBonus_HitSurvivor`.

## Changelog

### v1.0.0

- Initial release.
