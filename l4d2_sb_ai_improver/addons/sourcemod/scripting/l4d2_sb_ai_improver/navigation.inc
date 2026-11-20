void LBI_GetNavAreaCenter(int iNavArea, float fResult[3])
{
	Address hAddress = view_as<Address>(iNavArea);	
	fResult[0] = view_as<float>(LoadFromAddress(hAddress + view_as<Address>(g_iNavArea_Center), NumberType_Int32));
	fResult[1] = view_as<float>(LoadFromAddress(hAddress + view_as<Address>(g_iNavArea_Center + 4), NumberType_Int32));
	fResult[2] = view_as<float>(LoadFromAddress(hAddress + view_as<Address>(g_iNavArea_Center + 8), NumberType_Int32));
}

int LBI_GetNavAreaParent(int iNavArea)
{
	return (LoadFromAddress(view_as<Address>(iNavArea) + view_as<Address>(g_iNavArea_Parent), NumberType_Int32));
}

bool LBI_IsDamagingNavArea(int iNavArea, bool bIgnoreWitches = false)
{
	if (!iNavArea)
		return false;

	int iTickCount = LoadFromAddress(view_as<Address>(iNavArea) + view_as<Address>(g_iNavArea_DamagingTickCount), NumberType_Int32);
	if (GetGameTickCount() <= iTickCount)
	{
		if (!bIgnoreWitches && L4D2_GetWitchCount() > 0)
		{
			int iWitch = INVALID_ENT_REFERENCE;
			float fWitchPos[3], fClosePoint[3];
			while ((iWitch = FindEntityByClassname(iWitch, "witch")) != INVALID_ENT_REFERENCE)
			{
				GetEntityAbsOrigin(iWitch, fWitchPos);
				LBI_GetClosestPointOnNavArea(iNavArea, fWitchPos, fClosePoint);
				if (GetVectorDistance(fWitchPos, fClosePoint, true) <= 14400.0)return false;
			}
		}
		return true;
	}
	return false;
}

bool LBI_IsDamagingPosition(const float fPos[3])
{
	int iCloseArea = L4D_GetNearestNavArea(fPos);
	return (iCloseArea && LBI_IsDamagingNavArea(iCloseArea));
}

void LBI_GetNavAreaCorners(int iNavArea, float fNWCorner[3], float fSECorner[3])
{
	Address hAddress = view_as<Address>(iNavArea);
	for (int i = 0; i < 3; i++)
	{
		fNWCorner[i] = view_as<float>(LoadFromAddress(hAddress + view_as<Address>(g_iNavArea_NWCorner + (4 * i)), NumberType_Int32));
		fSECorner[i] = view_as<float>(LoadFromAddress(hAddress + view_as<Address>(g_iNavArea_SECorner + (4 * i)), NumberType_Int32));
	}
}

void LBI_GetClosestPointOnNavArea(int iNavArea, const float fPos[3], float fClosePoint[3])
{
	float fNWCorner[3], fSECorner[3];
	LBI_GetNavAreaCorners(iNavArea, fNWCorner, fSECorner);

	float fNewPos[3];
	fNewPos[0] = fsel((fPos[0] - fNWCorner[0]), fPos[0], fNWCorner[0]);
	fNewPos[0] = fsel((fNewPos[0] - fSECorner[0]), fSECorner[0], fNewPos[0]);
	
	fNewPos[1] = fsel((fPos[1] - fNWCorner[1]), fPos[1], fNWCorner[1]);
	fNewPos[1] = fsel((fNewPos[1] - fSECorner[1]), fSECorner[1], fNewPos[1]);

	fNewPos[2] = LBI_GetNavAreaZ(iNavArea, fNWCorner, fSECorner, fNewPos[0], fNewPos[1]);

	fClosePoint = fNewPos;
}

// The heights of the other two corners come right after the inverse sizes in CNavArea.
float LBI_GetNavAreaNEZ(int iNavArea)
{
	return view_as<float>(LoadFromAddress(view_as<Address>(iNavArea) + view_as<Address>(g_iNavArea_InvDYCorners + 4), NumberType_Int32));
}

float LBI_GetNavAreaSWZ(int iNavArea)
{
	return view_as<float>(LoadFromAddress(view_as<Address>(iNavArea) + view_as<Address>(g_iNavArea_InvDYCorners + 8), NumberType_Int32));
}

