bool SurvivorBot_IsTargetShootable(int iClient, int iTarget, int iAimTarget, int iCurWeapon, float fAimPos[3])
{
	// Only a boomer or a tank in the aim can stop the fire, so other infected skip these checks.
	int iTeam = GetClientTeam(iTarget);
	if (iTeam != 2 && iTeam != 3)
		return true;

	L4D2ZombieClassType iClass = L4D2ZombieClass_NotInfected;
	if (iTeam == 3)
	{
		iClass = L4D2_GetPlayerZombieClass(iTarget);
		if (iClass != L4D2ZombieClass_Boomer && iClass != L4D2ZombieClass_Tank)
			return true;
	}

	int iPrimarySlot = GetClientWeaponInventory(iClient, 0);
	bool bInViewCone = (iAimTarget == iTarget);
	if (!bInViewCone)
	{
		float fCone = 2.0 * (512.0 / GetVectorDistance(g_fClientEyePos[iClient], g_fClientCenteroid[iTarget]));
		if (iCurWeapon == iPrimarySlot && SurvivorHasShotgun(iClient))fCone *= 2.0;
		bInViewCone = (FVectorInViewCone(iClient, g_fClientCenteroid[iTarget], fCone) && IsVisibleEntity(iClient, iTarget));
	}

	if (bInViewCone && iTeam == 3)
	{
		if (iClass == L4D2ZombieClass_Boomer && GetClientDistance(iClient, iTarget, true) <= BOT_BOOMER_AVOID_RADIUS_SQR)
			return false;
		if (iClass == L4D2ZombieClass_Tank && iCurWeapon == iPrimarySlot && GetWeaponClip1(iCurWeapon) < 3 && IsWeaponReloading(iCurWeapon, false) && SurvivorHasShotgun(iClient))
			return false;
	}

	if (iTeam == 2 && !L4D_IsPlayerPinned(iTarget) && !L4D_IsPlayerIncapacitated(iClient))
	{
		if (bInViewCone && !g_bCvar_BotsShootThrough && (iCurWeapon == iPrimarySlot || iCurWeapon == GetClientWeaponInventory(iClient, 1) && (!SurvivorHasMeleeWeapon(iClient) || GetClientDistance(iClient, iTarget) <= 96.0)))
			return false;

		if (g_bCvar_BotsFriendlyFire)
		{
			if (GetClientDistance(iClient, iTarget, true) <= 256.0)
				return false;
			if (iCurWeapon == iPrimarySlot && GetWeaponClip1(iCurWeapon) != 0 && GetVectorDistance(fAimPos, g_fClientCenteroid[iTarget], true) <= 90000.0 && SurvivorHasTier3Weapon(iClient) == 1 && GetVectorVisible(fAimPos, g_fClientCenteroid[iTarget]))
				return false;
		}
	}

	return true;
}

