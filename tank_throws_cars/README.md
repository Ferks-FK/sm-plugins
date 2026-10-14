# [L4D2] Tank Throws Cars

The Tank picks up the car (or any other hittable) he is aiming at and throws it instead of a rock. SourceMod version of the [Tank Throws Cars](https://steamcommunity.com/sharedfiles/filedetails/?id=3809650828) Workshop mod.

- Only the hittable the Tank is aiming at, within range, is picked up. Otherwise he throws a normal rock.
- The prop is held in the Tank's hand during the throw animation and launched where he is looking.
- If the Tank dies, is staggered or is passed to another player before releasing it, the prop drops from his hand.
- A thrown car alarm loses its alarm, like a car punched by the Tank.
- Bot Tanks can also throw hittables (see `tank_throws_cars_range_try_throw`).

Requires SourceMod 1.11 or newer and the bundled `gamedata/tank_throws_cars.txt`.

## ConVars

| ConVar | Default | Description |
|---|---|---|
| `tank_throws_cars_range_pickup` | 50 | Max distance (on the ground, from the Tank's feet) to the spot of the prop the Tank is aiming at |
| `tank_throws_cars_aim_tolerance` | 0 | How many units the aim may miss a prop's edge and still pick it up |
| `tank_throws_cars_range_try_throw` | 50 | Bot Tanks only. -1 = bots never pick up props (always a normal rock). >= 0 = a bot aiming at a hittable this close throws it |
| `tank_throws_cars_throw_power` | 1200 | Force used when the Tank throws |
| `tank_throws_cars_release_forward_dist` | 50 | How far in front of the Tank the prop is released |

## Installation

1. Compile `addons/sourcemod/scripting/tank_throws_cars.sp` and put the `.smx` in `addons/sourcemod/plugins`.
2. Copy `addons/sourcemod/gamedata/tank_throws_cars.txt` to the server's `addons/sourcemod/gamedata`.

## Changelog

### v1.0.0

- Initial release.
