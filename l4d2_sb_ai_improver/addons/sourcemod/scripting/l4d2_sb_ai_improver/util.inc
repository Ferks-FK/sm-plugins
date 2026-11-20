float GetClientDistance(int iClient, int iTarget, bool bSquared = false)
{
	return GetVectorDistance(g_fClientAbsOrigin[iClient], g_fClientAbsOrigin[iTarget], bSquared);
}

float GetEntityDistance(int iEntity, int iTarget, bool bSquared = false)
{
	float fEntityPos[3]; GetEntityAbsOrigin(iEntity, fEntityPos);
	float fTargetPos[3]; GetEntityAbsOrigin(iTarget, fTargetPos);
	return (GetVectorDistance(fEntityPos, fTargetPos, bSquared));
}

bool IsValidVector(const float fVector[3])
{
	int iCheck;
	for (int i = 0; i < 3; ++i)
	{
		if (fVector[i] != 0.0000)break;
		++iCheck;
	}
	return view_as<bool>(iCheck != 3);
}

int GetEntityHealth(int iEntity)
{
	return (GetEntProp(iEntity, Prop_Data, "m_iHealth"));
}

int GetEntityMaxHealth(int iEntity)
{
	return (GetEntProp(iEntity, Prop_Data, "m_iMaxHealth"));
}

float GetWeaponCycleTime(int iWeapon)
{
	static char sWeaponName[64]; GetWeaponClassname(iWeapon, sWeaponName, sizeof(sWeaponName));
	if (!L4D2_IsValidWeapon(sWeaponName))return -1.0;
	return L4D2_GetFloatWeaponAttribute(sWeaponName, L4D2FWA_CycleTime);
}

