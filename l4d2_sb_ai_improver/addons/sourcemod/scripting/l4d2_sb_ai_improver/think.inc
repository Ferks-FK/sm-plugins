void SurvivorBotThink(int iClient, int &iButtons, int iWpnSlots[6])
{
	g_iSurvivorBot_DefibTarget[iClient] = -1;
	g_bSurvivorBot_PreventFire[iClient] = false;

	int iCurWeapon = L4D_GetPlayerCurrentWeapon(iClient);

	// This bot's own cached results, only rewritten on its own recompute turn.
	int iTeamLeader = g_iSurvivorBot_TeamLeader[iClient];
	int iNearestTeammate = g_iSurvivorBot_NearestTeammate[iClient];
	int iGameDifficulty = g_iSurvivorBot_GameDifficulty[iClient];
	int iDefibTarget = g_iSurvivorBot_NearestDeathModel[iClient];
	int iTeamCount = g_iSurvivorBot_TeamCount[iClient];
	int iAlivePlayers = g_iSurvivorBot_AlivePlayers[iClient];
	int iTankRock = g_iSurvivorBot_TankRock[iClient];
	int iTankTarget = g_iSurvivorBot_TankTarget[iClient];
	int iWitchHarasser = g_iSurvivorBot_WitchHarasser[iClient];
	int iTankProp = g_iSurvivorBot_TankProp[iClient];
	bool bFriendIsNearBoomer = g_bSurvivorBot_FriendIsNearBoomer[iClient];
	bool bFriendIsNearThrowArea = g_bSurvivorBot_FriendIsNearThrowArea[iClient];
	bool bTeamHasHumanPlayer = g_bSurvivorBot_TeamHasHumanPlayer[iClient];
	bool bDidRecomputeSharedState = false;
	// One bot per tick, so a horde's visibility checks don't all land on the same tick.
	if (!g_bBotProcessing_IsProcessed[iClient] && GetGameTime() > g_fBotProcessing_NextProcessTime && GetGameTickCount() != g_iBotProcessing_LastTick)
	{
		g_iBotProcessing_LastTick = GetGameTickCount();
		g_iDebug_LastCheckpoint[iClient] = 1;

		g_iSurvivorBot_NearbyFriends[iClient] = 0;
		g_iSurvivorBot_TargetInfected[iClient] = GetClosestInfected(iClient, 4000.0);
		UpdateNearbyInfectedCounts(iClient);
		// A molotov is only thrown at a tank, which never reads this count.
		if (g_bCvar_GrenadeThrow_Enabled && iWpnSlots[2] != -1 && SurvivorHasGrenade(iClient) != 2)
		{
			g_iSurvivorBot_GrenadeInfectedCount[iClient] = GetInfectedCount(iClient, g_fCvar_GrenadeThrow_ThrowRange, CalculateGrenadeThrowInfectedCount(), _, false);
		}

		iTeamLeader = iClient;
		iGameDifficulty = GetCurrentGameDifficulty();
		iTeamCount = GetTeamPlayerCount(2);
		iAlivePlayers = 1;
		
		iTankTarget = 0;
		iNearestTeammate = 0;
		g_iSurvivorBot_PinnedFriend[iClient] = 0;
		g_iSurvivorBot_IncapacitatedFriend[iClient] = 0;

		bFriendIsNearThrowArea = false;
		bFriendIsNearBoomer = false;
		bTeamHasHumanPlayer = false;

		int iTeam;
		L4D2ZombieClassType iClientClass;
		float fCurDist, fCurDist2;
		float fLastDist = -1.0;
		float fLastDist2 = -1.0;
		float fLastDist3 = -1.0;
		float fLastDist4 = -1.0;
		float fLastDist5 = -1.0;
		bool bUseFlowDist = ShouldUseFlowDistance();

		// Usually none, so the teammate checks below are skipped.
		int iBoomers[MAXPLAYERS+1], iBoomerCount;
		for (int i = 1; i <= MaxClients; i++)
		{
			if (IsSpecialInfected(i) && L4D2_GetPlayerZombieClass(i) == L4D2ZombieClass_Boomer)
				iBoomers[iBoomerCount++] = i;
		}

		for (int i = 1; i <= MaxClients; i++)
		{
			if (!IsClientInGame(i) || !IsPlayerAlive(i))
				continue;

			fCurDist = GetClientDistance(iClient, i, true);
			iTeam = GetClientTeam(i);

			if (iTeam == 2)
			{
				if (!bTeamHasHumanPlayer || !IsFakeClient(i))
				{
					fCurDist2 = ((bUseFlowDist && !bTeamHasHumanPlayer) ? L4D2Direct_GetFlowDistance(i) : fCurDist);
					if (fLastDist4 == -1.0 || bUseFlowDist && !bTeamHasHumanPlayer && fCurDist2 > fLastDist4 || fCurDist2 < fLastDist4)
					{
						iTeamLeader = i;
						fLastDist4 = fCurDist2;
						if (!bTeamHasHumanPlayer)bTeamHasHumanPlayer = (!IsFakeClient(i));
					}
				}

				if (i == iClient)
					continue;

				iAlivePlayers++;

				if (fLastDist5 == -1.0 || fCurDist < fLastDist5)
				{
					iNearestTeammate = i;
					fLastDist5 = fCurDist;
				}

				if ((fLastDist == -1.0 || fCurDist < fLastDist) && L4D_IsPlayerPinned(i))
				{
					g_iSurvivorBot_PinnedFriend[iClient] = i;
					fLastDist = fCurDist;
				}
				
				if ((fLastDist2 == -1.0 || fCurDist < fLastDist2) && L4D_IsPlayerIncapacitated(i))
				{
					g_iSurvivorBot_IncapacitatedFriend[iClient] = i;
					fLastDist2 = fCurDist;
				}

				if (!bFriendIsNearBoomer)
				{
					for (int j = 0; j < iBoomerCount; j++)
					{
						bFriendIsNearBoomer = (GetClientDistance(i, iBoomers[j], true) <= BOT_BOOMER_AVOID_RADIUS_SQR && IsVisibleEntity(iBoomers[j], i, MASK_VISIBLE_AND_NPCS));
						if (bFriendIsNearBoomer)break;
					}
				}

				if (fCurDist <= 262144.0 && !L4D_IsPlayerIncapacitated(i) && !L4D_IsPlayerPinned(i))
					g_iSurvivorBot_NearbyFriends[iClient] += 1;
				
				if (!bFriendIsNearThrowArea)
					bFriendIsNearThrowArea = (GetVectorDistance(g_fClientAbsOrigin[i], g_fSurvivorBot_Grenade_ThrowPos[iClient], true) <= BOT_GRENADE_CHECK_RADIUS_SQR && IsEntityExists(g_iSurvivorBot_Grenade_ThrowTarget[iClient]) && SurvivorHasGrenade(iClient) == 2);
			}
			else if (iTeam == 3 && !L4D_IsPlayerGhost(i))
			{
				iClientClass = L4D2_GetPlayerZombieClass(i);
				if ((fLastDist3 == -1.0 || fCurDist < fLastDist3) && iClientClass == L4D2ZombieClass_Tank && !L4D_IsPlayerIncapacitated(i) && (fCurDist <= 1048576.0 || fCurDist <= 16777216.0 && IsVisibleEntity(iClient, i)))
				{
					iTankTarget = i;
					fLastDist3 = fCurDist;
				}
			}
		}

		g_iDebug_LastCheckpoint[iClient] = 2; // finished team/infected loop

		UpdateEntityScans();

		iDefibTarget = 0;
		if (g_bCvar_DefibRevive_Enabled)
		{
			fLastDist = -1.0;
			int iSurvivor;
			for (int i = 0; i < g_iEntityScan_DeathModelCount; i++)
			{
				iSurvivor = EntRefToEntIndex(g_iEntityScan_DeathModels[i]);
				if (iSurvivor == INVALID_ENT_REFERENCE)continue;

				fCurDist = GetEntityDistance(iClient, iSurvivor, true);
				if (fLastDist == -1.0 || fCurDist < fLastDist)
				{
					iDefibTarget = iSurvivor;
					fLastDist = fCurDist;
				}
			}
		}

		g_iDebug_LastCheckpoint[iClient] = 3; // finished defib scan

		iTankProp = -1;
		iTankRock = -1;
		if (iTankTarget != 0)
		{
			fLastDist = -1.0;
			int iPropRock;
			for (int i = 0; i < g_iEntityScan_RockCount; i++)
			{
				iPropRock = EntRefToEntIndex(g_iEntityScan_Rocks[i]);
				if (iPropRock == INVALID_ENT_REFERENCE)continue;

				fCurDist = GetEntityDistance(iClient, iPropRock, true);
				if (fLastDist != -1.0 && fCurDist >= fLastDist || !IsVisibleEntity(iClient, iPropRock))
					continue;

				iTankRock = iPropRock;
				fLastDist = fCurDist;
			}

			fLastDist = -1.0;
			for (int i = 0; i < g_iEntityScan_PropCount; i++)
			{
				iPropRock = EntRefToEntIndex(g_iEntityScan_Props[i]);
				if (iPropRock == INVALID_ENT_REFERENCE)continue;

				fCurDist = GetEntityDistance(iTankTarget, iPropRock, true);
				if (fCurDist > 250000.0 || fLastDist != -1.0 && fCurDist >= fLastDist || !IsVisibleEntity(iTankTarget, iPropRock) || !IsVisibleEntity(iClient, iPropRock))
					continue;

				iTankProp = iPropRock;
				fLastDist = fCurDist;
			}
		}

		g_iDebug_LastCheckpoint[iClient] = 4; // finished tank rock/prop scan

		g_iSurvivorBot_WitchTarget[iClient] = -1;

		iWitchHarasser = 0;
		fLastDist = -1.0;
		int iWitchRef, iHarasserRef;
		for (int i = 0; i < g_hWitchList.Length; i++)
		{
			iWitchRef = EntRefToEntIndex(g_hWitchList.Get(i));
			if (iWitchRef == INVALID_ENT_REFERENCE || !IsEntityExists(iWitchRef))
			{
				g_hWitchList.Erase(i--);
				continue;
			}
			
			iHarasserRef = GetClientOfUserId(g_hWitchList.Get(i, 1));
			if (iWitchHarasser != 0 && iHarasserRef == 0)continue;

			fCurDist = GetEntityDistance(iClient, iWitchRef, true);
			if (fLastDist != -1.0 && fCurDist >= fLastDist)continue;

			g_iSurvivorBot_WitchTarget[iClient] = iWitchRef;
			iWitchHarasser = iHarasserRef; fLastDist = fCurDist;
		}
		g_bSurvivorBot_IsWitchHarasser[iClient] = (iWitchHarasser == iClient);

		g_iDebug_LastCheckpoint[iClient] = 5; // finished witch loop, about to mark processed

		g_iBotProcessing_ProcessedCount++;
		g_bBotProcessing_IsProcessed[iClient] = true;
		bDidRecomputeSharedState = true;
		g_iDebug_LastCheckpoint[iClient] = 6; // fully completed

		g_iSurvivorBot_TeamLeader[iClient] = iTeamLeader;
		g_iSurvivorBot_NearestTeammate[iClient] = iNearestTeammate;
		g_iSurvivorBot_GameDifficulty[iClient] = iGameDifficulty;
		g_iSurvivorBot_NearestDeathModel[iClient] = iDefibTarget;
		g_iSurvivorBot_TeamCount[iClient] = iTeamCount;
		g_iSurvivorBot_AlivePlayers[iClient] = iAlivePlayers;
		g_iSurvivorBot_TankRock[iClient] = iTankRock;
		g_iSurvivorBot_TankTarget[iClient] = iTankTarget;
		g_iSurvivorBot_WitchHarasser[iClient] = iWitchHarasser;
		g_iSurvivorBot_TankProp[iClient] = iTankProp;
		g_bSurvivorBot_FriendIsNearBoomer[iClient] = bFriendIsNearBoomer;
		g_bSurvivorBot_FriendIsNearThrowArea[iClient] = bFriendIsNearThrowArea;
		g_bSurvivorBot_TeamHasHumanPlayer[iClient] = bTeamHasHumanPlayer;
	}

	if (g_bCvar_Debug && bDidRecomputeSharedState)
	{
		// Only logs when something changed since this bot's last log.
		int iHumanOnTeam = bTeamHasHumanPlayer;
		if (iTankTarget != g_iDebug_Last_TankTarget[iClient] || iWitchHarasser != g_iDebug_Last_WitchHarasser[iClient] ||
			iTeamCount != g_iDebug_Last_TeamCount[iClient] || iAlivePlayers != g_iDebug_Last_AlivePlayers[iClient] ||
			iHumanOnTeam != g_iDebug_Last_HumanOnTeam[iClient])
		{
			g_iDebug_Last_TankTarget[iClient] = iTankTarget;
			g_iDebug_Last_WitchHarasser[iClient] = iWitchHarasser;
			g_iDebug_Last_TeamCount[iClient] = iTeamCount;
			g_iDebug_Last_AlivePlayers[iClient] = iAlivePlayers;
			g_iDebug_Last_HumanOnTeam[iClient] = iHumanOnTeam;

			DebugLog("client=%d(%N) iTeamLeader=%d(%s) nearestTeammate=%d iTankTarget=%d iWitchHarasser=%d iTeamCount=%d aliveTeammates=%d humanOnTeam=%d",
				iClient, iClient,
				iTeamLeader, (iTeamLeader == iClient ? "IS SELF" : "not self"),
				iNearestTeammate, iTankTarget, iWitchHarasser, iTeamCount, iAlivePlayers, bTeamHasHumanPlayer);
		}
	}

	if (L4D_IsPlayerHangingFromLedge(iClient))
		return;

	if (GetEntityMoveType(iClient) == MOVETYPE_LADDER)
	{
		g_fSurvivorBot_TimeSinceLeftLadder[iClient] = GetGameTime() + 2.0;
		return;
	}

	if (IsValidVector(g_fSurvivorBot_LookPosition[iClient]))
	{ 
		if (GetGameTime() < g_fSurvivorBot_LookPosition_Duration[iClient])
		{
			SnapViewToPosition(iClient, g_fSurvivorBot_LookPosition[iClient]);
		}
		else
		{
			SetVectorToZero(g_fSurvivorBot_LookPosition[iClient]);
		}
	}

	if (iCurWeapon != -1)
	{
		if (iCurWeapon == iWpnSlots[0] && GetSurvivorBotWeaponPreference(iClient) == L4D_WEAPON_PREFERENCE_SECONDARY)
		{
			SwitchWeaponSlot(iClient, 1);
		}

		if (g_bSurvivorBot_ForceWeaponReload[iClient])
		{
			iButtons |= IN_RELOAD;
			g_bSurvivorBot_ForceWeaponReload[iClient] = false;
		}
		else if (GetGameTime() <= g_fSurvivorBot_BlockWeaponReloadTime[iClient])
		{
			iButtons &= ~IN_RELOAD;
		}

		// Expensive, so it waits until the bot can actually fire.
		if (((iButtons & IN_ATTACK) != 0 || (iCurWeapon != iWpnSlots[0] && iCurWeapon != iWpnSlots[1]) || IsEntityExists(g_iSurvivorBot_TargetInfected[iClient]) || IsValidClient(iTankTarget) || IsValidClient(g_iSurvivorBot_PinnedFriend[iClient]) || IsEntityExists(g_iSurvivorBot_WitchTarget[iClient])) && !SurvivorBot_CanFreelyFireWeapon(iClient))
		{
			g_bSurvivorBot_PreventFire[iClient] = true;
			iButtons &= ~IN_ATTACK;
		}

		if (g_bSurvivorBot_ForceBash[iClient])
		{
			g_bSurvivorBot_ForceBash[iClient] = false;
			iButtons |= IN_ATTACK2;
		}
	
		// The calls into the game come last, since most bots already fail on the checks before them.
		if (iCurWeapon == iWpnSlots[0] && SurvivorHasPistol(iClient) != 0 && GetWeaponClip1(iWpnSlots[1]) != GetWeaponClipSize(iWpnSlots[1]) && !IsWeaponReloading(iCurWeapon, false) && GetGameTime() > GetWeaponNextFireTime(iCurWeapon)
			&& (SurvivorHasTier3Weapon(iClient) == 2 || GetWeaponClip1(iCurWeapon) == GetWeaponClipSize(iCurWeapon)) && LBI_IsSurvivorBotAvailable(iClient) && !LBI_IsSurvivorInCombat(iClient))
		{
			g_bSurvivorBot_ForceSwitchWeapon[iClient] = true;
			SwitchWeaponSlot(iClient, 1);
		}
	}

	int iPinnedFriend = g_iSurvivorBot_PinnedFriend[iClient];
	if (IsValidVector(g_fSurvivorBot_MovePos_Position[iClient]))
	{
		float fMovePos[3]; fMovePos = g_fSurvivorBot_MovePos_Position[iClient];
		float fMoveDist = GetVectorDistance(g_fClientAbsOrigin[iClient], fMovePos, true);						
		float fMoveTolerance = g_fSurvivorBot_MovePos_Tolerance[iClient];
		float fMoveDuration = g_fSurvivorBot_MovePos_Duration[iClient];

		// These orders end even on acid or fire, so the bot can get a new order to escape.
		if (GetGameTime() > fMoveDuration)
		{
			ClearMoveToPosition(iClient, "", "duration expired", true);
		}
		else if (fMoveTolerance >= 0.0 && fMoveDist <= (fMoveTolerance*fMoveTolerance))
		{
			ClearMoveToPosition(iClient, "", "reached tolerance", true);
		}
		else if (!g_bSurvivorBot_MovePos_IgnoreDamaging[iClient] && IsMovePosDamaging(iClient))
		{
			ClearMoveToPosition(iClient, "", "position now damaging", true);
		}
		else if (!IsMovePosReachable(iClient))
		{
			ClearMoveToPosition(iClient, "", "position unreachable", true);
		}
		// An escape keeps going, since stopping it on damage would leave the bot in the fire.
		else if (strcmp(g_sSurvivorBot_MovePos_Name[iClient], "EscapeInferno") != 0 && IsValidClient(iPinnedFriend) && L4D_GetPinnedInfected(iPinnedFriend) != 0 && L4D2_GetPlayerZombieClass(L4D_GetPinnedInfected(iPinnedFriend)) != L4D2ZombieClass_Smoker)
		{
			ClearMoveToPosition(iClient, "", "friend pinned, needs react");
		}
		else if (GetGameTime() > g_fSurvivorBot_NextMoveCommandTime[iClient])
		{
			if (g_bCvar_Debug && GetGameTime() > g_fDebug_NextMovePosLogTime[iClient])
			{
				g_fDebug_NextMovePosLogTime[iClient] = GetGameTime() + 2.0;
				DebugLog("client=%d(%N) MOVEPOS-ENROUTE name=%s remainingDist=%.1f secondsLeft=%.1f", iClient, iClient, g_sSurvivorBot_MovePos_Name[iClient], SquareRoot(fMoveDist), (fMoveDuration - GetGameTime()));
			}

			L4D2_CommandABot(iClient, 0, BOT_CMD_MOVE, fMovePos);
			g_fSurvivorBot_NextMoveCommandTime[iClient] = GetGameTime() + BOT_CMD_MOVE_INTERVAL;
		}
	}

	int iInfectedTarget = g_iSurvivorBot_TargetInfected[iClient];
	if (GetGameTime() > g_fSurvivorBot_PinnedReactTime[iClient] && IsValidClient(iPinnedFriend))
	{
		int iAttacker = L4D_GetPinnedInfected(iPinnedFriend);
		if (IsValidClient(iAttacker))
		{
			// The attacker can be anywhere, and aiming at one out of sight is expensive.
			bool bAttackerVisible = HasVisualContactWithEntity(iClient, iAttacker, false);
			float fAttackerAimPos[3];
			if (bAttackerVisible)GetTargetAimPart(iClient, iAttacker, fAttackerAimPos);

			float fFriendDist = GetVectorDistance(g_fClientEyePos[iClient], g_fClientCenteroid[iPinnedFriend], true);
			bool bCanShoot = (iCurWeapon != -1 && g_iCvar_HelpPinnedFriend_Enabled & (1 << 0) != 0 && fFriendDist <= (g_fCvar_HelpPinnedFriend_ShootRange*g_fCvar_HelpPinnedFriend_ShootRange) && (iCurWeapon != iWpnSlots[1] || !SurvivorHasMeleeWeapon(iClient) || GetClientDistance(iClient, iAttacker, true) <= (g_fCvar_ImprovedMelee_AttackRange*g_fCvar_ImprovedMelee_AttackRange)) && SurvivorBot_AbleToShootWeapon(iClient) && CheckIfCanRescueImmobilizedFriend(iClient));

			int iCanShove;
			if (g_iCvar_HelpPinnedFriend_Enabled & (1 << 1) != 0)
			{
				iCanShove = (fFriendDist <= (g_fCvar_HelpPinnedFriend_ShoveRange*g_fCvar_HelpPinnedFriend_ShoveRange) ? 1 : (GetVectorDistance(g_fClientEyePos[iClient], g_fClientCenteroid[iAttacker], true) <= (g_fCvar_HelpPinnedFriend_ShoveRange*g_fCvar_HelpPinnedFriend_ShoveRange) ? 2 : 0));
			}

			L4D2ZombieClassType iZombieClass = L4D2_GetPlayerZombieClass(iAttacker);
			if (iZombieClass != L4D2ZombieClass_Smoker)
			{
				if (iWpnSlots[0] != -1 && g_iWeapon_AmmoLeft[iWpnSlots[0]] > 0 && g_iWeapon_Clip1[iWpnSlots[0]] > 0 && iCurWeapon == iWpnSlots[1] && SurvivorHasMeleeWeapon(iClient) && fFriendDist > g_fCvar_ImprovedMelee_AimRange)
				{
					g_bSurvivorBot_ForceSwitchWeapon[iClient] = true;
					SwitchWeaponSlot(iClient, 0);
				}
				else
				{
					if (bCanShoot && fFriendDist <= 262144.0 && iCurWeapon == iWpnSlots[0] && IsWeaponReloading(iCurWeapon) && GetWeaponClip1(iWpnSlots[1]) > 0 && !SurvivorHasShotgun(iClient) && !SurvivorHasSniperRifle(iClient) && !SurvivorHasMeleeWeapon(iClient))
					{
						g_bSurvivorBot_ForceSwitchWeapon[iClient] = true;
						SwitchWeaponSlot(iClient, 1);
					}
				}

				if (iCanShove != 0 && iZombieClass != L4D2ZombieClass_Charger && !L4D_IsPlayerIncapacitated(iClient))
				{
					SnapViewToPosition(iClient, (iCanShove == 1 ? g_fClientCenteroid[iPinnedFriend] : g_fClientCenteroid[iAttacker]));
					iButtons |= IN_ATTACK2;
				}
				else if (bCanShoot && bAttackerVisible)
				{
					SnapViewToPosition(iClient, fAttackerAimPos);
					PressAttackButton(iClient, iButtons);
				}
			}
			else
			{
				if (!L4D_IsPlayerIncapacitated(iClient))
				{
					if (iCanShove != 0)
					{
						SnapViewToPosition(iClient, (iCanShove == 1 ? g_fClientCenteroid[iPinnedFriend] : g_fClientCenteroid[iAttacker]));
						iButtons |= IN_ATTACK2;
					}
				}

				if (bCanShoot)
				{
					if (bAttackerVisible)
					{
						SnapViewToPosition(iClient, fAttackerAimPos);
						PressAttackButton(iClient, iButtons);
					}
					else 
					{
						float fTipPos[3]; GetEntPropVector(L4D_GetPlayerCustomAbility(iAttacker), Prop_Send, "m_tipPosition", fTipPos);
						if (!IsValidVector(fTipPos))fTipPos = g_fClientEyePos[iPinnedFriend];	

						if (IsVisibleVector(iClient, fTipPos))
						{
							float fMidPos[3];
							fMidPos[0] = ((g_fClientEyePos[iAttacker][0] + fTipPos[0]) / 2.0);
							fMidPos[1] = ((g_fClientEyePos[iAttacker][1] + fTipPos[1]) / 2.0);
							fMidPos[2] = ((g_fClientEyePos[iAttacker][2] + fTipPos[2]) / 2.0);

							SnapViewToPosition(iClient, (IsVisibleVector(iClient, fMidPos) ? fMidPos : fTipPos));
							PressAttackButton(iClient, iButtons);
						}
					}
				}
			}
		}
	}

	// A dying tank would restart the retreat that the stop check below ends.
	if (IsValidClient(iTankTarget) && !L4D_IsPlayerIncapacitated(iTankTarget))
	{
		if (g_bCvar_Debug && iTankTarget != g_iDebug_Last_TankCheckTarget[iClient])
		{
			g_iDebug_Last_TankCheckTarget[iClient] = iTankTarget;
			DebugLog("client=%d(%N) TANK-CHECK tankTarget=%d(%N)", iClient, iClient, iTankTarget, iTankTarget);
		}

		int iVictim = GetTankChaseVictim(iTankTarget);
		float fTankDist = GetClientDistance(iClient, iTankTarget, true);
		float fHeightDist = (fTankDist + (g_fClientAbsOrigin[iClient][2] - g_fClientAbsOrigin[iTankTarget][2]));

		bool bRescuing = IsRescuingFriendFromTank(iClient);
		if (bRescuing)
		{
			ClearMoveToPosition(iClient, "TakeCover");
			ClearMoveToPosition(iClient, "RegroupTeam");
			ClearMoveToPosition(iClient, "SupportChased", "rescuing");
			ClearMoveToPosition(iClient, BOT_TANK_ROUTE_MOVE_NAME, "rescuing");
			ClearMoveToPosition(iClient, BOT_TANK_BLIND_MOVE_NAME, "rescuing");
			g_bSurvivorBot_SupportingVictim[iClient] = false;
			g_fSurvivorBot_TankFleeUntil[iClient] = 0.0;

			if (g_bSurvivorBot_RetreatCommanded[iClient])
			{
				g_bSurvivorBot_RetreatCommanded[iClient] = false;
				g_fSurvivorBot_NextRetreatTime[iClient] = 0.0;
				L4D2_CommandABot(iClient, 0, BOT_CMD_RESET);
			}
		}

		if (fTankDist <= BOT_TANK_KEEP_AWAY_DIST_SQR)ClearMoveToPosition(iClient, "RegroupTeam");
		if (fTankDist <= (g_fCvar_TankSupport_MinDist*g_fCvar_TankSupport_MinDist))ClearMoveToPosition(iClient, "SupportChased", "too close to tank");
		if (!bRescuing)UpdateTankSupport(iClient);
		bool bSupporting = (strcmp(g_sSurvivorBot_MovePos_Name[iClient], "SupportChased") == 0);
		bool bRegrouping = (!bRescuing && !g_bSurvivorBot_SupportingVictim[iClient] && fTankDist > BOT_TANK_KEEP_AWAY_DIST_SQR && (strcmp(g_sSurvivorBot_MovePos_Name[iClient], "RegroupTeam") == 0 || IsPositionAwayFromTeam(g_fClientAbsOrigin[iClient]) && TryRegroupWithTeam(iClient)));

		if (!bRescuing && !bRegrouping && !bSupporting && fHeightDist <= 1638400.0 && (g_bCvar_AvoidTanksWithProp && IsEntityExists(iTankProp) || g_bCvar_TakeCoverFromRocks && g_bInfectedBot_IsThrowing[iTankTarget]) && (iVictim == iClient || GetClientDistance(iClient, iVictim, true) <= 65536.0) && IsTankVisibleCached(iClient, iTankTarget))
		{
			g_fSurvivorBot_TankFleeUntil[iClient] = GetGameTime() + BOT_TANK_FLEE_AIM_TIME;

			// Staying near the team rejects more spots, and a failed search is expensive.
			// A bot on the route is already moving, so it keeps going instead of searching.
			if (strcmp(g_sSurvivorBot_MovePos_Name[iClient], "TakeCover") != 0 && strcmp(g_sSurvivorBot_MovePos_Name[iClient], BOT_TANK_ROUTE_MOVE_NAME) != 0 && GetGameTime() > g_fSurvivorBot_NextTankCoverSearchTime[iClient])
			{
				if (!TakeCoverFromEntity(iClient, (IsEntityExists(iTankProp) ? iTankProp : iTankTarget), 768.0, (g_fCvar_TankTeamLeash > 0.0)))
					g_fSurvivorBot_NextTankCoverSearchTime[iClient] = GetGameTime() + 0.5;
			}
		}
		else if (!bRescuing && !bRegrouping && !bSupporting)
		{
			bool bTankVisible = IsTankVisibleCached(iClient, iTankTarget);
			if (!g_bInfectedBot_IsThrowing[iTankTarget] || !bTankVisible)
			{
				ClearMoveToPosition(iClient, "TakeCover");	
			}
			if (bTankVisible)
			{
				ClearMoveToPosition(iClient, BOT_TANK_BLIND_MOVE_NAME, "tank in sight");
			}
			if (!L4D_IsPlayerIncapacitated(iClient) && (fTankDist <= BOT_TANK_KEEP_AWAY_DIST_SQR || fTankDist <= (bTankVisible ? g_fCvar_TankFleeDistSqr : g_fCvar_TankFleeBlindDistSqr)))
			{
				// Repeating this every think keeps replacing the retreat action.
				if (!TryFleeAlongMapRoute(iClient, iTankTarget) && GetGameTime() >= g_fSurvivorBot_NextRetreatTime[iClient])
				{
					g_fSurvivorBot_NextRetreatTime[iClient] = GetGameTime() + BOT_TANK_RETREAT_INTERVAL;
					if (bTankVisible)
					{
						L4D2_CommandABot(iClient, iTankTarget, BOT_CMD_RETREAT);
						g_bSurvivorBot_RetreatCommanded[iClient] = true;
					}
					// The game drops a retreat from a tank out of sight right away, so the bot moves out of its sight instead.
					else if (strcmp(g_sSurvivorBot_MovePos_Name[iClient], BOT_TANK_BLIND_MOVE_NAME) != 0)
					{
						TakeCoverFromEntity(iClient, iTankTarget, 768.0, (g_fCvar_TankTeamLeash > 0.0), BOT_TANK_BLIND_MOVE_NAME);
					}
				}
				if (g_bCvar_Debug && GetGameTime() >= g_fSurvivorBot_TankFleeUntil[iClient])DebugLog("client=%d(%N) TANK-FLEE-START tank=%d dist=%.1f visible=%d", iClient, iClient, iTankTarget, SquareRoot(fTankDist), bTankVisible);
				g_fSurvivorBot_TankFleeUntil[iClient] = GetGameTime() + BOT_TANK_FLEE_AIM_TIME;
			}
		}

		if (g_bCvar_TankRock_ShootEnabled && g_iCvar_TankRockHealth > 0 && IsEntityExists(iTankRock))
		{
			float fRockPos[3]; GetEntityCenteroid(iTankRock, fRockPos);

			if (GetVectorDistance(g_fClientEyePos[iClient], fRockPos, true) <= g_fCvar_TankRock_ShootRange && SurvivorBot_AbleToShootWeapon(iClient) && 
				(iCurWeapon != iWpnSlots[1] || !SurvivorHasMeleeWeapon(iClient)) && !IsSurvivorBusy(iClient) && HasVisualContactWithEntity(iClient, iTankRock, false, fRockPos))
			{
				SnapViewToPosition(iClient, fRockPos);
				PressAttackButton(iClient, iButtons);
			}
		}
	}
	else if (L4D2_IsTankInPlay())
	{
		if (IsRescuingFriendFromTank(iClient))
		{
			ClearMoveToPosition(iClient, "RegroupTeam");
			ClearMoveToPosition(iClient, "SupportChased", "rescuing");
			ClearMoveToPosition(iClient, BOT_TANK_ROUTE_MOVE_NAME, "rescuing");
			ClearMoveToPosition(iClient, BOT_TANK_BLIND_MOVE_NAME, "rescuing");
			g_bSurvivorBot_SupportingVictim[iClient] = false;
		}
		else
		{
			UpdateTankSupport(iClient);
			if (!g_bSurvivorBot_SupportingVictim[iClient] && strcmp(g_sSurvivorBot_MovePos_Name[iClient], "RegroupTeam") != 0 && IsPositionAwayFromTeam(g_fClientAbsOrigin[iClient]))
				TryRegroupWithTeam(iClient);
		}
	}

	UpdateTankRetreat(iClient);

	int iWitchTarget = g_iSurvivorBot_WitchTarget[iClient];
	// An angered witch stays in, since a sniper can shoot her from beyond this distance.
	if (IsEntityExists(iWitchTarget) && GetEntityHealth(iWitchTarget) > 0 && (IsValidClient(iWitchHarasser) || GetEntityDistance(iClient, iWitchTarget, true) <= BOT_WITCH_CHECK_DIST_SQR))
	{
		if (g_bCvar_Debug && (iWitchTarget != g_iDebug_Last_WitchCheckTarget[iClient] || iWitchHarasser != g_iDebug_Last_WitchCheckHarasser[iClient]))
		{
			g_iDebug_Last_WitchCheckTarget[iClient] = iWitchTarget;
			g_iDebug_Last_WitchCheckHarasser[iClient] = iWitchHarasser;
			DebugLog("client=%d(%N) WITCH-CHECK witchTarget=%d witchHarasser=%d(%N) dist=%.1f", iClient, iClient, iWitchTarget, iWitchHarasser, iWitchHarasser, GetEntityDistance(iClient, iWitchTarget));
		}

		float fWitchOrigin[3]; GetEntityAbsOrigin(iWitchTarget, fWitchOrigin);
		float fWitchDist = GetVectorDistance(g_fClientAbsOrigin[iClient], fWitchOrigin, true);

		// Only filled where the bot aims, since it is expensive and the witch can be anywhere.
		float fFirePos[3];

		int iHasShotgun = SurvivorHasShotgun(iClient);

		if (IsValidClient(iWitchHarasser))
		{
			if ((iCurWeapon == iWpnSlots[0] || iCurWeapon == iWpnSlots[1]) && SurvivorBot_AbleToShootWeapon(iClient))
			{
				float fShootRange = g_fCvar_TargetSelection_ShootRange;
				if (iCurWeapon == iWpnSlots[1])
				{
					if (!SurvivorHasMeleeWeapon(iClient))fShootRange = g_fCvar_TargetSelection_ShootRange4;
					else fShootRange = g_fCvar_ImprovedMelee_AttackRange;
				}
				else if (iHasShotgun)fShootRange = g_fCvar_TargetSelection_ShootRange2;
				else if (SurvivorHasSniperRifle(iClient))fShootRange = g_fCvar_TargetSelection_ShootRange3;

				// Only the two ranges below need the sight check.
				bool bInShootRange = (fWitchDist <= (fShootRange*fShootRange));
				bool bWitchVisible = ((bInShootRange || fWitchDist <= BOT_WITCH_CHECK_DIST_SQR) && HasVisualContactWithEntity(iClient, iWitchTarget, false));
				if (bInShootRange && bWitchVisible)
				{
					GetTargetAimPart(iClient, iWitchTarget, fFirePos);
					SnapViewToPosition(iClient, fFirePos);
					bool bFired = PressAttackButton(iClient, iButtons);
					if (iHasShotgun == 1 && bFired)g_bSurvivorBot_ForceBash[iClient] = true;

					if (fShootRange != g_fCvar_TargetSelection_ShootRange2 && fShootRange != g_fCvar_ImprovedMelee_AttackRange)
					{
						ClearMoveToPosition(iClient, "GoToWitch");
					}
				}
				else if (iWitchHarasser != iClient && fWitchDist <= BOT_WITCH_CHECK_DIST_SQR && GetGameTime() > g_fSurvivorBot_NextWitchMoveUpdateTime[iClient])
				{
					g_fSurvivorBot_NextWitchMoveUpdateTime[iClient] = GetGameTime() + 1.0;
					SetMoveToPosition(iClient, fWitchOrigin, 3, "GoToWitch", 0.0, ((bWitchVisible && !L4D_IsPlayerIncapacitated(iWitchHarasser)) ? (fShootRange > 192.0 ? 192.0 : fShootRange) : 0.0), true);
				}
			}

			// Repeating this every think would keep replacing the retreat.
			if (iWitchHarasser == iClient && fWitchDist <= 1048576.0 && !L4D_IsPlayerIncapacitated(iClient) && GetGameTime() >= g_fSurvivorBot_NextRetreatTime[iClient])
			{
				g_fSurvivorBot_NextRetreatTime[iClient] = GetGameTime() + BOT_TANK_RETREAT_INTERVAL;
				L4D2_CommandABot(iClient, iWitchTarget, BOT_CMD_RETREAT);
			}
		}
		else 
		{
			float fWalkDist = g_fCvar_WitchBehavior_WalkWhenNearby;
			if (fWalkDist != 0.0 && fWitchDist <= (fWalkDist*fWalkDist) && (GetEntPropFloat(iWitchTarget, Prop_Send, "m_rage") <= 0.2 && GetEntPropFloat(iWitchTarget, Prop_Send, "m_wanderrage") <= 0.66) && !LBI_IsSurvivorInCombat(iClient))
			{
				iButtons |= IN_SPEED;
			}

			int iCrowning = g_iCvar_WitchBehavior_AllowCrowning;
			if ((iCrowning == 2 || iCrowning == 1 && !bTeamHasHumanPlayer) && iCurWeapon == iWpnSlots[0] && fWitchDist <= 1048576.0 && !L4D_IsPlayerOnThirdStrike(iClient) && (!IsValidClient(iTeamLeader) || fWitchDist <= 262144.0) && !IsWeaponReloading(iCurWeapon, false) && iHasShotgun && IsEntityVisibleCached(iClient, iWitchTarget))
			{
				if (fWitchDist <= 4096.0)
				{
					ClearMoveToPosition(iClient, "GoToWitch");
					GetTargetAimPart(iClient, iWitchTarget, fFirePos);
					SnapViewToPosition(iClient, fFirePos);
					bool bFired = PressAttackButton(iClient, iButtons);
					if (iHasShotgun == 1 && bFired)g_bSurvivorBot_ForceBash[iClient] = true;
				}
				else if (LBI_IsSurvivorBotAvailable(iClient))
				{
					bool bApproachWitch = !ShouldUseFlowDistance();
					if (!bApproachWitch)
					{
						Address pArea = L4D2Direct_GetTerrorNavArea(fWitchOrigin);
						bApproachWitch = (pArea != Address_Null && L4D2Direct_GetTerrorNavAreaFlow(pArea) >= L4D2Direct_GetFlowDistance(iClient));
					}
					if (bApproachWitch && GetGameTime() > g_fSurvivorBot_NextWitchMoveUpdateTime[iClient])
					{
						g_fSurvivorBot_NextWitchMoveUpdateTime[iClient] = GetGameTime() + BOT_APPROACH_CHECK_INTERVAL;
						SetMoveToPosition(iClient, fWitchOrigin, 2, "GoToWitch", 0.0, 0.0, true);
					}

					if (fWitchDist <= 16384.0)
					{
						GetTargetAimPart(iClient, iWitchTarget, fFirePos);
						SnapViewToPosition(iClient, fFirePos);
					}
				}
			}
		}
	}

	float fInfectedDist = -1.0;
	if (IsEntityExists(iInfectedTarget))
	{
		float fInfectedPos[3]; GetEntityCenteroid(iInfectedTarget, fInfectedPos);
		float fInfectedOrigin[3]; GetEntityAbsOrigin(iInfectedTarget, fInfectedOrigin);
		fInfectedDist = GetVectorDistance(g_fClientAbsOrigin[iClient], fInfectedOrigin, true);
		
		L4D2ZombieClassType iInfectedClass = L4D2ZombieClass_NotInfected;
		if (IsValidClient(iInfectedTarget))iInfectedClass = L4D2_GetPlayerZombieClass(iInfectedTarget);

		if (g_bCvar_ImprovedMelee_Enabled)
		{
			int iMeleeType = SurvivorHasMeleeWeapon(iClient);
			if (iMeleeType != 0)
			{
				if (iCurWeapon == iWpnSlots[1])
				{
					// Expensive, so only done when the target is almost in reach.
					float fAimPosition[3];
					float fMeleeDistance = GetVectorDistance(g_fClientEyePos[iClient], fInfectedPos, true);
					float fMeleeAimRange = SquareRoot(g_fCvar_ImprovedMelee_AimRange);
					float fMeleeReach = ((g_fCvar_ImprovedMelee_AttackRange > fMeleeAimRange ? g_fCvar_ImprovedMelee_AttackRange : fMeleeAimRange) + BOT_MELEE_BONE_MARGIN);
					if (fMeleeDistance <= (fMeleeReach*fMeleeReach))
					{
						GetClosestToEyePosEntityBonePos(iClient, iInfectedTarget, fAimPosition);
						fMeleeDistance = GetVectorDistance(g_fClientEyePos[iClient], fAimPosition, true);
					}
					
					if (fMeleeDistance <= g_fCvar_ImprovedMelee_AimRange)
					{
						g_fSurvivorBot_BlockWeaponSwitchTime[iClient] = (GetGameTime() + (iMeleeType == 2 ? 3.0 : 1.0));
						SnapViewToPosition(iClient, fAimPosition);
					}

					if (!g_bSurvivorBot_PreventFire[iClient] && fMeleeDistance <= (g_fCvar_ImprovedMelee_AttackRange*g_fCvar_ImprovedMelee_AttackRange) && (iGameDifficulty == 4 || (!IsSurvivorBusy(iClient) || g_iSurvivorBot_ThreatInfectedCount[iClient] >= GetCommonHitsUntilDown(iClient, (float(g_iSurvivorBot_NearbyFriends[iClient]) / (iTeamCount - 1))))))
					{
						float fAttackTime = (iMeleeType == 2 ? GetRandomFloat(0.33, 0.8) : GetRandomFloat(0.1, 0.33));
						g_fSurvivorBot_MeleeAttackTime[iClient] = (GetGameTime() + fAttackTime);
					}

					if (GetGameTime() < g_fSurvivorBot_MeleeAttackTime[iClient])
					{
						bool bShouldShove = (iInfectedClass != L4D2ZombieClass_Charger && GetRandomInt(1, (iMeleeType == 2 ? 200 : g_iCvar_ImprovedMelee_ShoveChance)) == 1 && (!g_bExtensionActions || iInfectedClass != L4D2ZombieClass_NotInfected || !IsCommonInfectedStumbled(iInfectedTarget)));
						iButtons |= ( bShouldShove ? IN_ATTACK2 : IN_ATTACK );
					}

					// Throttled, since it searches paths for both the bot and its leader.
					if (GetGameTime() >= g_fSurvivorBot_NextApproachCheckTime[iClient])
					{
						g_fSurvivorBot_NextApproachCheckTime[iClient] = GetGameTime() + BOT_APPROACH_CHECK_INTERVAL;

						bool bStopApproaching = true;
						if (g_fCvar_ImprovedMelee_ApproachRange > 0.0 && GetGameTime() > g_fSurvivorBot_MeleeApproachTime[iClient] && !IsSurvivorBotBlindedByVomit(iClient) && !IsValidClient(iPinnedFriend) && (!IsValidClient(iTankTarget) || GetClientDistance(iClient, iTankTarget, true) > 1048576.0) && fInfectedDist <= g_fCvar_ImprovedMelee_ApproachRange && LBI_IsReachableEntity(iClient, iInfectedTarget)
							&& !IsFinaleEscapeVehicleArrived() && (iInfectedClass == L4D2ZombieClass_NotInfected || (L4D_IsPlayerStaggering(iInfectedTarget) || L4D_GetPinnedSurvivor(iInfectedTarget) != 0) && !L4D_IsAnySurvivorInCheckpoint())
						)
						{
							Address pArea;
							float fMovePos[3]; GetEntityAbsOrigin(iInfectedTarget, fMovePos);
							if (!ShouldUseFlowDistance() || (pArea = L4D2Direct_GetTerrorNavArea(fMovePos)) == Address_Null || L4D2Direct_GetTerrorNavAreaFlow(pArea) >= (L4D2Direct_GetFlowDistance(iClient) - SquareRoot(g_fCvar_ImprovedMelee_ApproachRange*0.5)))
							{
								// No path is shorter than the straight line, so the search is only needed inside it.
								float fLeaderLimit = (g_fCvar_ImprovedMelee_ApproachRange * 0.75);
								float fLeaderDist = -2.0;
								if (iInfectedClass == L4D2ZombieClass_NotInfected && iTeamLeader != iClient && IsValidClient(iTeamLeader))
									fLeaderDist = (GetVectorDistance(g_fClientAbsOrigin[iTeamLeader], fMovePos, true) > fLeaderLimit ? -1.0 : GetClientTravelDistance(iTeamLeader, fMovePos, true, (SquareRoot(fLeaderLimit) * BOT_PATH_LIMIT_MARGIN)));

								if (fLeaderDist == -2.0 || fLeaderDist != -1.0 && fLeaderDist <= fLeaderLimit)
								{
									float fTravelDist = GetVectorTravelDistance(fMovePos, g_fClientAbsOrigin[iClient], true, (SquareRoot(g_fCvar_ImprovedMelee_ApproachRange) * BOT_PATH_LIMIT_MARGIN));
									if (fTravelDist != -1.0 && fTravelDist <= g_fCvar_ImprovedMelee_ApproachRange)
									{
										SetMoveToPosition(iClient, fMovePos, 2, "ApproachMelee");
										bStopApproaching = false;
									}
								}
							}
						}
						if (bStopApproaching)ClearMoveToPosition(iClient, "ApproachMelee");
					}
				}
				else if (iCurWeapon == iWpnSlots[0])
				{
					int iMeleeSwitchCount = ((iMeleeType != 2) ? g_iCvar_ImprovedMelee_SwitchCount : g_iCvar_ImprovedMelee_SwitchCount2);
					float fMeleeSwitchRange = (g_fCvar_ImprovedMelee_SwitchRange * ((iMeleeType == 2) ? 1.5 : (SurvivorHasShotgun(iClient) ? 0.66 : 1.0)));
					if (fInfectedDist <= fMeleeSwitchRange && !IsValidClient(iTankTarget) && (!SurvivorHasShotgun(iClient) || GetWeaponClip1(iCurWeapon) <= 0))
					{ 
						if (iInfectedClass != L4D2ZombieClass_NotInfected)
						{
							if (g_iInfectedBot_CurrentVictim[iInfectedTarget] != iClient || L4D_GetPinnedSurvivor(iInfectedTarget) != 0 || L4D_IsPlayerStaggering(iInfectedTarget))
							{
								SwitchWeaponSlot(iClient, 1);
								g_fSurvivorBot_MeleeApproachTime[iClient] = GetGameTime() + ((iMeleeType == 2) ? 2.0 : 0.1);
							}
						}
						else if (g_iSurvivorBot_NearbyInfectedCount[iClient] >= iMeleeSwitchCount && !IsValidClient(iPinnedFriend) && (GetCurrentGameDifficulty() == 4 || !IsSurvivorBusy(iClient) || g_iSurvivorBot_ThreatInfectedCount[iClient] >= GetCommonHitsUntilDown(iClient, 0.75)))
						{
							SwitchWeaponSlot(iClient, 1);
							g_fSurvivorBot_MeleeApproachTime[iClient] = GetGameTime() + ((iMeleeType == 2) ? 2.0 : 0.66);
						}
					}
				}
			}
		}

		if (fInfectedDist <= 6400.0 && (g_iCvar_AutoShove_Enabled == 1 || g_iCvar_AutoShove_Enabled == 2 && !FVectorInViewAngle(iClient, fInfectedPos)) && !L4D_IsPlayerIncapacitated(iClient) && (!IsSurvivorBusy(iClient) || g_iSurvivorBot_ThreatInfectedCount[iClient] >= GetCommonHitsUntilDown(iClient, (float(g_iSurvivorBot_NearbyFriends[iClient]) / (iTeamCount - 1)))) && (!SurvivorHasMeleeWeapon(iClient) || iCurWeapon != iWpnSlots[1]))
		{
			if (IsSurvivorCarryingProp(iClient) || (iInfectedClass == L4D2ZombieClass_NotInfected || iInfectedClass != L4D2ZombieClass_Charger && iInfectedClass != L4D2ZombieClass_Tank && !L4D_IsPlayerStaggering(iInfectedTarget) && !IsUsingSpecialAbility(iInfectedTarget)) && GetRandomInt(1, 4) == 1)
			{
				SnapViewToPosition(iClient, fInfectedPos);				
				iButtons |= IN_ATTACK2;
			}
			else if (SurvivorBot_AbleToShootWeapon(iClient))
			{
				SnapViewToPosition(iClient, fInfectedPos);				
				PressAttackButton(iClient, iButtons);
			}
		}
	}

	// The tank is forced as the target: a closer common would win the choice below, and one behind the bot would leave it fleeing without firing.
	// Only with a gun or melee weapon, since with a grenade in hand this would ruin the throw.
	bool bWantsTankAim = ((iCurWeapon == iWpnSlots[0] || iCurWeapon == iWpnSlots[1]) && !L4D_IsPlayerIncapacitated(iClient) && IsValidClient(iTankTarget) && (GetGameTime() < g_fSurvivorBot_TankFleeUntil[iClient] || GetGameTime() >= g_fSurvivorBot_RescueUntil[iClient] && GetClientDistance(iClient, iTankTarget, true) <= g_fCvar_TankFleeDistSqr || GetTankChaseVictim(iTankTarget) == iClient && GetClientDistance(iClient, iTankTarget, true) <= (g_fCvar_TargetSelection_ShootRange*g_fCvar_TargetSelection_ShootRange)));
	bool bFleeingTank = (bWantsTankAim && IsTankVisibleCached(iClient, iTankTarget));
	if (bFleeingTank)g_fSurvivorBot_TankLastSeenTime[iClient] = GetGameTime();

	// With sight just lost the bot keeps facing the tank for a moment, like a player turning a corner. Firing below still needs sight.
	if (bWantsTankAim && !bFleeingTank && GetGameTime() < (g_fSurvivorBot_TankLastSeenTime[iClient] + BOT_TANK_LOOK_MEMORY_TIME) && !IsValidClient(iPinnedFriend) && !IsSurvivorBusy(iClient) && (!IsEntityExists(iInfectedTarget) || iInfectedTarget == iTankTarget || fInfectedDist > BOT_TANK_LOOK_THREAT_DIST_SQR))
	{
		float fLookPos[3]; GetEntityCenteroid(iTankTarget, fLookPos);
		SnapViewToPosition(iClient, fLookPos);

		if (g_bCvar_Debug && GetGameTime() > g_fDebug_NextFleeAimLogTime[iClient])
		{
			g_fDebug_NextFleeAimLogTime[iClient] = GetGameTime() + 0.5;
			DebugLog("client=%d(%N) FLEE-LOOK tank=%d dist=%.1f unseenFor=%.1f", iClient, iClient, iTankTarget, GetClientDistance(iClient, iTankTarget), GetGameTime() - g_fSurvivorBot_TankLastSeenTime[iClient]);
		}
	}

	if (g_bCvar_Debug && !bFleeingTank && IsValidClient(iTankTarget) && GetGameTime() < g_fSurvivorBot_TankFleeUntil[iClient] && GetGameTime() > g_fDebug_NextFleeAimLogTime[iClient])
	{
		g_fDebug_NextFleeAimLogTime[iClient] = GetGameTime() + 0.5;
		DebugLog("client=%d(%N) FLEE-AIM-SKIP visible=%d gunInHand=%d", iClient, iClient, IsTankVisibleCached(iClient, iTankTarget), (iCurWeapon == iWpnSlots[0] || iCurWeapon == iWpnSlots[1]));
	}

	if (!IsValidClient(iPinnedFriend) && !IsSurvivorBusy(iClient) && (bFleeingTank || g_bCvar_TargetSelection_Enabled && (IsValidClient(iTankTarget) || IsEntityExists(iInfectedTarget))))
	{
		int iFireTarget = iInfectedTarget;
		if (IsEntityExists(iFireTarget))
		{
			if (g_bCvar_TargetSelection_IgnoreDociles && !IsValidClient(iFireTarget) && !IsCommonInfectedAttacking(iFireTarget))
			{
				iFireTarget = -1;
			}
			if (IsValidClient(iTankTarget) && (iFireTarget == -1 || fInfectedDist > 1048576.0 || GetClientDistance(iClient, iTankTarget, true) < fInfectedDist))
			{
				iFireTarget = iTankTarget;
			}
		}
		else if (IsValidClient(iTankTarget))
		{
			iFireTarget = iTankTarget;
		}

		if (bFleeingTank)iFireTarget = iTankTarget;

		if (IsEntityExists(iFireTarget) && (bFleeingTank || HasVisualContactWithEntity(iClient, iFireTarget, (iFireTarget != iTankTarget))))
		{
			L4D2ZombieClassType iInfectedClass = L4D2ZombieClass_NotInfected;
			if (IsValidClient(iFireTarget))iInfectedClass = L4D2_GetPlayerZombieClass(iFireTarget);

			float fFirePos[3];
			if (iFireTarget == iTankTarget)GetTankAimPos(iClient, iFireTarget, fFirePos);
			else GetTargetAimPart(iClient, iFireTarget, fFirePos);

			float fTargetDist = GetVectorDistance(g_fClientEyePos[iClient], fFirePos, true);

			if (bFleeingTank)
			{
				// Outside the firing branches below, so the bot also faces the tank while reloading or out of range.
				SnapViewToPosition(iClient, fFirePos);

				if (iCurWeapon == iWpnSlots[1] && SurvivorHasMeleeWeapon(iClient) && GetSurvivorBotWeaponPreference(iClient) != L4D_WEAPON_PREFERENCE_SECONDARY && iWpnSlots[0] != -1 && (g_iWeapon_AmmoLeft[iWpnSlots[0]] > 0 || g_iWeapon_Clip1[iWpnSlots[0]] > 0) && GetGameTime() > g_fSurvivorBot_NextWeaponRangeSwitchTime[iClient])
				{
					g_fSurvivorBot_NextWeaponRangeSwitchTime[iClient] = GetGameTime() + GetRandomFloat(1.0, 3.0);
					g_bSurvivorBot_ForceSwitchWeapon[iClient] = true;
					SwitchWeaponSlot(iClient, 0);
				}
			}

			if (iInfectedClass != L4D2ZombieClass_Boomer || !bFriendIsNearBoomer && fTargetDist > BOT_BOOMER_AVOID_RADIUS_SQR)
			{
				if (iCurWeapon == iWpnSlots[0])
				{
					float fShootRange = g_fCvar_TargetSelection_ShootRange;
					if (SurvivorHasShotgun(iClient))
					{
						fShootRange = g_fCvar_TargetSelection_ShootRange2;
						if (fTargetDist <= (g_fCvar_TargetSelection_ShootRange4*g_fCvar_TargetSelection_ShootRange4) && fTargetDist > ((fShootRange * 1.1)*(fShootRange * 1.1)) && GetGameTime() > g_fSurvivorBot_NextWeaponRangeSwitchTime[iClient] && !IsWeaponReloading(iCurWeapon, false) && !SurvivorHasMeleeWeapon(iClient))
						{
							g_fSurvivorBot_NextWeaponRangeSwitchTime[iClient] = GetGameTime() + GetRandomFloat(1.0, 3.0);
							g_bSurvivorBot_ForceSwitchWeapon[iClient] = true;
							SwitchWeaponSlot(iClient, 1);
						}
					}
					else if (SurvivorHasSniperRifle(iClient))
					{
						fShootRange = g_fCvar_TargetSelection_ShootRange3;
					}

					if (fTargetDist <= (fShootRange*fShootRange) && SurvivorBot_AbleToShootWeapon(iClient))
					{
						SnapViewToPosition(iClient, fFirePos);
						PressAttackButton(iClient, iButtons);
					}
				}
				else if (iCurWeapon == iWpnSlots[1] && !SurvivorHasMeleeWeapon(iClient))
				{
					float fShotgunRange = (g_fCvar_TargetSelection_ShootRange2 * 0.75);
					if (fTargetDist <= (fShotgunRange*fShotgunRange) && GetGameTime() > g_fSurvivorBot_NextWeaponRangeSwitchTime[iClient] && GetClientPrimaryAmmo(iClient) > 0 && !IsWeaponReloading(iCurWeapon) && SurvivorHasShotgun(iClient))
					{
						g_fSurvivorBot_NextWeaponRangeSwitchTime[iClient] = GetGameTime() + GetRandomFloat(1.0, 3.0);
						g_bSurvivorBot_ForceSwitchWeapon[iClient] = true;
						SwitchWeaponSlot(iClient, 0);
					}

					if (L4D_IsPlayerIncapacitated(iClient) || fTargetDist <= (g_fCvar_TargetSelection_ShootRange4*g_fCvar_TargetSelection_ShootRange4) && SurvivorBot_AbleToShootWeapon(iClient))
					{
						SnapViewToPosition(iClient, fFirePos);
						PressAttackButton(iClient, iButtons);
					}
				}
			}

			if (g_bCvar_Debug && bFleeingTank && GetGameTime() > g_fDebug_NextFleeAimLogTime[iClient])
			{
				g_fDebug_NextFleeAimLogTime[iClient] = GetGameTime() + 0.5;

				// A yaw that stays near 180 means the aim didn't stick.
				float fAimAngles[3]; MakeVectorFromPoints(g_fClientEyePos[iClient], fFirePos, fAimAngles); GetVectorAngles(fAimAngles, fAimAngles);
				DebugLog("client=%d(%N) FLEE-AIM tank=%d dist=%.1f yawOff=%.1f slot=%d clip=%d reloading=%d preventFire=%d looked=%d fired=%d", iClient, iClient, iFireTarget, SquareRoot(fTargetDist), FloatAbs(NormalizeAngle(fAimAngles[1] - g_fClientEyeAng[iClient][1])), (iCurWeapon == iWpnSlots[0] ? 0 : 1), GetWeaponClip1(iCurWeapon), IsWeaponReloading(iCurWeapon), g_bSurvivorBot_PreventFire[iClient], g_bClient_IsLookingAtPosition[iClient], g_bClient_IsFiringWeapon[iClient]);
			}
		}
	}

	// Aiming runs every tick, but just carrying a grenade doesn't need these checks.
	if (g_bCvar_GrenadeThrow_Enabled && iWpnSlots[2] != -1 && (IsEntityExists(iInfectedTarget) || IsValidClient(iTankTarget)) && (iCurWeapon == iWpnSlots[2] || GetGameTime() >= g_fSurvivorBot_NextGrenadeCheckTime[iClient]))
	{
		g_fSurvivorBot_NextGrenadeCheckTime[iClient] = GetGameTime() + BOT_GRENADE_CHECK_INTERVAL;

		float fThrowPosition[3];
		int iThrowTarget = -1, iGrenadeType = SurvivorHasGrenade(iClient);

		bool bIsThrowTargetTank = false;
		if (iGrenadeType != 1 && IsValidClient(iTankTarget) && !L4D_IsPlayerIncapacitated(iTankTarget) && (GetEntityHealth(iTankTarget) - 1500) >= RoundFloat((GetEntityMaxHealth(iTankTarget) - 1500) * 0.33) && GetClientDistance(iClient, iTankTarget, true) <= (g_fCvar_GrenadeThrow_ThrowRange*g_fCvar_GrenadeThrow_ThrowRange))
		{
			iThrowTarget = iTankTarget;
			GetEntityAbsOrigin(iTankTarget, fThrowPosition);
			bIsThrowTargetTank = true;
		}
		else
		{
			int iPossibleTarget = (iGrenadeType != 2 ? GetGrenadeFarTarget(iClient) : iInfectedTarget);
			if (iPossibleTarget > 0)
			{
				iThrowTarget = iPossibleTarget;
				GetEntityAbsOrigin(iPossibleTarget, fThrowPosition);
			}
		}
		g_iSurvivorBot_Grenade_ThrowTarget[iClient] = iThrowTarget;

		if (IsEntityExists(iThrowTarget))
		{
			if (iGrenadeType == 2)
			{
				float fThrowAngles[3], fTargetForward[3], fMidPos[3];
				MakeVectorFromPoints(fThrowPosition, g_fClientEyePos[iClient], fThrowAngles);
				NormalizeVector(fThrowAngles, fThrowAngles);
				GetVectorAngles(fThrowAngles, fThrowAngles);

				GetAngleVectors(fThrowAngles, fTargetForward, NULL_VECTOR, NULL_VECTOR);
				NormalizeVector(fTargetForward, fTargetForward);

				float fThrowDist = GetVectorDistance(g_fClientAbsOrigin[iClient], fThrowPosition);
				fMidPos[0] = fThrowPosition[0] + (fTargetForward[0] * (fThrowDist * 0.5));
				fMidPos[1] = fThrowPosition[1] + (fTargetForward[1] * (fThrowDist * 0.5));
				fMidPos[2] = fThrowPosition[2] + (fTargetForward[2] * (fThrowDist * 0.5));

				if (GetVectorDistance(g_fClientAbsOrigin[iClient], fMidPos, true) > BOT_GRENADE_CHECK_RADIUS_SQR)
				{
					float fTraceStart[3]; fTraceStart = fMidPos; fTraceStart[2] += HUMAN_HALF_HEIGHT;
					float fAngleDown[3] = {90.0, 0.0, 0.0};
					Handle hGroundCheck = TR_TraceRayFilterEx(fTraceStart, fAngleDown, MASK_PLAYERSOLID, RayType_Infinite, Base_TraceFilter);
					float fTrHitPos[3]; TR_GetEndPosition(fTrHitPos, hGroundCheck); delete hGroundCheck;

					float fHeightDiff = FloatAbs(fTrHitPos[2] - fThrowPosition[2]);
					if (fHeightDiff < 96.0)
					{
						fThrowPosition = fMidPos;
					}
				}
			}

			if (iCurWeapon == iWpnSlots[2])
			{
				int iThrowArea = L4D_GetNearestNavArea(fThrowPosition, _, true, false, _, 3);
				if (iThrowArea)LBI_GetClosestPointOnNavArea(iThrowArea, fThrowPosition, fThrowPosition);

				float fThrowVel[3]; CalculateTrajectory(g_fClientEyePos[iClient], fThrowPosition, 700.0, 0.4, fThrowVel);
				AddVectors(g_fClientEyePos[iClient], fThrowVel, g_fSurvivorBot_Grenade_AimPos[iClient]);

				float fThrowTrajectory[3];
				fThrowTrajectory = fThrowPosition;
				fThrowTrajectory[2] += (g_fSurvivorBot_Grenade_AimPos[iClient][2] - g_fClientEyePos[iClient][2]);

				Handle hCeilingCheck = TR_TraceRayFilterEx(g_fClientEyePos[iClient], fThrowTrajectory, MASK_SOLID, RayType_EndPoint, Base_TraceFilter);
				g_fSurvivorBot_Grenade_AimPos[iClient][2] *= TR_GetFraction(hCeilingCheck); delete hCeilingCheck;

				g_fSurvivorBot_Grenade_ThrowPos[iClient] = fThrowPosition;

				if (g_iSurvivorBot_ThreatInfectedCount[iClient] < GetCommonHitsUntilDown(iClient, 0.33) && CheckIsUnableToThrowGrenade(iClient, iThrowTarget, g_fClientAbsOrigin[iClient], fThrowPosition, bFriendIsNearThrowArea, bIsThrowTargetTank))
				{
					g_bSurvivorBot_ForceSwitchWeapon[iClient] = true;
					SwitchWeaponSlot(iClient, ((GetClientPrimaryAmmo(iClient) > 0) ? 0 : 1));

					if (iGrenadeType == 2)
					{
						if (GetGameTime() > g_fSurvivorBot_Grenade_NextThrowTime_Molotov)
						{
							g_fSurvivorBot_Grenade_NextThrowTime_Molotov = GetGameTime() + GetRandomFloat(0.75, 1.5);
						}
					}
					else if (GetGameTime() > g_fSurvivorBot_Grenade_NextThrowTime)
					{
						g_fSurvivorBot_Grenade_NextThrowTime = GetGameTime() + GetRandomFloat(0.75, 1.5);
					}
				}
				else
				{
					SnapViewToPosition(iClient, g_fSurvivorBot_Grenade_AimPos[iClient]);
					PressAttackButton(iClient, iButtons);
				}
			}
			else if (CheckCanThrowGrenade(iClient, iThrowTarget, g_fClientAbsOrigin[iClient], fThrowPosition, bFriendIsNearThrowArea, bIsThrowTargetTank))
			{
				SwitchWeaponSlot(iClient, 2);
			}
		}
	}

	if (L4D_IsPlayerIncapacitated(iClient))
		return;

	if (g_bCvar_DeployUpgradePacks && iWpnSlots[0] != -1 && iWpnSlots[3] != -1 && SurvivorHasHealthKit(iClient) == 3 && !LBI_IsSurvivorInCombat(iClient))
	{
		bool bHasDeployedPackNearby = false;
		int iActiveDeployers = (GetSurvivorTeamActiveItemCount(L4D2WeaponId_IncendiaryAmmo) + GetSurvivorTeamActiveItemCount(L4D2WeaponId_FragAmmo));
		if (g_hDeployedAmmoPacks)
		{
			for (int i = 0; i < g_hDeployedAmmoPacks.Length; i++)
			{
				if (GetEntityDistance(iClient, g_hDeployedAmmoPacks.Get(i), true) > 589824.0)continue;
				bHasDeployedPackNearby = true;
				break;
			}
		}
		
		int iPrimSlot, iPrimaryCount, iUpgradedCount;
		for (int i = 1; i <= MaxClients; i++)
		{
			if (!IsClientSurvivor(i))
				continue;
				
			iPrimSlot = GetClientWeaponInventory(i, 0);
			if (iPrimSlot == -1)continue;
			
			iPrimaryCount++;
			if (GetEntProp(iPrimSlot, Prop_Send, "m_nUpgradedPrimaryAmmoLoaded", 1) > 0)
			{
				iUpgradedCount++;
			}
		}

		bool bCanSwitch = (!bHasDeployedPackNearby && iUpgradedCount < RoundFloat(iAlivePlayers * 0.25) && iPrimaryCount >= RoundFloat(iTeamCount * 0.25) && !IsValidClient(iTankTarget));
		if (iCurWeapon == iWpnSlots[3])
		{
			if (iActiveDeployers > 1 || IsValidClient(iTeamLeader) && GetClientDistance(iClient, iTeamLeader, true) >= 65536.0 || !bCanSwitch)
			{
				SwitchWeaponSlot(iClient, (GetClientPrimaryAmmo(iClient) > 0 ? 0 : 1));
			}
			else
			{
				PressAttackButton(iClient, iButtons);
			}
		}
		else if (bCanSwitch && iActiveDeployers == 0 && LBI_IsSurvivorBotAvailable(iClient) && (!IsValidClient(iTeamLeader) || GetClientDistance(iClient, iTeamLeader, true) <= 36864.0))
		{
			SwitchWeaponSlot(iClient, 3);
		}
	}

	if (g_bCvar_DefibRevive_Enabled && IsEntityExists(iDefibTarget) && !IsValidClient(iTankTarget) && !IsValidClient(iPinnedFriend) && !LBI_IsSurvivorInCombat(iClient))
	{
		float fDefibPos[3]; GetEntityAbsOrigin(iDefibTarget, fDefibPos);
		float fDefibDist = GetVectorDistance(g_fClientEyePos[iClient], fDefibPos, true);
		if (fDefibDist <= (g_fCvar_DefibRevive_ScanDist*g_fCvar_DefibRevive_ScanDist) && !IsDefibPosDamaging(iClient, fDefibPos))
		{
			g_iSurvivorBot_DefibTarget[iClient] = iDefibTarget;

			if (SurvivorHasHealthKit(iClient) == 2)
			{
				if (L4D2_GetPlayerUseActionTarget(iClient) == iDefibTarget || fDefibDist <= 9216.0)
				{
					if (iCurWeapon == iWpnSlots[3])
					{
						SnapViewToPosition(iClient, fDefibPos);
						PressAttackButton(iClient, iButtons);
					}
					else
					{
						SwitchWeaponSlot(iClient, 3);
					}
				}
				else if (GetGameTime() >= g_fSurvivorBot_NextDefibCheckTime[iClient] && !IsSurvivorBotBlindedByVomit(iClient) && !IsFinaleEscapeVehicleArrived() && LBI_IsSurvivorBotAvailable(iClient) && LBI_IsReachablePosition(iClient, fDefibPos) && 
					g_iSurvivorBot_NearbyInfectedCount[iClient] < GetCommonHitsUntilDown(iClient, 0.66) && !IsValidClient(g_iSurvivorBot_IncapacitatedFriend[iClient])
				)
				{
					g_fSurvivorBot_NextDefibCheckTime[iClient] = GetGameTime() + BOT_APPROACH_CHECK_INTERVAL;
					SetMoveToPosition(iClient, fDefibPos, 2, "DefibPlayer");
				}
			}
		}
	}

	if (g_iCvar_ItemScavenge_Items != 0 && GetGameTime() > g_fSurvivorBot_NextScavengeItemScanTime[iClient])
	{
		g_iSurvivorBot_ScavengeItem[iClient] = CheckForItemsToScavenge(iClient);		
		g_fSurvivorBot_ScavengeScanTime[iClient] = GetGameTime() + GetRandomFloat(0.5, 1.5);
		g_fSurvivorBot_NextScavengeItemScanTime[iClient] = g_fSurvivorBot_ScavengeScanTime[iClient];
	}

	int iScavengeItem = g_iSurvivorBot_ScavengeItem[iClient];
	if (IsEntityExists(iScavengeItem))
	{
		if (IsValidClient(GetEntityOwner(iScavengeItem)))
		{
			ClearMoveToPosition(iClient, "ScavengeItem");
			g_iSurvivorBot_ScavengeItem[iClient] = -1;
		}
		else
		{
			int iUseButton = IN_USE;
			bool bHoldKey = false;
			float fUseRange = g_fCvar_ItemScavenge_PickupRange;

			static char sScavengeItem[64]; GetEntityClassname(iScavengeItem, sScavengeItem, sizeof(sScavengeItem));
			if (strcmp(sScavengeItem, "point_prop_use_target") == 0 && IsSurvivorCarryingProp(iClient))
			{
				iUseButton = IN_ATTACK;
				bHoldKey = true;
				fUseRange = 32.0;

				if (g_bSurvivorBot_ScavengeUseExtended[iClient] && L4D2_GetPlayerUseAction(iClient) != L4D2UseAction_PouringGas)
				{
					g_bSurvivorBot_ScavengeUseExtended[iClient] = false;
				}
			}
			else if (strcmp(sScavengeItem, "func_button_timed") == 0)
			{
				bHoldKey = true;
				fUseRange = 96.0;

				if (g_bSurvivorBot_ScavengeUseExtended[iClient] && L4D2_GetPlayerUseAction(iClient) == L4D2UseAction_None && g_fSurvivorBot_NextScavengeItemScanTime[iClient] == g_fSurvivorBot_ScavengeScanTime[iClient])
				{
					g_bSurvivorBot_ScavengeUseExtended[iClient] = false;
				}
			}

			if (!IsSurvivorBotBlindedByVomit(iClient) && (bHoldKey || !IsSurvivorBusy(iClient, true, true, true)))
			{
				float fItemPos[3]; GetEntityCenteroid(iScavengeItem, fItemPos);
				if (GetVectorDistance(g_fClientEyePos[iClient], fItemPos, true) <= (fUseRange*fUseRange) && IsEntityVisibleCached(iClient, iScavengeItem))
				{
					if (GetGameTime() > g_fSurvivorBot_NextUsePressTime[iClient])
					{
						BotLookAtPosition(iClient, fItemPos, 0.33);

						// The FindUseEntity detour always answers with this item, so there's no need to ask the game.
						if (!bHoldKey)
						{
							g_fSurvivorBot_NextUsePressTime[iClient] = GetGameTime() + GetRandomFloat(0.1, 0.33);
							if (GetRandomInt(1, 2) == 1)iButtons |= iUseButton;
						}
						else
						{ 
							iButtons |= iUseButton;
							if (!g_bSurvivorBot_ScavengeUseExtended[iClient])
							{
								g_bSurvivorBot_ScavengeUseExtended[iClient] = true;

								float fAddTime;
								if (strcmp(sScavengeItem, "func_button_timed") != 0)
								{
									fAddTime = g_fCvar_GasCanUseDuration;
								}
								else
								{
									fAddTime = GetEntPropFloat(iScavengeItem, Prop_Data, "m_nUseTime");
								}

								if (L4D2_IsUnderAdrenalineEffect(iClient))fAddTime *= 0.5;
								g_fSurvivorBot_NextScavengeItemScanTime[iClient] = GetGameTime() + fAddTime + 0.33;
							}
						}
					}
				}
				// Checked a few times per second, with the leash left for last.
				else if (GetGameTime() >= g_fSurvivorBot_NextScavengeCheckTime[iClient])
				{
					g_fSurvivorBot_NextScavengeCheckTime[iClient] = GetGameTime() + BOT_APPROACH_CHECK_INTERVAL;

					float fScavengePos[3];
					fScavengePos = fItemPos;

					int iScavengeArea = L4D_GetNearestNavArea(fItemPos, _, true, true, false);
					if (iScavengeArea)LBI_GetClosestPointOnNavArea(iScavengeArea, fItemPos, fScavengePos);

					bool bScavengeLeashApproved = (g_iSurvivorBot_NearbyInfectedCount[iClient] < GetCommonHitsUntilDown(iClient, 0.66) && !LBI_IsDamagingPosition(fScavengePos) && !IsFinaleEscapeVehicleArrived() &&
						(!IsValidClient(iTankTarget) || GetClientDistance(iClient, iTankTarget, true) > 262144.0 && GetVectorDistance(g_fClientAbsOrigin[iTankTarget], fScavengePos, true) > 147456.0) &&
						!IsValidClient(g_iSurvivorBot_IncapacitatedFriend[iClient]) && LBI_IsReachableEntity(iClient, iScavengeItem));

					float fLeaderDist = -2.0;
					float fLeashDist = 0.0;
					if (bScavengeLeashApproved)
					{
						float fMaxDist = g_fCvar_ItemScavenge_ApproachVisibleRange;
						if (iScavengeArea && !LBI_IsNavAreaPartiallyVisible(iScavengeArea, g_fClientEyePos[iClient], iClient))
							fMaxDist = g_fCvar_ItemScavenge_ApproachRange;

						if (!bTeamHasHumanPlayer)fMaxDist *= g_fCvar_ItemScavenge_NoHumansRangeMultiplier;
						fLeashDist = (fMaxDist * 0.75);

						// The leader is just the bot furthest along the map, so a leader bot is still leashed to its nearest teammate.
						int iLeashTo = ((iTeamLeader != iClient && IsValidClient(iTeamLeader)) ? iTeamLeader : (IsValidClient(iNearestTeammate) ? iNearestTeammate : 0));
						if (iLeashTo)
						{
							// No path is shorter than the straight line, so the search is only needed inside it.
							fLeaderDist = (GetVectorDistance(g_fClientAbsOrigin[iLeashTo], fScavengePos, true) > (fLeashDist*fLeashDist) ? -1.0 : GetClientTravelDistance(iLeashTo, fScavengePos, true, (fLeashDist * BOT_PATH_LIMIT_MARGIN)));
							bScavengeLeashApproved = (fLeaderDist != -1.0 && fLeaderDist <= (fLeashDist*fLeashDist));
						}
					}

					if (g_bCvar_Debug && GetGameTime() > g_fDebug_NextScavengeLogTime[iClient])
					{
						g_fDebug_NextScavengeLogTime[iClient] = GetGameTime() + 0.5;
						DebugLog("client=%d(%N) SCAVENGE-LEASH item=%d teamLeader=%d(%s) nearestTeammate=%d leaderDist=%.1f maxDistSq=%.1f approved=%d",
							iClient, iClient, iScavengeItem, iTeamLeader, (iTeamLeader == iClient ? "IS SELF" : "not self"),
							iNearestTeammate, fLeaderDist, (fLeashDist*fLeashDist), bScavengeLeashApproved);
					}

					if (bScavengeLeashApproved)
					{
						SetMoveToPosition(iClient, fScavengePos, 1, "ScavengeItem");
					}
					else
					{
						ClearMoveToPosition(iClient, "ScavengeItem");
						g_iSurvivorBot_ScavengeItem[iClient] = -1;
					}
				}
			}
		}
	}
}

