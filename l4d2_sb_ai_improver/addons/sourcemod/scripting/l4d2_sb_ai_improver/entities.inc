int GetEntityOwner(int iEntity)
{
	int iOwner = GetEntPropEnt(iEntity, Prop_Data, "m_hOwnerEntity");
	if (!IsClientSurvivor(iOwner) || L4D_GetPlayerCurrentWeapon(iOwner) == iEntity)
		return iOwner;

	for (int i = 0; i <= 5; i++)
	{
		if (GetClientWeaponInventory(iOwner, i) != iEntity)continue;
		return iOwner;
	}
	return -1;
}

public void OnEntityCreated(int iEntity, const char[] sClassname)
{
	if (iEntity <= 0 || iEntity > MAXENTITIES)
		return;
		
	g_iItem_Used[iEntity] = 0;
	g_iEntCache_IsDoor[iEntity] = 0;
	g_bEntCache_IsWeapon[iEntity] = false;
	g_iEntCache_WeaponTier[iEntity] = 0;
	g_iEntCache_WeaponClass[iEntity] = WPNCLASS_UNKNOWN;
	CheckEntityForStuff(iEntity, sClassname);
}

void CheckEntityForStuff(int iEntity, const char[] sClassname)
{
	if (g_hWitchList && strcmp(sClassname, "witch") == 0)
	{
		int iWitchRef;
		for (int i = 0; i < g_hWitchList.Length; i++)
		{
			iWitchRef = EntRefToEntIndex(g_hWitchList.Get(i));
			if (iWitchRef == INVALID_ENT_REFERENCE || !IsEntityExists(iWitchRef))
			{
				g_hWitchList.Erase(i--);
				continue;
			}
			if (iWitchRef == iEntity)return;
		}

		int iIndex = g_hWitchList.Push(EntIndexToEntRef(iEntity));
		g_hWitchList.Set(iIndex, 0, 1); return;
	}

	if (strcmp(sClassname, "weapon_ammo_spawn") == 0)
	{
		PushEntityIntoArrayList(g_hAmmopileList, iEntity);
		return;
	}
	if (strcmp(sClassname, "upgrade_laser_sight") == 0)
	{
		PushEntityIntoArrayList(g_hLaserSightList, iEntity);
		return;
	}
	if (strcmp(sClassname, "upgrade_ammo_explosive") == 0 || strcmp(sClassname, "upgrade_ammo_incendiary") == 0)
	{
		PushEntityIntoArrayList(g_hDeployedAmmoPacks, iEntity);
		return;
	}
	
	if (strcmp(sClassname, "pipe_bomb_projectile") == 0 || strcmp(sClassname, "vomitjar_projectile") == 0)
	{
		g_fSurvivorBot_Grenade_NextThrowTime = GetGameTime() + GetRandomFloat(g_fCvar_GrenadeThrow_NextThrowTime1, g_fCvar_GrenadeThrow_NextThrowTime2);
		return;
	}
	if (strcmp(sClassname, "molotov_projectile") == 0)
	{
		g_fSurvivorBot_Grenade_NextThrowTime_Molotov = GetGameTime() + GetRandomFloat(g_fCvar_GrenadeThrow_NextThrowTime1, g_fCvar_GrenadeThrow_NextThrowTime2);
		return;
	}

	L4D2WeaponId iWeaponID = L4D2_GetWeaponId(iEntity);
	if (iWeaponID == L4D2WeaponId_None)return;

	if (iWeaponID == L4D2WeaponId_Pistol || iWeaponID == L4D2WeaponId_PistolMagnum)
		PushEntityIntoArrayList(g_hPistolList, iEntity);
	else if (iWeaponID == L4D2WeaponId_Melee || iWeaponID == L4D2WeaponId_Chainsaw)
		PushEntityIntoArrayList(g_hMeleeList, iEntity);
	else if (iWeaponID == L4D2WeaponId_Smg || iWeaponID == L4D2WeaponId_SmgSilenced || iWeaponID == L4D2WeaponId_SmgMP5)
		PushEntityIntoArrayList(g_hSMGList, iEntity);
	else if (iWeaponID == L4D2WeaponId_Pumpshotgun || iWeaponID == L4D2WeaponId_ShotgunChrome)
		PushEntityIntoArrayList(g_hShotgunT1List, iEntity);
	else if (iWeaponID == L4D2WeaponId_Rifle || iWeaponID == L4D2WeaponId_RifleAK47 || iWeaponID == L4D2WeaponId_RifleDesert || iWeaponID == L4D2WeaponId_RifleSG552)
		PushEntityIntoArrayList(g_hAssaultRifleList, iEntity);
	else if (iWeaponID == L4D2WeaponId_Autoshotgun || iWeaponID == L4D2WeaponId_ShotgunSpas)
		PushEntityIntoArrayList(g_hShotgunT2List, iEntity);
	else if (iWeaponID == L4D2WeaponId_HuntingRifle || iWeaponID == L4D2WeaponId_SniperMilitary || iWeaponID == L4D2WeaponId_SniperScout || iWeaponID == L4D2WeaponId_SniperAWP)
		PushEntityIntoArrayList(g_hSniperRifleList, iEntity);
	else if (iWeaponID == L4D2WeaponId_PipeBomb || iWeaponID == L4D2WeaponId_Molotov || iWeaponID == L4D2WeaponId_Vomitjar)
		PushEntityIntoArrayList(g_hGrenadeList, iEntity);
	else if (iWeaponID == L4D2WeaponId_RifleM60 || iWeaponID == L4D2WeaponId_GrenadeLauncher)
		PushEntityIntoArrayList(g_hTier3List, iEntity);
	else if (iWeaponID == L4D2WeaponId_FragAmmo || iWeaponID == L4D2WeaponId_IncendiaryAmmo)
		PushEntityIntoArrayList(g_hUpgradePackList, iEntity);
	else if (iWeaponID == L4D2WeaponId_FirstAidKit)
		PushEntityIntoArrayList(g_hFirstAidKitList, iEntity);
	else if (iWeaponID == L4D2WeaponId_Defibrillator)
		PushEntityIntoArrayList(g_hDefibrillatorList, iEntity);
	else if (iWeaponID == L4D2WeaponId_PainPills)
		PushEntityIntoArrayList(g_hPainPillsList, iEntity);
	else if (iWeaponID == L4D2WeaponId_Adrenaline)
		PushEntityIntoArrayList(g_hAdrenalineList, iEntity);

	if (iWeaponID == L4D2WeaponId_Chainsaw)
	{
		g_iWeapon_Clip1[iEntity] = g_iCvar_MaxAmmo_Chainsaw;
		g_iWeapon_MaxAmmo[iEntity] = g_iCvar_MaxAmmo_Chainsaw;
		g_iWeapon_AmmoLeft[iEntity] = g_iCvar_MaxAmmo_Chainsaw;
	}
	else if (iWeaponID != L4D2WeaponId_RifleM60 && iWeaponID != L4D2WeaponId_GrenadeLauncher)
	{
		static char sWeaponName[64];
		if (L4D2_GetWeaponNameByWeaponId(iWeaponID, sWeaponName, sizeof(sWeaponName)) && L4D2_GetIntWeaponAttribute(sWeaponName, L4D2IWA_Tier) != 0)
		{
			g_iWeapon_Clip1[iEntity] = L4D2_GetIntWeaponAttribute(sWeaponName, L4D2IWA_ClipSize);
			g_iWeapon_MaxAmmo[iEntity] = GetWeaponMaxAmmo(iEntity, iWeaponID);
			g_iWeapon_AmmoLeft[iEntity] = g_iWeapon_MaxAmmo[iEntity];
		}
	}

}