int GetWeaponMaxAmmo(int iWeapon, L4D2WeaponId iWeaponID = L4D2WeaponId_None)
{
	int iAmmoType = GetWeaponAmmoType(iWeapon);
	if (iAmmoType != -1)
	{
		switch(iAmmoType)
		{
			case 1, 2: 	return g_iCvar_MaxAmmo_Pistol;
			case 3: 	return g_iCvar_MaxAmmo_AssaultRifle;
			case 5: 	return g_iCvar_MaxAmmo_SMG;
			case 6: 	return (g_iCvar_MaxAmmo_M60 > 0 ? g_iCvar_MaxAmmo_M60 : L4D2_GetIntWeaponAttribute("weapon_rifle_m60", L4D2IWA_ClipSize));
			case 7: 	return g_iCvar_MaxAmmo_Shotgun;
			case 8: 	return g_iCvar_MaxAmmo_AutoShotgun;
			case 9: 	return g_iCvar_MaxAmmo_HuntRifle;
			case 10: 	return g_iCvar_MaxAmmo_SniperRifle;
			case 12: 	return g_iCvar_MaxAmmo_PipeBomb;
			case 13: 	return g_iCvar_MaxAmmo_Molotov;
			case 14: 	return g_iCvar_MaxAmmo_VomitJar;
			case 15: 	return g_iCvar_MaxAmmo_PainPills;
			case 16: 	
			{
				if (iWeaponID == L4D2WeaponId_None)iWeaponID = L4D2_GetWeaponId(iWeapon);
				return ((iWeaponID == L4D2WeaponId_FragAmmo || iWeaponID == L4D2WeaponId_IncendiaryAmmo) ? g_iCvar_MaxAmmo_AmmoPack : g_iCvar_MaxAmmo_Medkit);
			}
			case 17: 	return g_iCvar_MaxAmmo_GrenLauncher;
			case 18: 	return g_iCvar_MaxAmmo_Adrenaline;
			case 19: 	return g_iCvar_MaxAmmo_Chainsaw;
			default:	return -1;
		}
	}

	if (iWeaponID == L4D2WeaponId_None)
		iWeaponID = L4D2_GetWeaponId(iWeapon);

	if (iWeaponID == L4D2WeaponId_Pistol || iWeaponID == L4D2WeaponId_PistolMagnum)
		return g_iCvar_MaxAmmo_SMG;
	else if (iWeaponID == L4D2WeaponId_Smg || iWeaponID == L4D2WeaponId_SmgSilenced || iWeaponID == L4D2WeaponId_SmgMP5)
		return g_iCvar_MaxAmmo_SMG;
	else if (iWeaponID == L4D2WeaponId_Pumpshotgun || iWeaponID == L4D2WeaponId_ShotgunChrome)
		return g_iCvar_MaxAmmo_Shotgun;
	else if (iWeaponID == L4D2WeaponId_Autoshotgun || iWeaponID == L4D2WeaponId_ShotgunSpas)
		return g_iCvar_MaxAmmo_AutoShotgun;
	else if (iWeaponID == L4D2WeaponId_Rifle || iWeaponID == L4D2WeaponId_RifleAK47 || iWeaponID == L4D2WeaponId_RifleDesert || iWeaponID == L4D2WeaponId_RifleSG552)
		return g_iCvar_MaxAmmo_AssaultRifle;
	else if (iWeaponID == L4D2WeaponId_HuntingRifle)
		return g_iCvar_MaxAmmo_HuntRifle;
	else if (iWeaponID == L4D2WeaponId_SniperMilitary || iWeaponID == L4D2WeaponId_SniperScout || iWeaponID == L4D2WeaponId_SniperAWP)
		return g_iCvar_MaxAmmo_SniperRifle;
	else if (iWeaponID == L4D2WeaponId_GrenadeLauncher)
		return g_iCvar_MaxAmmo_GrenLauncher;
	else if (iWeaponID == L4D2WeaponId_RifleM60)
		return g_iCvar_MaxAmmo_M60;
	else if (iWeaponID == L4D2WeaponId_FirstAidKit)
		return g_iCvar_MaxAmmo_Medkit;
	else if (iWeaponID == L4D2WeaponId_Adrenaline)
		return g_iCvar_MaxAmmo_Adrenaline;
	else if (iWeaponID == L4D2WeaponId_PainPills)
		return g_iCvar_MaxAmmo_PainPills;
	else if (iWeaponID == L4D2WeaponId_FragAmmo || iWeaponID == L4D2WeaponId_IncendiaryAmmo)
		return g_iCvar_MaxAmmo_AmmoPack;
	else if (iWeaponID == L4D2WeaponId_Chainsaw)
		return g_iCvar_MaxAmmo_Chainsaw;
	else if (iWeaponID == L4D2WeaponId_PipeBomb)
		return g_iCvar_MaxAmmo_PipeBomb;
	else if (iWeaponID == L4D2WeaponId_Molotov)
		return g_iCvar_MaxAmmo_Molotov;
	else if (iWeaponID == L4D2WeaponId_Vomitjar)
		return g_iCvar_MaxAmmo_VomitJar;

	return -1;
}

int GetWeaponTier(int iWeapon)
{
	bool bInRange = (0 < iWeapon <= MAXENTITIES);
	if (bInRange && g_iEntCache_WeaponTier[iWeapon] != 0)
		return (g_iEntCache_WeaponTier[iWeapon] - 1);

	if (!IsEntityWeapon(iWeapon))return -1;

	char sWeaponName[64]; GetWeaponClassname(iWeapon, sWeaponName, sizeof(sWeaponName));
	if (!L4D2_IsValidWeapon(sWeaponName))return -1;

	int iTier = L4D2_GetIntWeaponAttribute(sWeaponName, L4D2IWA_Tier);
	iTier = ((strcmp(sWeaponName[7], "rifle_m60") == 0 || strcmp(sWeaponName[15], "launcher") == 0) && iTier == 0 ? 3 : iTier);

	// As in IsEntityWeaponCached, only a real answer is kept, so a wrong "not a weapon" can't stick.
	if (bInRange && iTier >= 0)g_iEntCache_WeaponTier[iWeapon] = (iTier + 1);
	return iTier;
}

bool IsSurvivorCarryingProp(int iClient)
{
	return (IsWeaponSlotActive(iClient, 5));

}

