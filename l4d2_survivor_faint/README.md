[Turn yourself into a ragdoll and go jumping around.](https://forums.alliedmods.net/showthread.php?t=352746)

Convars
------
- `l4d2_faint_impulse_interval` - Minimum interval between movement impulses (s). Higher = slower. Default: `"0.06"`
- `l4d2_faint_admins_only` - Restrict sm_faint to admins (0/1). Default: `"0"`
- `l4d2_faint_require_grounded` - Require players to be on the ground to faint (0/1). Default: `"1"`
- `l4d2_faint_fall_damage` - Remove faint when ragdoll falls far enough to cause damage (0/1). Default: `"1"`

Commands
------
- `sm_faint` - Toggle ragdoll on your own survivor.
- `sm_faintplayer` - Toggle faint on another player: sm_faintplayer <#userid|name>

Changelog
------

### V1.4.0 (10-Oct-2026)

- Fixed players ending up with no usable weapon after make faint while holding a carryable (propane tank, gas can, gnome...). The item is now dropped before the faint and can still be picked up or explode.
- Fixed players being left frozen and unable to switch weapons if the faint was interrupted unexpectedly.
- Weapons are only restored after faint if the player still owns them.

### V1.3.0 (05-June-2026)

- Added the “l4d2_faint_require_grounded” cvar, which prevents survivors from fainting while not on the ground. (This prevents some teleport-related issues)- Added the “l4d2_faint_fall_damage” cvar, which removes the faint effect from players in free fall, causing them to take damage or die, depending on the height.
- Now when the ragdoll collides with “invisible walls”, it gets knocked back instead of teleported. (This is more funny, and helps prevent the ragdoll from getting stuck on an edge.)
- Faint detection in elevators and moving platforms has been improved.


### V1.2.0 (31-May-2026)

- Fixed another issue where a player with the “Faint” ability activated could be teleported off the map when grabbed by a charger.


### V1.1.0 (30-May-2026)

- Fixed an issue that would teleport the player off the map when using the command during the campaign intro cutscenes.
- The ragdoll's glow has been changed, now the colors are based on the player health rather than the standard blue hue.


### V1.0.0 (28-May-2026)

- Initial release.