public Action L4D2_OnChooseVictim(int iInfected, int &iTarget)
{
	g_iInfectedBot_CurrentVictim[iInfected] = iTarget;
	return Plugin_Continue;
}

public void OnActionCreated(BehaviorAction hAction, int iActor, const char[] sName)
{
	if (strcmp(sName[8], "LegsRegroup") == 0)
	{
		hAction.OnUpdatePost = OnRegroupWithTeamAction;
		hAction.OnResume = OnRegroupWithTeamResume;
	}
	else if (strcmp(sName[8], "LiberateBesiegedFriend") == 0)
	{
		hAction.OnUpdatePost = OnMoveToIncapacitatedFriendAction;
	}
	else if (strcmp(sName[8], "LegsRetreat") == 0)
	{
		hAction.OnUpdatePost = OnRetreatAction;
		if (g_bCvar_Debug && IsValidClient(iActor))
		{
			g_iDebug_RetreatActionCount[iActor]++;
			if (GetGameTime() >= g_fDebug_NextRetreatActionLogTime[iActor])
			{
				g_fDebug_NextRetreatActionLogTime[iActor] = GetGameTime() + 1.0;
				DebugLog("client=%d(%N) LEGS-RETREAT-START count=%d", iActor, iActor, g_iDebug_RetreatActionCount[iActor]);
				g_iDebug_RetreatActionCount[iActor] = 0;
			}
		}
	}
}

Action OnRetreatAction(BehaviorAction hAction, int iActor, float fInterval, ActionResult hResult)
{
	// The witch code also retreats, and that one must keep running with no tank around.
	if (!IsValidClient(iActor) || !L4D2_IsTankInPlay() || g_bSurvivorBot_IsWitchHarasser[iActor])
		return Plugin_Continue;

	// The game also starts retreats on its own, and one of those would pull the bot off the route.
	if (strcmp(g_sSurvivorBot_MovePos_Name[iActor], BOT_TANK_ROUTE_MOVE_NAME) != 0 && IsTankInFleeRange(iActor, true))
		return Plugin_Continue;

	if (g_bCvar_Debug && GetGameTime() >= g_fDebug_NextRetreatStopLogTime[iActor])
	{
		g_fDebug_NextRetreatStopLogTime[iActor] = GetGameTime() + 1.0;
		DebugLog("client=%d(%N) TANK-FLEE-STOP source=action", iActor, iActor);
	}

	hResult.type = DONE;
	return Plugin_Changed;
}

Action OnRegroupWithTeamAction(BehaviorAction hAction, int iActor, float fInterval, ActionResult hResult)
{
	int iLeader = (hAction.Get(0x34) & 0xFFF);
	if (!IsValidClient(iLeader))return Plugin_Continue;

	int iPathDangerous = LBI_IsPathToPositionDangerous(iActor, g_fClientAbsOrigin[iLeader]);
	if (iPathDangerous == -1)return Plugin_Continue;

	// The game ends a retreat from a tank out of sight right away, so one is only built with the tank in sight.
	if (iPathDangerous != 0 && IsTankInFleeRange(iActor) && IsTankVisibleCached(iActor, iPathDangerous))
	{
		BehaviorAction hRetreat = CreateSurvivorLegsRetreatAction(iPathDangerous);
		if (hRetreat == INVALID_ACTION)return Plugin_Continue;

		// Suspending the regroup lets it resume after the retreat instead of being thrown away.
		hResult.type = SUSPEND_FOR;
		hResult.action = hRetreat;
		if (g_bCvar_Debug)
		{
			g_iDebug_RegroupSuspendCount[iActor]++;
			if (GetGameTime() >= g_fDebug_NextRegroupSuspendLogTime[iActor])
			{
				g_fDebug_NextRegroupSuspendLogTime[iActor] = GetGameTime() + 1.0;
				DebugLog("client=%d(%N) LEGS-REGROUP-SUSPEND leader=%d(%N) threat=%d count=%d", iActor, iActor, iLeader, iLeader, iPathDangerous, g_iDebug_RegroupSuspendCount[iActor]);
				g_iDebug_RegroupSuspendCount[iActor] = 0;
			}
		}
		g_fSurvivorBot_TankFleeUntil[iActor] = GetGameTime() + BOT_TANK_FLEE_AIM_TIME;
		return Plugin_Handled;
	}

	hResult.type = DONE;
	return Plugin_Changed;
}

// Only logs: proves that the regroup comes back after a retreat it was suspended for.
Action OnRegroupWithTeamResume(BehaviorAction hAction, int iActor, BehaviorAction hInterrupting, ActionResult hResult)
{
	if (!g_bCvar_Debug || !IsValidClient(iActor))return Plugin_Continue;

	g_iDebug_RegroupResumeCount[iActor]++;
	if (GetGameTime() >= g_fDebug_NextRegroupResumeLogTime[iActor])
	{
		g_fDebug_NextRegroupResumeLogTime[iActor] = GetGameTime() + 1.0;
		DebugLog("client=%d(%N) LEGS-REGROUP-RESUME count=%d", iActor, iActor, g_iDebug_RegroupResumeCount[iActor]);
		g_iDebug_RegroupResumeCount[iActor] = 0;
	}
	return Plugin_Continue;
}

Action OnMoveToIncapacitatedFriendAction(BehaviorAction hAction, int iActor, float fInterval, ActionResult hResult)
{
	int iFriend = (hAction.Get(0x34) & 0xFFF);
	if (!IsValidClient(iFriend) || L4D_GetPlayerReviveTarget(iActor) == iFriend || GetClientDistance(iActor, iFriend, true) <= 15625.0 && IsVisibleEntity(iActor, iFriend))
		return Plugin_Continue;

	if (L4D2_IsTankInPlay() && CanRescueFriendFromTank(iActor, iFriend))
		return Plugin_Continue;

	int iPathDangerous = LBI_IsPathToPositionDangerous(iActor, g_fClientAbsOrigin[iFriend], LBI_PDSLOT_RESCUE);
	if (iPathDangerous == -1)return Plugin_Continue;

	if (g_bCvar_Debug && GetGameTime() > g_fDebug_NextRescueAbandonLogTime[iActor])
	{
		g_fDebug_NextRescueAbandonLogTime[iActor] = GetGameTime() + 1.0;
		DebugLog("client=%d(%N) RESCUE-ABANDONED friend=%d(%N) pathResult=%d", iActor, iActor, iFriend, iFriend, iPathDangerous);
	}

	// Ending the rescue lets the bot's own tank flee take over.
	hResult.type = DONE;
	return Plugin_Changed;
}

BehaviorAction CreateSurvivorLegsRetreatAction(int iThreat)
{
	return g_hSurvivorLegsRetreat.Execute(iThreat);
}
