// Shared by all bots. Destroyed infected leave the list right away.
void UpdateInfectedSnapshot()
{
	float fNow = GetGameTime();
	if (IsCacheTimeValid(g_fInfectedSnap_NextTime, fNow, BOT_INFECTED_SNAP_TIME))
		return;

	g_fInfectedSnap_NextTime = fNow + BOT_INFECTED_SNAP_TIME;

	for (int i = 0; i < g_iInfectedSnap_Count; i++)
		g_iInfectedSnap_Slot[g_iInfectedSnap_Entity[i]] = 0;

	int iCount, iInfected = INVALID_ENT_REFERENCE;
	while ((iInfected = FindEntityByClassname(iInfected, "infected")) != INVALID_ENT_REFERENCE)
	{
		if (iInfected > MAXENTITIES || !IsCommonInfectedAlive(iInfected))
			continue;

		g_iInfectedSnap_Entity[iCount] = iInfected;
		g_bInfectedSnap_Attacking[iCount] = IsCommonInfectedAttacking(iInfected);
		GetEntityAbsOrigin(iInfected, g_fInfectedSnap_Origin[iCount]);
		GetEntityCenteroid(iInfected, g_fInfectedSnap_Center[iCount]);
		iCount++;
		g_iInfectedSnap_Slot[iInfected] = iCount;
	}
	g_iInfectedSnap_Count = iCount;

	g_iInfectedSnap_PipeBomb = FindEntityByClassname(-1, "pipe_bomb_projectile");
	if (g_iInfectedSnap_PipeBomb > 0)GetEntityAbsOrigin(g_iInfectedSnap_PipeBomb, g_fInfectedSnap_PipeBombPos);
	g_bInfectedSnap_BileThrown = (FindEntityByClassname(-1, "info_goal_infected_chase") != -1);
}

void RemoveFromInfectedSnapshot(int iEntity)
{
	int iSlot = (g_iInfectedSnap_Slot[iEntity] - 1);
	int iLast = (g_iInfectedSnap_Count - 1);

	g_iInfectedSnap_Slot[iEntity] = 0;
	g_iInfectedSnap_Count = iLast;
	if (iSlot == iLast)
		return;

	g_iInfectedSnap_Entity[iSlot] = g_iInfectedSnap_Entity[iLast];
	g_bInfectedSnap_Attacking[iSlot] = g_bInfectedSnap_Attacking[iLast];
	g_fInfectedSnap_Origin[iSlot] = g_fInfectedSnap_Origin[iLast];
	g_fInfectedSnap_Center[iSlot] = g_fInfectedSnap_Center[iLast];
	g_iInfectedSnap_Slot[g_iInfectedSnap_Entity[iSlot]] = (iSlot + 1);
}

