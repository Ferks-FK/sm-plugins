public Action L4D2_OnFindScavengeItem(int iClient, int &iItem)
{
	if (!IsEntityExists(iItem))
		return Plugin_Continue;

	char sItemClass[64]; 
	GetWeaponClassname(iItem, sItemClass, sizeof(sItemClass));

	int iPrimarySlot = GetClientWeaponInventory(iClient, 0);
	if (IsEntityExists(iPrimarySlot))
	{
		int iWpnTier = GetWeaponTier(iPrimarySlot);

		int iBotPreference = GetSurvivorBotWeaponPreference(iClient);
		if (iBotPreference != 0)
		{
			if ((iWpnTier == 2 || iWpnTier == 1 && iBotPreference == L4D_WEAPON_PREFERENCE_SMG) && WeaponHasEnoughAmmoLeft(iPrimarySlot))
			{
				if (iBotPreference != L4D_WEAPON_PREFERENCE_ASSAULTRIFLE && (strcmp(sItemClass[7], "rifle") == 0 || strcmp(sItemClass[7], "rifle_ak47") == 0 || strcmp(sItemClass[7], "rifle_desert") == 0 || strcmp(sItemClass[7], "rifle_sg552") == 0))
					return Plugin_Handled;
				if (iBotPreference != L4D_WEAPON_PREFERENCE_SHOTGUN && (strcmp(sItemClass[7], "autoshotgun") == 0 || strcmp(sItemClass[7], "shotgun_spas") == 0))
					return Plugin_Handled;
				if (iBotPreference != L4D_WEAPON_PREFERENCE_SNIPERRIFLE && (strcmp(sItemClass[7], "sniper_military") == 0 || strcmp(sItemClass[7], "sniper_awp") == 0 || strcmp(sItemClass[7], "sniper_scout") == 0 || strcmp(sItemClass[7], "hunting_rifle") == 0))
					return Plugin_Handled;
			}
		}

		int iItemTier = GetWeaponTier(iItem);
		if (iWpnTier == 3) 
		{
			if (strcmp(sItemClass[7], "ammo_spawn") == 0)
				return Plugin_Handled;

			if (iItemTier == 1 || iItemTier == 2) 
			{
				if (SurvivorHasTier3Weapon(iClient) == 2)
				{
					if (GetWeaponClip1(iPrimarySlot) > RoundFloat(GetWeaponClipSize(iPrimarySlot) * 0.2) && GetSurvivorTeamItemCount(L4D2WeaponId_RifleM60) <= g_iCvar_MaxWeaponTier3_M60)
					{
						return Plugin_Handled;
					}
				}
				else if (GetClientPrimaryAmmo(iClient) > RoundFloat(GetWeaponMaxAmmo(iPrimarySlot) * 0.2) && GetSurvivorTeamItemCount(L4D2WeaponId_GrenadeLauncher) <= g_iCvar_MaxWeaponTier3_GLauncher)
				{
					return Plugin_Handled;
				}
			}
		}
		else if (iItemTier != 0 && GetClientPrimaryAmmo(iClient) < GetWeaponMaxAmmo(iPrimarySlot))
		{
			int iAmmoPileItem = GetItemFromArrayList(g_hAmmopileList, iClient, 1024.0, _, _, _, false);
			if (iAmmoPileItem != -1)return Plugin_Handled;
		}
	}

	int iSecondarySlot = GetClientWeaponInventory(iClient, 1);
	if (IsEntityExists(iSecondarySlot))
	{
		if (g_bCvar_BotWeaponPreference_ForceMagnum && SurvivorHasPistol(iClient) == 3 && strcmp(sItemClass[7], "pistol") == 0)
			return Plugin_Handled;

		if (strcmp(sItemClass[7], "melee") == 0 && (!SurvivorHasMeleeWeapon(iClient) && ((GetSurvivorTeamItemCount(L4D2WeaponId_Chainsaw) + GetSurvivorTeamItemCount(L4D2WeaponId_Melee)) >= g_iCvar_MaxMeleeSurvivors) || SurvivorHasMeleeWeapon(iClient) == 2 && GetSurvivorTeamItemCount(L4D2WeaponId_Chainsaw) <= g_iCvar_ImprovedMelee_ChainsawLimit)) 
			return Plugin_Handled;

		if ((strcmp(sItemClass[7], "pistol") == 0 || strcmp(sItemClass[7], "pistol_magnum") == 0) && SurvivorHasMeleeWeapon(iClient) == 2 && GetSurvivorTeamItemCount(L4D2WeaponId_Chainsaw) <= g_iCvar_ImprovedMelee_ChainsawLimit && g_iWeapon_Clip1[iSecondarySlot] > RoundFloat(GetWeaponMaxAmmo(iSecondarySlot) * 0.25) && GetSurvivorTeamItemCount(L4D2WeaponId_Melee) <= g_iCvar_MaxMeleeSurvivors)
			return Plugin_Handled;
	}

	if (IsWeaponSlotActive(iClient, 3) && (strcmp(sItemClass[7], "first_aid_kit") == 0 || strcmp(sItemClass[7], "defibrillator") == 0) && SurvivorHasHealthKit(iClient) == 3)
		return Plugin_Handled;

	if (strcmp(sItemClass[7], "first_aid_kit") == 0 && IsEntityExists(g_iSurvivorBot_DefibTarget[iClient]) && SurvivorHasHealthKit(iClient) == 2)
		return Plugin_Handled;

	return Plugin_Continue;
}

