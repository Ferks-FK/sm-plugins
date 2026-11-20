void ClearMoveToPosition(int iClient, const char[] sCheckName = "", const char[] sReason = "", bool bEvenOnDamage = false)
{
	if (sCheckName[0] != 0 && strcmp(g_sSurvivorBot_MovePos_Name[iClient], sCheckName) != 0)
		return;

	if (!IsValidVector(g_fSurvivorBot_MovePos_Position[iClient]) || !bEvenOnDamage && LBI_IsDamagingNavArea(g_iClientNavArea[iClient]))
		return;

	if (g_bCvar_Debug)
	{
		DebugLog("client=%d(%N) MOVEPOS-CLEAR name=%s reason=%s", iClient, iClient, g_sSurvivorBot_MovePos_Name[iClient], (sReason[0] != 0 ? sReason : "unspecified"));
	}

	g_iSurvivorBot_MovePos_Priority[iClient] = -1;
	g_fSurvivorBot_MovePos_Duration[iClient] = GetGameTime();
	g_fSurvivorBot_MovePos_Tolerance[iClient] = -1.0;
	g_bSurvivorBot_MovePos_IgnoreDamaging[iClient] = false;
	g_sSurvivorBot_MovePos_Name[iClient][0] = 0;

	SetVectorToZero(g_fSurvivorBot_MovePos_Position[iClient]);
	g_fSurvivorBot_NextRetreatTime[iClient] = 0.0;
	L4D2_CommandABot(iClient, 0, BOT_CMD_RESET);
}

bool CanSetMoveToPosition(int iClient, int iPriority)
{
	return (iPriority >= g_iSurvivorBot_MovePos_Priority[iClient] && !IsValidVector(g_fSurvivorBot_MovePos_Position[iClient]));
}

void CacheMovePosAreas(int iClient)
{
	if (g_bSurvivorBot_MovePos_AreasCached[iClient])
		return;

	g_iSurvivorBot_MovePos_Area[iClient] = L4D_GetNearestNavArea(g_fSurvivorBot_MovePos_Position[iClient]);
	g_iSurvivorBot_MovePos_ReachArea[iClient] = L4D_GetNearestNavArea(g_fSurvivorBot_MovePos_Position[iClient], 200.0, true, true, false, 0);
	g_bSurvivorBot_MovePos_AreasCached[iClient] = true;
}

bool IsMovePosDamaging(int iClient)
{
	CacheMovePosAreas(iClient);
	return (g_iSurvivorBot_MovePos_Area[iClient] && LBI_IsDamagingNavArea(g_iSurvivorBot_MovePos_Area[iClient]));
}

// Checked every tick and a path search is expensive, so the answer is kept until the bot or the goal moves.
#define BOT_MOVEPOS_REACH_CACHE_TIME	0.5

bool IsMovePosReachable(int iClient)
{
	CacheMovePosAreas(iClient);
	int iGoal = g_iSurvivorBot_MovePos_ReachArea[iClient];
	if (!iGoal)return false;

	int iStart = g_iClientNavArea[iClient];
	float fNow = GetGameTime();
	if (iStart != g_iSurvivorBot_MovePosReach_Start[iClient] || iGoal != g_iSurvivorBot_MovePosReach_Goal[iClient] || fNow >= g_fSurvivorBot_MovePosReach_Until[iClient])
	{
		g_iSurvivorBot_MovePosReach_Start[iClient] = iStart;
		g_iSurvivorBot_MovePosReach_Goal[iClient] = iGoal;
		g_fSurvivorBot_MovePosReach_Until[iClient] = fNow + BOT_MOVEPOS_REACH_CACHE_TIME;
		g_bSurvivorBot_MovePosReach_Result[iClient] = LBI_IsReachableNavArea(iClient, iGoal);
	}
	return g_bSurvivorBot_MovePosReach_Result[iClient];
}