// Shared by all bots, so each one doesn't scan the entity list on its own.
void UpdateEntityScans()
{
	float fNow = GetGameTime();
	bool bTankInPlay = L4D2_IsTankInPlay();

	if (!IsCacheTimeValid(g_fEntityScan_NextFastTime, fNow, BOT_ENTITY_SCAN_FAST_TIME))
	{
		g_fEntityScan_NextFastTime = fNow + BOT_ENTITY_SCAN_FAST_TIME;
		g_iEntityScan_RockCount = 0;
		if (bTankInPlay)
		{
			int iRock = INVALID_ENT_REFERENCE;
			while (g_iEntityScan_RockCount < BOT_ENTITY_SCAN_MAX && (iRock = FindEntityByClassname(iRock, "tank_rock")) != INVALID_ENT_REFERENCE)
				g_iEntityScan_Rocks[g_iEntityScan_RockCount++] = EntIndexToEntRef(iRock);
		}
	}

	if (IsCacheTimeValid(g_fEntityScan_NextSlowTime, fNow, BOT_ENTITY_SCAN_SLOW_TIME))
		return;

	g_fEntityScan_NextSlowTime = fNow + BOT_ENTITY_SCAN_SLOW_TIME;

	int iEntity;
	g_iEntityScan_DeathModelCount = 0;
	if (g_bCvar_DefibRevive_Enabled)
	{
		iEntity = INVALID_ENT_REFERENCE;
		while (g_iEntityScan_DeathModelCount < BOT_ENTITY_SCAN_MAX && (iEntity = FindEntityByClassname(iEntity, "survivor_death_model")) != INVALID_ENT_REFERENCE)
			g_iEntityScan_DeathModels[g_iEntityScan_DeathModelCount++] = EntIndexToEntRef(iEntity);
	}

	g_iEntityScan_PropCount = 0;
	if (bTankInPlay)
	{
		iEntity = INVALID_ENT_REFERENCE;
		while (g_iEntityScan_PropCount < BOT_ENTITY_SCAN_MAX && (iEntity = FindEntityByClassname(iEntity, "prop_car_alarm")) != INVALID_ENT_REFERENCE)
			g_iEntityScan_Props[g_iEntityScan_PropCount++] = EntIndexToEntRef(iEntity);

		iEntity = INVALID_ENT_REFERENCE;
		while (g_iEntityScan_PropCount < BOT_ENTITY_SCAN_MAX && (iEntity = FindEntityByClassname(iEntity, "prop_physics")) != INVALID_ENT_REFERENCE)
		{
			if (HasEntProp(iEntity, Prop_Send, "m_hasTankGlow") && GetEntProp(iEntity, Prop_Send, "m_hasTankGlow") != 0)
				g_iEntityScan_Props[g_iEntityScan_PropCount++] = EntIndexToEntRef(iEntity);
		}
	}
}

int GetGrenadeFarTarget(int iClient)
{
	float fNow = GetGameTime();
	if (fNow >= g_fSurvivorBot_NextGrenadeTargetTime[iClient])
	{
		g_fSurvivorBot_NextGrenadeTargetTime[iClient] = fNow + BOT_GRENADE_CHECK_INTERVAL;
		g_iSurvivorBot_GrenadeFarTarget[iClient] = GetFarthestInfected(iClient, g_fCvar_GrenadeThrow_ThrowRange);
	}
	return (IsEntityExists(g_iSurvivorBot_GrenadeFarTarget[iClient]) ? g_iSurvivorBot_GrenadeFarTarget[iClient] : -1);
}

int GetFarthestInfected(int iClient, float fDistance = -1.0)
{
	UpdateInfectedSnapshot();

	static int iCandidates[MAXENTITIES+1];
	static float fCandidateDist[MAXENTITIES+1];
	int iCount;

	float fInfectedDist;
	for (int i = 0; i < g_iInfectedSnap_Count; i++)
	{
		fInfectedDist = GetVectorDistance(g_fClientEyePos[iClient], g_fInfectedSnap_Center[i], true);
		if (fDistance > 0.0 && fInfectedDist > (fDistance*fDistance))
			continue;

		iCandidates[iCount] = i;
		fCandidateDist[iCount++] = fInfectedDist;
	}

	// Farthest first: the first one in sight is the answer, so most of a horde needs no trace.
	int iBest, iSlot;
	for (int iMisses = 0; iCount > 0 && iMisses < BOT_INFECTED_SIGHT_MISSES; iMisses++)
	{
		iBest = 0;
		for (int i = 1; i < iCount; i++)
		{
			if (fCandidateDist[i] > fCandidateDist[iBest])iBest = i;
		}

		iSlot = iCandidates[iBest];
		if (IsVisibleVector(iClient, g_fInfectedSnap_Center[iSlot]))
			return g_iInfectedSnap_Entity[iSlot];

		iCount--;
		iCandidates[iBest] = iCandidates[iCount];
		fCandidateDist[iBest] = fCandidateDist[iCount];
	}

	return -1;
}

