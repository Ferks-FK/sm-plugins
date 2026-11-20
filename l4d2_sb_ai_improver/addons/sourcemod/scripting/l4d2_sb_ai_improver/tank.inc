bool IsThreatenedByTank(int iClient)
{
	for (int i = 1; i <= MaxClients; i++)
	{
		if (!IsSpecialInfected(i) || L4D2_GetPlayerZombieClass(i) != L4D2ZombieClass_Tank || L4D_IsPlayerIncapacitated(i))
			continue;

		if (GetTankChaseVictim(i) == iClient || GetClientDistance(i, iClient, true) <= BOT_TANK_KEEP_AWAY_DIST_SQR)
			return true;
	}
	return false;
}

// The standing survivor closest to the others, skipping anyone a tank is near or after.
// Measuring the leash from one shared point keeps the team from settling into separate
// pairs, which a distance to the nearest teammate would allow.
int GetTeamAnchor()
{
	int iTick = GetGameTickCount();
	if (iTick == g_iTeamAnchor_Tick)
		return g_iTeamAnchor;

	g_iTeamAnchor_Tick = iTick;
	g_iTeamAnchor = 0;

	int iCandidates[MAXPLAYERS+1], iCount;
	for (int i = 1; i <= MaxClients; i++)
	{
		if (!IsClientSurvivor(i) || L4D_IsPlayerIncapacitated(i) || L4D_IsPlayerPinned(i) || IsThreatenedByTank(i))
			continue;

		iCandidates[iCount++] = i;
	}

	float fSum, fLowestSum = -1.0;
	for (int i = 0; i < iCount; i++)
	{
		fSum = 0.0;
		for (int j = 0; j < iCount; j++)
		{
			fSum += GetClientDistance(iCandidates[i], iCandidates[j]);
		}

		if (fLowestSum != -1.0 && fSum >= fLowestSum)
			continue;

		g_iTeamAnchor = iCandidates[i];
		fLowestSum = fSum;
	}
	return g_iTeamAnchor;
}

bool IsPositionAwayFromTeam(const float fPos[3])
{
	if (g_fCvar_TankTeamLeash <= 0.0)
		return false;

	int iAnchor = GetTeamAnchor();
	return (iAnchor != 0 && GetVectorDistance(g_fClientAbsOrigin[iAnchor], fPos, true) > (g_fCvar_TankTeamLeash*g_fCvar_TankTeamLeash));
}

bool DenyTankRescue(int iBot, int iFriend, const char[] sReason)
{
	if (g_iSurvivorBot_RescueFriend[iBot] == iFriend)g_fSurvivorBot_RescueUntil[iBot] = 0.0;

	if (g_bCvar_Debug && GetGameTime() > g_fDebug_NextRescueLogTime[iBot])
	{
		g_fDebug_NextRescueLogTime[iBot] = GetGameTime() + 1.0;
		DebugLog("client=%d(%N) RESCUE-DENIED friend=%d(%N) reason=%s", iBot, iBot, iFriend, iFriend, sReason);
	}
	return false;
}