bool WeaponHasEnoughAmmoLeft(int iWeapon)
{
	return (g_iWeapon_MaxAmmo[iWeapon] > 0 && (g_iWeapon_AmmoLeft[iWeapon] + g_iWeapon_Clip1[iWeapon]) >= RoundFloat(g_iWeapon_MaxAmmo[iWeapon] * 0.33));
}

bool IsEntityWeapon(int iEntity, bool bNoSpawn = false)
{
	if (!IsEntityExists(iEntity))
		return false;

	char sEntClass[64]; GetWeaponClassname(iEntity, sEntClass, sizeof(sEntClass));
	if (strcmp(sEntClass, "predicted_viewmodel") == 0 || bNoSpawn && strcmp(sEntClass, "weapon_spawn") == 0)
		return false;

	ReplaceString(sEntClass, sizeof(sEntClass), "_spawn", "", false);
	return (L4D2_IsValidWeaponName(sEntClass));
}

// Asked for every survivor every tick. Only a positive answer is kept, so a wrong "not a weapon" can't stick.
bool IsEntityWeaponCached(int iEntity)
{
	bool bInRange = (0 < iEntity <= MAXENTITIES);
	if (bInRange && g_bEntCache_IsWeapon[iEntity])
		return true;

	bool bIsWeapon = IsEntityWeapon(iEntity, true);
	if (bInRange && bIsWeapon)g_bEntCache_IsWeapon[iEntity] = true;
	return bIsWeapon;
}