bool SurvivorBot_CanFreelyFireWeapon(int iClient)
{	
	int iCurWeapon = L4D_GetPlayerCurrentWeapon(iClient);
	if (g_bCvar_AlwaysCarryProp && iCurWeapon == GetClientWeaponInventory(iClient, 5))
	{
		if (g_bCvar_AlwaysCarryProp)return false;
		int iTeamCount = (g_iSurvivorBot_NearbyFriends[iClient] / 2); if (iTeamCount < 1)iTeamCount = 1;
		int iDropLimitCount = RoundFloat(GetCommonHitsUntilDown(iClient, 0.5) * float(iTeamCount));
		return (g_iSurvivorBot_ThreatInfectedCount[iClient] >= iDropLimitCount);
	}

	// Only the sb_friendlyfire checks read fAimPos, and only for a grenade launcher.
	float fAimPos[3];
	if (g_bCvar_BotsFriendlyFire && SurvivorHasTier3Weapon(iClient) == 1)GetClientAimPosition(iClient, fAimPos);
	if (iCurWeapon == GetClientWeaponInventory(iClient, 0))
	{
		int iClip = GetWeaponClip1(iCurWeapon);
		if (g_bCvar_BotsFriendlyFire && iClip != 0 && GetVectorDistance(fAimPos, g_fClientCenteroid[iClient], true) <= 90000.0 && SurvivorHasTier3Weapon(iClient) == 1 && GetVectorVisible(fAimPos, g_fClientCenteroid[iClient]))
		{
			if (IsEntityExists(g_iSurvivorBot_TargetInfected[iClient]) && (!SurvivorHasMeleeWeapon(iClient) || GetVectorDistance(fAimPos, g_fClientCenteroid[iClient], true) <= g_fCvar_ImprovedMelee_SwitchRange))
				SwitchWeaponSlot(iClient, 1);

			return false;
		}

		if (iClip < 2 && IsWeaponReloading(iCurWeapon, false) && SurvivorHasShotgun(iClient))
			return false;
	}

	// Who stands in the aim doesn't change between ticks, so it's checked less often.
	float fNow = GetGameTime();
	if (fNow >= g_fSurvivorBot_NextFireCheckTime[iClient])
	{
		g_fSurvivorBot_NextFireCheckTime[iClient] = fNow + BOT_FIRE_CHECK_INTERVAL;
		g_bSurvivorBot_FireCheckResult[iClient] = true;

		int iAimTarget = GetClientAimTarget(iClient, false);
		float fCurDist, fLastDist = -1.0;
		for (int i = 1; i <= MaxClients; i++)
		{
			if (i == iClient || !IsClientInGame(i) || !IsPlayerAlive(i))continue;

			fCurDist = GetVectorDistance(g_fClientEyePos[iClient], g_fClientCenteroid[i], true);
			if (fLastDist != -1.0 && fCurDist >= fLastDist)continue;
			fLastDist = fCurDist;

			if (SurvivorBot_IsTargetShootable(iClient, i, iAimTarget, iCurWeapon, fAimPos))continue;

			g_bSurvivorBot_FireCheckResult[iClient] = false;
			break;
		}
	}

	return (g_bSurvivorBot_FireCheckResult[iClient] && SurvivorBot_AbleToShootWeapon(iClient));
}

bool SurvivorBot_AbleToShootWeapon(int iClient)
{
	return (!IsWeaponReloading(L4D_GetPlayerCurrentWeapon(iClient)) && !L4D_IsPlayerStaggering(iClient));
}

void GetClosestToEyePosEntityBonePos(int iClient, int iTarget, float fAimPos[3])
{
	// A melee bot asks every tick, and each call reads many bones.
	float fNow = GetGameTime();
	if (g_iSurvivorBot_MeleeBoneTarget[iClient] == iTarget && IsCacheTimeValid(g_fSurvivorBot_MeleeBoneUntil[iClient], fNow, BOT_AIM_PART_CACHE_TIME))
	{
		fAimPos = g_fSurvivorBot_MeleeBonePos[iClient];
		return;
	}

	float fBoneDist, fBonePos[3], fAimPartPos[3], fLastDist = -1.0;
	if (SDKCall(g_hLookupBone, iTarget, "ValveBiped.Bip01_Pelvis") != -1)
	{
		for (int i = 0; i < sizeof(g_sBoneNames_Old); i++)
		{
			if (!LBI_GetBonePosition(iTarget, g_sBoneNames_Old[i], fBonePos))
				continue;

			fBoneDist = GetVectorDistance(g_fClientEyePos[iClient], fBonePos, true);
			if (fLastDist != -1.0 && fBoneDist >= fLastDist)continue;

			fLastDist = fBoneDist;
			fAimPartPos = fBonePos;
		}
	}
	else
	{
		for (int i = 0; i < sizeof(g_sBoneNames_New); i++)
		{
			if (!LBI_GetBonePosition(iTarget, g_sBoneNames_New[i], fBonePos))
				continue;

			fBoneDist = GetVectorDistance(g_fClientEyePos[iClient], fBonePos, true);
			if (fLastDist != -1.0 && fBoneDist >= fLastDist)continue;
			
			fLastDist = fBoneDist;
			fAimPartPos = fBonePos;
		}
	}
	fAimPos = fAimPartPos;

	g_iSurvivorBot_MeleeBoneTarget[iClient] = iTarget;
	g_fSurvivorBot_MeleeBonePos[iClient] = fAimPartPos;
	g_fSurvivorBot_MeleeBoneUntil[iClient] = fNow + BOT_AIM_PART_CACHE_TIME;
}

