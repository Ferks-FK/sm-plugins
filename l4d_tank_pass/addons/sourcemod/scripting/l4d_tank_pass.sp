#define PLUGIN_VERSION "2.7.0"

#pragma semicolon 1
#pragma newdecls required
/*
|--------------------------------------------------------------------------
| INCLUDES
|--------------------------------------------------------------------------
*/
#include <sourcemod>
#include <sdktools>
#include <left4dhooks>
#include <colors>
#undef REQUIRE_PLUGIN
#include <adminmenu>
/*
|--------------------------------------------------------------------------
| MACROS
|--------------------------------------------------------------------------
*/
#define IGNITE_TIME 3600.0
#define DISPLAY_TIME 10
#define FORWARD_ARGS "TP_OnTankPass", ET_Ignore, Param_Cell, Param_Cell
#define SOURCEMOD_V_COMPAT (SOURCEMOD_V_MAJOR >= 1 && SOURCEMOD_V_MINOR >= 10 || SOURCEMOD_V_MAJOR > 2)

/*
|--------------------------------------------------------------------------
| VARIABLES
|--------------------------------------------------------------------------
*/
enum
{
	Validate_Default,
	Validate_NotiyfyTarget,
	Validate_SkipTarget
}

enum
{
	Menu_Pass,
	Menu_ForcePass,
	Menu_ForceAdmPass,
	Menu_Take
}

#if SOURCEMOD_V_COMPAT
GlobalForward g_fwdOnTankPass;
#else
Handle g_fwdOnTankPass;
#endif
int g_iCvarTankHealth, g_iCvarPassedCount, g_iTakeOverPassedCount, g_iTankId[MAXPLAYERS+1], g_iPassedCount[MAXPLAYERS+1];
char g_sCvarCmd[32];
TopMenu g_hTopMenu;
bool g_bCvarDamage, g_bCvarFire, g_bCvarReplace, g_bCvarExtinguish, g_bCvarNotify, g_bCvarQuickPass, g_bCvarMenu, g_bCvarConfirm, g_bIsFinale, g_bIsIgnited[MAXPLAYERS+1], g_bIsBlocked[MAXPLAYERS+1];
ConVar g_hCvarTankHealth, g_hCvarTankBonusHealth;

// debug cvar + anti-double-count guard
bool g_bCvarDebug;
bool g_bForcingPass;   // reentrancy guard: true while WE are executing a frustration pass, so nested forwards/replace events don't loop or double-count
bool g_bFrustrationPass; // true only during a frustration pass; tells TransferPass to skip its increment
int g_iPendingPassTank = -1;    // userid of tank waiting for a deferred frustration pass (-1 = none)
int g_iPendingPassTarget = -1;  // userid of the chosen human target

int ZC_TANK;
public APLRes AskPluginLoad2(Handle myself, bool late, char[] error, int err_max)
{
	EngineVersion test = GetEngineVersion();

	if( test == Engine_Left4Dead )
	{
		ZC_TANK = 5;
	}
	else if( test == Engine_Left4Dead2 )
	{
		ZC_TANK = 8;
	}
	else
	{
		strcopy(error, err_max, "Plugin only supports Left 4 Dead 1 & 2.");
		return APLRes_SilentFailure;
	}

	return APLRes_Success;
}

public Plugin myinfo =
{
	name = "[L4D & L4D2] Tank Pass",
	author = "Scratchy [Laika] & raziEiL [disawar1], harry, Ferks-FK",
	description = "Allows the Tank to pass control to another player.",
	version = PLUGIN_VERSION,
	url = "https://steamcommunity.com/id/raziEiL/"
}