float LBI_GetNavAreaZ(int iNavArea, const float fNWCorner[3], const float fSECorner[3], float x, float y)
{
	float fInvDXCorners = view_as<float>(LoadFromAddress(view_as<Address>(iNavArea) + view_as<Address>(g_iNavArea_InvDXCorners), NumberType_Int32));
	float fInvDYCorners = view_as<float>(LoadFromAddress(view_as<Address>(iNavArea) + view_as<Address>(g_iNavArea_InvDYCorners), NumberType_Int32));

	float fNEZ = LBI_GetNavAreaNEZ(iNavArea);
	if (fInvDXCorners == 0.0 || fInvDYCorners == 0.0)
		return fNEZ;

	float fSWZ = LBI_GetNavAreaSWZ(iNavArea);

	float u = (x - fNWCorner[0]) * fInvDXCorners;
	float v = (y - fNWCorner[1]) * fInvDYCorners;

	u = fsel(u, u, 0.0);
	u = fsel(u - 1.0, 1.0, u);

	v = fsel(v, v, 0.0);
	v = fsel(v - 1.0, 1.0, v);

	float fNorthZ = fNWCorner[2] + u * (fNEZ - fNWCorner[2]);
	float fSouthZ = fSWZ + u * (fSECorner[2] - fSWZ);

	return (fNorthZ + v * (fSouthZ - fNorthZ));
}

stock float fsel(float fComparand, float fValGE, float fLT)
{
	return (fComparand >= 0.0 ? fValGE : fLT);
}

void LBI_GetNavAreaCorner(int iNavArea, int iCorner, float fResult[3])
{
	Address hAddress = view_as<Address>(iNavArea);
	switch(iCorner)
	{
		case 0:
		{
			fResult[0] = view_as<float>(LoadFromAddress(hAddress + view_as<Address>(g_iNavArea_NWCorner), NumberType_Int32));
			fResult[1] = view_as<float>(LoadFromAddress(hAddress + view_as<Address>(g_iNavArea_NWCorner+4), NumberType_Int32));
			fResult[2] = view_as<float>(LoadFromAddress(hAddress + view_as<Address>(g_iNavArea_NWCorner+8), NumberType_Int32));
		}
		case 1:
		{
			fResult[0] = view_as<float>(LoadFromAddress(hAddress + view_as<Address>(g_iNavArea_SECorner), NumberType_Int32));
			fResult[1] = view_as<float>(LoadFromAddress(hAddress + view_as<Address>(g_iNavArea_NWCorner+4), NumberType_Int32));
			fResult[2] = LBI_GetNavAreaNEZ(iNavArea);
		}
		case 2:
		{
			fResult[0] = view_as<float>(LoadFromAddress(hAddress + view_as<Address>(g_iNavArea_NWCorner), NumberType_Int32));
			fResult[1] = view_as<float>(LoadFromAddress(hAddress + view_as<Address>(g_iNavArea_SECorner+4), NumberType_Int32));
			fResult[2] = LBI_GetNavAreaSWZ(iNavArea);
		}
		case 3:
		{
			fResult[0] = view_as<float>(LoadFromAddress(hAddress + view_as<Address>(g_iNavArea_SECorner), NumberType_Int32));
			fResult[1] = view_as<float>(LoadFromAddress(hAddress + view_as<Address>(g_iNavArea_SECorner+4), NumberType_Int32));
			fResult[2] = view_as<float>(LoadFromAddress(hAddress + view_as<Address>(g_iNavArea_SECorner+8), NumberType_Int32));
		}
		default:
		{
			return;
		}
	}
}

bool LBI_IsNavAreaPartiallyVisible(int iNavArea, const float fEyePos[3], int iIgnoreEntity = -1)
{
	float fOffset = (0.75 * HUMAN_HEIGHT);
	
	float fCenter[3]; LBI_GetNavAreaCenter(iNavArea, fCenter);
	fCenter[2] += fOffset;

	Handle hResult = TR_TraceRayFilterEx(fEyePos, fCenter, MASK_VISIBLE_AND_NPCS, RayType_EndPoint, CTraceFilterNoNPCsOrPlayer, iIgnoreEntity);
	float fFraction = TR_GetFraction(hResult); delete hResult;
	if (fFraction == 1.0)return true;

	float fEyeToCenter[3];
	MakeVectorFromPoints(fEyePos, fCenter, fEyeToCenter);
	NormalizeVector(fEyeToCenter, fEyeToCenter);

	float fCorner[3], fEyeToCorner[3];
	for (int i = 0; i < 4; ++i)
	{
		LBI_GetNavAreaCorner(iNavArea, i, fCorner);
		fCorner[2] += fOffset;

		MakeVectorFromPoints(fEyePos, fCorner, fEyeToCorner);
		NormalizeVector(fEyeToCorner, fEyeToCorner);
		if (GetVectorDotProduct(fEyeToCorner, fEyeToCenter) >= 0.98)
			continue;

		hResult = TR_TraceRayFilterEx(fEyePos, fCorner, MASK_VISIBLE_AND_NPCS, RayType_EndPoint, CTraceFilterNoNPCsOrPlayer, iIgnoreEntity);
		fFraction = TR_GetFraction(hResult); delete hResult;
		if (fFraction == 1.0)return true;
	}

	return false;
}