// Lets one bot go for a downed friend while a tank is alive, as long as the tank is not after it and is away from both.
// An ongoing rescue uses a shorter tank distance, otherwise the bot would keep starting and dropping it near the limit.
bool CanRescueFriendFromTank(int iBot, int iFriend)
{
	if (g_fCvar_TankRescue_RangeSqr <= 0.0 || !IsClientSurvivor(iFriend) || !L4D_IsPlayerIncapacitated(iFriend) || L4D_IsPlayerIncapacitated(iBot) || L4D_IsPlayerPinned(iBot))
		return false;

	float fNow = GetGameTime();
	bool bOngoing = (fNow < g_fSurvivorBot_RescueUntil[iBot] && g_iSurvivorBot_RescueFriend[iBot] == iFriend);
	float fFriendDist = GetClientDistance(iBot, iFriend, true);
	bool bFarFriend = (fFriendDist > g_fCvar_TankRescue_RangeSqr);
	if (!bOngoing && fFriendDist > (g_fCvar_TankRescue_RangeSqr * BOT_TANK_RESCUE_FAR_RANGE_MULT_SQR))
		return DenyTankRescue(iBot, iFriend, "friend too far");

	float fMinTankDist = (bOngoing ? g_fCvar_TankRescue_AbortDistSqr : g_fCvar_TankRescue_SafeDistSqr);
	for (int i = 1; i <= MaxClients; i++)
	{
		if (!IsSpecialInfected(i) || L4D2_GetPlayerZombieClass(i) != L4D2ZombieClass_Tank || L4D_IsPlayerIncapacitated(i))
			continue;

		if (GetTankChaseVictim(i) == iBot)
			return DenyTankRescue(iBot, iFriend, "tank is after this bot");

		if (GetClientDistance(i, iBot, true) <= fMinTankDist)
			return DenyTankRescue(iBot, iFriend, "tank too close to bot");

		if (GetClientDistance(i, iFriend, true) <= fMinTankDist)
			return DenyTankRescue(iBot, iFriend, "tank too close to friend");
	}

	// -1 means no tank was found along the path, while 0 is a tank on it that is chasing someone else.
	// It is also the answer when there is no path, and the bot would stop fleeing for a rescue it can't do.
	if (bFarFriend && LBI_IsPathToPositionDangerous(iBot, g_fClientAbsOrigin[iFriend], LBI_PDSLOT_RESCUE) != -1)
		return DenyTankRescue(iBot, iFriend, "far friend and tank on the path");

	if (bFarFriend && !IsRescueAreaReachable(iBot, g_iClientNavArea[iFriend]))
		return DenyTankRescue(iBot, iFriend, "far friend out of reach");

	if (!bOngoing)
	{
		for (int i = 1; i <= MaxClients; i++)
		{
			if (i == iBot || !IsClientSurvivor(i) || !IsFakeClient(i) || L4D_IsPlayerIncapacitated(i) || L4D_IsPlayerPinned(i))
				continue;

			if (fNow < g_fSurvivorBot_RescueUntil[i])
			{
				if (g_iSurvivorBot_RescueFriend[i] == iFriend)
					return DenyTankRescue(iBot, iFriend, "friend already has a rescuer");
				continue;
			}

			// A bot being chased or with a tank next to it can't take the rescue, so it must not hold the others back either.
			if (GetClientDistance(i, iFriend, true) < fFriendDist && !IsThreatenedByTank(i))
				return DenyTankRescue(iBot, iFriend, "another bot is closer");
		}

		if (g_bCvar_Debug)DebugLog("client=%d(%N) RESCUE-ALLOWED friend=%d(%N) dist=%.1f", iBot, iBot, iFriend, iFriend, SquareRoot(fFriendDist));
	}

	g_iSurvivorBot_RescueFriend[iBot] = iFriend;
	g_fSurvivorBot_RescueUntil[iBot] = fNow + 1.0;
	return true;
}

// The rescue check runs constantly while a tank is alive, and a downed friend doesn't move, so the answer is kept for a moment.
#define BOT_RESCUE_REACH_CACHE_TIME	0.5

bool IsRescueAreaReachable(int iBot, int iArea)
{
	if (!iArea)
		return false;

	float fNow = GetGameTime();
	if (iArea != g_iSurvivorBot_RescueReachArea[iBot] || fNow >= g_fSurvivorBot_RescueReachUntil[iBot])
	{
		g_iSurvivorBot_RescueReachArea[iBot] = iArea;
		g_fSurvivorBot_RescueReachUntil[iBot] = fNow + BOT_RESCUE_REACH_CACHE_TIME;
		g_bSurvivorBot_RescueReachable[iBot] = LBI_IsReachableNavArea(iBot, iArea);
	}
	return g_bSurvivorBot_RescueReachable[iBot];
}

// The retreat command can keep the game's own rescue from starting, so the think checks this too.
bool IsRescuingFriendFromTank(int iClient)
{
	int iFriend = L4D_GetPlayerReviveTarget(iClient);
	if (iFriend <= 0)iFriend = (GetGameTime() < g_fSurvivorBot_RescueUntil[iClient] ? g_iSurvivorBot_RescueFriend[iClient] : g_iSurvivorBot_IncapacitatedFriend[iClient]);
	return (CanRescueFriendFromTank(iClient, iFriend));
}

// A player-controlled tank never goes through L4D2_OnChooseVictim, and the AI's choice can be someone it already downed.
// Both fall back to the standing survivor closest to the tank.
// Cached per tick, since it is asked many times.
int g_iTankChaseVictim[MAXPLAYERS+1];
int g_iTankChaseVictim_Tick[MAXPLAYERS+1] = {-1, ...};

