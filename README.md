# SourceMod Plugins

SourceMod plugins for L4D(2) servers, mostly focused on Versus.

## Plugins

| Plugin | Game | Description |
|---|---|---|
| [autorecorder](autorecorder) | L4D / L4D2 | Records demos automatically based on the player count, with optional upload to an external site/panel |
| [l4d2_adrenaline_reload](l4d2_adrenaline_reload) | L4D2 | Faster weapon reloads only while the survivor is under the adrenaline effect |
| [l4d2_fix_teams](l4d2_fix_teams) | L4D2 | Fixes incorrect team swapping in Versus, adapted for public servers with high player turnover |
| [l4d2_limit_items](l4d2_limit_items) | L4D2 | Limits the number of item spawns on the map |
| [l4d2_rock_bonus](l4d2_rock_bonus) | L4D2 | Versus score bonus for breaking Tank rocks, with a penalty when a rock hits a survivor |
| [l4d2_sb_ai_improver](l4d2_sb_ai_improver) | L4D2 | Improves the AI and behaviour of survivor bots |
| [l4d2_survivor_faint](l4d2_survivor_faint) | L4D2 | Turns survivors into a ragdoll to go jumping around |
| [l4d_tank_alltalk](l4d_tank_alltalk) | L4D2 | Enables `sv_alltalk` for a while after the Tank dies or the round ends |
| [l4d_tank_damage_announce](l4d_tank_damage_announce) | L4D2 | Announces the damage each survivor dealt to the Tank |
| [l4d_tank_pass](l4d_tank_pass) | L4D / L4D2 | Allows the Tank to pass control to another player |

## Installation

Each plugin folder follows the server layout (`addons/sourcemod/...`):

1. Compile the `.sp` from `addons/sourcemod/scripting` (with any bundled `include` files) and put the `.smx` in `addons/sourcemod/plugins`.
2. Copy the `translations` folder, when the plugin has one.
3. Check the plugin's README for its requirements (such as [left4dhooks](https://forums.alliedmods.net/showthread.php?t=321696)) and ConVars.
