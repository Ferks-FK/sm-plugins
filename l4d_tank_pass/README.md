Allows the tank to pass control to another player.

Original: https://forums.alliedmods.net/showpost.php?p=2841587&postcount=25

### v2.8.1

------
- Fixed: When the player controlling the tank disconnects or switches teams, the pass count is not taken into account, and the tank may be assigned to the AI even if passes are available.

### v2.8.0

------
- Frustration passes now use the game's native "X gets Tank" window instead of an instant swap. The plugin only steers who receives the Tank.
- New cvar `l4d_tank_pass_unique` (default 1): frustration gives the Tank to a player who hasn't controlled it yet. A player can only get it again when everyone already had it. The Tank only goes to the AI when the pass limit is reached. The list resets when the Tank dies.
- Fixed: after the last pass (e.g. 3/3), the next frustration offered the Tank to another player instead of the AI.

### v2.7.0

------
- Now the plugin intercepts when the tank's frustration reaches 0, and counts it as a pass by the plugin. (As long as there are passes, the tank will always go to a player; it will only go to the AI when the passes run out.)
- Better chat colors.