int GetTankChaseVictim(int iTank)
{
	int iTick = GetGameTickCount();
	if (g_iTankChaseVictim_Tick[iTank] != iTick)
	{
		g_iTankChaseVictim_Tick[iTank] = iTick;
		g_iTankChaseVictim[iTank] = FindTankChaseVictim(iTank);
	}
	return g_iTankChaseVictim[iTank];
}

int FindTankChaseVictim(int iTank)
{
	int iVictim = g_iInfectedBot_CurrentVictim[iTank];
	if (IsFakeClient(iTank) && IsClientSurvivor(iVictim) && !L4D_IsPlayerIncapacitated(iVictim))
		return iVictim;

	iVictim = 0;
	float fCurDist, fLastDist = -1.0;
	for (int i = 1; i <= MaxClients; i++)
	{
		if (!IsClientSurvivor(i) || L4D_IsPlayerIncapacitated(i))
			continue;

		fCurDist = GetClientDistance(iTank, i, true);
		if (fLastDist != -1.0 && fCurDist >= fLastDist)
			continue;

		iVictim = i;
		fLastDist = fCurDist;
	}
	return iVictim;
}

// Returns 0 while the closest tank is too far from its victim to count as chasing it.
int GetTankSupportVictim(int iClient, int &iTank)
{
	iTank = 0;
	float fCurDist, fLastDist = -1.0;
	for (int i = 1; i <= MaxClients; i++)
	{
		if (!IsSpecialInfected(i) || L4D2_GetPlayerZombieClass(i) != L4D2ZombieClass_Tank || L4D_IsPlayerIncapacitated(i))
			continue;

		fCurDist = GetClientDistance(iClient, i, true);
		if (fLastDist != -1.0 && fCurDist >= fLastDist)
			continue;

		iTank = i;
		fLastDist = fCurDist;
	}
	if (!iTank)
		return 0;

	// Holding the choice for a moment keeps the bot from turning around on every victim change.
	int iVictim = g_iSurvivorBot_SupportVictim[iClient];
	if (GetGameTime() >= g_fSurvivorBot_SupportVictimUntil[iClient] || !IsClientSurvivor(iVictim) || L4D_IsPlayerIncapacitated(iVictim))
	{
		iVictim = GetTankChaseVictim(iTank);
		g_iSurvivorBot_SupportVictim[iClient] = iVictim;
		g_fSurvivorBot_SupportVictimUntil[iClient] = GetGameTime() + 1.0;
	}

	return (iVictim != 0 && GetClientDistance(iTank, iVictim, true) <= BOT_TANK_CHASE_DIST_SQR ? iVictim : 0);
}

// Moves the bot toward the survivor a tank is chasing. The tank is usually between them, so unlike
// a regroup this ignores the path danger check and relies on stopping short of the tank instead.
void UpdateTankSupport(int iClient)
{
	if (g_fCvar_TankSupport_RangeSqr <= 0.0)
	{
		g_bSurvivorBot_SupportingVictim[iClient] = false;
		return;
	}

	float fNow = GetGameTime();
	if (fNow <= g_fSurvivorBot_NextTankSupportTime[iClient])
		return;

	g_fSurvivorBot_NextTankSupportTime[iClient] = fNow + 0.5;

	int iTank, iVictim = GetTankSupportVictim(iClient, iTank);
	g_bSurvivorBot_SupportingVictim[iClient] = (iVictim != 0 && iVictim != iClient && !L4D_IsPlayerIncapacitated(iClient) && !L4D_IsPlayerPinned(iClient));
	if (!g_bSurvivorBot_SupportingVictim[iClient])
	{
		ClearMoveToPosition(iClient, "SupportChased", "no one to support");
		return;
	}

	ClearMoveToPosition(iClient, "RegroupTeam", "supporting chased survivor");

	float fVictimDist = GetClientDistance(iClient, iVictim, true);
	bool bSeesTank = IsTankVisibleCached(iClient, iTank);

	if (strcmp(g_sSurvivorBot_MovePos_Name[iClient], "SupportChased") == 0)
	{
		// Stopping well inside the range keeps the bot from flipping between moving and holding at its edge.
		if (bSeesTank && fVictimDist <= (g_fCvar_TankSupport_RangeSqr * 0.44))
		{
			ClearMoveToPosition(iClient, "SupportChased", "in range and sees tank");
			return;
		}

		g_fSurvivorBot_MovePos_Position[iClient] = g_fClientAbsOrigin[iVictim];
		g_bSurvivorBot_MovePos_AreasCached[iClient] = false;
		g_fSurvivorBot_MovePos_Duration[iClient] = fNow + 2.0;
		return;
	}

	if (fVictimDist <= g_fCvar_TankSupport_RangeSqr && (bSeesTank || fVictimDist <= BOT_TANK_SUPPORT_CLOSE_DIST_SQR))
		return;

	// Resuming farther than where it stopped, so a tank walking away doesn't make the bot stutter.
	float fResumeDist = (g_fCvar_TankSupport_MinDist + 100.0);
	if (GetClientDistance(iClient, iTank, true) <= (fResumeDist*fResumeDist))
	{
		if (g_bCvar_Debug)DebugLog("client=%d(%N) SUPPORT-HOLD victim=%d(%N) reason=tank too close", iClient, iClient, iVictim, iVictim);
		return;
	}

	if (IsSurvivorBusy(iClient))
		return;

	ClearMoveToPosition(iClient, "ScavengeItem");
	ClearMoveToPosition(iClient, "ApproachMelee");

	float fGoalPos[3]; fGoalPos = g_fClientAbsOrigin[iVictim];
	SetMoveToPosition(iClient, fGoalPos, 2, "SupportChased", 0.66, SquareRoot(BOT_TANK_SUPPORT_CLOSE_DIST_SQR));

	if (g_bCvar_Debug && strcmp(g_sSurvivorBot_MovePos_Name[iClient], "SupportChased") == 0)
		DebugLog("client=%d(%N) SUPPORT-START victim=%d(%N) tank=%d victimDist=%.1f seesTank=%d", iClient, iClient, iVictim, iVictim, iTank, SquareRoot(fVictimDist), bSeesTank);
}