// The upper bound discards a value left from the previous map.
bool IsCacheTimeValid(float fUntil, float fNow, float fInterval)
{
	float fLeft = (fUntil - fNow);
	return (fLeft > 0.0 && fLeft <= (fInterval * 2.0));
}

// Neither a sleeping witch nor the wanted item moves, so the answer is kept for a moment.
bool IsEntityVisibleCached(int iClient, int iEntity)
{
	float fNow = GetGameTime();
	if (iEntity != g_iSurvivorBot_SightEntity[iClient] || fNow >= g_fSurvivorBot_SightUntil[iClient])
	{
		g_iSurvivorBot_SightEntity[iClient] = iEntity;
		g_fSurvivorBot_SightUntil[iClient] = fNow + BOT_ENTITY_SIGHT_CACHE_TIME;
		g_bSurvivorBot_Sight[iClient] = IsVisibleEntity(iClient, iEntity);
	}
	return g_bSurvivorBot_Sight[iClient];
}

// Several checks of a tick ask the same thing about the same tank, and the answer holds for a few ticks.
bool IsTankVisibleCached(int iClient, int iTank)
{
	float fNow = GetGameTime();
	if (IsCacheTimeValid(g_fTankSight_Until[iClient][iTank], fNow, BOT_TANK_SIGHT_CACHE_TIME))
		return g_bTankSight_Visible[iClient][iTank];

	g_bTankSight_Visible[iClient][iTank] = IsVisibleEntity(iClient, iTank, MASK_SHOT_HULL);
	g_fTankSight_Until[iClient][iTank] = fNow + BOT_TANK_SIGHT_CACHE_TIME;
	return g_bTankSight_Visible[iClient][iTank];
}

// Reading a bone is expensive, and every bot wants the same bone of the same tank.
void GetTankAimPos(int iClient, int iTank, float fAimPos[3])
{
	if (IsWeaponSlotActive(iClient, 0) && SurvivorHasTier3Weapon(iClient) == 1)
	{
		GetEntityAbsOrigin(iTank, fAimPos);
		return;
	}

	int iTick = GetGameTickCount();
	int iBoneAge = (iTick - g_iTankAimBone_Tick[iTank]);
	if (iBoneAge < 0 || iBoneAge >= BOT_TANK_AIM_CACHE_TICKS)
	{
		g_iTankAimBone_Tick[iTank] = iTick;
		if (SDKCall(g_hLookupBone, iTank, "ValveBiped.Bip01_Pelvis") != -1)
			g_bTankAimBone_Valid[iTank] = LBI_GetBonePosition(iTank, "ValveBiped.Bip01_Spine2", g_fTankAimBone_Pos[iTank]);
		else
			g_bTankAimBone_Valid[iTank] = LBI_GetBonePosition(iTank, "bip_spine_2", g_fTankAimBone_Pos[iTank]);
	}

	float fNow = GetGameTime();
	if (fNow >= g_fSurvivorBot_TankAimSightUntil[iClient])
	{
		g_fSurvivorBot_TankAimSightUntil[iClient] = fNow + BOT_TANK_SIGHT_CACHE_TIME;
		g_bSurvivorBot_TankAimSight[iClient] = (g_bTankAimBone_Valid[iTank] && IsVisibleVector(iClient, g_fTankAimBone_Pos[iTank]));
	}

	if (g_bTankAimBone_Valid[iTank] && g_bSurvivorBot_TankAimSight[iClient])
		fAimPos = g_fTankAimBone_Pos[iTank];
	else
		GetEntityCenteroid(iTank, fAimPos);
}

