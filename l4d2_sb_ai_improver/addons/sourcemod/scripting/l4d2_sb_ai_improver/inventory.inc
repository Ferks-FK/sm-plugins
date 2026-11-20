void SwitchWeaponSlot(int iClient, int iSlot)
{
	int iWeapon = GetClientWeaponInventory(iClient, iSlot);
	if (iWeapon == -1 || L4D_GetPlayerCurrentWeapon(iClient) == iWeapon)
		return;

	static char sWeaponName[64]; GetEdictClassname(iWeapon, sWeaponName, sizeof(sWeaponName));
	FakeClientCommand(iClient, "use %s", sWeaponName);
}

bool IsWeaponSlotActive(int iClient, int iSlot)
{
	return (GetClientWeaponInventory(iClient, iSlot) == L4D_GetPlayerCurrentWeapon(iClient));
}

// Checked many times per tick, and a weapon's class never changes.
int GetWeaponClassId(int iWeapon)
{
	bool bInRange = (0 < iWeapon <= MAXENTITIES);
	if (bInRange && g_iEntCache_WeaponClass[iWeapon] != WPNCLASS_UNKNOWN)
		return g_iEntCache_WeaponClass[iWeapon];

	static char sName[64]; GetEdictClassname(iWeapon, sName, sizeof(sName));

	int iClass = WPNCLASS_OTHER;
	if (strcmp(sName[7], "smg") == 0 || strcmp(sName[7], "smg_silenced") == 0 || strcmp(sName[7], "smg_mp5") == 0)iClass = WPNCLASS_SMG;
	else if (strcmp(sName[7], "rifle") == 0 || strcmp(sName[7], "rifle_ak47") == 0 || strcmp(sName[7], "rifle_desert") == 0 || strcmp(sName[7], "rifle_sg552") == 0)iClass = WPNCLASS_RIFLE;
	else if (strcmp(sName[7], "pumpshotgun") == 0 || strcmp(sName[7], "shotgun_chrome") == 0)iClass = WPNCLASS_SHOTGUN_T1;
	else if (strcmp(sName[7], "autoshotgun") == 0 || strcmp(sName[7], "shotgun_spas") == 0)iClass = WPNCLASS_SHOTGUN_T2;
	else if (strcmp(sName[7], "hunting_rifle") == 0 || strcmp(sName[7], "sniper_military") == 0)iClass = WPNCLASS_SNIPER_T1;
	else if (strcmp(sName[7], "sniper_awp") == 0 || strcmp(sName[7], "sniper_scout") == 0)iClass = WPNCLASS_SNIPER_T2;
	else if (strcmp(sName[7], "grenade_launcher") == 0)iClass = WPNCLASS_GRENADE_LAUNCHER;
	else if (strcmp(sName[7], "rifle_m60") == 0)iClass = WPNCLASS_M60;
	else if (strcmp(sName[7], "melee") == 0)iClass = WPNCLASS_MELEE;
	else if (strcmp(sName[7], "chainsaw") == 0)iClass = WPNCLASS_CHAINSAW;
	else if (strcmp(sName[7], "pistol") == 0)iClass = WPNCLASS_PISTOL;
	else if (strcmp(sName[7], "pistol_magnum") == 0)iClass = WPNCLASS_MAGNUM;
	else if (strcmp(sName[7], "pipe_bomb") == 0)iClass = WPNCLASS_PIPE_BOMB;
	else if (strcmp(sName[7], "molotov") == 0)iClass = WPNCLASS_MOLOTOV;
	else if (strcmp(sName[7], "vomitjar") == 0)iClass = WPNCLASS_VOMITJAR;
	else if (strcmp(sName[7], "first_aid_kit") == 0)iClass = WPNCLASS_FIRST_AID_KIT;
	else if (strcmp(sName[7], "defibrillator") == 0)iClass = WPNCLASS_DEFIBRILLATOR;
	else if (strncmp(sName[7], "upgradepack", 11) == 0)iClass = WPNCLASS_UPGRADE_PACK;

	if (bInRange)g_iEntCache_WeaponClass[iWeapon] = iClass;
	return iClass;
}