// The stop range is wider than the start range, so a bot at the edge doesn't flip between fleeing and stopping.
bool IsTankInFleeRange(int iClient, bool bStopRange = false)
{
	float fTankDist;
	float fCloseDist = (bStopRange ? BOT_TANK_KEEP_AWAY_STOP_DIST_SQR : BOT_TANK_KEEP_AWAY_DIST_SQR);
	float fSightDist = (bStopRange ? g_fCvar_TankFleeStopDistSqr : g_fCvar_TankFleeDistSqr);
	float fBlindDist = (bStopRange ? g_fCvar_TankFleeBlindStopDistSqr : g_fCvar_TankFleeBlindDistSqr);
	for (int i = 1; i <= MaxClients; i++)
	{
		if (!IsSpecialInfected(i) || L4D2_GetPlayerZombieClass(i) != L4D2ZombieClass_Tank || L4D_IsPlayerIncapacitated(i))
			continue;

		fTankDist = GetClientDistance(iClient, i, true);
		if (fTankDist <= fCloseDist)
			return true;

		if ((fTankDist <= fSightDist || fTankDist <= fBlindDist) && fTankDist <= (IsTankVisibleCached(iClient, i) ? fSightDist : fBlindDist))
			return true;
	}
	return false;
}

// A retreat command stays active until it is reset, so leaving flee range doesn't end it on its own.
void UpdateTankRetreat(int iClient)
{
	if (strcmp(g_sSurvivorBot_MovePos_Name[iClient], BOT_TANK_ROUTE_MOVE_NAME) == 0 && (!g_bCvar_TankFleeMapRoute || !ShouldUseFlowDistance() || L4D_IsPlayerIncapacitated(iClient) || !IsTankInFleeRange(iClient, true)))
	{
		ClearMoveToPosition(iClient, BOT_TANK_ROUTE_MOVE_NAME, "flee ended");
		if (g_bCvar_Debug && strcmp(g_sSurvivorBot_MovePos_Name[iClient], BOT_TANK_ROUTE_MOVE_NAME) != 0)DebugLog("client=%d(%N) TANK-FLEE-STOP source=route", iClient, iClient);
	}

	if (strcmp(g_sSurvivorBot_MovePos_Name[iClient], BOT_TANK_BLIND_MOVE_NAME) == 0 && (L4D_IsPlayerIncapacitated(iClient) || !IsTankInFleeRange(iClient, true)))
	{
		ClearMoveToPosition(iClient, BOT_TANK_BLIND_MOVE_NAME, "flee ended");
	}

	if (!g_bSurvivorBot_RetreatCommanded[iClient])
		return;

	// A move order has replaced the retreat, and a reset would cancel that move instead.
	if (IsValidVector(g_fSurvivorBot_MovePos_Position[iClient]))
	{
		g_bSurvivorBot_RetreatCommanded[iClient] = false;
		return;
	}

	if (IsTankInFleeRange(iClient, true))
		return;

	g_bSurvivorBot_RetreatCommanded[iClient] = false;
	g_fSurvivorBot_NextRetreatTime[iClient] = 0.0;
	L4D2_CommandABot(iClient, 0, BOT_CMD_RESET);

	if (g_bCvar_Debug)DebugLog("client=%d(%N) TANK-FLEE-STOP source=command", iClient, iClient);
}