// A few ticks of difference in the aim can't be seen.
void GetTargetAimPart(int iClient, int iTarget, float fAimPos[3])
{
	float fNow = GetGameTime();
	if (g_iSurvivorBot_AimPartTarget[iClient] == iTarget && IsCacheTimeValid(g_fSurvivorBot_AimPartUntil[iClient], fNow, BOT_AIM_PART_CACHE_TIME))
	{
		fAimPos = g_fSurvivorBot_AimPartPos[iClient];
		return;
	}

	FindTargetAimPart(iClient, iTarget, fAimPos);

	g_iSurvivorBot_AimPartTarget[iClient] = iTarget;
	g_fSurvivorBot_AimPartPos[iClient] = fAimPos;
	g_fSurvivorBot_AimPartUntil[iClient] = fNow + BOT_AIM_PART_CACHE_TIME;
}

void FindTargetAimPart(int iClient, int iTarget, float fAimPos[3])
{
	if (IsWeaponSlotActive(iClient, 0) && SurvivorHasTier3Weapon(iClient) == 1 && (!IsValidClient(iTarget) || L4D2_GetPlayerZombieClass(iTarget) != L4D2ZombieClass_Jockey))
	{
		GetEntityAbsOrigin(iTarget, fAimPos);
		return;
	}

	static char sAimBone[64];
	float fDist = GetEntityDistance(iClient, iTarget, true);
	bool bIsUsingOldSkeleton = (SDKCall(g_hLookupBone, iTarget, "ValveBiped.Bip01_Pelvis") != -1);
	if (IsWitch(iTarget) && fDist <= 65536.0 && IsWeaponSlotActive(iClient, 0) && SurvivorHasShotgun(iClient) || (L4D_IsPlayerIncapacitated(iClient) && fDist <= 147456.0 || L4D2_IsRealismMode() && fDist <= 262144.0) && (!IsValidClient(iTarget) || L4D2_GetPlayerZombieClass(iTarget) != L4D2ZombieClass_Tank))
	{
		sAimBone = (bIsUsingOldSkeleton ? "ValveBiped.Bip01_Head1" : "bip_head");
	}
	else
	{
		sAimBone = (bIsUsingOldSkeleton ? "ValveBiped.Bip01_Spine2" : "bip_spine_2");
	}

	float fAimPartPos[3]; 
	LBI_GetBonePosition(iTarget, sAimBone, fAimPartPos);

	if (!IsVisibleVector(iClient, fAimPartPos))
	{
		bool bVisibleOther = false;
		if (bIsUsingOldSkeleton)
		{
			for (int i = 0; i < sizeof(g_sBoneNames_Old); i++)
			{
				if (!LBI_GetBonePosition(iTarget, g_sBoneNames_Old[i], fAimPartPos))
				continue;

				if (IsVisibleVector(iClient, fAimPartPos))
				{
					bVisibleOther = true;
					break;
				}
			}
		}
		else
		{
			for (int i = 0; i < sizeof(g_sBoneNames_New); i++)
			{
				if (!LBI_GetBonePosition(iTarget, g_sBoneNames_New[i], fAimPartPos))
				continue;

				if (IsVisibleVector(iClient, fAimPartPos))
				{
					bVisibleOther = true;
					break;
				}
			}
		}

		// No bone passed, and an empty position would aim at the map origin.
		if (!bVisibleOther)
		{
			GetEntityCenteroid(iTarget, fAimPos);
			return;
		}
	}

	fAimPos = fAimPartPos;
}

bool CheckIfCanRescueImmobilizedFriend(int iClient)
{
	if (IsSurvivorBotBlindedByVomit(iClient))
		return false;

	if (!L4D_IsPlayerIncapacitated(iClient)) 
	{		
		if (g_iSurvivorBot_ThreatInfectedCount[iClient] >= GetCommonHitsUntilDown(iClient, 0.33))
			return false;

		if (IsSurvivorBusy(iClient))
			return false;
	}

	return true;
}

void BotLookAtPosition(int iClient, float fLookPos[3], float fLookDuration = 0.33)
{
	g_fSurvivorBot_LookPosition[iClient] = fLookPos;
	g_fSurvivorBot_LookPosition_Duration[iClient] = GetGameTime() + fLookDuration;
}