bool ShouldUseFlowDistance()
{
	if (!L4D_IsSurvivalMode() && !L4D2_IsScavengeMode())
	{
		int iFinStage = L4D2_GetCurrentFinaleStage();
		return (iFinStage == 18 || iFinStage == 0);
	}

	return false;
}

void PushEntityIntoArrayList(ArrayList hArrayList, int iEntity)
{
	if (!hArrayList)return;
	int iEntRef = EntIndexToEntRef(iEntity);
	int iArrayEnt = hArrayList.FindValue(iEntRef);
	if (iArrayEnt == -1)hArrayList.Push(iEntRef);
	if (0 < iEntity <= MAXENTITIES)g_bEntityInItemList[iEntity] = true;
}

public void OnEntityDestroyed(int iEntity)
{
	if (iEntity <= 0 || iEntity > MAXENTITIES) 
		return;
		
	g_iItem_Used[iEntity] = 0;
	g_iEntCache_IsDoor[iEntity] = 0;
	g_bEntCache_IsWeapon[iEntity] = false;
	g_iEntCache_WeaponTier[iEntity] = 0;
	g_iEntCache_WeaponClass[iEntity] = WPNCLASS_UNKNOWN;
	if (g_iInfectedSnap_Slot[iEntity] != 0)RemoveFromInfectedSnapshot(iEntity);

	if (g_bEntityInItemList[iEntity])
	{
		g_bEntityInItemList[iEntity] = false;

		CheckArrayListForEntityRemoval(g_hMeleeList, iEntity);
		CheckArrayListForEntityRemoval(g_hPistolList, iEntity);
		CheckArrayListForEntityRemoval(g_hSMGList, iEntity);
		CheckArrayListForEntityRemoval(g_hShotgunT1List, iEntity);
		CheckArrayListForEntityRemoval(g_hShotgunT2List, iEntity);
		CheckArrayListForEntityRemoval(g_hAssaultRifleList, iEntity);
		CheckArrayListForEntityRemoval(g_hSniperRifleList, iEntity);
		CheckArrayListForEntityRemoval(g_hTier3List, iEntity);
		CheckArrayListForEntityRemoval(g_hFirstAidKitList, iEntity);
		CheckArrayListForEntityRemoval(g_hDefibrillatorList, iEntity);
		CheckArrayListForEntityRemoval(g_hPainPillsList, iEntity);
		CheckArrayListForEntityRemoval(g_hAdrenalineList, iEntity);
		CheckArrayListForEntityRemoval(g_hGrenadeList, iEntity);
		CheckArrayListForEntityRemoval(g_hUpgradePackList, iEntity);

		CheckArrayListForEntityRemoval(g_hAmmopileList, iEntity);
		CheckArrayListForEntityRemoval(g_hLaserSightList, iEntity);
		CheckArrayListForEntityRemoval(g_hDeployedAmmoPacks, iEntity);
	}

	if (g_bEntityHasVisionMemory[iEntity])
	{
		g_bEntityHasVisionMemory[iEntity] = false;

		for (int i = 1; i <= MaxClients; i++)
		{
			g_iSurvivorBot_VisionMemory_State[i][iEntity] = g_iSurvivorBot_VisionMemory_State_FOV[i][iEntity] = 0;
			g_fSurvivorBot_VisionMemory_Time[i][iEntity] = g_fSurvivorBot_VisionMemory_Time_FOV[i][iEntity] = 0.0;
		}
	}
}