// The game's retreat runs to the farthest spot in a straight line, which is often a dead end.
// Following the map route instead leads the bot along the main path.
bool TryFleeAlongMapRoute(int iClient, int iTank)
{
	if (!g_bCvar_TankFleeMapRoute || !ShouldUseFlowDistance() || L4D_IsPlayerIncapacitated(iClient))
		return false;

	bool bOnRoute = (strcmp(g_sSurvivorBot_MovePos_Name[iClient], BOT_TANK_ROUTE_MOVE_NAME) == 0);
	float fClientFlow = L4D2Direct_GetFlowDistance(iClient);
	float fTankFlow = L4D2Direct_GetFlowDistance(iTank);
	if (fClientFlow <= 0.0 || fTankFlow <= 0.0)
		return bOnRoute;

	int iDirection = (fClientFlow >= fTankFlow ? 1 : -1);
	if (bOnRoute)
	{
		// The margin keeps a tank running alongside the bot from flipping its direction.
		if (iDirection == g_iSurvivorBot_TankRouteDirection[iClient] || FloatAbs(fClientFlow - fTankFlow) < BOT_TANK_ROUTE_FLIP_MARGIN)
			return true;

		ClearMoveToPosition(iClient, BOT_TANK_ROUTE_MOVE_NAME, "tank changed side");
		if (strcmp(g_sSurvivorBot_MovePos_Name[iClient], BOT_TANK_ROUTE_MOVE_NAME) == 0)
			return true;
	}

	if (GetGameTime() <= g_fSurvivorBot_NextTankRouteTime[iClient] || L4D_IsPlayerPinned(iClient) || IsValidClient(g_iSurvivorBot_PinnedFriend[iClient]))
		return false;

	// A stronger move order would refuse the route after an expensive search.
	if (IsValidVector(g_fSurvivorBot_MovePos_Position[iClient]) && strcmp(g_sSurvivorBot_MovePos_Name[iClient], "ScavengeItem") != 0 && strcmp(g_sSurvivorBot_MovePos_Name[iClient], "ApproachMelee") != 0)
		return false;

	// Limited even on success, since the move can end right after it starts.
	g_fSurvivorBot_NextTankRouteTime[iClient] = GetGameTime() + BOT_TANK_ROUTE_RETRY_TIME;

	if (!g_bTankRoute_AreasFound)
		FindTankRouteAreas();

	strcopy(g_sDebug_RouteFailReason, sizeof(g_sDebug_RouteFailReason), "no reachable goal");
	g_iTankRoute_Searches = 0;
	int iGoalArea = GetFleeRouteGoalArea(iClient, fClientFlow, iDirection);
	float fWaypointPos[3];
	int iWaypoint = (iGoalArea ? GetFleeRouteWaypoint(iClient, iGoalArea, fWaypointPos) : 0);
	if (iWaypoint)
	{
		ClearMoveToPosition(iClient, "ScavengeItem");
		ClearMoveToPosition(iClient, "ApproachMelee");

		// The tolerance also refuses a bot already at the end of the route, which then uses the game retreat.
		SetMoveToPosition(iClient, fWaypointPos, 2, BOT_TANK_ROUTE_MOVE_NAME, 0.66, BOT_TANK_ROUTE_ARRIVE_DIST);

		strcopy(g_sDebug_RouteFailReason, sizeof(g_sDebug_RouteFailReason), "move refused");
		if (strcmp(g_sSurvivorBot_MovePos_Name[iClient], BOT_TANK_ROUTE_MOVE_NAME) == 0)
		{
			g_iSurvivorBot_TankRouteDirection[iClient] = iDirection;
			g_fSurvivorBot_NextMoveCommandTime[iClient] = 0.0;
			if (g_bCvar_Debug)DebugLog("client=%d(%N) TANK-ROUTE-START direction=%s tank=%d botFlow=%.1f tankFlow=%.1f searches=%d", iClient, iClient, (iDirection > 0 ? "forward" : "back"), iTank, fClientFlow, fTankFlow, g_iTankRoute_Searches);
			return true;
		}
	}

	g_fSurvivorBot_NextTankRouteTime[iClient] = GetGameTime() + BOT_TANK_ROUTE_FAIL_RETRY_TIME;
	if (g_bCvar_Debug)DebugLog("client=%d(%N) TANK-ROUTE-FALLBACK direction=%s reason=%s goalArea=%d waypoint=%d botFlow=%.1f tankFlow=%.1f searches=%d", iClient, iClient, (iDirection > 0 ? "forward" : "back"), g_sDebug_RouteFailReason, iGoalArea, iWaypoint, fClientFlow, fTankFlow, g_iTankRoute_Searches);
	return false;
}