void CheckEntityForVisibility(int iClient, int iEntity, bool bFOVOnly = false, float fOverridePos[3] = NULL_VECTOR)
{
	g_bEntityHasVisionMemory[iEntity] = true;
	g_bSurvivorBot_VisionMemoryUsed[iClient] = true;

	float fVisionTime = (bFOVOnly ? g_fSurvivorBot_VisionMemory_Time_FOV[iClient][iEntity] : g_fSurvivorBot_VisionMemory_Time[iClient][iEntity]);
	if (GetGameTime() < fVisionTime)return;

	float fCheckPos[3];
	if (IsNullVector(fOverridePos))GetEntityCenteroid(iEntity, fCheckPos);
	else fCheckPos = fOverridePos;
	if (bFOVOnly && !FVectorInViewCone(iClient, fCheckPos))return;

	float fNoticeTime;
	bool bIsVisible = IsVisibleEntity(iClient, iEntity);
	float fEntityDist = GetVectorDistance(g_fClientEyePos[iClient], fCheckPos, true);
	float fDot = RadToDeg(ArcCosine(GetLineOfSightDotProduct(iClient, fCheckPos)));
	
	float fMaxDist = 16777216.0;

	int iVisionState = (bFOVOnly ? g_iSurvivorBot_VisionMemory_State_FOV[iClient][iEntity] : g_iSurvivorBot_VisionMemory_State[iClient][iEntity]);
	if (!bFOVOnly)
	{
		fNoticeTime = (ClampFloat(0.66 * (fDot / 165.0) + (fEntityDist / fMaxDist), 0.1, 1.5) * g_fCvar_Vision_NoticeTimeScale);
		switch(iVisionState)
		{
			case 0:
			{
				// Without a time, a target never seen would be rechecked every tick.
				if (!bIsVisible)
				{
					g_fSurvivorBot_VisionMemory_Time[iClient][iEntity] = GetGameTime() + BOT_VISION_RECHECK_TIME;
					return;
				}
				g_iSurvivorBot_VisionMemory_State[iClient][iEntity] = 1;
				g_fSurvivorBot_VisionMemory_Time[iClient][iEntity] = GetGameTime() + fNoticeTime;
			}
			case 1:
			{
				g_iSurvivorBot_VisionMemory_State[iClient][iEntity] = (bIsVisible ? 2 : 0);
			}
			case 2:
			{
				// Something just seen won't be noticed as gone any later than a few ticks.
				if (!bIsVisible)g_iSurvivorBot_VisionMemory_State[iClient][iEntity] = 3;
				else g_fSurvivorBot_VisionMemory_Time[iClient][iEntity] = GetGameTime() + BOT_VISION_RECHECK_TIME;
			}
			case 3:
			{
				if (bIsVisible)
				{
					g_iSurvivorBot_VisionMemory_State[iClient][iEntity] = 2;
					return;
				}
				if ((GetGameTime() - fVisionTime) >= 15.0)
				{
					g_iSurvivorBot_VisionMemory_State[iClient][iEntity] = 0;
					return;
				}
				g_fSurvivorBot_VisionMemory_Time[iClient][iEntity] = GetGameTime() + ClampFloat(fNoticeTime * 0.33, 0.1, fNoticeTime);
			}
		}
	}
	else
	{
		fNoticeTime = (ClampFloat(0.33 * (fDot / g_fCvar_Vision_FieldOfView) + (fEntityDist / fMaxDist), 0.1, 0.75) * g_fCvar_Vision_NoticeTimeScale);
		switch(iVisionState)
		{
			case 0:
			{
				if (!bIsVisible)
				{
					g_fSurvivorBot_VisionMemory_Time_FOV[iClient][iEntity] = GetGameTime() + BOT_VISION_RECHECK_TIME;
					return;
				}
				g_iSurvivorBot_VisionMemory_State_FOV[iClient][iEntity] = 1;
				g_fSurvivorBot_VisionMemory_Time_FOV[iClient][iEntity] = GetGameTime() + fNoticeTime;
			}
			case 1:
			{
				g_iSurvivorBot_VisionMemory_State_FOV[iClient][iEntity] = (bIsVisible ? 2 : 0);
			}
			case 2:
			{
				if (!bIsVisible)g_iSurvivorBot_VisionMemory_State_FOV[iClient][iEntity] = 3;
				else g_fSurvivorBot_VisionMemory_Time_FOV[iClient][iEntity] = GetGameTime() + BOT_VISION_RECHECK_TIME;
			}
			case 3:
			{
				g_fSurvivorBot_VisionMemory_Time_FOV[iClient][iEntity] = GetGameTime() + fNoticeTime;
				if (bIsVisible)
				{
					g_iSurvivorBot_VisionMemory_State_FOV[iClient][iEntity] = 2;
					return;
				}
				if ((GetGameTime() - fVisionTime) >= 15.0)
				{
					g_iSurvivorBot_VisionMemory_State_FOV[iClient][iEntity] = 0;
					return;
				}
			}
		}
	}
}