bool CTraceFilterNoNPCsOrPlayer(int iEntity, int iContentsMask, int iIgnore)
{
	if (iEntity == 0 || IsValidClient(iEntity))
		return true;

	static char sClassname[64]; GetEntityClassname(iEntity, sClassname, sizeof(sClassname));
	if (strncmp(sClassname, "func_door", 9) == 0 || strncmp(sClassname, "prop_door", 9) == 0)
		return false;

	if (strcmp(sClassname, "func_brush") == 0)
	{
		int iSolidity = GetEntProp(iEntity, Prop_Data, "m_iSolidity");
		return (iSolidity == 2);
	}

	if ((strcmp(sClassname, "func_breakable_surf") == 0 || strcmp(sClassname, "func_breakable") == 0 && GetEntityHealth(iEntity) > 0) && GetEntProp(iEntity, Prop_Data, "m_takedamage") == 2)
		return false;

	if (strcmp(sClassname, "func_playerinfected_clip") == 0)
		return false;

	return (iEntity != iIgnore);
}

bool LBI_IsPositionInsideCheckpoint(const float fPos[3])
{
	if (!L4D2_IsGenericCooperativeMode())
		return false;

	Address pNavArea = view_as<Address>(L4D_GetNearestNavArea(fPos));
	if (pNavArea == Address_Null)return false;

	int iAttributes = L4D_GetNavArea_SpawnAttributes(pNavArea);
	return ((iAttributes & NAV_SPAWN_FINALE) == 0 && (iAttributes & NAV_SPAWN_CHECKPOINT) != 0);
}

// Reads the path left by the last search. The squared result is the length squared: adding up squared
// segments would make a path of many short areas look far shorter than it is.
float GetBuiltPathLength(int iGoalArea, const float fStartPos[3], const float fGoalPos[3], bool bSquared)
{
	float fDistance;
	int iArea = LBI_GetNavAreaParent(iGoalArea);
	if (!iArea)
	{
		fDistance = GetVectorDistance(fStartPos, fGoalPos);
	}
	else
	{
		float fPoint[3], fNextPoint[3];
		LBI_GetClosestPointOnNavArea(iArea, fGoalPos, fPoint);
		fDistance = GetVectorDistance(fPoint, fGoalPos);

		int iParent;
		while ((iParent = LBI_GetNavAreaParent(iArea)) != 0)
		{
			LBI_GetClosestPointOnNavArea(iParent, fGoalPos, fNextPoint);
			fDistance += GetVectorDistance(fPoint, fNextPoint);
			fPoint = fNextPoint;
			iArea = iParent;
		}

		fDistance += GetVectorDistance(fStartPos, fPoint);
	}

	return (bSquared ? (fDistance * fDistance) : fDistance);
}

float GetClientTravelDistance(int iClient, float fGoalPos[3], bool bSquared = false, float fMaxLength = 0.0)
{
	if (!g_bMapStarted)return -1.0;

	int iStartArea = g_iClientNavArea[iClient];
	if (!iStartArea)return -1.0;

	int iGoalArea = L4D_GetNearestNavArea(fGoalPos, _, true, true, true, GetClientTeam(iClient));
	if (!iGoalArea)return -1.0;

	if (!L4D2_NavAreaBuildPath(view_as<Address>(iStartArea), view_as<Address>(iGoalArea), fMaxLength, GetClientTeam(iClient), false))
		return -1.0;

	return GetBuiltPathLength(iGoalArea, g_fClientAbsOrigin[iClient], fGoalPos, bSquared);
}