void FindTankRouteAreas()
{
	g_bTankRoute_AreasFound = true;
	g_iTankRoute_StartArea = 0;
	g_iTankRoute_EndArea = 0;
	for (int i = 0; i < BOT_TANK_ROUTE_FLOW_BUCKETS; i++)
		g_iTankRoute_FlowAreas[i] = 0;

	ArrayList hAreas = new ArrayList();
	L4D_GetAllNavAreas(hAreas);

	float fMaxFlow = L4D2Direct_GetMapMaxFlowDistance();
	g_fTankRoute_MaxFlow = fMaxFlow;
	float fLowestFlow = -1.0, fHighestFlow = -1.0, fFlow;
	int iArea;
	for (int i = 0; i < hAreas.Length; i++)
	{
		iArea = hAreas.Get(i);

		// Areas out of the map flow hold invalid values.
		fFlow = L4D2Direct_GetTerrorNavAreaFlow(view_as<Address>(iArea));
		if (fFlow <= 0.0 || fFlow > fMaxFlow)
			continue;

		int iBucket = GetTankRouteFlowBucket(fFlow);
		if (!g_iTankRoute_FlowAreas[iBucket])g_iTankRoute_FlowAreas[iBucket] = iArea;

		if (fLowestFlow == -1.0 || fFlow < fLowestFlow)
		{
			fLowestFlow = fFlow;
			g_iTankRoute_StartArea = iArea;
		}
		if (fFlow > fHighestFlow)
		{
			fHighestFlow = fFlow;
			g_iTankRoute_EndArea = iArea;
		}
	}
	delete hAreas;

	if (g_bCvar_Debug)DebugLog("TANK-ROUTE-AREAS startArea=%d startFlow=%.1f endArea=%d endFlow=%.1f maxFlow=%.1f", g_iTankRoute_StartArea, fLowestFlow, g_iTankRoute_EndArea, fHighestFlow, fMaxFlow);
}

int GetTankRouteFlowBucket(float fFlow)
{
	int iBucket = RoundToFloor(fFlow / g_fTankRoute_MaxFlow * float(BOT_TANK_ROUTE_FLOW_BUCKETS));
	if (iBucket < 0)return 0;
	return (iBucket < BOT_TANK_ROUTE_FLOW_BUCKETS ? iBucket : (BOT_TANK_ROUTE_FLOW_BUCKETS - 1));
}

// A close goal keeps the search short: one to the end of the map could stall the server.
int GetFleeRouteGoalArea(int iClient, float fClientFlow, int iDirection)
{
	if (g_fTankRoute_MaxFlow <= 0.0)
		return 0;

	// An area picked by flow alone can be out of reach, as in a side room or past a closed door, so closer goals get a try.
	int iClientArea = g_iClientNavArea[iClient];
	if (!iClientArea)
		return 0;

	int iArea, iLastArea;
	for (int i = 0; i < BOT_TANK_ROUTE_GOAL_TRIES; i++)
	{
		iArea = GetTankRouteAreaAtFlow(fClientFlow + ((BOT_TANK_ROUTE_GOAL_FLOW - (BOT_TANK_ROUTE_GOAL_FLOW_STEP * float(i))) * float(iDirection)), iDirection);
		if (!iArea || iArea == iLastArea || !LBI_IsReachableNavArea(iClient, iArea))
		{
			iLastArea = iArea;
			continue;
		}
		iLastArea = iArea;

		strcopy(g_sDebug_RouteFailReason, sizeof(g_sDebug_RouteFailReason), "search failed");
		g_iTankRoute_Searches++;
		if (L4D2_NavAreaBuildPath(view_as<Address>(iClientArea), view_as<Address>(iArea), BOT_TANK_ROUTE_MAX_PATH, 2, false))
			return iArea;

		// Searches are expensive, so the last try goes to the closest goal.
		if (g_iTankRoute_Searches >= BOT_TANK_ROUTE_GOAL_SEARCHES)
			return 0;

		if (i < (BOT_TANK_ROUTE_GOAL_TRIES - 2))
			i = (BOT_TANK_ROUTE_GOAL_TRIES - 2);
	}
	return 0;
}