bool SurvivorHasSMG(int iClient)
{
	int iSlot = GetClientWeaponInventory(iClient, 0);
	if (iSlot == -1)return false;

	return (GetWeaponClassId(iSlot) == WPNCLASS_SMG);
}

bool SurvivorHasAssaultRifle(int iClient)
{
	int iSlot = GetClientWeaponInventory(iClient, 0);
	if (iSlot == -1)return false;

	return (GetWeaponClassId(iSlot) == WPNCLASS_RIFLE);
}

int SurvivorHasShotgun(int iClient)
{
	int iSlot = GetClientWeaponInventory(iClient, 0);
	if (iSlot == -1)return 0;

	int iClass = GetWeaponClassId(iSlot);
	return (iClass == WPNCLASS_SHOTGUN_T1 ? 1 : (iClass == WPNCLASS_SHOTGUN_T2 ? 2 : 0));
}

int SurvivorHasSniperRifle(int iClient)
{
	int iSlot = GetClientWeaponInventory(iClient, 0);
	if (iSlot == -1)return 0;

	int iClass = GetWeaponClassId(iSlot);
	return (iClass == WPNCLASS_SNIPER_T1 ? 1 : (iClass == WPNCLASS_SNIPER_T2 ? 2 : 0));
}

int SurvivorHasTier3Weapon(int iClient)
{
	int iSlot = GetClientWeaponInventory(iClient, 0);
	if (iSlot == -1)return 0;

	int iClass = GetWeaponClassId(iSlot);
	return (iClass == WPNCLASS_GRENADE_LAUNCHER ? 1 : (iClass == WPNCLASS_M60 ? 2 : 0));
}

int SurvivorHasGrenade(int iClient)
{
	int iSlot = GetClientWeaponInventory(iClient, 2);
	if (iSlot == -1)return 0;

	switch(GetWeaponClassId(iSlot))
	{
		case WPNCLASS_PIPE_BOMB: return 1;
		case WPNCLASS_MOLOTOV: return 2;
		case WPNCLASS_VOMITJAR: return 3;
		default: return 0;
	}
}

int SurvivorHasHealthKit(int iClient)
{
	int iSlot = GetClientWeaponInventory(iClient, 3);
	if (iSlot == -1)return false;

	switch(GetWeaponClassId(iSlot))
	{
		case WPNCLASS_FIRST_AID_KIT: return 1;
		case WPNCLASS_DEFIBRILLATOR: return 2;
		case WPNCLASS_UPGRADE_PACK: return 3;
		default: return 0;
	}
}

int SurvivorHasMeleeWeapon(int iClient)
{
	int iSlot = GetClientWeaponInventory(iClient, 1);
	if (iSlot == -1)return 0;

	int iClass = GetWeaponClassId(iSlot);
	return (iClass == WPNCLASS_MELEE ? 1 : (iClass == WPNCLASS_CHAINSAW ? 2 : 0));
}

int SurvivorHasPistol(int iClient)
{
	int iSlot = GetClientWeaponInventory(iClient, 1);
	if (iSlot == -1)return 0;

	int iClass = GetWeaponClassId(iSlot);
	if (iClass == WPNCLASS_PISTOL)
		return ((GetEntProp(iSlot, Prop_Send, "m_isDualWielding") != 0 || GetEntProp(iSlot, Prop_Send, "m_hasDualWeapons") != 0) ? 2 : 1);
	else if (iClass == WPNCLASS_MAGNUM)
		return 3;
	return 0;
}