bool IsUsingSpecialAbility(int iClient)
{
	if (!IsSpecialInfected(iClient))
		return false;

	int iAbilityEntity = L4D_GetPlayerCustomAbility(iClient);
	if (iAbilityEntity == -1)return false;

	static char sProperty[16];
	switch(L4D2_GetPlayerZombieClass(iClient))
	{
		case L4D2ZombieClass_Boomer: 	sProperty = "m_isSpraying";
		case L4D2ZombieClass_Hunter: 	sProperty = "m_isLunging";
		case L4D2ZombieClass_Jockey: 	sProperty = "m_isLeaping";
		case L4D2ZombieClass_Charger: 	sProperty = "m_isCharging";
		case L4D2ZombieClass_Smoker: 	sProperty = "m_tongueState";
		default: 						return false;
	}

	if (!HasEntProp(iAbilityEntity, Prop_Send, sProperty))
		return false;

	return (GetEntProp(iAbilityEntity, Prop_Send, sProperty) > 0);
}

int CalculateGrenadeThrowInfectedCount()
{
	int iFreeSurvivors;
	for (int i = 1; i <= MaxClients; i++)
	{
		if (!IsClientSurvivor(i) || L4D_IsPlayerBoomerBiled(i) || L4D_IsPlayerIncapacitated(i) || L4D_IsPlayerPinned(i) || GetClientRealHealth(i) <= RoundFloat(g_iCvar_SurvivorLimpHealth * 0.8))
			continue;

		iFreeSurvivors++;
		if (IsWeaponSlotActive(i, 1) && SurvivorHasMeleeWeapon(i) == 2)iFreeSurvivors++;
	}

	float fCountScale = g_fCvar_GrenadeThrow_HordeSize;
	int iFinalCount = RoundFloat(iFreeSurvivors * fCountScale);
	if (iFinalCount < 1)iFinalCount = RoundFloat(fCountScale);
	if (L4D2_IsTankInPlay())iFinalCount = RoundFloat(iFinalCount * 0.66);
	return iFinalCount;
}