void CheckArrayListForEntityRemoval(ArrayList hArrayList, int iEntity)
{
	if (!hArrayList)return;
	int iArrayEnt = hArrayList.FindValue(EntIndexToEntRef(iEntity));
	if (iArrayEnt != -1)hArrayList.Erase(iArrayEnt);
}

public void OnMapStart()
{
	g_bMapStarted = true;
	g_fHasAliveSurvivorBots_NextCheck = 0.0;
	GetCurrentMap(g_sCurrentMapName, sizeof(g_sCurrentMapName));
	for (int i = 1; i <= MaxClients; i++)g_fClient_ThinkFunctionDelay[i] = GetGameTime() + (g_bLateLoad ? 1.0 : 10.0);
	CreateEntityArrayLists();

	int iEntity = INVALID_ENT_REFERENCE;
	static char sEntClassname[64];
	while ((iEntity = FindEntityByClassname(iEntity, "*")) != INVALID_ENT_REFERENCE)
	{
		if (!IsEntityExists(iEntity))continue;
		GetEntityClassname(iEntity, sEntClassname, sizeof(sEntClassname));
		CheckEntityForStuff(iEntity, sEntClassname);
	}
}

public void OnMapEnd()
{
	g_bMapStarted = false;
	g_bCutsceneIsPlaying = false;
	g_bTankRoute_AreasFound = false;
	g_sCurrentMapName[0] = 0;

	// These hold entities of the map that is ending.
	for (int i = 0; i < g_iInfectedSnap_Count; i++)
		g_iInfectedSnap_Slot[g_iInfectedSnap_Entity[i]] = 0;

	g_iInfectedSnap_Count = 0;
	g_fInfectedSnap_NextTime = 0.0;
	g_iEntityScan_RockCount = g_iEntityScan_PropCount = g_iEntityScan_DeathModelCount = 0;
	g_fEntityScan_NextFastTime = g_fEntityScan_NextSlowTime = 0.0;

	ClearEntityArrayLists();

	// Nav area pointers die with the map, and OnPlayerRunCmd may not refresh a client's for a long time.
	for (int i = 1; i <= MaxClients; i++)
	{
		g_bSurvivorBot_MovePos_AreasCached[i] = false;
		g_iClientNavArea[i] = 0;
	}
}