int GetTankRouteAreaAtFlow(float fFlow, int iDirection)
{
	for (int i = GetTankRouteFlowBucket(fFlow); i >= 0 && i < BOT_TANK_ROUTE_FLOW_BUCKETS; i += iDirection)
	{
		if (g_iTankRoute_FlowAreas[i])
			return g_iTankRoute_FlowAreas[i];
	}
	return (iDirection > 0 ? g_iTankRoute_EndArea : g_iTankRoute_StartArea);
}

// A nearby route point instead of its goal keeps the reachability check cheap.
int GetFleeRouteWaypoint(int iClient, int iGoalArea, float fWaypointPos[3])
{
	int iTanks[MAXPLAYERS+1], iTankCount;
	for (int i = 1; i <= MaxClients; i++)
	{
		if (IsSpecialInfected(i) && L4D2_GetPlayerZombieClass(i) == L4D2ZombieClass_Tank && !L4D_IsPlayerIncapacitated(i))
			iTanks[iTankCount++] = i;
	}

	int iWaypoint = iGoalArea;
	int iNearAreas[BOT_TANK_ROUTE_NEAR_AREAS], iNearCount;
	float fNWCorner[3], fSECorner[3], fAreaCenter[3], fAreaPos[3], fAreaDist;
	for (int iArea = iGoalArea; iArea && LBI_GetNavAreaParent(iArea); iArea = LBI_GetNavAreaParent(iArea))
	{
		LBI_GetNavAreaCorners(iArea, fNWCorner, fSECorner);
		AddVectors(fNWCorner, fSECorner, fAreaCenter);
		ScaleVector(fAreaCenter, 0.5);

		// The walk goes from the goal back to the bot, so the last area found out here is the first one the bot reaches.
		fAreaDist = GetVectorDistance(g_fClientAbsOrigin[iClient], fAreaCenter, true);
		if (fAreaDist >= BOT_TANK_ROUTE_LEG_DIST_SQR)
		{
			iWaypoint = iArea;
			iNearCount = 0;
			continue;
		}

		// The areas around the bot don't count, or a tank already next to it would rule out every route.
		if (fAreaDist <= 40000.0)
			continue;

		for (int i = 0; i < iTankCount; i++)
		{
			LBI_GetClosestPointOnNavArea(iArea, g_fClientAbsOrigin[iTanks[i]], fAreaPos);
			if (GetVectorDistance(g_fClientAbsOrigin[iTanks[i]], fAreaPos, true) <= 22500.0)
			{
				strcopy(g_sDebug_RouteFailReason, sizeof(g_sDebug_RouteFailReason), "tank on the path");
				return 0;
			}
		}

		if (iArea != iWaypoint && fAreaDist >= BOT_TANK_ROUTE_MIN_LEG_DIST_SQR && iNearCount < BOT_TANK_ROUTE_NEAR_AREAS)
			iNearAreas[iNearCount++] = iArea;
	}

	// The think refuses a move its reachability check rejects, so it's checked here too, after the walk.
	GetFleeRoutePoint(iWaypoint, fWaypointPos);
	if (LBI_IsReachablePosition(iClient, fWaypointPos))
		return iWaypoint;

	int iStep = ((iNearCount + BOT_TANK_ROUTE_NEAR_CHECKS - 1) / BOT_TANK_ROUTE_NEAR_CHECKS);
	for (int i = 0; i < iNearCount; i += iStep)
	{
		GetFleeRoutePoint(iNearAreas[i], fWaypointPos);
		if (LBI_IsReachablePosition(iClient, fWaypointPos))
		{
			if (g_bCvar_Debug)DebugLog("client=%d(%N) TANK-ROUTE-NEAR area=%d pos=[%.1f %.1f %.1f]", iClient, iClient, iNearAreas[i], fWaypointPos[0], fWaypointPos[1], fWaypointPos[2]);
			return iNearAreas[i];
		}
	}
	strcopy(g_sDebug_RouteFailReason, sizeof(g_sDebug_RouteFailReason), "leg out of reach");
	return 0;
}