bool CheckCanThrowGrenade(int iClient, int iTarget, float fClientPos[3], float fThrowPos[3], bool bTeammateNearThrowArea, bool bIsThrowTargetTank)
{
	if (g_iSurvivorBot_ThreatInfectedCount[iClient] >= GetCommonHitsUntilDown(iClient, 0.33))
		return false;

	if (IsWeaponReloading(L4D_GetPlayerCurrentWeapon(iClient)))
		return false;

	if (IsSurvivorBusy(iClient, _, true, true))
		return false;

	int iGrenadeType = SurvivorHasGrenade(iClient);
	if (iGrenadeType == 0)return false;

	int iGrenadeBit = (iGrenadeType == 2 ? 1 : iGrenadeType == 3 ? 2 : 0);
	if (g_iCvar_GrenadeThrow_GrenadeTypes & (1 << iGrenadeBit) == 0)return false;

	if (iGrenadeType == 2) 
	{
		if (bTeammateNearThrowArea)
		{
			return false;
		}

		if (GetGameTime() < g_fSurvivorBot_Grenade_NextThrowTime_Molotov)
		{
			return false;
		}

		if ((fThrowPos[2] - fClientPos[2]) > 256.0)
		{
			return false;
		}
					
		if (IsEntityOnFire(iTarget))
		{
			return false;
		}

		if (GetVectorDistance(fClientPos, fThrowPos, true) <= BOT_GRENADE_CHECK_RADIUS_SQR)
		{
			return false;
		}

		if (IsFinaleEscapeVehicleArrived())
		{
			return false;
		}
	}
	else if (GetGameTime() < g_fSurvivorBot_Grenade_NextThrowTime)
	{
		return false;
	}

	if (GetPinnedSurvivorCount() != 0)
		return false;

	int iActiveGrenades = (GetSurvivorTeamActiveItemCount(L4D2WeaponId_PipeBomb) + GetSurvivorTeamActiveItemCount(L4D2WeaponId_Molotov) + GetSurvivorTeamActiveItemCount(L4D2WeaponId_Vomitjar));
	if (iActiveGrenades >= 1)
		return false;

	if (bIsThrowTargetTank == true)
	{
		if (iGrenadeType == 1)
			return false;

		if (iGrenadeType == 3)
		{
			if (GetGameTime() <= g_fInfectedBot_CoveredInVomitTime[iTarget])
				return false;

			if (GetInfectedCount(iTarget, g_fCvar_ChaseBileRange, 10, _, false) < 10)
				return false;
		}

		if (!IsTankVisibleCached(iClient, iTarget))
			return false;
	}
	else
	{
		if (iGrenadeType == 2)
			return false;

		int iThrowCount = CalculateGrenadeThrowInfectedCount();		
		if (g_iSurvivorBot_GrenadeInfectedCount[iClient] < iThrowCount)
		{
			return false;
		}

		int iChaseEnt = INVALID_ENT_REFERENCE;
		float fItRange = (g_fCvar_ChaseBileRange*g_fCvar_ChaseBileRange);
		while ((iChaseEnt = FindEntityByClassname(iChaseEnt, "info_goal_infected_chase")) != INVALID_ENT_REFERENCE)
		{
			if (GetEntityDistance(iChaseEnt, iTarget, true) > fItRange)continue;
			return false;
		}

		iChaseEnt = INVALID_ENT_REFERENCE;
		while ((iChaseEnt = FindEntityByClassname(iChaseEnt, "pipe_bomb_projectile")) != INVALID_ENT_REFERENCE)
		{
			if (GetEntityDistance(iChaseEnt, iTarget, true) > 1048576.0)continue;
			return false;
		}
	}

	if (iGrenadeType == 1)
	{
		for (int i = 1; i <= MaxClients; i++)
		{
			if (!IsClientSurvivor(i) || !L4D_IsPlayerBoomerBiled(i))continue;
			return false;
		}
	}

	return true;
}

bool CheckIsUnableToThrowGrenade(int iClient, int iTarget, float fClientPos[3], float fThrowPos[3], bool bTeammateNearThrowArea, bool bIsThrowTargetTank)
{
	int iActiveGrenades = (GetSurvivorTeamActiveItemCount(L4D2WeaponId_PipeBomb) + GetSurvivorTeamActiveItemCount(L4D2WeaponId_Molotov) + GetSurvivorTeamActiveItemCount(L4D2WeaponId_Vomitjar));
	if (iActiveGrenades > 1)
		return true;

	int iGrenadeType = SurvivorHasGrenade(iClient);
	if (iGrenadeType == 2)
	{
		if (bTeammateNearThrowArea)
			return true;

		if ((fThrowPos[2] - fClientPos[2]) > 256.0)
			return true;

		if (GetVectorDistance(fClientPos, fThrowPos, true) <= BOT_GRENADE_CHECK_RADIUS_SQR)
			return true;
					
		if (IsEntityOnFire(iTarget))
			return true;
	}

	if (bIsThrowTargetTank)
	{
		if (iGrenadeType == 1)
			return true;

		if (iGrenadeType == 3)
		{
			if (GetGameTime() <= g_fInfectedBot_CoveredInVomitTime[iTarget])
				return true;

			if (GetInfectedCount(iTarget, g_fCvar_ChaseBileRange, 10, _, false) < 10)
				return true;
		}

		if (!IsTankVisibleCached(iClient, iTarget))
			return true;
	}
	else
	{
		if (iGrenadeType == 2)
			return true;

		int iThrowCount = CalculateGrenadeThrowInfectedCount();
		if (g_iSurvivorBot_GrenadeInfectedCount[iClient] < RoundFloat(iThrowCount * 0.33))
		{
			return true;
		}

		int iChaseEnt = INVALID_ENT_REFERENCE;
		float fItRange = (g_fCvar_ChaseBileRange*g_fCvar_ChaseBileRange);
		while ((iChaseEnt = FindEntityByClassname(iChaseEnt, "info_goal_infected_chase")) != INVALID_ENT_REFERENCE)
		{
			if (GetEntityDistance(iChaseEnt, iTarget, true) > fItRange)continue;
			return true;
		}

		iChaseEnt = INVALID_ENT_REFERENCE;
		while ((iChaseEnt = FindEntityByClassname(iChaseEnt, "pipe_bomb_projectile")) != INVALID_ENT_REFERENCE)
		{
			if (GetEntityDistance(iChaseEnt, iTarget, true) > 1048576.0)continue;
			return true;
		}
	}

	if (iGrenadeType == 1)
	{
		for (int i = 1; i <= MaxClients; i++)
		{
			if (!IsClientSurvivor(i) || !L4D_IsPlayerBoomerBiled(i))continue;
			return true;
		}
	}

	return false;
}

