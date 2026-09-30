#if defined _l4d2_rock_bonus_included
 #endinput
#endif
#define _l4d2_rock_bonus_included

/**
 * Tank rock events recognized by the plugin.
 *
 * To add a new event:
 *   1. Add an entry here (before RockBonus_EventCount).
 *   2. In l4d2_rock_bonus.sp: create a points ConVar, add a case to
 *      GetEventPoints() and the phrase name to g_sEventPhrase (same order).
 *   3. Add the phrase to translations/l4d2_rock_bonus.phrases.txt.
 *   4. Call TriggerEvent(<event>, client) where it happens.
 */
enum RockBonusEvent
{
	RockBonus_Skeet = 0,     // A survivor broke the rock mid-air
	RockBonus_HitSurvivor,   // The rock hit a survivor

	RockBonus_EventCount
};

/**
 * Called before an event changes the round bonus.
 *
 * @param client    Survivor involved (who broke it / who was hit).
 * @param event     Event type.
 * @param points    Event points (may be negative). Change it and return Plugin_Changed.
 * @return          Plugin_Handled/Plugin_Stop cancels the event.
 */
forward Action RockBonus_OnEvent(int client, RockBonusEvent event, int &points);

/**
 * Called after an event changed the round bonus.
 * Not called when the event didn't change it (bonus already at 0 or at the max).
 *
 * @param client      Survivor involved.
 * @param event       Event type.
 * @param applied     Points actually applied (the bonus stays within [0, max]).
 * @param roundBonus  Round bonus after the event.
 */
forward void RockBonus_OnEventPost(int client, RockBonusEvent event, int applied, int roundBonus);

/**
 * Returns the bonus accumulated in the current round, always within
 * [0, l4d2_rock_bonus_max].
 */
native int RockBonus_GetRoundBonus();

/**
 * Returns how many times the event happened in the current round.
 */
native int RockBonus_GetEventCount(RockBonusEvent event);

public SharedPlugin __pl_l4d2_rock_bonus =
{
	name = "l4d2_rock_bonus",
	file = "l4d2_rock_bonus.smx",
#if defined REQUIRE_PLUGIN
	required = 1,
#else
	required = 0,
#endif
};

#if !defined REQUIRE_PLUGIN
public void __pl_l4d2_rock_bonus_SetNTVOptional()
{
	MarkNativeAsOptional("RockBonus_GetRoundBonus");
	MarkNativeAsOptional("RockBonus_GetEventCount");
}
#endif