bool GetWeaponClassname(int iWeapon, char[] sBuffer, int iMaxLength)
{
	L4D2WeaponId iWeaponID = L4D2_GetWeaponId(iWeapon);
	if (iWeaponID != L4D2WeaponId_None)
	{
		L4D2_GetWeaponNameByWeaponId(iWeaponID, sBuffer, iMaxLength);
		if (strcmp(sBuffer, "weapon_ammo") == 0)strcopy(sBuffer, iMaxLength, "weapon_ammo_spawn");
		return true;
	}

	char sWeaponModel[PLATFORM_MAX_PATH];
	GetEntityModelname(iWeapon, sWeaponModel, sizeof(sWeaponModel));

	// SIDEARMS
	if (strcmp(sWeaponModel[24], "w_pistol_a.mdl", false) == 0 || strcmp(sWeaponModel[24], "w_pistol_a_dual.mdl", false) == 0 || strcmp(sWeaponModel[24], "w_pistol_b.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_pistol_magnum");
	else if (strcmp(sWeaponModel[24], "w_desert_eagle.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_pistol_magnum");

	// SMGS
	else if (strcmp(sWeaponModel[24], "w_smg_uzi.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_smg");
	else if (strcmp(sWeaponModel[24], "w_smg_a.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_smg_silenced");
	else if (strcmp(sWeaponModel[24], "w_smg_mp5.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_smg_mp5");

	// TIER 1 SHOTGUNS
	else if (strcmp(sWeaponModel[24], "w_pumpshotgun_a.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_shotgun_chrome");
	else if (strcmp(sWeaponModel[24], "w_shotgun.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_pumpshotgun");

	// ASSAULT RIFLES
	else if (strcmp(sWeaponModel[24], "w_rifle_m16a2.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_rifle");
	else if (strcmp(sWeaponModel[24], "w_rifle_ak47.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_rifle_ak47");
	else if (strcmp(sWeaponModel[24], "w_desert_rifle.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_rifle_desert");
	else if (strcmp(sWeaponModel[24], "w_rifle_sg552.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_rifle_sg552");

	// TIER 2 SHOTGUNS
	else if (strcmp(sWeaponModel[24], "w_autoshot_m4super.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_autoshotgun");
	else if (strcmp(sWeaponModel[24], "w_shotgun_spas.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_shotgun_spas");

	// SNIPER RIFLES
	else if (strcmp(sWeaponModel[24], "w_sniper_mini14.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_hunting_rifle");
	else if (strcmp(sWeaponModel[24], "w_sniper_military.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_sniper_military");
	else if (strcmp(sWeaponModel[24], "w_sniper_scout.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_sniper_scout");
	else if (strcmp(sWeaponModel[24], "w_sniper_awp.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_sniper_awp");

	// TIER 3 WEAPONS
	else if (strcmp(sWeaponModel[24], "w_grenade_launcher.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_grenade_launcher");
	else if (strcmp(sWeaponModel[24], "w_m60.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_rifle_m60");

	// GRENADES
	else if (strcmp(sWeaponModel[24], "w_eq_pipebomb.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_pipe_bomb");
	else if (strcmp(sWeaponModel[24], "w_eq_molotov.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_molotov");
	else if (strcmp(sWeaponModel[24], "w_eq_bile_flask.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_vomitjar");

	// MEDICAL ITEMS
	else if (strcmp(sWeaponModel[24], "w_eq_medkit.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_first_aid_kit");
	else if (strcmp(sWeaponModel[24], "w_eq_painpills.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_pain_pills");
	else if (strcmp(sWeaponModel[24], "w_eq_defibrillator.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_defibrillator");
	else if (strcmp(sWeaponModel[24], "w_eq_adrenaline.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_adrenaline");

	// UPGRADE PACKS
	else if (strcmp(sWeaponModel[24], "w_eq_incendiary_ammopack.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_upgradepack_incendiary");
	else if (strcmp(sWeaponModel[24], "w_eq_explosive_ammopack.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_upgradepack_explosive");

	// AMMO PILE
	else if (strcmp(sWeaponModel[20], "ammo_stack.mdl", false) == 0 || strcmp(sWeaponModel[36], "coffeeammo.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_ammo_spawn");
	
	// CARRIABLE PROPS
	else if (strcmp(sWeaponModel[18], "gascan001a.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_gascan");
	else if (strcmp(sWeaponModel[18], "propanecanister001a.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_propanetank");
	else if (strcmp(sWeaponModel[23], "oxygentank01.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_oxygentank");
	else if (strcmp(sWeaponModel[18], "gnome.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_gnome");
	else if (strcmp(sWeaponModel[24], "w_cola.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_cola_bottles");
	else if (strcmp(sWeaponModel[18], "explosive_box001.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_fireworkcrate");

	// MELEES
	else if (strcmp(sWeaponModel[21], "w_chainsaw.mdl", false) == 0)
		strcopy(sBuffer, iMaxLength, "weapon_chainsaw");
	else
	{
		for (int i = 0; i < sizeof(g_sMeleeWeaponMdls); i++)
		{
			if (strcmp(sWeaponModel, g_sMeleeWeaponMdls[i], false) == 0)
			{
				strcopy(sBuffer, iMaxLength, "weapon_melee");
				break;
			}
		}
	}

	if (sBuffer[0] == 0)
	{
		GetEntityClassname(iWeapon, sBuffer, iMaxLength);
		return false;
	}

	return true;
}

int GetClientSurvivorType(int iClient)
{
	// Asked every tick, and each answer reads the model name.
	float fNow = GetGameTime();
	if (IsCacheTimeValid(g_fClient_SurvivorTypeUntil[iClient], fNow, BOT_SURVIVOR_TYPE_CACHE_TIME))
		return g_iClient_SurvivorType[iClient];

	static char sModelname[PLATFORM_MAX_PATH]; GetClientModel(iClient, sModelname, sizeof(sModelname));

	int iType;
	switch(sModelname[29])
	{
		case 'b': 	iType = L4D_SURVIVOR_NICK;
		case 'd': 	iType = L4D_SURVIVOR_ROCHELLE;
		case 'c': 	iType = L4D_SURVIVOR_COACH;
		case 'h': 	iType = L4D_SURVIVOR_ELLIS;
		case 'v': 	iType = L4D_SURVIVOR_BILL;
		case 'n': 	iType = L4D_SURVIVOR_ZOEY;
		case 'e': 	iType = L4D_SURVIVOR_FRANCIS;
		case 'a': 	iType = L4D_SURVIVOR_LOUIS;
		default:	iType = 0;
	}

	g_iClient_SurvivorType[iClient] = iType;
	g_fClient_SurvivorTypeUntil[iClient] = fNow + BOT_SURVIVOR_TYPE_CACHE_TIME;
	return iType;
}

bool IsEntityExists(int iEntity)
{
	return (iEntity > 0 && (iEntity <= MAXENTITIES && IsValidEdict(iEntity) || iEntity > MAXENTITIES && IsValidEntity(iEntity)));
}

bool IsCommonInfected(int iEntity)
{
	static char sEntClass[64]; GetEntityClassname(iEntity, sEntClass, sizeof(sEntClass));
	return (strcmp(sEntClass, "infected") == 0);
}

bool IsCommonInfectedAttacking(int iEntity)
{
	if (HasEntProp(iEntity, Prop_Send, "m_mobRush") && GetEntProp(iEntity, Prop_Send, "m_mobRush") != 0)
		return true;

	return (HasEntProp(iEntity, Prop_Send, "m_clientLookatTarget") && GetEntProp(iEntity, Prop_Send, "m_clientLookatTarget") != -1);
}

bool IsCommonInfectedAlive(int iEntity)
{
	return (GetEntProp(iEntity, Prop_Data, "m_lifeState") == 0 && GetEntProp(iEntity, Prop_Send, "m_bIsBurning") == 0);
}

bool IsCommonInfectedStumbled(int iEntity)
{
	if (!g_bExtensionActions)return false;
	return (ActionsManager.GetAction(iEntity, "InfectedShoved") != INVALID_ACTION);
}