int CheckForItemsToScavenge(int iClient)
{
	int iItem = -1;
	int iItemBits = g_iCvar_ItemScavenge_Items;

	int iArrayItem;
	ArrayList hItemList = new ArrayList();

	int iPrimarySlot = GetClientWeaponInventory(iClient, 0);
	int iTier3Primary = SurvivorHasTier3Weapon(iClient);
	int iWpnPreference = GetSurvivorBotWeaponPreference(iClient);

	if (iItemBits & (1 << 13) != 0 && iPrimarySlot == -1)
	{
		iArrayItem = GetItemFromArrayList(g_hAssaultRifleList, iClient);
		if (iArrayItem != -1)hItemList.Push(iArrayItem);

		iArrayItem = GetItemFromArrayList(g_hShotgunT2List, iClient);
		if (iArrayItem != -1)hItemList.Push(iArrayItem);

		iArrayItem = GetItemFromArrayList(g_hSniperRifleList, iClient);
		if (iArrayItem != -1)hItemList.Push(iArrayItem);

		iArrayItem = GetItemFromArrayList(g_hShotgunT1List, iClient);
		if (iArrayItem != -1)hItemList.Push(iArrayItem);

		iArrayItem = GetItemFromArrayList(g_hSMGList, iClient);
		if (iArrayItem != -1)hItemList.Push(iArrayItem);
	}

	if (iWpnPreference != 0 && iWpnPreference != L4D_WEAPON_PREFERENCE_SECONDARY && iTier3Primary == 0)
	{
		ArrayList hWeaponList;
		bool bHasWep = false;

		switch(iWpnPreference)
		{
			case L4D_WEAPON_PREFERENCE_ASSAULTRIFLE:
			{
				hWeaponList = g_hAssaultRifleList;
				bHasWep = SurvivorHasAssaultRifle(iClient);
			}
			case L4D_WEAPON_PREFERENCE_SHOTGUN:
			{
				hWeaponList = g_hShotgunT2List;
				bHasWep = SurvivorHasShotgun(iClient) > 0;
			}
			case L4D_WEAPON_PREFERENCE_SNIPERRIFLE:
			{
				hWeaponList = g_hSniperRifleList;
				bHasWep = SurvivorHasSniperRifle(iClient) > 0;
			}
			case L4D_WEAPON_PREFERENCE_SMG:
			{
				hWeaponList = g_hSMGList;
				bHasWep = SurvivorHasSMG(iClient);
			}
		}

		if (!bHasWep && iItemBits & (1 << 13) != 0 || GetWeaponTier(iPrimarySlot) == 1 && iWpnPreference != L4D_WEAPON_PREFERENCE_SMG)
		{
			iArrayItem = GetItemFromArrayList(hWeaponList, iClient);
			if (iArrayItem != -1 && (IsWeaponNearAmmoPile(iArrayItem, iClient) || WeaponHasEnoughAmmoLeft(iArrayItem)))
			{
				hItemList.Push(iArrayItem);
			}
		}
	}

	if (iWpnPreference != L4D_WEAPON_PREFERENCE_SECONDARY)
	{
		if (iTier3Primary != 1 && GetSurvivorTeamItemCount(L4D2WeaponId_GrenadeLauncher) < g_iCvar_MaxWeaponTier3_GLauncher)
		{
			iArrayItem = GetItemFromArrayList(g_hTier3List, iClient, _, "weapon_grenade_launcher");
			if (iArrayItem != -1)
			{
				hItemList.Push(iArrayItem);
			}
		}

		if (iTier3Primary != 2 && GetSurvivorTeamItemCount(L4D2WeaponId_RifleM60) < g_iCvar_MaxWeaponTier3_M60)
		{
			iArrayItem = GetItemFromArrayList(g_hTier3List, iClient, _, "weapon_rifle_m60");
			if (iArrayItem != -1)
			{
				hItemList.Push(iArrayItem);
			}
		}
	}

	if (iItemBits & (1 << 4) != 0 && GetClientWeaponInventory(iClient, 3) == -1 || IsEntityExists(g_iSurvivorBot_DefibTarget[iClient]) && SurvivorHasHealthKit(iClient) != 2)
	{
		iArrayItem = GetItemFromArrayList(g_hDefibrillatorList, iClient);
		if (iArrayItem != -1)hItemList.Push(iArrayItem);
	}

	if (GetClientWeaponInventory(iClient, 3) == -1)
	{
		iArrayItem = (iItemBits & (1 << 3) == 0 ? -1 : GetItemFromArrayList(g_hFirstAidKitList, iClient));
		if (iArrayItem != -1)hItemList.Push(iArrayItem);

		iArrayItem = (iItemBits & (1 << 5) == 0 ? -1 : GetItemFromArrayList(g_hUpgradePackList, iClient));
		if (iArrayItem != -1)hItemList.Push(iArrayItem);
	}

	if (GetClientWeaponInventory(iClient, 4) == -1)
	{
		iArrayItem = (iItemBits & (1 << 6) == 0 ? -1 : GetItemFromArrayList(g_hPainPillsList, iClient));
		if (iArrayItem != -1)hItemList.Push(iArrayItem);

		iArrayItem = (iItemBits & (1 << 7) == 0 ? -1 : GetItemFromArrayList(g_hAdrenalineList, iClient));
		if (iArrayItem != -1)hItemList.Push(iArrayItem);
	}

	int iGrenadeSlot = GetClientWeaponInventory(iClient, 2);
	if (iGrenadeSlot == -1)
	{
		iArrayItem = ((iItemBits & (1 << 0) != 0) ? GetItemFromArrayList(g_hGrenadeList, iClient, _, "weapon_pipe_bomb") : -1);
		if (iArrayItem != -1)hItemList.Push(iArrayItem);

		iArrayItem = ((iItemBits & (1 << 1) != 0) ? GetItemFromArrayList(g_hGrenadeList, iClient, _, "weapon_molotov") : -1);
		if (iArrayItem != -1)hItemList.Push(iArrayItem);

		iArrayItem = ((iItemBits & (1 << 2) != 0) ? GetItemFromArrayList(g_hGrenadeList, iClient, _, "weapon_vomitjar") : -1);
		if (iArrayItem != -1)hItemList.Push(iArrayItem);
	}
	else if (g_bCvar_SwapSameTypeGrenades)
	{	
		static char sGrenadeSlot[64]; 
		GetEdictClassname(iGrenadeSlot, sGrenadeSlot, sizeof(sGrenadeSlot));

		static char sAvoidGrenade[64];
		FormatEx(sAvoidGrenade, sizeof(sAvoidGrenade), "!!%s", sGrenadeSlot);

		int iGrenadeTypeLimit = RoundFloat((GetSurvivorTeamItemCount(L4D2WeaponId_PipeBomb) + GetSurvivorTeamItemCount(L4D2WeaponId_Molotov) + GetSurvivorTeamItemCount(L4D2WeaponId_Vomitjar)) * 0.55);
		if (iGrenadeTypeLimit < 1)iGrenadeTypeLimit = 1;

		if (GetSurvivorTeamItemCount(L4D2_GetWeaponId(iGrenadeSlot)) > iGrenadeTypeLimit)
		{
			iArrayItem = GetItemFromArrayList(g_hGrenadeList, iClient, _, sAvoidGrenade);
			if (iArrayItem != -1)hItemList.Push(iArrayItem);
		}
	}

	if (iPrimarySlot != -1)
	{
		static char sPrimarySlot[64]; 
		GetEdictClassname(iPrimarySlot, sPrimarySlot, sizeof(sPrimarySlot));

		if (iTier3Primary == 0 && iItemBits & (1 << 10) != 0)
		{
			int iPrimaryMaxAmmo = GetWeaponMaxAmmo(iPrimarySlot);

			int iMinAmmo = iPrimaryMaxAmmo;
			if (!L4D_IsInFirstCheckpoint(iClient))
				iMinAmmo = RoundFloat(iMinAmmo * ((!LBI_IsSurvivorInCombat(iClient) && !L4D_HasVisibleThreats(iClient)) ? 0.75 : 0.5));

			if (GetClientPrimaryAmmo(iClient) < iMinAmmo)
			{
				iArrayItem = GetItemFromArrayList(g_hAmmopileList, iClient);
				if (iArrayItem != -1)hItemList.Push(iArrayItem);
			}
		}

		if (g_bCvar_SwitchOffCSSWeapons)
		{
			if (strcmp(sPrimarySlot[7], "smg_mp5") == 0)
			{				
				iArrayItem = GetItemFromArrayList(g_hSMGList, iClient, _, "!!smg_mp5");
				if (iArrayItem != -1 && (IsWeaponNearAmmoPile(iArrayItem, iClient) || WeaponHasEnoughAmmoLeft(iArrayItem)))
				{
					hItemList.Push(iArrayItem);
				}
			}
			else if (strcmp(sPrimarySlot[7], "rifle_sg552") == 0) 
			{				
				iArrayItem = GetItemFromArrayList(g_hAssaultRifleList, iClient, _, "!!rifle_sg552");
				if (iArrayItem != -1 && (IsWeaponNearAmmoPile(iArrayItem, iClient) || WeaponHasEnoughAmmoLeft(iArrayItem)))
				{
					hItemList.Push(iArrayItem);
				}
			}
			else if (strcmp(sPrimarySlot[7], "sniper_scout") == 0 || strcmp(sPrimarySlot[7], "sniper_awp") == 0)
			{				
				iArrayItem = GetItemFromArrayList(g_hSniperRifleList, iClient, _, "!!sniper_scout");
				if (iArrayItem != -1 && (IsWeaponNearAmmoPile(iArrayItem, iClient) || WeaponHasEnoughAmmoLeft(iArrayItem)))
				{
					hItemList.Push(iArrayItem);
				}
				else
				{
					iArrayItem = GetItemFromArrayList(g_hSniperRifleList, iClient, _, "!!sniper_awp");
					if (iArrayItem != -1 && (IsWeaponNearAmmoPile(iArrayItem, iClient) || WeaponHasEnoughAmmoLeft(iArrayItem)))
					{
						hItemList.Push(iArrayItem);
					}	
				}
			}
		}

		if (g_bCvar_SwapSameTypePrimaries)
		{
			if (iWpnPreference != L4D_WEAPON_PREFERENCE_SMG)
			{
				int iSMGCount = (GetSurvivorTeamItemCount(L4D2WeaponId_Smg) + GetSurvivorTeamItemCount(L4D2WeaponId_SmgSilenced) + GetSurvivorTeamItemCount(L4D2WeaponId_SmgMP5));
				int iShotgunCount = (GetSurvivorTeamItemCount(L4D2WeaponId_ShotgunChrome) + GetSurvivorTeamItemCount(L4D2WeaponId_Pumpshotgun));

				int iTier1Limit = RoundToCeil((iSMGCount + iShotgunCount) * 0.5);
				if (iTier1Limit < 1)iTier1Limit = 1;

				if (iShotgunCount > iTier1Limit && (strcmp(sPrimarySlot[7], "shotgun_chrome") == 0 || strcmp(sPrimarySlot[7], "pumpshotgun") == 0))
				{
					iArrayItem = GetItemFromArrayList(g_hSMGList, iClient);
					if (iArrayItem != -1 && (IsWeaponNearAmmoPile(iArrayItem, iClient) || WeaponHasEnoughAmmoLeft(iArrayItem)))
					{
						hItemList.Push(iArrayItem);
					}
				}
				else if (iSMGCount > iTier1Limit && (strcmp(sPrimarySlot[7], "smg") == 0 || strcmp(sPrimarySlot[7], "smg_silenced") == 0 || strcmp(sPrimarySlot[7], "smg_mp5") == 0))
				{
					iArrayItem = GetItemFromArrayList(g_hShotgunT1List, iClient);
					if (iArrayItem != -1 && (IsWeaponNearAmmoPile(iArrayItem, iClient) || WeaponHasEnoughAmmoLeft(iArrayItem)))
					{
						hItemList.Push(iArrayItem);
					}
				}
			}

			int iPrimaryCount = GetSurvivorTeamItemCount(L4D2_GetWeaponId(iPrimarySlot));
			static char sAvoidWeapon[64];
			FormatEx(sAvoidWeapon, sizeof(sAvoidWeapon), "!!%s", sPrimarySlot);

			int iWepLimit = -1;
			ArrayList hWepArray;
			if (SurvivorHasShotgun(iClient))
			{
				hWepArray = g_hShotgunT2List;
				iWepLimit = RoundFloat((GetSurvivorTeamItemCount(L4D2WeaponId_Autoshotgun) + GetSurvivorTeamItemCount(L4D2WeaponId_ShotgunSpas)) * 0.5);
			}
			else if (SurvivorHasAssaultRifle(iClient))
			{
				hWepArray = g_hAssaultRifleList;
				iWepLimit = RoundFloat((GetSurvivorTeamItemCount(L4D2WeaponId_Rifle) + GetSurvivorTeamItemCount(L4D2WeaponId_RifleAK47) + GetSurvivorTeamItemCount(L4D2WeaponId_RifleDesert) + GetSurvivorTeamItemCount(L4D2WeaponId_RifleSG552)) * 0.5);
			}
			else if (SurvivorHasSniperRifle(iClient))
			{
				hWepArray = g_hSniperRifleList;
				iWepLimit = RoundFloat((GetSurvivorTeamItemCount(L4D2WeaponId_HuntingRifle) + GetSurvivorTeamItemCount(L4D2WeaponId_SniperMilitary) + GetSurvivorTeamItemCount(L4D2WeaponId_SniperScout) + GetSurvivorTeamItemCount(L4D2WeaponId_SniperAWP)) * 0.5);
			}
			if (iWepLimit != -1 && iWepLimit < 1)iWepLimit = 1;

			if (iPrimaryCount > iWepLimit)
			{
				iArrayItem = GetItemFromArrayList(hWepArray, iClient, _, sAvoidWeapon);
				if (iArrayItem != -1 && (WeaponHasEnoughAmmoLeft(iArrayItem) || IsWeaponNearAmmoPile(iArrayItem, iClient)))
				{
					hItemList.Push(iArrayItem);
				}
			}
		}

		int iUpgradeBits = GetEntProp(iPrimarySlot, Prop_Send, "m_upgradeBitVec");
		if (!(iUpgradeBits & L4D2_WEPUPGFLAG_LASER) && iItemBits & (1 << 8) != 0)
		{
			iArrayItem = GetItemFromArrayList(g_hLaserSightList, iClient);
			if (iArrayItem != -1)hItemList.Push(iArrayItem);
		}
		if (iItemBits & (1 << 9) != 0 && !(iUpgradeBits & L4D2_WEPUPGFLAG_INCENDIARY) && !(iUpgradeBits & L4D2_WEPUPGFLAG_EXPLOSIVE))
		{
			iArrayItem = GetItemFromArrayList(g_hDeployedAmmoPacks, iClient);
			if (iArrayItem != -1)hItemList.Push(iArrayItem);
		}
	}

	int iSecondarySlot = GetClientWeaponInventory(iClient, 1);
	if (iSecondarySlot != -1)
	{
		int iMeleeCount = GetSurvivorTeamItemCount(L4D2WeaponId_Melee);
		int iChainsawCount = GetSurvivorTeamItemCount(L4D2WeaponId_Chainsaw);
		int iMeleeType = SurvivorHasMeleeWeapon(iClient);

		if (iMeleeType != 0)
		{
			if (iMeleeType != 2)
			{
				if (iItemBits & (1 << 11) != 0 && (iMeleeCount + iChainsawCount) <= g_iCvar_MaxMeleeSurvivors && iChainsawCount < g_iCvar_ImprovedMelee_ChainsawLimit)
				{
					iArrayItem = GetItemFromArrayList(g_hMeleeList, iClient, _, "weapon_chainsaw");
					if (iArrayItem != -1 && g_iWeapon_Clip1[iArrayItem] > RoundFloat(GetWeaponMaxAmmo(iArrayItem) * 0.25))
					{
						hItemList.Push(iArrayItem);
					}
				}
			}
			else if (iChainsawCount > g_iCvar_ImprovedMelee_ChainsawLimit || g_iWeapon_Clip1[iSecondarySlot] <= RoundFloat(GetWeaponMaxAmmo(iSecondarySlot) * 0.25))
			{
				bool bFoundMelee = false;
				if (iMeleeCount < g_iCvar_MaxMeleeSurvivors)
				{
					iArrayItem = GetItemFromArrayList(g_hMeleeList, iClient, _, "weapon_melee");
					if (iArrayItem != -1)
					{
						bFoundMelee = true;
						hItemList.Push(iArrayItem); 
					}
				}
				if (!bFoundMelee)
				{
					iArrayItem = GetItemFromArrayList(g_hPistolList, iClient);
					if (iArrayItem != -1)hItemList.Push(iArrayItem);
				}
			}

			if ((iMeleeCount + iChainsawCount) > g_iCvar_MaxMeleeSurvivors && (iMeleeType != 2 || iChainsawCount > g_iCvar_ImprovedMelee_ChainsawLimit))
			{
				iArrayItem = GetItemFromArrayList(g_hPistolList, iClient);
				if (iArrayItem != -1)hItemList.Push(iArrayItem);
			}
		}
		else if (iItemBits & (1 << 12) != 0)
		{
			if ((iMeleeCount + iChainsawCount) < g_iCvar_MaxMeleeSurvivors)
			{
				iArrayItem = GetItemFromArrayList(g_hMeleeList, iClient, _, "weapon_melee");
				if (iArrayItem != -1)hItemList.Push(iArrayItem);
			}

			int iHasPistol = SurvivorHasPistol(iClient);
			if (iHasPistol != 0)
			{
				if (g_bCvar_BotWeaponPreference_ForceMagnum && iHasPistol != 3)
				{
					iArrayItem = GetItemFromArrayList(g_hPistolList, iClient, _, "weapon_pistol_magnum");
					if (iArrayItem != -1)hItemList.Push(iArrayItem);
				}
				else if (iHasPistol == 1)
				{
					iArrayItem = GetItemFromArrayList(g_hPistolList, iClient, _, "weapon_pistol");
					if (iArrayItem != -1)hItemList.Push(iArrayItem);
				} 
			}
		}
	}

	if (hItemList.Length > 0)
	{
		int iCurItem;
		float fCurDist;
		float fLastDist = -1.0;
		for (int i = 0; i < hItemList.Length; i++)
		{
			iCurItem = hItemList.Get(i);

			// Nothing to compare with a single candidate, so skip the path search.
			if (hItemList.Length == 1)
			{
				iItem = iCurItem;
				break;
			}

			fCurDist = GetEntityTravelDistance(iClient, iCurItem, true, (g_fCvar_ItemScavenge_ApproachVisibleRange * BOT_ITEM_SEARCH_MULT));
			if (fCurDist == -1.0 || fLastDist != -1.0 && fCurDist >= fLastDist)continue;

			iItem = iCurItem;
			fLastDist = fCurDist;
		}
	}

	delete hItemList;
	return iItem;
}