Action OnSurvivorSwitchWeapon(int iClient, int iWeapon) 
{
	if (!IsClientSurvivor(iClient) || !IsFakeClient(iClient) || g_bSurvivorBot_ForceSwitchWeapon[iClient] || !IsEntityExists(iWeapon) || L4D_IsPlayerIncapacitated(iClient))
	{
		g_bSurvivorBot_ForceSwitchWeapon[iClient] = false;
		return Plugin_Continue;
	}

	int iCurWeapon = L4D_GetPlayerCurrentWeapon(iClient);
	if (iCurWeapon == -1 || iWeapon == iCurWeapon || GetWeaponClip1(iCurWeapon) < 0)
	{
		g_bSurvivorBot_ForceSwitchWeapon[iClient] = false;
		return Plugin_Continue;
	}

	if (iWeapon == GetClientWeaponInventory(iClient, 0))
	{
		if (GetSurvivorBotWeaponPreference(iClient) == L4D_WEAPON_PREFERENCE_SECONDARY)
		{
			SwitchWeaponSlot(iClient, 1);
			return Plugin_Handled;
		}

		if (iCurWeapon == GetClientWeaponInventory(iClient, 1))
		{
			if (SurvivorHasMeleeWeapon(iClient))
			{
				if (GetGameTime() <= g_fSurvivorBot_BlockWeaponSwitchTime[iClient])
				{
					return Plugin_Handled;
				}
			}
			else if (IsWeaponReloading(iCurWeapon) || GetWeaponClip1(iCurWeapon) != GetWeaponClipSize(iCurWeapon) && !LBI_IsSurvivorInCombat(iClient))
			{
				g_bSurvivorBot_ForceWeaponReload[iClient] = true;
				return Plugin_Handled;
			}
		}
	}
	else if (iWeapon == GetClientWeaponInventory(iClient, 1)) 
	{
		if (iCurWeapon == GetClientWeaponInventory(iClient, 0) && GetClientPrimaryAmmo(iClient) > 0)
		{
			if (g_iSurvivorBot_NearbyInfectedCount[iClient] < g_iCvar_ImprovedMelee_SwitchCount2 && SurvivorHasMeleeWeapon(iClient) == 2)
			{
				return Plugin_Handled;
			}

			if (GetSurvivorBotWeaponPreference(iClient) != L4D_WEAPON_PREFERENCE_SECONDARY && (!SurvivorHasMeleeWeapon(iClient) || g_iSurvivorBot_NearbyInfectedCount[iClient] < g_iCvar_ImprovedMelee_SwitchCount) && ((g_bCvar_DontSwitchToPistol && SurvivorHasSniperRifle(iClient)) || SurvivorHasShotgun(iClient)))
			{
				return Plugin_Handled;
			}
		}
	}

	if (IsEntityExists(g_iSurvivorBot_DefibTarget[iClient]) && iCurWeapon == GetClientWeaponInventory(iClient, 3) && SurvivorHasHealthKit(iClient) == 2)
	{
		return Plugin_Handled;
	}

	if (g_bCvar_AlwaysCarryProp && IsSurvivorCarryingProp(iClient) && (iWeapon == GetClientWeaponInventory(iClient, 0) || iWeapon == GetClientWeaponInventory(iClient, 1)))
	{
		int iTeamCount = (g_iSurvivorBot_NearbyFriends[iClient] / 2); if (iTeamCount < 1)iTeamCount = 1;
		int iDropLimitCount = RoundFloat(GetCommonHitsUntilDown(iClient, 0.5) * float(iTeamCount));
		if (g_iSurvivorBot_ThreatInfectedCount[iClient] < iDropLimitCount)return Plugin_Handled;
	}

	g_bSurvivorBot_ForceSwitchWeapon[iClient] = false;
	return Plugin_Continue;
}