void CreateEntityArrayLists()
{
	// Runs on every map start: reuse the lists instead of leaking the previous handles.
	if (g_hMeleeList)
	{
		ClearEntityArrayLists();
		return;
	}

	g_hMeleeList 			= new ArrayList();
	g_hPistolList 			= new ArrayList();
	g_hSMGList 				= new ArrayList();
	g_hShotgunT1List 		= new ArrayList();
	g_hShotgunT2List 		= new ArrayList();
	g_hAssaultRifleList 	= new ArrayList();
	g_hSniperRifleList 		= new ArrayList();
	g_hTier3List 			= new ArrayList();
	g_hAmmopileList 		= new ArrayList();
	g_hUpgradePackList 		= new ArrayList();
	g_hLaserSightList 		= new ArrayList();
	g_hFirstAidKitList 		= new ArrayList();
	g_hDefibrillatorList 	= new ArrayList();
	g_hPainPillsList 		= new ArrayList();
	g_hAdrenalineList 		= new ArrayList();
	g_hGrenadeList 			= new ArrayList();
	g_hDeployedAmmoPacks 	= new ArrayList();
	g_hWitchList 			= new ArrayList(2);
}

void ClearEntityArrayLists()
{
	g_hMeleeList.Clear();
	g_hPistolList.Clear();
	g_hSMGList.Clear();
	g_hShotgunT1List.Clear();
	g_hShotgunT2List.Clear();
	g_hAssaultRifleList.Clear();
	g_hSniperRifleList.Clear();
	g_hTier3List.Clear();
	g_hAmmopileList.Clear();
	g_hUpgradePackList.Clear();
	g_hLaserSightList.Clear();
	g_hFirstAidKitList.Clear();
	g_hDefibrillatorList.Clear();
	g_hPainPillsList.Clear();
	g_hAdrenalineList.Clear();
	g_hGrenadeList.Clear();
	g_hDeployedAmmoPacks.Clear();
	g_hWitchList.Clear();
}

void GetEntityModelname(int iEntity, char[] sModelName, int iMaxLength)
{
	GetEntPropString(iEntity, Prop_Data, "m_ModelName", sModelName, iMaxLength);
}