int GetSurvivorTeamActiveItemCount(const L4D2WeaponId iWeaponID)
{
	int iCount, iWeaponSlot, iCurWeapon;
	for (int i = 1; i <= MaxClients; i++)
	{
		if (!IsClientSurvivor(i))continue;
		iCurWeapon = L4D_GetPlayerCurrentWeapon(i);

		for (int j = 0; j <= 5; j++)
		{
			iWeaponSlot = GetClientWeaponInventory(i, j);
			if (IsEntityExists(iWeaponSlot) && iCurWeapon == iWeaponSlot && L4D2_GetWeaponId(iWeaponSlot) == iWeaponID)
			{
				iCount++; 
				break;
			}
		}
	}
	return iCount;
}

int GetSurvivorTeamItemCount(const L4D2WeaponId iWeaponID)
{
	// Called dozens of times per item scan, so count everything once per tick.
	int iTick = GetGameTickCount();
	if (iTick != g_iTeamItemCount_Tick)
	{
		g_iTeamItemCount_Tick = iTick;
		for (int i = 0; i < sizeof(g_iTeamItemCount); i++)g_iTeamItemCount[i] = 0;

		int iWeaponSlot, iSlotID, iClientSlots[6];
		for (int i = 1; i <= MaxClients; i++)
		{
			if (!IsClientSurvivor(i))continue;

			for (int j = 0; j <= 5; j++)
			{
				iClientSlots[j] = -1;

				iWeaponSlot = GetClientWeaponInventory(i, j);
				if (!IsEntityExists(iWeaponSlot))continue;

				iSlotID = view_as<int>(L4D2_GetWeaponId(iWeaponSlot));
				if (iSlotID < 0 || iSlotID >= sizeof(g_iTeamItemCount))continue;
				iClientSlots[j] = iSlotID;

				// A survivor counts once per item type, even with the same type in two slots.
				bool bCounted = false;
				for (int k = 0; k < j; k++)
				{
					if (iClientSlots[k] == iSlotID)
					{
						bCounted = true;
						break;
					}
				}
				if (!bCounted)g_iTeamItemCount[iSlotID]++;
			}
		}
	}

	int iID = view_as<int>(iWeaponID);
	return ((iID < 0 || iID >= sizeof(g_iTeamItemCount)) ? 0 : g_iTeamItemCount[iID]);
}

bool IsWeaponReloading(int iWeapon, bool bIgnoreShotguns = true)
{
	if (!IsEntityExists(iWeapon) || !HasEntProp(iWeapon, Prop_Data, "m_bInReload"))
		return false;

	bool bInReload = !!GetEntProp(iWeapon, Prop_Data, "m_bInReload");
	if (bInReload && bIgnoreShotguns)
	{
		int iClass = GetWeaponClassId(iWeapon);
		return (iClass != WPNCLASS_SHOTGUN_T1 && iClass != WPNCLASS_SHOTGUN_T2);
	}
	return (bInReload);
}

stock float GetWeaponNextFireTime(int iWeapon)
{
	return (GetEntPropFloat(iWeapon, Prop_Send, "m_flNextPrimaryAttack"));
}

stock int GetWeaponClip1(int iWeapon) 
{
	return (GetEntProp(iWeapon, Prop_Send, "m_iClip1"));
}

stock int GetWeaponClipSize(int iWeapon)
{
	return (SDKCall(g_hGetMaxClip1, iWeapon));
}

stock int GetTeamPlayerCount(int iTeam, bool bOnlyAlive=false, bool bOnlyBots=false)
{
	int iCount;
	int iTeamCount = GetTeamClientCount(iTeam);
	for (int i = 1; i <= MaxClients; i++)
	{
		if (IsClientInGame(i) && (!bOnlyAlive || IsPlayerAlive(i)) && (!bOnlyBots || IsFakeClient(i)) && GetClientTeam(i) == iTeam)
		{
			iCount++;
			if (iCount >= iTeamCount)break;
		}
	}
	return iCount;
}