void CalculateTrajectory(float fStartPos[3], float fEndPos[3], float fVelocity, float fGravityScale = 1.0, float fResult[3])
{
	MakeVectorFromPoints(fStartPos, fEndPos, fResult);
	fResult[2] = 0.0;

	float fPos_X = GetVectorLength(fResult);
	float fPos_Y = fEndPos[2] - fStartPos[2];

	float fGravity = (g_fCvar_ServerGravity * fGravityScale);

	float fSqrtCalc1 = (fVelocity * fVelocity * fVelocity * fVelocity);
	float fSqrtCalc2 = fGravity * ((fGravity * (fPos_X * fPos_X)) + (2.0 * fPos_Y * (fVelocity * fVelocity)));

	float fCalcSum = (fSqrtCalc1 - fSqrtCalc2);	
	if (fCalcSum < 0.0)fCalcSum = FloatAbs(fCalcSum);

	float fAngSqrt = SquareRoot(fCalcSum);
	float fAngPos = ArcTangent(((fVelocity * fVelocity) + fAngSqrt) / (fGravity * fPos_X));
	float fAngNeg = ArcTangent(((fVelocity * fVelocity) - fAngSqrt) / (fGravity * fPos_X));

	float fPitch = ((fAngPos > fAngNeg) ? fAngNeg : fAngPos);
	fResult[2] = (Tangent(fPitch) * fPos_X);

	NormalizeVector(fResult, fResult);
	ScaleVector(fResult, fVelocity);
}

float GetCommonInfectedDamage()
{
	switch(GetCurrentGameDifficulty())
	{
		case 1:	return 1.0;
		case 3:	return 5.0;
		case 4: return 20.0;
		default: return 2.0;
	}
}

int GetCommonHitsUntilDown(int iClient, float fScale = 1.0)
{
	int iHits = RoundToFloor((GetClientRealHealth(iClient) / GetCommonInfectedDamage()) * fScale);
	if (iHits < 1)iHits = 1;
	return iHits;
}

float GetClientMaxSpeed(int iClient)
{
	return (GetEntPropFloat(iClient, Prop_Send, "m_flMaxspeed") * GetEntPropFloat(iClient, Prop_Data, "m_flLaggedMovementValue"));
}