Action OnSurvivorTakeDamage(int iClient, int &iAttacker, int &iInflictor, float &fDamage, int &iDamageType) 
{
	if (GetClientTeam(iClient) != 2 || !IsPlayerAlive(iClient))
		return Plugin_Continue;

	if (IsWitch(iAttacker))
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
			if (iWitchRef == iAttacker)
			{
				g_hWitchList.Set(i, GetClientUserId(iClient), 1);
				break;
			}
		}
	}

	if (!IsFakeClient(iClient))
		return Plugin_Continue;

	if (IsSurvivorBotBlindedByVomit(iClient) && (IsValidClient(iAttacker) && GetClientTeam(iAttacker) == 3 || IsCommonInfected(iAttacker)))
	{
		float fLookPos[3]; GetEntityCenteroid(iAttacker, fLookPos);
		BotLookAtPosition(iClient, fLookPos, 1.0);
		g_bSurvivorBot_ForceBash[iClient] = true;
	}

	if (g_bCvar_NoFallDmgOnLadderFail && iDamageType & DMG_FALL && GetGameTime() <= g_fSurvivorBot_TimeSinceLeftLadder[iClient])
	{
		fDamage = 0.0;
		return Plugin_Changed; 
	}

	if (!g_bCvar_SpitterAcidEvasion || !IsEntityExists(iInflictor) || strcmp(g_sSurvivorBot_MovePos_Name[iClient], "EscapeInferno") == 0)
		return Plugin_Continue; 

	static char sInfClass[16]; GetEntityClassname(iInflictor, sInfClass, sizeof(sInfClass));
	if (strcmp(sInfClass, "insect_swarm") != 0 && strcmp(sInfClass, "inferno") != 0)return Plugin_Continue; 

	// Runs on every hit, so the search is skipped when the bot already has another order.
	float fNow = GetGameTime();
	if (!CanSetMoveToPosition(iClient, 4) || fNow < g_fSurvivorBot_NextEscapeSearchTime[iClient])
		return Plugin_Continue;

	g_fSurvivorBot_NextEscapeSearchTime[iClient] = fNow + BOT_ESCAPE_SEARCH_INTERVAL;

	int iNavArea;
	float fCurDist, fLastDist = -1.0;
	float fEscapePos[3], fPathPos[3];
	for (int i = 0; i < BOT_ESCAPE_SEARCH_TRIES; i++)
	{
		LBI_TryGetPathableLocationWithin(iClient, 250.0 + (100.0 * i), fPathPos);
		if (!IsValidVector(fPathPos))continue;

		// No path is shorter than the straight line, so this one can't beat the best so far.
		if (fLastDist != -1.0 && GetVectorDistance(g_fClientAbsOrigin[iClient], fPathPos, true) >= fLastDist)
			continue;

		iNavArea = L4D_GetNearestNavArea(fPathPos);
		if (!iNavArea || LBI_IsDamagingNavArea(iNavArea, true) || !LBI_IsReachableNavArea(iClient, iNavArea))
			continue;

		fCurDist = GetClientTravelDistance(iClient, fPathPos, true, BOT_ESCAPE_SEARCH_MAX);
		if (fCurDist == -1.0 || fLastDist != -1.0 && fCurDist >= fLastDist)
			continue;

		fLastDist = fCurDist;
		fEscapePos = fPathPos;
	}

	if (IsValidVector(fEscapePos))
	{
		SetMoveToPosition(iClient, fEscapePos, 4, "EscapeInferno", BOT_ESCAPE_EXTRA_TIME, 5.0, true, true);

		// The time of the order is short, and the regular interval between commands could take most of it.
		if (strcmp(g_sSurvivorBot_MovePos_Name[iClient], "EscapeInferno") == 0)
			g_fSurvivorBot_NextMoveCommandTime[iClient] = 0.0;
	}

	return Plugin_Continue; 
}

// Fire doesn't start and stop that fast, so the answer is kept for a moment.
bool IsDefibPosDamaging(int iClient, const float fPos[3])
{
	if (GetGameTime() >= g_fSurvivorBot_NextDefibDamageCheckTime[iClient])
	{
		g_fSurvivorBot_NextDefibDamageCheckTime[iClient] = GetGameTime() + BOT_APPROACH_CHECK_INTERVAL;
		g_bSurvivorBot_DefibPosDamaging[iClient] = LBI_IsDamagingPosition(fPos);
	}
	return g_bSurvivorBot_DefibPosDamaging[iClient];
}