bool HasVisualContactWithEntity(int iClient, int iEntity, bool bFOVState = true, float fOverridePos[3] = NULL_VECTOR)
{
	CheckEntityForVisibility(iClient, iEntity, bFOVState, fOverridePos);
	int iState = (bFOVState ? g_iSurvivorBot_VisionMemory_State_FOV[iClient][iEntity] : g_iSurvivorBot_VisionMemory_State[iClient][iEntity]);
	return (iState == 2);
}

stock float ClampFloat(float fValue, float fMin, float fMax)
{
	return (fValue > fMax) ? fMax : ((fValue < fMin) ? fMin : fValue);
}

int GetClosestInfected(int iClient, float fDistance = -1.0)
{
	int iCloseInfected = -1;
	float fInfectedDist, fLastDist = -1.0;

	bool bIsChasingSomething = false;
	int iInfected;

	UpdateInfectedSnapshot();

	float fClientPos[3]; GetEntityAbsOrigin(iClient, fClientPos);

	int iMeleeTargets[MAXPLAYERS+1], iMeleeTargetCount;
	for (int i = 1; i <= MaxClients; i++)
	{
		if (iClient == i || g_iSurvivorBot_TargetInfected[i] <= 0 || !IsClientSurvivor(i) || !IsWeaponSlotActive(i, 1) || !IsEntityExists(g_iSurvivorBot_TargetInfected[i]) || SurvivorHasMeleeWeapon(i) == 0)
			continue;

		iMeleeTargets[iMeleeTargetCount++] = g_iSurvivorBot_TargetInfected[i];
	}

	static int iCandidates[MAXENTITIES+1];
	static float fCandidateDist[MAXENTITIES+1];
	int iCount;

	for (int n = 0; n < g_iInfectedSnap_Count; n++)
	{
		iInfected = g_iInfectedSnap_Entity[n];

		fInfectedDist = GetVectorDistance(fClientPos, g_fInfectedSnap_Origin[n], true);
		if (fDistance > 0.0 && fInfectedDist > (fDistance*fDistance))
			continue;

		bIsChasingSomething = (fInfectedDist > 25600.0 && (g_bInfectedSnap_BileThrown || g_iInfectedSnap_PipeBomb > 0 && GetVectorDistance(g_fInfectedSnap_Origin[n], g_fInfectedSnap_PipeBombPos, true) <= 65536.0));
		if (!bIsChasingSomething && fInfectedDist > 9216.0)
		{
			for (int i = 0; i < iMeleeTargetCount; i++)
			{
				bIsChasingSomething = (iMeleeTargets[i] == iInfected);
				if (bIsChasingSomething)break;
			}
		}

		if (bIsChasingSomething)
			continue;

		iCandidates[iCount] = iInfected;
		fCandidateDist[iCount++] = fInfectedDist;
	}

	// Nearest first: the first one in sight is the answer, so a horde behind a wall doesn't cost traces for each of its members.
	int iBest;
	for (int iMisses = 0; iCount > 0 && iMisses < BOT_INFECTED_SIGHT_MISSES; iMisses++)
	{
		iBest = 0;
		for (int i = 1; i < iCount; i++)
		{
			if (fCandidateDist[i] < fCandidateDist[iBest])iBest = i;
		}

		if (IsVisibleEntity(iClient, iCandidates[iBest]))
		{
			iCloseInfected = iCandidates[iBest];
			fLastDist = fCandidateDist[iBest];
			break;
		}

		iCount--;
		iCandidates[iBest] = iCandidates[iCount];
		fCandidateDist[iBest] = fCandidateDist[iCount];
	}

	bool bIsAttacking = false;
	for (iInfected = 1; iInfected <= MaxClients; iInfected++)
	{
		if (!IsSpecialInfected(iInfected) || L4D2_GetPlayerZombieClass(iInfected) == L4D2ZombieClass_Tank || bIsAttacking && !IsUsingSpecialAbility(iInfected))
			continue;

		fInfectedDist = GetClientDistance(iClient, iInfected, true);
		if (fDistance > 0.0 && fInfectedDist > (fDistance*fDistance) || fLastDist != -1.0 && fInfectedDist >= fLastDist || !IsVisibleEntity(iClient, iInfected, MASK_VISIBLE_AND_NPCS))
			continue;

		iCloseInfected = iInfected;
		fLastDist = fInfectedDist;
		bIsAttacking = IsUsingSpecialAbility(iInfected);
	}

	return iCloseInfected;
}