static const char g_sSemiAutoWeapons[][] = 
{
	"pistol",
	"pistol_magnum",
	"pumpshotgun",
	"shotgun_chrome",
	"autoshotgun",
	"shotgun_spas",
	"hunting_rifle",
	"sniper_military",
	"sniper_scout",
	"sniper_awp",
	"grenade_launcher",
	"pain_pills",
	"adrenaline",
	"pipe_bomb",
	"molotov",
	"vomitjar"
};
bool PressAttackButton(int iClient, int &buttons, float fFireRate = -1.0)
{
	if (g_bClient_IsFiringWeapon[iClient])
		return false;

	// Only a weapon with a fire rate sets this, and the checks below are expensive.
	if (GetGameTime() <= g_fSurvivorBot_NextPressAttackTime[iClient])
		return false;

	int iWeapon = L4D_GetPlayerCurrentWeapon(iClient);
	if (iWeapon == -1)return false;

	if (IsFakeClient(iClient) && (g_bCvar_BotsDontShoot || g_bSurvivorBot_PreventFire[iClient]))
		return false;

	static char sClassname[64];
	GetEdictClassname(iWeapon, sClassname, sizeof(sClassname));

	float fNextFireT = fFireRate;
	int bIsPistol = (strcmp(sClassname[7], "pistol") == 0 ? 1 : (strcmp(sClassname[7], "pistol_magnum") == 0 ? 2 : 0));
	if (fNextFireT <= 0.0 && (bIsPistol != 0 || GetWeaponTier(iWeapon) > 0))
	{
		float fCycleTime = GetWeaponCycleTime(iWeapon);
		float fAimPos[3]; GetClientAimPosition(iClient, fAimPos);

		float fClampDist = 2048.0;
		if (bIsPistol == 2)fClampDist *= 0.5;
		else if (GetEntProp(iWeapon, Prop_Send, "m_upgradeBitVec") & L4D2_WEPUPGFLAG_LASER)fClampDist *= 2.0;

		fNextFireT = (fCycleTime * (GetVectorDistance(g_fClientEyePos[iClient], fAimPos) / fClampDist));
	}

	if (fNextFireT < GetGameFrameTime())
	{
		for (int i = 0; i < sizeof(g_sSemiAutoWeapons); i++)
		{
			if (strcmp(sClassname[7], g_sSemiAutoWeapons[i]) == 0)
			{
				fNextFireT = GetGameFrameTime();
				break;
			}
		}
	}

	if (fNextFireT <= 0.0 || GetGameTime() > g_fSurvivorBot_NextPressAttackTime[iClient])
	{
		if (GetWeaponClip1(iWeapon) > 0)g_fSurvivorBot_BlockWeaponReloadTime[iClient] = GetGameTime() + 2.0;
		buttons |= IN_ATTACK;
		g_bClient_IsFiringWeapon[iClient] = true;
		g_fSurvivorBot_NextPressAttackTime[iClient] = GetGameTime() + fNextFireT;
		return true;
	}
	return false;
}

int GetWeaponAmmoType(int iWeapon)
{
	if (!HasEntProp(iWeapon, Prop_Send, "m_iPrimaryAmmoType"))return -1;
	return (GetEntProp(iWeapon, Prop_Send, "m_iPrimaryAmmoType"));
}

int GetClientPrimaryAmmo(int iClient)
{	
	int iPrimaryWeapon = GetClientWeaponInventory(iClient, 0);
	if (iPrimaryWeapon == -1)return -1;

	int iAmmoType = GetWeaponAmmoType(iPrimaryWeapon);
	if (iAmmoType == -1)return -1;

	return (GetEntProp(iClient, Prop_Send, "m_iAmmo", _, iAmmoType));
}

public Action L4D_OnVomitedUpon(int victim, int &attacker, bool &boomerExplosion)
{
	if (IsFakeClient(victim) && GetGameTime() >= g_fSurvivorBot_VomitBlindedTime[victim])
		g_fSurvivorBot_VomitBlindedTime[victim] = GetGameTime() + g_fCvar_BotsVomitBlindTime;

	return Plugin_Continue;
}

public Action L4D2_OnHitByVomitJar(int victim, int &attacker)
{
	float fInterval = 180.0;
	if (IsSpecialInfected(victim))
		fInterval = (IsFakeClient(victim) ? g_fCvar_BileCoverDuration_Bot : g_fCvar_BileCoverDuration_PZ);

	g_fInfectedBot_CoveredInVomitTime[victim] = GetGameTime() + fInterval;
	return Plugin_Continue;
}

bool L4D_IsPlayerBoomerBiled(int iClient)
{
	return (GetGameTime() <= GetEntPropFloat(iClient, Prop_Send, "m_itTimer", 1));
}

bool L4D2_IsUnderAdrenalineEffect(int iClient)
{
	return (!!GetEntProp(iClient, Prop_Send, "m_bAdrenalineActive"));
}