public void OnPluginStart()
{
	// forward void TP_OnTankPass(int old_tank, int new_tank);
#if SOURCEMOD_V_COMPAT
	g_fwdOnTankPass = new GlobalForward(FORWARD_ARGS);
#else
	g_fwdOnTankPass = CreateGlobalForward(FORWARD_ARGS);
#endif
	LoadTranslations("l4d_tank_pass.phrases");
	LoadTranslations("common.phrases");

	g_hCvarTankHealth = FindConVar("z_tank_health");
	g_hCvarTankBonusHealth = FindConVar("versus_tank_bonus_health");
	g_iCvarTankHealth = CalcTankHealth();

	if (g_hCvarTankBonusHealth)
		g_hCvarTankBonusHealth.AddChangeHook(OnCvarChange_TankHealth);
	g_hCvarTankHealth.AddChangeHook(OnCvarChange_TankHealth);

	CreateConVar("l4d_tank_pass_version", PLUGIN_VERSION, "Tank Pass plugin version.", FCVAR_NOTIFY|FCVAR_DONTRECORD);

	ConVar cVar = CreateConVar("l4d_tank_pass_command", "sm_tankhud", "Execute command according convar value on old_tank and new_tank to close 3d party HUD.", FCVAR_NOTIFY);
	cVar.GetString(g_sCvarCmd, sizeof(g_sCvarCmd));
	cVar.AddChangeHook(OnCvarChange_Exec);

	cVar = CreateConVar("l4d_tank_pass_replace", "1", "0=Kill the alive player before the Tank pass, 1=Replace the alive player with an infected bot before the Tank pass.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_bCvarReplace = cVar.BoolValue;
	cVar.AddChangeHook(OnCvarChange_Replace);

	cVar = CreateConVar("l4d_tank_pass_damage", "0", "0=Allow to pass the Tank when taking any damage, 1=Prevent to pass the Tank when taking any damage.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_bCvarDamage = cVar.BoolValue;
	cVar.AddChangeHook(OnCvarChange_Damage);

	cVar = CreateConVar("l4d_tank_pass_fire", "1", "0=Allow to pass the Tank when on fire, 1=Prevent to pass the Tank when on fire.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_bCvarFire = cVar.BoolValue;
	cVar.AddChangeHook(OnCvarChange_Fire);

	cVar = CreateConVar("l4d_tank_pass_extinguish", "0", "If \"l4d_tank_pass_fire\" convar set to 0: 0=Ignite the new Tank when passed, 1=Extinguish the new Tank when passed.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_bCvarExtinguish = cVar.BoolValue;
	cVar.AddChangeHook(OnCvarChange_Extinguish);

	cVar = CreateConVar("l4d_tank_pass_takeover", "1", "Sets the Tank passed count according convar value when taking control of the Tank AI. If >1 the tank will be replaced with a bot when the his frustration reaches 0.", FCVAR_NOTIFY, true, 1.0, true, 2.0);
	g_iTakeOverPassedCount = cVar.IntValue;
	cVar.AddChangeHook(OnCvarChange_TakeOver);

	cVar = CreateConVar("l4d_tank_pass_notify", "1", "0=Off, 1=Display pass command info to the Tank through chat messages.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_bCvarNotify = cVar.BoolValue;
	cVar.AddChangeHook(OnCvarChange_Notify);

	cVar = CreateConVar("l4d_tank_pass_logic", "1", "0=\"X gets Tank\" window, 1=Quick pass except finales", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_bCvarQuickPass = cVar.BoolValue;
	cVar.AddChangeHook(OnCvarChange_QuickPass);

	cVar = CreateConVar("l4d_tank_pass_menu", "1", "0=Off, 1=Display the menu when the Tank is spawned", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_bCvarMenu = cVar.BoolValue;
	cVar.AddChangeHook(OnCvarChange_Menu);

	cVar = CreateConVar("l4d_tank_pass_count", "1", "0=Off, >0=The number of times the Tank can be passed by plugin (Frustration counts as pass).", FCVAR_NOTIFY, true, 0.0);
	g_iCvarPassedCount = cVar.IntValue;
	cVar.AddChangeHook(OnCvarChange_PassCount);

	cVar = CreateConVar("l4d_tank_pass_confirm", "1", "0=Off, 1=Ask the player if he wants to get the Tank.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_bCvarConfirm = cVar.BoolValue;
	cVar.AddChangeHook(OnCvarChange_Confirm);

	cVar = CreateConVar("l4d_tank_pass_debug", "0", "0=Off, 1=Print debug messages to server console/chat for pass logic.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_bCvarDebug = cVar.BoolValue;
	cVar.AddChangeHook(OnCvarChange_Debug);

	HookEvent("tank_spawn", Event_TankSpawn);
	HookEvent("finale_start", Event_FinalStart, EventHookMode_PostNoCopy);
	HookEvent("round_start", Event_RoundStart, EventHookMode_PostNoCopy);
	HookEvent("entity_killed", Event_EntityKilled);
	HookEvent("player_bot_replace", Event_PlayerBotReplace);
	HookEvent("bot_player_replace", Event_BotPlayerReplace);

	RegConsoleCmd("sm_pass", Command_TankPass, "Pass the Tank control to another player.");
	RegConsoleCmd("sm_passtank", Command_TankPass, "Pass the Tank control to another player.");
	RegConsoleCmd("sm_tankpass", Command_TankPass, "Pass the Tank control to another player.");
	RegAdminCmd("sm_forcepass", Command_ForcePass, ADMFLAG_KICK, "sm_forcepass <#userid|name> - Force to pass the Tank to target player.");
	RegAdminCmd("sm_taketank", Command_TakeTank, ADMFLAG_KICK, "sm_taketank <#userid|name> - Take control of the Tank AI.");

	TopMenu topmenu;
	if (LibraryExists("adminmenu") && ((topmenu = GetAdminTopMenu()) != null))
		OnAdminMenuReady(topmenu);

	AutoExecConfig(true, "l4d_tank_pass");
}
/*
|--------------------------------------------------------------------------
| DEBUG
|--------------------------------------------------------------------------
*/
void PrintDebug(const char[] format, any ...)
{
	if (!g_bCvarDebug)
		return;

	char sBuffer[256];
	VFormat(sBuffer, sizeof(sBuffer), format, 2);
	LogMessage("[TankPass-DBG] %s", sBuffer);
	PrintToServer("[TankPass-DBG] %s", sBuffer);
	// also to infected humans, so you can watch live in-game
	for (int i = 1; i <= MaxClients; i++)
		if (IsClientInGame(i) && !IsFakeClient(i) && GetClientTeam(i) == 3)
			PrintToConsole(i, "[TankPass-DBG] %s", sBuffer);
}

public void OnCvarChange_Debug(ConVar convar, const char[] oldValue, const char[] newValue)
{
	if (!StrEqual(oldValue, newValue))
		g_bCvarDebug = convar.BoolValue;
}
/*
|--------------------------------------------------------------------------
| OVERRIDE NATIVE 2-PASS LIMIT
|--------------------------------------------------------------------------
| The game natively only lets a Tank go to a human up to ~2 times per round
| (L4D2Direct TankPassedCount), then forces AI. We intercept the moment the
| engine tries to offer the Tank to a bot: while the plugin still has passes
| left, we redirect to a random human and keep the native counter low so the
| game never forces AI on its own. Only when the plugin limit is reached do we
| let it go to AI.
*/
public Action L4D_OnTryOfferingTankBot(int tank_index, bool &enterStatis)
{
	PrintDebug("OnTryOfferingTankBot: tank=%d enterStatis=%d forcing=%d passCount=%d limit=%d nativePassed=%d",
		tank_index, (enterStatis ? 1 : 0), (g_bForcingPass ? 1 : 0),
		IsClient(tank_index) ? g_iPassedCount[tank_index] : -1, g_iCvarPassedCount, L4D2Direct_GetTankPassedCount());

	// Feature off -> vanilla behaviour
	if (g_iCvarPassedCount == 0)
	{
		PrintDebug("  -> feature OFF. Continue.");
		return Plugin_Continue;
	}

	// If we're the ones executing a pass right now, let it flow (avoid recursion)
	if (g_bForcingPass)
	{
		PrintDebug("  -> reentrancy guard active, Continue (our own pass in progress).");
		return Plugin_Continue;
	}

	// Only intervene for a live HUMAN tank
	if (!IsValidTank(tank_index) || IsFakeClient(tank_index))
	{
		PrintDebug("  -> not a valid human tank. Continue.");
		return Plugin_Continue;
	}

	// Safety: L4D2Direct_SetTankPassedCount is GLOBAL on the director. The native docs warn
	// that manipulating it with more than one tank alive can cause weird behaviour, so we
	// only override the limit when exactly one tank is in play (matches ValidateOffer's check).
	if (GetTankCount() > 1)
	{
		PrintDebug("  -> more than one tank alive (%d). Not intervening (global counter safety). Continue.", GetTankCount());
		return Plugin_Continue;
	}

	// Passes still available?
	if (g_iPassedCount[tank_index] < g_iCvarPassedCount)
	{
		int target = FindRandomInfectedHuman(tank_index);
		PrintDebug("  -> passes left (%d < %d). Random human target=%d", g_iPassedCount[tank_index], g_iCvarPassedCount, target);

		if (target == -1)
		{
			PrintDebug("  -> no human available. Continue (engine may send to AI).");
			return Plugin_Continue;
		}

		// IMPORTANT: we must NOT execute the pass here. This forward fires from *inside* the
		// engine's OnTryOfferingTankBot routine; calling TankPass() (which re-enters
		// ReplaceTank / TryOfferingTankBot) reenters the engine mid-routine and crashes it
		// (null deref). Instead we defer execution to the next frame, once the engine has
		// finished its own routine and state is stable.
		if (g_iPendingPassTank != -1)
		{
			PrintDebug("  -> a deferred pass is already queued, skipping to avoid overlap. Continue.");
			return Plugin_Continue;
		}

		g_iPendingPassTank = GetClientUserId(tank_index);
		g_iPendingPassTarget = GetClientUserId(target);
		RequestFrame(Frame_ExecutePass);

		enterStatis = false;
		PrintDebug("  -> queued deferred pass tank=%d -> target=%d (RequestFrame). Plugin_Handled (block AI now).", tank_index, target);
		return Plugin_Handled;
	}

	// Passes exhausted -> allow AI
	PrintDebug("  -> passes exhausted (%d >= %d). Continue -> AI.", g_iPassedCount[tank_index], g_iCvarPassedCount);
	return Plugin_Continue;
}

// Executed one frame after the forward, safely OUTSIDE the engine's OnTryOfferingTankBot call stack.
void Frame_ExecutePass(any data)
{
	int tank = GetClientOfUserId(g_iPendingPassTank);
	int target = GetClientOfUserId(g_iPendingPassTarget);
	g_iPendingPassTank = -1;
	g_iPendingPassTarget = -1;

	PrintDebug("Frame_ExecutePass: tank=%d target=%d", tank, target);

	// Revalidate: players may have left / states may have changed during the frame gap.
	if (!IsValidTank(tank))
	{
		PrintDebug("  -> tank no longer valid, aborting deferred pass.");
		return;
	}
	if (!IsValidTarget(target))
	{
		// target gone; try another random human
		target = FindRandomInfectedHuman(tank);
		PrintDebug("  -> original target gone, re-picked target=%d", target);
		if (target == -1)
		{
			PrintDebug("  -> still no human available, aborting (tank stays / engine decides next time).");
			return;
		}
	}

	int newCount = g_iPassedCount[tank] + 1;

	g_bForcingPass = true;
	g_bFrustrationPass = true;
	PrintDebug("  -> executing deferred pass tank=%d -> target=%d (count -> %d/%d)", tank, target, newCount, g_iCvarPassedCount);
	TankPass(tank, target, 0, true);
	g_bFrustrationPass = false;
	g_bForcingPass = false;

	// Apply count to whoever holds the tank now.
	int holder = GetTank();
	if (holder == 0)
		holder = target;
	g_iPassedCount[holder] = newCount;
	if (g_iPassedCount[holder] > g_iCvarPassedCount)
		g_iPassedCount[holder] = g_iCvarPassedCount;

	// Keep native counter low so the game never forces AI on its own.
	L4D2Direct_SetTankPassedCount(0);

	PrintDebug("  -> deferred pass done. applied count %d/%d to holder=%d. nativePassed reset to 0.", g_iPassedCount[holder], g_iCvarPassedCount, holder);
}

int FindRandomInfectedHuman(int exclude)
{
	int candidates[MAXPLAYERS + 1];
	int count = 0;
	for (int i = 1; i <= MaxClients; i++)
	{
		if (i == exclude)
			continue;
		if (IsInfected(i) && !IsFakeClient(i) && !IsPlayerTank(i))
			candidates[count++] = i;
	}
	if (count == 0)
		return -1;
	return candidates[GetRandomInt(0, count - 1)];
}
/*
|--------------------------------------------------------------------------
| ADM MENU
|--------------------------------------------------------------------------
*/
public void OnAdminMenuReady(Handle aTopMenu)
{
	TopMenu topmenu = TopMenu.FromHandle(aTopMenu);

	if (topmenu == g_hTopMenu)
		return;

	g_hTopMenu = topmenu;

	TopMenuObject player_commands = g_hTopMenu.FindCategory(ADMINMENU_PLAYERCOMMANDS);

	if (player_commands != INVALID_TOPMENUOBJECT){
		g_hTopMenu.AddItem("sm_forcepass", AdminMenu_ForcePass, player_commands, "sm_forcepass", ADMFLAG_KICK);
		g_hTopMenu.AddItem("sm_taketank", AdminMenu_TakeTank, player_commands, "sm_taketank", ADMFLAG_KICK);
	}
}

public void AdminMenu_ForcePass(TopMenu topmenu, TopMenuAction action, TopMenuObject object_id, int param, char[] buffer, int maxlength)
{
	switch (action)
	{
		case TopMenuAction_DisplayOption:
		{
			Format(buffer, maxlength, "%T", "phrase10", param);
		}
		case TopMenuAction_SelectOption:
		{
			if (GetTank())
				TankPassMenu(param, Menu_ForceAdmPass);
			else {
				CPrintToChat(param, "%t", "phrase7");
				if (g_hTopMenu != null)
					g_hTopMenu.Display(param, TopMenuPosition_LastCategory);
			}
		}
	}
}

public int MenuForceAdmHandler(Menu menu, MenuAction action, int admin, int param2)
{
	switch (action)
	{
		case MenuAction_Select:
		{
			char sId[12];
			menu.GetItem(param2, sId, sizeof(sId));
			int target = GetClientOfUserId(StringToInt(sId));
			int tank = GetTank();

			if (ValidateOffer(Validate_Default, tank, target, admin))
				TankPass(tank, target, admin);

			if (g_hTopMenu != null)
				g_hTopMenu.Display(admin, TopMenuPosition_LastCategory);

			return 0;
		}
		case MenuAction_Cancel:
		{
			if (param2 == MenuCancel_ExitBack && g_hTopMenu != null)
				g_hTopMenu.Display(admin, TopMenuPosition_LastCategory);

			return 0;
		}
		case MenuAction_End:
		{
			delete menu;

			return 0;
		}
	}

	return 0;
}

public void AdminMenu_TakeTank(TopMenu topmenu, TopMenuAction action, TopMenuObject object_id, int param, char[] buffer, int maxlength)
{
	switch (action)
	{
		case TopMenuAction_DisplayOption:
		{
			Format(buffer, maxlength, "%T", "phrase12", param);
		}
		case TopMenuAction_SelectOption:
		{
			if (GetTankBot())
				TankPassMenu(param, Menu_Take);
			else {
				CPrintToChat(param, "%t", "phrase7");
				if (g_hTopMenu != null)
					g_hTopMenu.Display(param, TopMenuPosition_LastCategory);
			}
		}
	}
}

public int MenuTakeAdmHandler(Menu menu, MenuAction action, int admin, int param2)
{
	switch (action)
	{
		case MenuAction_Select:
		{
			char sId[12];
			menu.GetItem(param2, sId, sizeof(sId));
			TakeOverTank(GetClientOfUserId(StringToInt(sId)), admin);

			if (g_hTopMenu != null)
				g_hTopMenu.Display(admin, TopMenuPosition_LastCategory);

			return 0;
		}
		case MenuAction_Cancel:
		{
			if (param2 == MenuCancel_ExitBack && g_hTopMenu != null)
				g_hTopMenu.Display(admin, TopMenuPosition_LastCategory);

			return 0;
		}
		case MenuAction_End:
		{
			delete menu;

			return 0;
		}
	}

	return 0;
}
/*
|--------------------------------------------------------------------------
| MENU
|--------------------------------------------------------------------------
*/
void PreTankPassMenu(int client)
{
	if (client && ValidateOffer(Validate_SkipTarget, client))
		TankPassMenu(client, g_bCvarConfirm ? Menu_Pass : Menu_ForcePass);
}

void TankPassMenu(int client, int menuType = Menu_Pass)
{
	bool hasTarget;
	Menu menu;

	switch (menuType)
	{
		case Menu_Pass:
			menu = new Menu(MenuPassHandler);
		case Menu_ForcePass:
			menu = new Menu(MenuForceHandler);
		case Menu_ForceAdmPass:
			menu = new Menu(MenuForceAdmHandler);
		case Menu_Take:
			menu = new Menu(MenuTakeAdmHandler);
	}

	menu.SetTitle("%T", "phrase4", client);

	int players[MAXPLAYERS + 1];
	int count = 0;

	for (int i = 1; i <= MaxClients; i++)
	{
		if (IsValidTarget(i))
		{
			players[count++] = i;
			hasTarget = true;
		}
	}

	// Sort by name
	SortCustom1D(players, count, SortPlayersByName);

	// Add to menu
	char name[MAX_NAME_LENGTH];
	char sId[12];

	for (int i = 0; i < count; i++)
	{
		int userid = GetClientUserId(players[i]);
		IntToString(userid, sId, sizeof(sId));
		GetClientName(players[i], name, sizeof(name));

		menu.AddItem(sId, name);
	}

	if (!hasTarget){
		CPrintToChat(client, "%t", "phrase7");
		delete menu;
		return;
	}
	if (menuType == Menu_Pass || menuType == Menu_ForcePass){
		ExecCmd(client);
		menu.ExitButton = true;
		menu.Display(client, MENU_TIME_FOREVER);
	}
	else {
		menu.ExitBackButton = true;
		menu.Display(client, MENU_TIME_FOREVER);
	}
}

public int SortPlayersByName(int elem1, int elem2, const int[] array, Handle hndl)
{
	char name1[64], name2[64];

	GetClientName(elem1, name1, sizeof(name1));
	GetClientName(elem2, name2, sizeof(name2));

	return strcmp(name1, name2, false);
}

public int MenuForceHandler(Menu menu, MenuAction action, int tank, int param2)
{
	switch (action)
	{
		case MenuAction_Select:
		{
			char sId[12];
			menu.GetItem(param2, sId, sizeof(sId));
			int target = GetClientOfUserId(StringToInt(sId));

			if (ValidateOffer(Validate_Default, tank, target))
				TankPass(tank, target);

			return 0;
		}
		case MenuAction_End:
		{
			delete menu;

			return 0;
		}
	}

	return 0;
}

public int MenuPassHandler(Menu menu, MenuAction action, int tank, int param2)
{
	switch (action)
	{
		case MenuAction_Select:
		{
			char sId[12];
			menu.GetItem(param2, sId, sizeof(sId));
			int target = GetClientOfUserId(StringToInt(sId));

			if (ValidateOffer(Validate_Default, tank, target))
				OfferMenu(tank, target);

			return 0;
		}
		case MenuAction_End:
		{
			delete menu;

			return 0;
		}
	}

	return 0;
}

void OfferMenu(int tank, int target)
{
	g_iTankId[target] = GetClientUserId(tank);
	ExecCmd(target);
	char sTemp[64];
	Menu menu = new Menu(OfferMenuHandler);
	FormatEx(sTemp, sizeof(sTemp), "%T", "phrase5", target);
	menu.SetTitle(sTemp);
	FormatEx(sTemp, sizeof(sTemp), "%T", "Yes", target);
	menu.AddItem("", sTemp);
	FormatEx(sTemp, sizeof(sTemp), "%T", "No", target);
	menu.AddItem("", sTemp);
	menu.ExitButton = true;
	menu.Display(target, MENU_TIME_FOREVER);
}

public int OfferMenuHandler(Menu menu, MenuAction action, int target, int param2)
{
	switch (action)
	{
		case MenuAction_Select:
		{
			int tank = GetClientOfUserId(g_iTankId[target]);

			if (param2 == 0){
				if (ValidateOffer(Validate_NotiyfyTarget, tank, target))
					TankPass(tank, target);
			}
			else if (IsValidTank(tank) && IsClientAndInGame(target))
				CPrintToChat(tank, "%t", "phrase6", target);

			return 0;
		}
		case MenuAction_Cancel:
		{
			int tank = GetClientOfUserId(g_iTankId[target]);
			if (IsValidTank(tank) && IsClientAndInGame(target))
				CPrintToChat(tank, "%t", "phrase6", target);

			return 0;
		}
		case MenuAction_End:
		{
			delete menu;

			return 0;
		}
	}

	return 0;
}
/*
|--------------------------------------------------------------------------
| COMMANDS
|--------------------------------------------------------------------------
*/
public Action Command_TankPass(int client, int args)
{
	if (!g_bIsBlocked[client])
		PreTankPassMenu(client);
	return Plugin_Handled;
}

public Action Command_ForcePass(int client, int args)
{
	if (client && args){

		char sArg[32], sName[MAX_TARGET_LENGTH];
		int iTargetList[MAXPLAYERS+1], iCount;
		bool bIsML;
		GetCmdArg(1, sArg, sizeof(sArg));

		if ((iCount = ProcessTargetString(
			sArg,
			client,
			iTargetList,
			MAXPLAYERS+1,
			COMMAND_FILTER_NO_MULTI|COMMAND_FILTER_NO_BOTS,
			sName, sizeof(sName),
			bIsML)) <= 0){
			ReplyToTargetError(client, iCount);
			return Plugin_Handled;
		}

		int tank = GetTank();

		if (ValidateOffer(Validate_Default, tank, iTargetList[0], client))
			TankPass(tank, iTargetList[0], client);
	}
	else
		ReplyToCommand(client, "sm_forcepass <#userid|name>");

	return Plugin_Handled;
}

public Action Command_TakeTank(int client, int args)
{
	if (client && args){

		char sArg[32], sName[MAX_TARGET_LENGTH];
		int iTargetList[MAXPLAYERS+1], iCount;
		bool bIsML;
		GetCmdArg(1, sArg, sizeof(sArg));

		if ((iCount = ProcessTargetString(
			sArg,
			client,
			iTargetList,
			MAXPLAYERS+1,
			COMMAND_FILTER_NO_MULTI|COMMAND_FILTER_NO_BOTS,
			sName, sizeof(sName),
			bIsML)) <= 0){
			ReplyToTargetError(client, iCount);
			return Plugin_Handled;
		}

		TakeOverTank(client, iTargetList[0]);
	}
	else
		ReplyToCommand(client, "sm_taketank <#userid|name>");

	return Plugin_Handled;
}
/*
|--------------------------------------------------------------------------
| EVENTS
|--------------------------------------------------------------------------
*/
public void OnClientPutInServer(int client)
{
	if (client)
		ResetPassData(client);
}

public void Event_RoundStart(Event h_Event, char[] s_Name, bool b_DontBroadcast)
{
    for (int i = 1; i <= MaxClients; i++)
        ResetPassData(i);

    g_bIsFinale = false;
    g_iPendingPassTank = -1;
    g_iPendingPassTarget = -1;
    g_bFrustrationPass = false;
    g_bForcingPass = false;
    PrintDebug("round_start: pass data reset for all clients.");
}

public void Event_FinalStart(Event h_Event, char[] s_Name, bool b_DontBroadcast)
{
	g_bIsFinale = true;
}

public void Event_TankSpawn(Event h_Event, char[] s_Name, bool b_DontBroadcast)
{
	int client = GetClientOfUserId(h_Event.GetInt("userid"));

	if (IsClientAndInGame(client) && !IsFakeClient(client)){
		ResetPassData(client);
		if (!g_bCvarNotify) return;

		g_bIsBlocked[client] = true;
		DataPack pack;
		CreateDataTimer(0.2, Timer_Notify, pack); // waiting for bot_player_replace fired
		pack.WriteCell(GetClientUserId(client));
		pack.WriteCell(client);
	}
}

Action Timer_Notify(Handle timer, DataPack pack)
{
	pack.Reset();
	int userId = pack.ReadCell();
	int client = pack.ReadCell();

	g_bIsBlocked[client] = false;
	client = GetClientOfUserId(userId);

	if (IsAllowToPass(client) && IsValidTank(client)){

		if (g_bCvarMenu)
			PreTankPassMenu(client);

		CPrintToChat(client, "%t", "phrase1");
	}

	return Plugin_Stop;
}

public void Event_EntityKilled(Event h_Event, char[] s_Name, bool b_DontBroadcast)
{
	int entity = h_Event.GetInt("entindex_killed");
	if (IsClient(entity) && IsPlayerTank(entity))
		RequestFrame(OnEntKilled, entity);
}

public void OnEntKilled(int client)
{
	if (!IsAliveTank(client))
		ResetPassData(client);
}

public void Event_PlayerBotReplace(Event h_Event, char[] s_Name, bool b_DontBroadcast)
{
	int client = GetClientOfUserId(h_Event.GetInt("player"));
	if (!g_iPassedCount[client]) return;
	int bot = GetClientOfUserId(h_Event.GetInt("bot"));

	PrintDebug("player_bot_replace: player=%d bot=%d passCount=%d", client, bot, g_iPassedCount[client]);
	if (IsReplaceableTank(bot, client))
		TransferPass(client, bot);
}

// fired after tank_spawn
public void Event_BotPlayerReplace(Event h_Event, char[] s_Name, bool b_DontBroadcast)
{
	int bot = GetClientOfUserId(h_Event.GetInt("bot"));
	if (!g_iPassedCount[bot]) return;
	int client = GetClientOfUserId(h_Event.GetInt("player"));

	if (IsReplaceableTank(bot, client))
		TransferPass(bot, client, false);
}

public void L4D_OnReplaceTank(int tank, int newtank)
{
	OnReplaceTank(tank, newtank);
}

// support nyx extension
public Action L4D2_OnReplaceTank(int tank, int newtank)
{
	OnReplaceTank(tank, newtank);
	return Plugin_Continue;
}

void OnReplaceTank(int tank, int newtank)
{
	if (tank == newtank) return;

	if (g_bIsIgnited[tank])
		RequestFrame(OnFrameIgnite, GetClientUserId(newtank));

	TransferPass(tank, newtank);
}

public void OnFrameIgnite(int client)
{
	client = GetClientOfUserId(client);
	if (IsValidTank(client))
		 IgniteEntity(client, IGNITE_TIME);
}
/*
|--------------------------------------------------------------------------
| FUNCTIONS
|--------------------------------------------------------------------------
*/
void TankPass(int tank, int target, int admin = 0, bool auto = false)
{
	// guard the whole routine. Quick-pass calls L4D2Direct_TryOfferingTankBot() below,
	// which re-fires our L4D_OnTryOfferingTankBot; without this guard that forward would
	// queue a SECOND (frustration) pass, duplicating the chat message and fighting this
	// pass for the tank (intermittent "message twice / control not passed" bug).
	bool bWasForcing = g_bForcingPass;
	g_bForcingPass = true;

	if (admin){
		PrintToTeam(3, 0, "%t", "phrase9", target);
		LogAction(admin, target, "\"%L\" has passed the Tank from \"%L\" to \"%L\"", admin, tank, target);
	}
	else if (auto)   // automatic pass caused by frustration
		PrintToTeam(3, 0, "%t", "phrase15", target, g_iPassedCount[tank] + 1, g_iCvarPassedCount);
	else if (g_iCvarPassedCount == 1)
		PrintToTeam(3, 0, "%t", "phrase3", tank, target);
	else
		PrintToTeam(3, 0, "%t", "phrase14", tank, target, g_iPassedCount[tank] + 1, g_iCvarPassedCount);

	bool isOnFire = IsOnFire(tank);

	if (g_bIsFinale || !g_bCvarQuickPass){
		if (IsMustIgnite(isOnFire))
			g_bIsIgnited[tank] = true;

		if (!g_bCvarReplace && IsReplaceableSI(target))
			ForcePlayerSuicide(target);

		for (int i = 1; i <= MaxClients; i++)
			if (i != target && IsInfected(i) && !IsFakeClient(i))
				L4D2Direct_SetTankTickets(i, 0);

		L4D2Direct_SetTankTickets(target, 10000);
		SetPassCount(tank, true);
		L4D2Direct_TryOfferingTankBot(tank, false);
	}
	else {
		if (IsReplaceableSI(target)){

			if (g_bCvarReplace)
				L4D_ReplaceWithBot(target);

			ForcePlayerSuicide(target);
		}
		// left4dhooks bugfix
		float vPos[3], vAng[3];
		GetClientAbsOrigin(tank, vPos);
		GetClientAbsAngles(tank, vAng);
		TeleportEntity(target, vPos, vAng, NULL_VECTOR);

		SetPassCount(tank);
		L4D_ReplaceTank(tank, target);

		if (IsMustIgnite(isOnFire))
			IgniteEntity(target, IGNITE_TIME);
	}

	PrintDebug("TankPass(menu/admin): tank=%d target=%d admin=%d passCount now=%d/%d", tank, target, admin, g_iPassedCount[tank], g_iCvarPassedCount);

	Call_StartForward(g_fwdOnTankPass);
	Call_PushCell(tank);
	Call_PushCell(target);
	Call_Finish();

	// restore guard to whatever it was before (so nested calls from Frame_ExecutePass still behave)
	g_bForcingPass = bWasForcing;
}

void TakeOverTank(int admin, int target)
{
	int tank = GetTankBot();

	if (tank && IsValidTarget(target)){
		int currentHealth = GetEntProp(tank, Prop_Data, "m_iHealth");

		L4D_TakeOverZombieBot(target, tank);
		L4D2Direct_SetTankPassedCount(g_iTakeOverPassedCount);

		SetEntProp(target, Prop_Data, "m_iHealth", currentHealth);
		SetEntProp(target, Prop_Send, "m_iHealth", currentHealth);
	}
	else
		PrintToChat(admin, "%t", "Player no longer available");
}

void SetPassCount(int tank, bool offer = false)
{
	int count = (g_iPassedCount[tank] + 1) >= g_iCvarPassedCount ? 2 : 1;
	if (offer) count--;
	L4D2Direct_SetTankPassedCount(count);
}

void TransferPass(int tank, int newtank, bool count = true)
{
	// during a forced frustration pass, TankPass() already handled counting via SetPassCount;
	// skip the native-event increment to avoid double counting.
	if (count && g_bFrustrationPass)
	{
		PrintDebug("TransferPass: guard active, skip increment tank=%d newtank=%d", tank, newtank);
		count = false;
	}

	if (count)
		g_iPassedCount[tank]++;

	if (g_iPassedCount[tank] > g_iCvarPassedCount)
		g_iPassedCount[tank] = g_iCvarPassedCount;

	g_iPassedCount[newtank] = g_iPassedCount[tank];

	PrintDebug("TransferPass: tank=%d -> newtank=%d counted=%d finalCount=%d/%d", tank, newtank, count, g_iPassedCount[newtank], g_iCvarPassedCount);

	ResetPassData(tank);
}

void ResetPassData(int client)
{
	g_iPassedCount[client] = 0;
	g_bIsIgnited[client] = false;
}

void ExecCmd(int client)
{
	if (g_sCvarCmd[0] && GetClientMenu(client) == MenuSource_Normal)
		FakeClientCommand(client, g_sCvarCmd);
}

int GetTank()
{
	for (int i = 1; i <= MaxClients; i++){
		if (IsValidTank(i))
			return i;
	}
	return 0;
}

int GetTankCount()
{
	int count;
	for (int i = 1; i <= MaxClients; i++){
		if (IsValidTank(i))
			count++;
	}
	return count;
}

int GetTankBot()
{
	for (int i = 1; i <= MaxClients; i++){
		if (IsValidTankBot(i))
			return i;
	}
	return 0;
}

bool ValidateOffer(int validate = Validate_Default, int tank, int target = 0, int admin = 0)
{
	bool hasTarget = validate == Validate_SkipTarget ? true : IsValidTarget(target);
	bool hasTank = IsValidTank(tank);
	int client = admin ? admin : tank;

	if (!hasTank){
		if (IsClientAndInGame(client))
			CPrintToChat(client, "%t", "phrase7");
		if (validate == Validate_NotiyfyTarget && hasTarget)
			CPrintToChat(target, "%t", "phrase7");
		return false;
	}
	if (GetTankCount() > 1)
	{
		CPrintToChat(client, "%t", "phrase7");
		return false;
	}
	if (!IsAllowToPass(tank)){
		if (hasTank){
			if (g_iCvarPassedCount == 1)
				CPrintToChat(client, "%t", "phrase2");
			else
				CPrintToChat(client, "%t", "phrase13", g_iPassedCount[tank], g_iCvarPassedCount);
		}
		if (validate == Validate_NotiyfyTarget && hasTarget){
			if (g_iCvarPassedCount == 1)
				CPrintToChat(client, "%t", "phrase2");
			else
				CPrintToChat(client, "%t", "phrase13", g_iPassedCount[tank], g_iCvarPassedCount);
		}
		return false;
	}
	if (!hasTarget){
		CPrintToChat(client, "%t", "Player no longer available");
		return false;
	}
	if (g_bCvarFire && IsOnFire(tank)){
		CPrintToChat(client, "%t", "phrase8");
		if (validate == Validate_NotiyfyTarget)
			CPrintToChat(target, "%t", "phrase8");
		return false;
	}
	if (g_bCvarDamage && GetClientHealth(tank) != g_iCvarTankHealth){
		CPrintToChat(client, "%t", "phrase11");
		if (validate == Validate_NotiyfyTarget)
			CPrintToChat(target, "%t", "phrase11");
		return false;
	}
	return true;
}
/*
|--------------------------------------------------------------------------
| CONDITION
|--------------------------------------------------------------------------
*/
bool IsMustIgnite(bool ignited)
{
	return ignited && !g_bCvarFire && !g_bCvarExtinguish;
}

bool IsAllowToPass(int tank)
{
	return g_iPassedCount[tank] < g_iCvarPassedCount;
}

bool IsReplaceableTank(int client, int bot)
{
	return IsClient(client) && IsClient(bot) && IsTank(client) && IsTank(bot);
}

bool IsReplaceableSI(int client)
{
	return IsPlayerAlive(client) && !L4D_IsPlayerGhost(client);
}

bool IsValidTarget(int target)
{
	return IsValid(target) && (!IsPlayerTank(target) || !IsPlayerAlive(target));
}

bool IsValidTank(int tank)
{
	return IsValid(tank) && IsAliveTank(tank);
}

bool IsValidTankBot(int tank)
{
	return IsInfected(tank) && IsFakeClient(tank) && IsAliveTank(tank);
}

bool IsAliveTank(int tank)
{
	return IsClientAndInGame(tank) && IsTank(tank) && IsPlayerAlive(tank);
}

bool IsTank(int tank)
{
	return IsPlayerTank(tank) && !L4D_IsPlayerIncapacitated(tank);
}

bool IsValid(int client)
{
	return IsInfectedAndInGame(client) && !IsFakeClient(client);
}
/*
|--------------------------------------------------------------------------
| CVARS
|--------------------------------------------------------------------------
*/
public void OnCvarChange_Exec(ConVar convar, const char[] oldValue, const char[] newValue)
{
	if (!StrEqual(oldValue, newValue))
		convar.GetString(g_sCvarCmd, sizeof(g_sCvarCmd));
}

public void OnCvarChange_Replace(ConVar convar, const char[] oldValue, const char[] newValue)
{
	if (!StrEqual(oldValue, newValue))
		g_bCvarReplace = convar.BoolValue;
}

public void OnCvarChange_Damage(ConVar convar, const char[] oldValue, const char[] newValue)
{
	if (!StrEqual(oldValue, newValue))
		g_bCvarDamage = convar.BoolValue;
}

public void OnCvarChange_Fire(ConVar convar, const char[] oldValue, const char[] newValue)
{
	if (!StrEqual(oldValue, newValue))
		g_bCvarFire = convar.BoolValue;
}

public void OnCvarChange_Extinguish(ConVar convar, const char[] oldValue, const char[] newValue)
{
	if (!StrEqual(oldValue, newValue))
		g_bCvarExtinguish = convar.BoolValue;
}

public void OnCvarChange_TakeOver(ConVar convar, const char[] oldValue, const char[] newValue)
{
	if (!StrEqual(oldValue, newValue))
		g_iTakeOverPassedCount = convar.IntValue;
}

public void OnCvarChange_TankHealth(ConVar convar, const char[] oldValue, const char[] newValue)
{
	if (!StrEqual(oldValue, newValue))
		g_iCvarTankHealth = CalcTankHealth();
}

int CalcTankHealth()
{
	return RoundToNearest(g_hCvarTankHealth.FloatValue * (g_hCvarTankBonusHealth ? g_hCvarTankBonusHealth.FloatValue : 1.5));
}

public void OnCvarChange_Notify(ConVar convar, const char[] oldValue, const char[] newValue)
{
	if (!StrEqual(oldValue, newValue))
		g_bCvarNotify = convar.BoolValue;
}

public void OnCvarChange_QuickPass(ConVar convar, const char[] oldValue, const char[] newValue)
{
	if (!StrEqual(oldValue, newValue))
		g_bCvarQuickPass = convar.BoolValue;
}

public void OnCvarChange_Menu(ConVar convar, const char[] oldValue, const char[] newValue)
{
	if (!StrEqual(oldValue, newValue))
		g_bCvarMenu = convar.BoolValue;
}

public void OnCvarChange_PassCount(ConVar convar, const char[] oldValue, const char[] newValue)
{
	if (!StrEqual(oldValue, newValue))
		g_iCvarPassedCount = convar.IntValue;
}

public void OnCvarChange_Confirm(ConVar convar, const char[] oldValue, const char[] newValue)
{
	if (!StrEqual(oldValue, newValue))
		g_bCvarConfirm = convar.BoolValue;
}

bool IsInfectedAndInGame(int client)
{
	return IsClient(client) && IsInfected(client);
}


bool IsClient(int client)
{
	return client > 0 && client <= MaxClients;
}

bool IsInfected(int client)
{
	return IsClientInGame(client) && GetClientTeam(client) == 3;
}

bool IsClientAndInGame(int client)
{
	return IsClient(client) && IsClientInGame(client);
}

bool IsPlayerTank(int client)
{
	return GetEntProp(client, Prop_Send, "m_zombieClass") == ZC_TANK;
}

void PrintToTeam(int team, int msgType, const char[] text, any ...)
{
	bool bTrans = StrContains(text, "%t") != -1;

	char sTemp[256];
	for (int i = 1; i <= MaxClients; i++){

		if (IsClientInGame(i) && GetClientTeam(i) == team && !IsFakeClient(i)){

			if (bTrans)
				SetGlobalTransTarget(i);

			VFormat(sTemp, sizeof(sTemp), text, 4);

			switch (msgType){

				case 0:
					CPrintToChat(i, sTemp);
				case 1:
					PrintHintText(i, sTemp);
				case 2:
					PrintCenterText(i, sTemp);
			}
		}
	}
}

bool IsOnFire(int entity)
{
	return (GetEntityFlags(entity) & FL_ONFIRE) == FL_ONFIRE;
}
