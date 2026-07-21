Announces to chat how much damage and what percentage each survivor dealt to the Tank once it dies (or when survivors wipe/juke it), sorted from highest to lowest. Also shows the Tank's remaining health when it survives. All chat output is now handled through translations (English, Portuguese and Spanish included).

Convars
------
- `l4d_tankdamage_enabled` - Announce damage done to Tanks when enabled. Default: `"1"`
- `l4d_tankdamage_reward_pills` - Reward the survivor who dealt the most damage to the Tank with pain pills on Tank death, but only if their light-health slot (pills/adrenaline) is empty. Default: `"0"`

Forwards
------
- `OnTankDeath` - Called once the Tank dies and damage has been announced.

Translations
------
- Chat strings were moved out of the source into `translations/l4d_tank_damage_announce.phrases.txt`.

Original: https://github.com/SirPlease/L4D2-Competitive-Rework/blob/master/addons/sourcemod/scripting/l4d_tank_damage_announce.sp