float GetEntityTravelDistance(int iClient, int iEntity, bool bSquared = false, float fMaxLength = 0.0)
{
	if (!g_bMapStarted)return -1.0;

	int iStartArea, iTeam; float fEntityPos[3];
	if (IsValidClient(iClient))
	{
		iTeam = GetClientTeam(iClient);
		iStartArea = g_iClientNavArea[iClient];
		fEntityPos = g_fClientAbsOrigin[iClient];
	}
	else
	{
		GetEntityAbsOrigin(iClient, fEntityPos);
		iStartArea = L4D_GetNearestNavArea(fEntityPos, _, true, true, true, iTeam);
	}
	if (!iStartArea)return -1.0;

	int iGoalArea; float fTargetPos[3];
	if (IsValidClient(iEntity))
	{
		iGoalArea = g_iClientNavArea[iEntity];
		fTargetPos = g_fClientAbsOrigin[iEntity];
	}
	else
	{
		GetEntityAbsOrigin(iEntity, fTargetPos);
		iGoalArea = L4D_GetNearestNavArea(fTargetPos, _, true, true, true, iTeam);
	}
	if (!iGoalArea)return -1.0;

	if (!L4D2_NavAreaBuildPath(view_as<Address>(iStartArea), view_as<Address>(iGoalArea), fMaxLength, iTeam, false))
		return -1.0;

	return GetBuiltPathLength(iGoalArea, fEntityPos, fTargetPos, bSquared);
}

float GetVectorTravelDistance(float fStartPos[3], float fGoalPos[3], bool bSquared = false, float fMaxLength = 2048.0, int iTeam = 2)
{
	if (!g_bMapStarted)return -1.0;

	int iStartArea = L4D_GetNearestNavArea(fStartPos, _, true, true, true, iTeam);
	if (!iStartArea)return -1.0;

	int iGoalArea = L4D_GetNearestNavArea(fGoalPos, _, true, true, true, iTeam);
	if (!iGoalArea)return -1.0;

	if (!L4D2_NavAreaBuildPath(view_as<Address>(iStartArea), view_as<Address>(iGoalArea), fMaxLength, iTeam, false))
		return -1.0;

	return GetBuiltPathLength(iGoalArea, fStartPos, fGoalPos, bSquared);
}

bool LBI_GetBonePosition(int iEntity, const char[] sBoneName, float fBuffer[3])
{
	if (!IsEntityExists(iEntity))return false;

	int iBoneIndex = SDKCall(g_hLookupBone, iEntity, sBoneName);
	if (iBoneIndex == -1)return false;

	static float fUnusedAngles[3];
	SDKCall(g_hGetBonePosition, iEntity, iBoneIndex, fBuffer, fUnusedAngles);

	return (IsValidVector(fBuffer));
}

bool LBI_IsSurvivorInCombat(int iClient, bool bUnknown = false)
{
	return (SDKCall(g_hIsInCombat, iClient, bUnknown));
}

bool LBI_IsSurvivorBotAvailable(int iClient)
{
	return (SDKCall(g_hIsAvailable, iClient));
}

bool LBI_IsReachableNavArea(int iClient, int iGoalArea, int iStartArea = -1)
{
	int iLastArea = g_iClientNavArea[iClient];
	if (!iLastArea)return false;
	
	if (iStartArea == -1)iStartArea = iLastArea;
	return (iStartArea && (iStartArea == iGoalArea || SDKCall(g_hIsReachableNavArea, iClient, iStartArea, iGoalArea)));
}

bool LBI_IsReachablePosition(int iClient, const float fPos[3])
{
	int iNearArea = L4D_GetNearestNavArea(fPos, 200.0, true, true, false, 0);
	return (iNearArea && LBI_IsReachableNavArea(iClient, iNearArea));
}

bool LBI_IsReachableEntity(int iClient, int iEntity)
{
	if (!IsEntityExists(iEntity) || IsValidClient(iEntity) && !g_iClientNavArea[iEntity])return false;
	float fEntityPos[3]; GetEntityAbsOrigin(iEntity, fEntityPos);
	return (LBI_IsReachablePosition(iClient, fEntityPos));
}