stock int IsFinaleEscapeVehicleArrived()
{
	return (L4D2_IsGenericCooperativeMode() && L4D_IsMissionFinalMap() && L4D2_GetCurrentFinaleStage() == 6);
}

stock int GetCurrentGameDifficulty()
{
	if (!L4D2_IsGenericCooperativeMode())return 2;
	switch(g_sCvar_GameDifficulty[0])
	{
		case 'E', 'e': return 1;
		case 'H', 'h': return 3;
		case 'I', 'i': return 4;
		default: return 2;
	}
}

stock int GetClientRealHealth(int iClient)
{
	return RoundFloat(GetClientHealth(iClient) + L4D_GetTempHealth(iClient));
}

bool IsSurvivorBotBlindedByVomit(int iClient)
{
	return (GetGameTime() < g_fSurvivorBot_VomitBlindedTime[iClient]);
}

stock bool IsEntityOnFire(int iEntity)
{
	return (GetEntityFlags(iEntity) & FL_ONFIRE) != 0;
}

stock bool IsValidClient(int iClient) 
{
	return (1 <= iClient <= MaxClients && IsClientInGame(iClient)); 
}

stock bool IsClientSurvivor(int iClient)
{
	return (IsValidClient(iClient) && GetClientTeam(iClient) == 2 && IsPlayerAlive(iClient));
}

stock void SetVectorToZero(float fVec[3])
{
	for (int i = 0; i < 3; i++)fVec[i] = 0.0;
}

stock bool GetEntityAbsOrigin(int iEntity, float fResult[3])
{
	if (!IsEntityExists(iEntity))return false;
	SDKCall(g_hCalcAbsolutePosition, iEntity);
	GetEntPropVector(iEntity, Prop_Data, "m_vecAbsOrigin", fResult);
	return (IsValidVector(fResult));
}

// Faster version for players, used on every tick.
void GetPlayerCenteroidFast(int iClient, const float fAbsOrigin[3], float fResult[3])
{
	if (g_iOffs_vecMins == -1 || g_iOffs_vecMaxs == -1)
	{
		GetEntityCenteroid(iClient, fResult);
		return;
	}

	float fMins[3], fMaxs[3];
	GetEntDataVector(iClient, g_iOffs_vecMins, fMins);
	GetEntDataVector(iClient, g_iOffs_vecMaxs, fMaxs);

	fResult[0] = fAbsOrigin[0] + (fMins[0] + fMaxs[0]) * 0.5;
	fResult[1] = fAbsOrigin[1] + (fMins[1] + fMaxs[1]) * 0.5;
	fResult[2] = fAbsOrigin[2] + (fMins[2] + fMaxs[2]) * 0.5;
}

stock bool GetEntityCenteroid(int iEntity, float fResult[3])
{
	static char sClass[64];
	GetEntityAbsOrigin(iEntity, fResult);

	if (!GetEntityNetClass(iEntity, sClass, sizeof(sClass)))
		return false;

	// The offsets only depend on the entity class, so the last ones are kept.
	static char sLastClass[64];
	static int iOffsMins = -1, iOffsMaxs = -1;
	if (strcmp(sClass, sLastClass) != 0)
	{
		strcopy(sLastClass, sizeof(sLastClass), sClass);
		iOffsMins = FindSendPropInfo(sClass, "m_vecMins");
		iOffsMaxs = FindSendPropInfo(sClass, "m_vecMaxs");
	}

	if (iOffsMins == -1 || iOffsMaxs == -1)
		return false;

	float fMins[3], fMaxs[3];
	GetEntDataVector(iEntity, iOffsMins, fMins);
	GetEntDataVector(iEntity, iOffsMaxs, fMaxs);

	fResult[0] += (fMins[0] + fMaxs[0]) * 0.5;
	fResult[1] += (fMins[1] + fMaxs[1]) * 0.5;
	fResult[2] += (fMins[2] + fMaxs[2]) * 0.5;

	return true;
}