void GetFleeRoutePoint(int iArea, float fPos[3])
{
	float fNWCorner[3], fSECorner[3], fMidPos[3];
	LBI_GetNavAreaCorners(iArea, fNWCorner, fSECorner);
	AddVectors(fNWCorner, fSECorner, fMidPos);
	ScaleVector(fMidPos, 0.5);
	LBI_GetClosestPointOnNavArea(iArea, fMidPos, fPos);
}

bool TryRegroupWithTeam(int iClient)
{
	if (GetGameTime() <= g_fSurvivorBot_NextTeamRegroupTime[iClient] || L4D_IsPlayerIncapacitated(iClient) || L4D_IsPlayerPinned(iClient) || IsSurvivorBusy(iClient))
		return false;

	g_fSurvivorBot_NextTeamRegroupTime[iClient] = GetGameTime() + 0.5;

	int iAnchor = GetTeamAnchor();
	if (!iAnchor || iAnchor == iClient)
		return false;

	float fGoalPos[3]; fGoalPos = g_fClientAbsOrigin[iAnchor];
	int iPathDangerous = LBI_IsPathToPositionDangerous(iClient, fGoalPos, LBI_PDSLOT_TEAM);
	if (iPathDangerous != -1)
	{
		if (g_bCvar_Debug)DebugLog("client=%d(%N) REGROUP-BLOCKED anchor=%d(%N) pathResult=%d", iClient, iClient, iAnchor, iAnchor, iPathDangerous);
		return false;
	}

	ClearMoveToPosition(iClient, "ScavengeItem");
	ClearMoveToPosition(iClient, "ApproachMelee");

	// Stopping well inside the leash keeps the bot from flipping between regrouping and fleeing at its edge.
	SetMoveToPosition(iClient, fGoalPos, 2, "RegroupTeam", 0.66, (g_fCvar_TankTeamLeash * 0.5));
	return (strcmp(g_sSurvivorBot_MovePos_Name[iClient], "RegroupTeam") == 0);
}

bool TakeCoverFromPosition(int iClient, float fPosition[3], float fSearchDist = 384.0, bool bStayNearTeam = false, const char[] sMoveName = "TakeCover")
{
	// The move would be refused, so don't search for a spot.
	if (!CanSetMoveToPosition(iClient, 3))
		return false;

	float fSelfToPos[3]; MakeVectorFromPoints(g_fClientEyePos[iClient], fPosition, fSelfToPos);
	NormalizeVector(fSelfToPos, fSelfToPos);

	float fDot, fPathPos[3], fPathOffset[3], fSelfToMovePos[3];
	for (int i = 0; i < 10; i++)
	{
		LBI_TryGetPathableLocationWithin(iClient, fSearchDist, fPathPos);
		if (!IsValidVector(fPathPos) || LBI_IsDamagingPosition(fPathPos))continue;
		if (bStayNearTeam && (GetVectorDistance(fPathPos, fPosition, true) <= BOT_TANK_KEEP_AWAY_DIST_SQR || IsPositionAwayFromTeam(fPathPos)))continue;

		fPathOffset = fPathPos; fPathOffset[2] += HUMAN_HALF_HEIGHT;

		MakeVectorFromPoints(g_fClientEyePos[iClient], fPathPos, fSelfToMovePos);
		NormalizeVector(fSelfToMovePos, fSelfToMovePos);

		fDot = GetVectorDotProduct(fSelfToPos, fSelfToMovePos);
		if (fDot > 0.2 || GetVectorVisible(fPosition, fPathOffset))continue;

		// It can still refuse the move, and reporting success would make the caller search again on the next tick.
		SetMoveToPosition(iClient, fPathPos, 3, sMoveName);
		if (strcmp(g_sSurvivorBot_MovePos_Name[iClient], sMoveName) == 0)
			return true;
	}

	return false;
}

bool TakeCoverFromEntity(int iClient, int iEntity, float fSearchDist = 384.0, bool bStayNearTeam = false, const char[] sMoveName = "TakeCover")
{
	float fEntityPos[3];
	if (IsValidClient(iEntity))GetClientEyePosition(iEntity, fEntityPos);
	else GetEntityCenteroid(iEntity, fEntityPos);
	return (TakeCoverFromPosition(iClient, fEntityPos, fSearchDist, bStayNearTeam, sMoveName));
}