void SetMoveToPosition(int iClient, float fMovePos[3], int iPriority, const char[] sName = "", float fAddDuration = 0.66, float fDistTolerance = -1.0, bool bIgnoreDamaging = false, bool bIgnoreCheckpoints = false)
{
	if (!CanSetMoveToPosition(iClient, iPriority))
		return;

	if (fDistTolerance >= 0.0 && GetVectorDistance(g_fClientAbsOrigin[iClient], fMovePos, true) <= (fDistTolerance*fDistTolerance))
		return;

	if (!bIgnoreDamaging && (LBI_IsDamagingNavArea(g_iClientNavArea[iClient]) || LBI_IsDamagingPosition(fMovePos)))
		return;

	if (!bIgnoreCheckpoints && LBI_IsPositionInsideCheckpoint(g_fClientAbsOrigin[iClient]) && !LBI_IsPositionInsideCheckpoint(fMovePos))
		return;

	// The check made every think would cancel this move right away, and a failed search is very slow.
	if (!LBI_IsReachablePosition(iClient, fMovePos))
		return;

	float fTravelDist = GetClientTravelDistance(iClient, fMovePos, false, BOT_FAR_PATH_SEARCH_MAX);
	if (fTravelDist <= 0.0)fTravelDist = GetVectorDistance(g_fClientAbsOrigin[iClient], fMovePos, false);

	float fMaxSpeed = GetClientMaxSpeed(iClient);
	g_fSurvivorBot_MovePos_Duration[iClient] = GetGameTime() + (fTravelDist / fMaxSpeed) + fAddDuration;

	if (g_bCvar_Debug)
	{
		DebugLog("client=%d(%N) MOVEPOS-SET name=%s priority=%d pos=[%.1f %.1f %.1f] travelDist=%.1f durationLeft=%.1f", iClient, iClient, sName, iPriority, fMovePos[0], fMovePos[1], fMovePos[2], fTravelDist, (g_fSurvivorBot_MovePos_Duration[iClient] - GetGameTime()));
	}

	strcopy(g_sSurvivorBot_MovePos_Name[iClient], 512, sName);

	g_fSurvivorBot_MovePos_Position[iClient] = fMovePos;
	g_bSurvivorBot_MovePos_AreasCached[iClient] = false;
	g_iSurvivorBot_MovePos_Priority[iClient] = iPriority;
	g_fSurvivorBot_MovePos_Tolerance[iClient] = fDistTolerance;
	g_bSurvivorBot_MovePos_IgnoreDamaging[iClient] = bIgnoreDamaging;
}

void LBI_TryGetPathableLocationWithin(int iClient, float fRadius, float fBuffer[3])
{
	char sBuffer[512];
	FormatEx(sBuffer, sizeof(sBuffer), "local ply = GetPlayerFromUserID(%i);\
		local location = ply.TryGetPathableLocationWithin(%f);\
		local spaceChar = 32;\
		<RETURN>location.x.tostring() + spaceChar.tochar() + location.y.tostring() + spaceChar.tochar() + location.z.tostring()</RETURN>", GetClientUserId(iClient), fRadius);
	L4D2_GetVScriptOutput(sBuffer, sBuffer, sizeof(sBuffer));

	char sCoordinate[64];
	SplitString(sBuffer, " ", sCoordinate, sizeof(sCoordinate));
	fBuffer[0] = StringToFloat(sCoordinate);
	FormatEx(sCoordinate, sizeof(sCoordinate), "%s ", sCoordinate);
	ReplaceString(sBuffer, sizeof(sBuffer), sCoordinate, "");

	SplitString(sBuffer, " ", sCoordinate, sizeof(sCoordinate));
	fBuffer[1] = StringToFloat(sCoordinate);
	FormatEx(sCoordinate, sizeof(sCoordinate), "%s ", sCoordinate);
	ReplaceString(sBuffer, sizeof(sBuffer), sCoordinate, "");

	fBuffer[2] = StringToFloat(sBuffer);
}