int GetInfectedCount(int iClient, float fDistanceLimit = -1.0, int iMaxLimit = -1, bool bVisible = true, bool bAttackingOnly = true)
{
	int iCount, iMisses;

	float fClientPos[3];
	GetEntityCenteroid(iClient, fClientPos);

	UpdateInfectedSnapshot();

	for (int i = 0; i < g_iInfectedSnap_Count; i++)
	{
		if (bAttackingOnly && !g_bInfectedSnap_Attacking[i])
			continue;

		if (fDistanceLimit > 0.0 && GetVectorDistance(fClientPos, g_fInfectedSnap_Center[i], true) > (fDistanceLimit*fDistanceLimit))
			continue;

		if (bVisible && (IsValidClient(iClient) ? !IsVisibleVector(iClient, g_fInfectedSnap_Center[i], MASK_VISIBLE_AND_NPCS) : !GetVectorVisible(fClientPos, g_fInfectedSnap_Center[i])))
		{
			// After this many misses most of the horde is out of sight.
			if (iMaxLimit > 0 && ++iMisses >= (iMaxLimit > BOT_INFECTED_SIGHT_MISSES ? iMaxLimit : BOT_INFECTED_SIGHT_MISSES))return iCount;
			continue;
		}

		iCount++;
		if (iMaxLimit > 0 && iCount >= iMaxLimit)return iCount;
	}

	return iCount;
}

// Same as GetInfectedCount with radii 125, 300 and 500, in one pass with one visibility test per infected.
void UpdateNearbyInfectedCounts(int iClient)
{
	int iThreat, iNearest, iNearby;

	float fClientPos[3];
	GetEntityCenteroid(iClient, fClientPos);

	UpdateInfectedSnapshot();

	float fDist;
	for (int i = 0; i < g_iInfectedSnap_Count; i++)
	{
		if (!g_bInfectedSnap_Attacking[i])
			continue;

		fDist = GetVectorDistance(fClientPos, g_fInfectedSnap_Center[i], true);
		if (fDist > 250000.0 || !IsVisibleVector(iClient, g_fInfectedSnap_Center[i], MASK_VISIBLE_AND_NPCS))
			continue;

		// The second check refines the counts up close, where they decide reactions.
		if (fDist <= BOT_INFECTED_CLOSE_SIGHT_DIST_SQR && !GetVectorVisible(fClientPos, g_fInfectedSnap_Center[i]))
			continue;

		iNearby++;
		if (fDist <= 90000.0)iNearest++;
		if (fDist <= 15625.0)iThreat++;
	}

	g_iSurvivorBot_ThreatInfectedCount[iClient] = iThreat;
	g_iSurvivorBot_NearestInfectedCount[iClient] = iNearest;
	g_iSurvivorBot_NearbyInfectedCount[iClient] = iNearby;
}

bool IsSpecialInfected(int iClient)
{
	return (IsValidClient(iClient) && GetClientTeam(iClient) == 3 && IsPlayerAlive(iClient) && !L4D_IsPlayerGhost(iClient));
}

bool IsWitch(int iEntity)
{
	if (!IsEntityExists(iEntity))
		return false;

	static char sClass[12]; GetEntityClassname(iEntity, sClass, sizeof(sClass));
	return (strcmp(sClass, "witch") == 0);
}