int GetSurvivorBotWeaponPreference(int iClient)
{
	switch(GetClientSurvivorType(iClient))
	{
		case L4D_SURVIVOR_ROCHELLE:	return g_iCvar_BotWeaponPreference_Rochelle;
		case L4D_SURVIVOR_COACH:	return g_iCvar_BotWeaponPreference_Coach;
		case L4D_SURVIVOR_ELLIS:	return g_iCvar_BotWeaponPreference_Ellis;
		case L4D_SURVIVOR_BILL:		return g_iCvar_BotWeaponPreference_Bill;
		case L4D_SURVIVOR_ZOEY:		return g_iCvar_BotWeaponPreference_Zoey;
		case L4D_SURVIVOR_FRANCIS:	return g_iCvar_BotWeaponPreference_Francis;
		case L4D_SURVIVOR_LOUIS:	return g_iCvar_BotWeaponPreference_Louis;
		default:					return g_iCvar_BotWeaponPreference_Nick;
	}
}

int GetPinnedSurvivorCount()
{
	int iCount;
	for (int i = 1; i <= MaxClients; i++)
	{
		if (!IsClientSurvivor(i) || !L4D_IsPlayerPinned(i))continue;
		iCount++;
	}
	return iCount;
}

int ItemSpawnerHasEnoughItems(int iSpawner)
{
	return (!HasEntProp(iSpawner, Prop_Data, "m_itemCount") ? 9999 : GetEntProp(iSpawner, Prop_Data, "m_itemCount"));
}

