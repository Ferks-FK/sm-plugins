Speeds up weapon reloads only while the survivor is under the adrenaline effect. Applied per player, on that player's own weapon. Weapons not listed keep their vanilla reload.

New Commands
------
- `l4d2_adrenaline_reload <gun> <time>` - Reload duration in seconds for that weapon while under adrenaline. `<gun>` is the weapon name without the `weapon_` prefix. Absolute value, not a multiplier. Shotguns are not supported.

Usage example
------
```
l4d2_adrenaline_reload smg          1.75
l4d2_adrenaline_reload smg_silenced 1.90
```

Requires [left4dhooks](https://forums.alliedmods.net/showthread.php?t=321696).

Reload timing based on `l4d2_smg_reload_tweak` by Visor: https://github.com/SirPlease/L4D2-Competitive-Rework/blob/master/addons/sourcemod/scripting/archive/l4d2_smg_reload_tweak.sp