bool IsSurvivorBusy(int iClient, bool bHoldingGrenade = false, bool bHoldingMedkit = false, bool bHoldingPills = false)
{
	if (bHoldingGrenade && IsWeaponSlotActive(iClient, 2) || bHoldingMedkit && IsWeaponSlotActive(iClient, 3) || bHoldingPills && IsWeaponSlotActive(iClient, 4))
		return true;

	L4D2UseAction iUseAction = L4D2_GetPlayerUseAction(iClient);
	if (iUseAction != L4D2UseAction_None && (bHoldingMedkit || iUseAction != L4D2UseAction_Healing && iUseAction != L4D2UseAction_Defibing))
		return true;

	if (L4D_GetPlayerReviveTarget(iClient) > 0)
		return true;

	if (L4D_IsPlayerStaggering(iClient))
		return true;

	return false;
}

bool IsVisibleVector(int iClient, float fPos[3], int iMask = MASK_SHOT)
{
	if (IsFakeClient(iClient) && GetClientTeam(iClient) == 2 && IsSurvivorBotBlindedByVomit(iClient))
		return false;

	Handle hResult = TR_TraceRayFilterEx(g_fClientEyePos[iClient], fPos, iMask, RayType_EndPoint, Base_TraceFilter);
	float fFraction = TR_GetFraction(hResult); delete hResult;
	return (fFraction == 1.0);
}

bool IsVisibleEntity(int iClient, int iTarget, int iMask = MASK_SHOT)
{
	// The infected snapshot can still hold an entity that was just destroyed.
	if (!IsEntityExists(iTarget))
		return false;

	if (IsFakeClient(iClient) && GetClientTeam(iClient) == 2 && IsSurvivorBotBlindedByVomit(iClient))
		return false;

	float fTargetPos[3];
	GetEntityAbsOrigin(iTarget, fTargetPos);

	Handle hResult = TR_TraceRayFilterEx(g_fClientEyePos[iClient], fTargetPos, iMask, RayType_EndPoint, Base_TraceFilter, iTarget);
	bool bDidHit = (TR_GetFraction(hResult) == 1.0 && !TR_StartSolid(hResult) || TR_GetEntityIndex(hResult) == iTarget); delete hResult;
	if (!bDidHit)
	{
		float fViewOffset[3]; 
		GetEntPropVector(iTarget, Prop_Data, "m_vecViewOffset", fViewOffset);
		AddVectors(fTargetPos, fViewOffset, fTargetPos);

		hResult = TR_TraceRayFilterEx(g_fClientEyePos[iClient], fTargetPos, iMask, RayType_EndPoint, Base_TraceFilter, iTarget);
		bDidHit = (TR_GetFraction(hResult) == 1.0 && !TR_StartSolid(hResult) || TR_GetEntityIndex(hResult) == iTarget); delete hResult;
		if (!bDidHit)
		{
			GetEntityCenteroid(iTarget, fTargetPos);
			
			hResult = TR_TraceRayFilterEx(g_fClientEyePos[iClient], fTargetPos, iMask, RayType_EndPoint, Base_TraceFilter, iTarget);
			bDidHit = (TR_GetFraction(hResult) == 1.0 && !TR_StartSolid(hResult) || TR_GetEntityIndex(hResult) == iTarget); delete hResult;
		}
	}
	return (bDidHit);
}

float GetLineOfSightDotProduct(int iClient, const float fVecSpot[3])
{
	float fLineOfSight[3]; 
	MakeVectorFromPoints(g_fClientEyePos[iClient], fVecSpot, fLineOfSight);
	NormalizeVector(fLineOfSight, fLineOfSight);

	float fEyeDirection[3];
	GetAngleVectors(g_fClientEyeAng[iClient], fEyeDirection, NULL_VECTOR, NULL_VECTOR);
	NormalizeVector(fEyeDirection, fEyeDirection);

	return GetVectorDotProduct(fLineOfSight, fEyeDirection);
}

bool FVectorInViewCone(int iClient, const float fVecSpot[3], float fCone = -1.0)
{
	if (fCone == -1.0)fCone = g_fCvar_Vision_FieldOfView;
	return (RadToDeg(ArcCosine(GetLineOfSightDotProduct(iClient, fVecSpot))) <= fCone);
}