MRESReturn DTR_OnSurvivorBotGetAvoidRange(int iClient, Handle hReturn, Handle hParams)
{
	int iTarget = DHookGetParam(hParams, 1); 
	float fAvoidRange = DHookGetReturn(hReturn);
	float fInitRange = DHookGetReturn(hReturn);

	if (fAvoidRange == 125.0)
	{
		if (iTarget <= MaxClients)
		{
			fAvoidRange = ((!IsUsingSpecialAbility(iTarget) && IsValidClient(L4D_GetPinnedSurvivor(iTarget))) ? 0.0 : 200.0);
		}
		else if (SurvivorHasMeleeWeapon(iClient))
		{
			fAvoidRange = 0.0;
		}
	}
	else if (fAvoidRange == 450.0)
	{
		// Otherwise the game would keep starting a retreat that ends right away.
		if (L4D_IsPlayerIncapacitated(iClient) || strcmp(g_sSurvivorBot_MovePos_Name[iClient], BOT_TANK_ROUTE_MOVE_NAME) == 0 || strcmp(g_sSurvivorBot_MovePos_Name[iClient], BOT_TANK_BLIND_MOVE_NAME) == 0)
		{
			fAvoidRange = 0.0;
		}
		else
		{
			int iVictim = GetTankChaseVictim(iTarget);
			fAvoidRange = ((iVictim == iClient || iVictim > 0 && GetClientDistance(iVictim, iClient, true) <= 160000.0) ? 700.0 : 300.0);

			// The retreat action ends itself outside the flee range, so the game would keep starting it.
			if (!g_bSurvivorBot_IsWitchHarasser[iClient] && GetClientDistance(iClient, iTarget, true) <= (fAvoidRange*fAvoidRange) && !IsTankInFleeRange(iClient, true))
				fAvoidRange = 0.0;
		}
	}
	else if (fAvoidRange == 300.0 && g_iSurvivorBot_WitchTarget[iClient] != -1 && g_bSurvivorBot_IsWitchHarasser[iClient])
	{
		fAvoidRange = 800.0;
	}

	if (IsSurvivorCarryingProp(iClient))
	{
		fAvoidRange += 200.0;
	}


	if (fInitRange != fAvoidRange)
	{
		DHookSetReturn(hReturn, fAvoidRange);
		return MRES_ChangedOverride;
	}

	return MRES_Ignored;
}

MRESReturn DTR_OnInfernoTouchNavArea(int iInferno, Handle hReturn, Handle hParams)
{
	bool bIsTouching = DHookGetReturn(hReturn);
	if (!bIsTouching)return MRES_Ignored;

	int iNavArea = DHookGetParam(hParams, 1);
	if (!iNavArea)return MRES_Ignored;

	float fAreaPos[3];
	bool bCanBlock = true;
	for (int i = 1; i <= MaxClients; i++)
	{
		if (!IsClientInGame(i) || !IsFakeClient(i) || !IsPlayerAlive(i) || GetClientTeam(i) != 2 || L4D_IsPlayerIncapacitated(i) || L4D_IsPlayerPinned(i))
			continue;

		bCanBlock = (g_iClientNavArea[i] != iNavArea);
		if (!bCanBlock)break;

		LBI_GetClosestPointOnNavArea(iNavArea, g_fClientAbsOrigin[i], fAreaPos);
		bCanBlock = (GetVectorDistance(g_fClientAbsOrigin[i], fAreaPos, true) > 4096.0);
		if (!bCanBlock)break;
	}

	if (bCanBlock)
		SDKCall(g_hMarkNavAreaAsBlocked, iNavArea, 2, iInferno, true);

	return MRES_Ignored;
}

MRESReturn DTR_OnFindUseEntity(int iClient, Handle hReturn, Handle hParams)
{
	if (!IsValidClient(iClient) || !IsFakeClient(iClient))
		return MRES_Ignored;

	int iScavengeItem = g_iSurvivorBot_ScavengeItem[iClient];
	if (!IsEntityExists(iScavengeItem))return MRES_Ignored;

	DHookSetReturn(hReturn, iScavengeItem);
	return MRES_ChangedOverride;
}

