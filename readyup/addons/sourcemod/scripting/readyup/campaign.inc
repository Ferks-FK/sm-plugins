#if defined _readyup_campaign_included
 #endinput
#endif
#define _readyup_campaign_included

// ========================
//  Mix-gated activation
// ========================
// Ready-up stays dormant until a mix is called (l4d2_mix -> OnMixStarted).
// Once armed, it keeps working for every remaining chapter of the campaign,
// and is disarmed again when the campaign changes (finale ended, vote, admin changelevel, early victory...).

// Delay to check the new map after a chapter transition, must run after l4d2_map_transitions restores scores (1s after map start)
#define CONTINUATION_CHECK_DELAY 3.0

static bool
	s_bCampaignArmed,		// ready-up is active for the rest of the current campaign
	s_bChapterTransition,	// current map ended normally (versus 2nd half / coop map_transition)
	s_bVerifyContinuation,	// new map came from a chapter transition, check if it still is the same match
	s_bDormantRoundLive;	// dormant round already announced as live

// The current campaign was started by a mix.
// False while the continuation check is pending, the new map may be a new game.
bool IsMixMatch()
{
	return IsReadyUpArmed();
}

// Armed and not waiting for the continuation check
bool IsReadyUpArmed()
{
	return s_bCampaignArmed && !s_bVerifyContinuation;
}

// Footer natives also accept strings while dormant and before the round goes live,
// so plugins that add them on round_start (boss percent, panel text...) show up if ready-up starts later in this round.
bool IsFooterOpen()
{
	// nativeFooter is only created in OnPluginStart, natives can be called before that
	if (nativeFooter == null)
		return false;

	return inReadyUp || (!IsReadyUpArmed() && !s_bDormantRoundLive);
}

void Campaign_OnMapStart()
{
	if (s_bVerifyContinuation)
	{
		CreateTimer(CONTINUATION_CHECK_DELAY, Timer_VerifyContinuation, _, TIMER_FLAG_NO_MAPCHANGE);
	}
}

static Action Timer_VerifyContinuation(Handle timer)
{
	if (!s_bVerifyContinuation)
		return Plugin_Stop;

	s_bVerifyContinuation = false;

	// A normal transition never lands on the first map of a campaign.
	// It does when the match moved to another campaign: l4d2_map_transitions keeps the scores,
	// a new game (finale ended, l4d2_early_victory, vote on the scoreboard...) resets them.
	if (L4D_IsFirstMapInScenario()
		&& L4D2Direct_GetVSCampaignScore(0) == 0
		&& L4D2Direct_GetVSCampaignScore(1) == 0)
	{
		s_bCampaignArmed = false;
		return Plugin_Stop;
	}

	// Same match, start the ready-up that round_start skipped
	if (!inReadyUp && !s_bDormantRoundLive && !L4D_HasAnySurvivorLeftSafeArea())
	{
		InitiateReadyUp(.keepFooter = true);
	}

	return Plugin_Stop;
}

void Campaign_OnRoundStart()
{
	s_bChapterTransition = false;
	s_bDormantRoundLive = false;
}

// Called when a mix starts
void ArmReadyUp()
{
	s_bCampaignArmed = true;
	s_bVerifyContinuation = false;

	if (inReadyUp)
		return;

	// Round already underway, ready-up kicks in on the next round_start
	if (s_bDormantRoundLive || L4D_HasAnySurvivorLeftSafeArea())
		return;

	// Keep the footer collected while dormant
	InitiateReadyUp(.keepFooter = true);

	if (!inReadyUp)
		return;

	// l4d2_mix sends "sm_hide" before firing OnMixStarted, when ready-up was still dormant.
	// Hide the panel here so it doesn't cover the captain/pick menus. "sm_show" is sent when the mix stops.
	for (int i = 1; i <= MaxClients; i++)
	{
		if (IsClientInGame(i) && !IsFakeClient(i))
		{
			SetPlayerHiddenPanel(i, true);
		}
	}
}

// Versus: the 2nd half ended, the server is about to load the next map
void Campaign_OnEndVersusRound()
{
	if (GameRules_GetProp("m_bInSecondHalfOfRound"))
	{
		s_bChapterTransition = true;
	}
}

// Coop/Realism: survivors reached the end saferoom
void Campaign_OnMapTransition()
{
	s_bChapterTransition = true;
}

// Any map change that isn't a chapter transition means a new campaign (or a restarted one).
// After a chapter transition, the new map is checked (see Timer_VerifyContinuation).
void Campaign_OnMapEnd()
{
	if (!s_bChapterTransition || s_bVerifyContinuation)
	{
		s_bCampaignArmed = false;
		s_bVerifyContinuation = false;
	}
	else
	{
		s_bVerifyContinuation = s_bCampaignArmed;
	}

	s_bChapterTransition = false;
	s_bDormantRoundLive = false;
}

// When dormant, still run the countdown/live forwards once survivors leave the saferoom,
// so plugins relying on them keep working (l4d2_mix locks !mix on OnRoundIsLive,
// l4d2_alltalk_before_round_start disables alltalk on OnRoundLiveCountdown, starting_items gives items on OnRoundIsLive...).
void Campaign_OnDormantRoundLive()
{
	if (IsReadyUpArmed() || inReadyUp || s_bDormantRoundLive || !l4d_ready_enabled.BoolValue)
		return;

	s_bDormantRoundLive = true;

	UTIL_WrapperForward(g_hPreCountdownForward);
	UTIL_WrapperForward(g_hCountdownForward);
	UTIL_WrapperForward(g_hPreLiveForward);
	UTIL_WrapperForward(g_hLiveForward);
}