float GetViewAnglesDotProduct(int iClient, const float fVecSpot[3])
{
	float fLineOfSight[3]; 
	MakeVectorFromPoints(g_fClientAbsOrigin[iClient], fVecSpot, fLineOfSight);
	fLineOfSight[2] = 0.0;
	NormalizeVector(fLineOfSight, fLineOfSight);

	float fDirection[3]; GetClientAbsAngles(iClient, fDirection);
	GetAngleVectors(fDirection, fDirection, NULL_VECTOR, NULL_VECTOR);
	fDirection[2] = 0.0;
	NormalizeVector(fDirection, fDirection);

	return GetVectorDotProduct(fLineOfSight, fDirection);
}

bool FVectorInViewAngle(int iClient, const float fVecSpot[3], float fAngle = -1.0)
{
	if (fAngle == -1.0)fAngle = g_fCvar_Vision_FieldOfView;
	return (RadToDeg(ArcCosine(GetViewAnglesDotProduct(iClient, fVecSpot))) <= fAngle);
}

bool FEntityInViewAngle(int iClient, int iEntity, float fAngle = -1.0)
{
	if (fAngle == -1.0)fAngle = g_fCvar_Vision_FieldOfView;
	float fEntityAbsOrigin[3]; GetEntityCenteroid(iEntity, fEntityAbsOrigin);
	return (RadToDeg(ArcCosine(GetViewAnglesDotProduct(iClient, fEntityAbsOrigin))) <= fAngle);
}

void SnapViewToPosition(int iClient, const float fPos[3])
{
	if (g_bClient_IsLookingAtPosition[iClient])
		return;
	
	float fDesiredDir[3];
	MakeVectorFromPoints(g_fClientEyePos[iClient], fPos, fDesiredDir);
	GetVectorAngles(fDesiredDir, fDesiredDir);

	float fEyeAngles[3];
	fEyeAngles[0] = (g_fClientEyeAng[iClient][0] + NormalizeAngle(fDesiredDir[0] - g_fClientEyeAng[iClient][0]));
	fEyeAngles[1] = (g_fClientEyeAng[iClient][1] + NormalizeAngle(fDesiredDir[1] - g_fClientEyeAng[iClient][1]));
	fEyeAngles[2] = 0.0;

	TeleportEntity(iClient, NULL_VECTOR, fEyeAngles, NULL_VECTOR);
	g_bClient_IsLookingAtPosition[iClient] = true;
}

float NormalizeAngle(float fAngle)
{
	fAngle = (fAngle - RoundToFloor(fAngle / 360.0) * 360.0);
	if (fAngle > 180.0)fAngle -= 360.0;
	else if (fAngle < -180.0)fAngle += 360.0;
	return fAngle;
}

void GetClientAimPosition(int iClient, float fAimPos[3])
{
	Handle hResult = TR_TraceRayFilterEx(g_fClientEyePos[iClient], g_fClientEyeAng[iClient], MASK_SHOT, RayType_Infinite, Base_TraceFilter);
	TR_GetEndPosition(fAimPos, hResult); delete hResult;
}

bool GetVectorVisible(float fStart[3], float fEnd[3], int iMask = MASK_VISIBLE_AND_NPCS)
{
	Handle hResult = TR_TraceRayFilterEx(fStart, fEnd, iMask, RayType_EndPoint, Base_TraceFilter);
	float fFraction = TR_GetFraction(hResult); delete hResult; return (fFraction == 1.0);
}

bool Base_TraceFilter(int iEntity, int iContentsMask, int iData)
{
	if (iEntity == iData)
		return true;

	if (iEntity < 0 || iEntity > MAXENTITIES)
		return (HasEntProp(iEntity, Prop_Data, "m_eDoorState") && L4D_GetDoorState(iEntity) != DOOR_STATE_OPENED);

	// Runs for every entity a trace crosses, so it skips the slower property lookup.
	if (g_iEntCache_IsDoor[iEntity] == 0)
		g_iEntCache_IsDoor[iEntity] = (HasEntProp(iEntity, Prop_Data, "m_eDoorState") ? 1 : 2);

	return (g_iEntCache_IsDoor[iEntity] == 1 && L4D_GetDoorState(iEntity) != DOOR_STATE_OPENED);
}