bool IsWeaponNearAmmoPile(int iWeapon, int iOwner = -1)
{
	int iAmmoPile = GetItemFromArrayList(g_hAmmopileList, iWeapon, _, _, _, false);
	return (iAmmoPile != -1 && (iOwner == -1 || LBI_IsReachableEntity(iOwner, iAmmoPile)));
}

int GetItemFromArrayList(ArrayList hArrayList, int iClient, float fDistance = -1.0, const char[] sEntityName = "", const char[] sModelName = "", bool bCheckIsReachable = true, bool bCheckIsVisible = true)
{
	if (!hArrayList || hArrayList.Length <= 0)return -1;
	if (fDistance == -1.0)fDistance = g_fCvar_ItemScavenge_ApproachVisibleRange;

	float fClientPos[3];
	GetEntityAbsOrigin(iClient, fClientPos);

	int iEntIndex, iNavArea, iUseCount;
	float fEntityPos[3];
	float fCheckDist, fCurDist;
	bool bIsTaken, bInUseRange, bValidClient = IsValidClient(iClient);

 	char sWeaponName[MAX_TARGET_LENGTH];
	char sEntityModel[PLATFORM_MAX_PATH];

	int iPrimarySlot, iPrimaryTier;
	float fPickupRange;
	bool bPrimaryHasAmmo, bIsCoop, bInCheckpoint;
	if (bValidClient)
	{
		iPrimarySlot = GetClientWeaponInventory(iClient, 0);
		if (IsEntityExists(iPrimarySlot))
		{
			bPrimaryHasAmmo = WeaponHasEnoughAmmoLeft(iPrimarySlot);
			iPrimaryTier = GetWeaponTier(iPrimarySlot);
		}

		bIsCoop = L4D2_IsGenericCooperativeMode(); 
		bInCheckpoint = LBI_IsPositionInsideCheckpoint(g_fClientAbsOrigin[iClient]);
		fPickupRange = (g_fCvar_ItemScavenge_PickupRange*g_fCvar_ItemScavenge_PickupRange);
	}

	for (int i = 0; i < hArrayList.Length; i++)
	{		
		iEntIndex = EntRefToEntIndex(hArrayList.Get(i));
		if (iEntIndex == INVALID_ENT_REFERENCE || IsValidClient(GetEntityOwner(iEntIndex)))
			continue;

		iUseCount = ItemSpawnerHasEnoughItems(iEntIndex);
		if (iUseCount == 0)continue;

		if (!GetEntityAbsOrigin(iEntIndex, fEntityPos) || GetVectorDistance(fClientPos, fEntityPos, true) > g_fCvar_ItemScavenge_MapSearchRange)
			continue;

		if (sModelName[0] != 0)
		{
			GetEntityModelname(iEntIndex, sEntityModel, sizeof(sEntityModel));
			if (strcmp(sEntityModel, sModelName, false) != 0)continue;
		}

		GetEntityClassname(iEntIndex, sWeaponName, sizeof(sWeaponName));
		if (g_iItem_Used[iEntIndex] & (1 << (iClient - 1)) != 0 && (strcmp(sWeaponName, "upgrade_ammo_explosive") == 0 || strcmp(sWeaponName, "upgrade_ammo_incendiary") == 0) || strcmp(sWeaponName, "prop_dynamic") == 0)
			continue;

		GetWeaponClassname(iEntIndex, sWeaponName, sizeof(sWeaponName));
		if (sEntityName[0] != 0)
		{
			if (sEntityName[0] == '!' && sEntityName[1] == '!') 
			{
				if (strcmp(sWeaponName, sEntityName[2], false) == 0)
				{
					continue;
				}
			}
			else if (strcmp(sWeaponName, sEntityName, false) != 0)
			{
				continue;
			}
		}
		
		fCheckDist = fDistance; 
		if (bValidClient)
		{
			if (g_bCvar_SwitchOffCSSWeapons && iPrimarySlot != -1 && bPrimaryHasAmmo && (iPrimaryTier == 1 && strcmp(sWeaponName[7], "smg_mp5") == 0 || iPrimaryTier == 2 && (strcmp(sWeaponName[7], "rifle_sg552") == 0 || strcmp(sWeaponName[7], "sniper_scout") == 0 || strcmp(sWeaponName[7], "sniper_awp") == 0)))
				continue;

			if (g_iWeapon_AmmoLeft[iEntIndex] <= RoundFloat(g_iWeapon_MaxAmmo[iEntIndex] * 0.2) && strcmp(sWeaponName[7], "grenade_launcher") == 0)
				continue;

			if ((g_iWeapon_Clip1[iEntIndex] <= 0 || g_iWeapon_Clip1[iEntIndex] <= RoundFloat(L4D2_GetIntWeaponAttribute("weapon_rifle_m60", L4D2IWA_ClipSize) * 0.2)) && strcmp(sWeaponName[7], "rifle_m60") == 0)
				continue;

			if (iUseCount == 1 && strcmp(sWeaponName[7], "ammo_spawn") != 0)
			{
				bIsTaken = false;
				for (int j = 1; j <= MaxClients; j++)
				{
					if (j == iClient || !IsClientSurvivor(j) || !IsFakeClient(j) || iEntIndex != g_iSurvivorBot_ScavengeItem[j] || !IsEntityExists(g_iSurvivorBot_ScavengeItem[j]))
						continue;

					bIsTaken = true;
					break;
				}
				if (bIsTaken)continue;
			}

			bInUseRange = (GetVectorDistance(g_fClientEyePos[iClient], fEntityPos, true) <= fPickupRange);
			if (!bInUseRange)
			{
				// fCheckDist never exceeds fDistance, so this item would be rejected below anyway.
				if (GetVectorDistance(fClientPos, fEntityPos, true) > (fDistance*fDistance))
					continue;

				if (bIsCoop && bInCheckpoint && !LBI_IsPositionInsideCheckpoint(fEntityPos))
					continue;

				if (bCheckIsReachable && !LBI_IsReachableEntity(iClient, iEntIndex))
					continue;

				if (bCheckIsVisible && fDistance > g_fCvar_ItemScavenge_ApproachRange)
				{ 
					iNavArea = L4D_GetNearestNavArea(fEntityPos, _, true, true, false);
					if (iNavArea && !LBI_IsNavAreaPartiallyVisible(iNavArea, g_fClientEyePos[iClient], iClient))
					{
						fCheckDist = g_fCvar_ItemScavenge_ApproachRange;
					}
				}
			}
		}

		fCurDist = GetVectorDistance(fClientPos, fEntityPos, true);
		if (!bInUseRange && fCurDist > (fCheckDist*fCheckDist))continue;

		return iEntIndex;
	}

	return -1;
}