int LBI_IsPathToPositionDangerous(int iClient, float fGoalPos[3], int iSlot = LBI_PDSLOT_REGROUP)
{
	if (!g_bMapStarted)return -1;

	if (!L4D2_IsTankInPlay())
	{
		g_bPathDanger_HasCache[iClient][LBI_PDSLOT_REGROUP] = false;
		g_bPathDanger_HasCache[iClient][LBI_PDSLOT_RESCUE]  = false;
		g_bPathDanger_HasCache[iClient][LBI_PDSLOT_TEAM]    = false;
		return -1;
	}

	float fNow = GetGameTime();

	if (g_bPathDanger_HasCache[iClient][iSlot]
		&& fNow < g_fPathDanger_NextCheck[iClient][iSlot]
		&& GetVectorDistance(g_fPathDanger_CachedGoal[iClient][iSlot], fGoalPos, true) <= LBI_PATHDANGER_GOALTOL)
	{
		return g_iPathDanger_Cached[iClient][iSlot];
	}

	int iResult = LBI_IsPathToPositionDangerous_Compute(iClient, fGoalPos);

	// Any answer other than -1 stops the bot, so it expires quickly.
	float fCooldown = (iResult != -1) ? LBI_PATHDANGER_CD_BLOCK : LBI_PATHDANGER_COOLDOWN;

	// The jitter keeps all bots from recomputing on the same frame.
	g_fPathDanger_NextCheck[iClient][iSlot]  = fNow + fCooldown + GetRandomFloat(0.0, 0.08);
	g_iPathDanger_Cached[iClient][iSlot]     = iResult;
	g_fPathDanger_CachedGoal[iClient][iSlot] = fGoalPos;
	g_bPathDanger_HasCache[iClient][iSlot]   = true;

	return iResult;
}

int LBI_IsPathToPositionDangerous_Compute(int iClient, float fGoalPos[3])
{
	// The wrapper above already checked IsTankInPlay.
	{
		ArrayList hTankList = new ArrayList();
		float fGoalOffset[3]; fGoalOffset = fGoalPos; fGoalOffset[2] += HUMAN_HALF_HEIGHT;

		float fTankDist, fGoalDist;
		for (int i = 1; i <= MaxClients; i++)
		{
			if (i == iClient || !IsClientInGame(i) || !IsPlayerAlive(i) || L4D2_GetPlayerZombieClass(i) != L4D2ZombieClass_Tank || L4D_IsPlayerIncapacitated(i))
				continue;

			fGoalDist = GetVectorDistance(g_fClientAbsOrigin[iClient], fGoalPos, true);
			if (fGoalDist <= 90000.0 && GetVectorDistance(g_fClientAbsOrigin[i], fGoalPos, true) <= 22500.0 && IsVisibleVector(i, fGoalOffset, MASK_VISIBLE_AND_NPCS))
			{
				delete hTankList;
				return i;
			}

			if (fGoalDist > 250000)
			{
				// No path is shorter than the straight line, so a tank too far away needs no search.
				fTankDist = (GetClientDistance(i, iClient, true) > BOT_TANK_PATH_NEAR_DIST_SQR ? -1.0 : GetClientTravelDistance(i, g_fClientAbsOrigin[iClient], true, BOT_TANK_PATH_NEAR_SEARCH_MAX));
				if (fTankDist != -1.0 && (fTankDist <= 62500.0 || fTankDist <= BOT_TANK_PATH_NEAR_DIST_SQR && GetTankChaseVictim(i) == iClient && IsVisibleEntity(iClient, i, MASK_VISIBLE_AND_NPCS)))
				{
					delete hTankList;
					return i;
				}
			}

			hTankList.Push(i);
		}

		int iClientArea = g_iClientNavArea[iClient];
		if (!iClientArea)
		{
			delete hTankList;
			return -1;
		}

		int iGoalArea = L4D_GetNearestNavArea(fGoalPos, _, true, true, true);
		if (!iGoalArea)
		{
			delete hTankList;
			return -1;
		}

		// A search with no path is very slow, and this check is much cheaper.
		if (!LBI_IsReachableNavArea(iClient, iGoalArea) || !L4D2_NavAreaBuildPath(view_as<Address>(iClientArea), view_as<Address>(iGoalArea), BOT_FAR_PATH_SEARCH_MAX, 2, false))
		{
			delete hTankList;
			return -1;
		}

		int iParent = LBI_GetNavAreaParent(iGoalArea);
		if (iParent)
		{
			int iTank; float fAreaPos[3];
			for (; LBI_GetNavAreaParent(iParent); iParent = LBI_GetNavAreaParent(iParent))
			{
				for (int i = 0; i < hTankList.Length; i++)
				{
					iTank = hTankList.Get(i);

					if (g_iClientNavArea[iTank] != iParent)
					{
						LBI_GetClosestPointOnNavArea(iParent, g_fClientAbsOrigin[iTank], fAreaPos);
						if (GetTankChaseVictim(iTank) != iClient && GetVectorDistance(g_fClientAbsOrigin[iTank], fAreaPos, true) > 22500.0)continue;
					}

					delete hTankList;
					return (GetTankChaseVictim(iTank) == iClient ? iTank : 0);
				}
			}
		}

		delete hTankList;
	}

	return -1;
}
