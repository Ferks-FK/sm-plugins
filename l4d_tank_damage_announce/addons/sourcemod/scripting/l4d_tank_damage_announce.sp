/*        L4D_TANK_DAMAGE_ANNOUNCE
*         L4D_TANK_DAMAGE_ANNOUNCE
*/

#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <sdktools>
#include <colors>

#define TEAM_SURVIVOR       2
#define TEAM_INFECTED       3
#define ZOMBIECLASS_TANK    8       // Zombie class of the tank, used to find tank after he have been passed to another player

// Weapon slots (from l4d2util_constants) - kept local to avoid a hard dependency.
#define WEAPON_SLOT_LIGHT_HEALTH    4   // L4D2WeaponSlot_LightHealthItem (pills / adrenaline)

// SourceMod version guard for GivePlayerItem (fixed in v1.11.6754)
#if !defined SOURCEMOD_V_MINOR
	#define SOURCEMOD_V_MINOR 0
#endif
#if !defined SOURCEMOD_V_REV
	#define SOURCEMOD_V_REV 0
#endif

bool
	g_bEnabled              = true,
	g_bAnnounceTankDamage   = false,    // Whether or not tank damage should be announced
	g_bIsTankInPlay         = false,    // Whether or not the tank is active
	g_bPrintedHealth        = false,    // Is Remaining Health showed?
	g_bWasTank[MAXPLAYERS + 1];         // Was Player Tank before he died.

int
	g_iWasTankAI            = 0,
	g_iOffset_Incapacitated = 0,        // Used to check if tank is dying
	g_iTankClient           = 0,        // Which client is currently playing as tank
	g_iLastTankHealth       = 0,        // Used to award the killing blow the exact right amount of damage
	g_iSurvivorLimit        = 4,        // For survivor array in damage print
	g_iDamage[MAXPLAYERS + 1];

float
	g_fMaxTankHealth        = 6000.0;

ConVar
	g_hCvarEnabled          = null,
	g_hCvarTankHealth       = null,
	g_hCvarDifficulty       = null,
	g_hCvarSurvivorLimit    = null,
	g_hCvarRewardPills      = null;     // NEW: reward top damager with pills

GlobalForward
	g_fwdOnTankDeath        = null;

/*
* Version 0.6.6
* - Better looking Output.
* - Added Tank Name display when Tank dies, normally it only showed the Tank's name if the Tank survived
*
* Version 0.6.6b
* - Fixed Printing Two Tanks when last map Tank survived.
* Added by; Sir
*
* Version 0.6.7
* - Added Campaign Difficulty Support.
* Added by; Sir
*
* Version 0.7.0
* - Ported to the modern (transitional) syntax.
* - Externalised all chat output to translations (l4d_tank_damage_announce.phrases.txt).
* - Added an optional reward: the survivor who dealt the most damage to the tank
*   receives pain pills on tank death if their light-health slot is empty
*   (cvar l4d_tankdamage_reward_pills).
*/

public Plugin myinfo =
{
	name = "Tank Damage Announce L4D2",
	author = "Griffin and Blade, Ferks-FK",
	description = "Announce damage dealt to tanks by survivors",
	version = "0.7.0",
	url = "https://github.com/SirPlease/L4D2-Competitive-Rework"
};

public void OnPluginStart()
{
	LoadTranslation("l4d_tank_damage_announce.phrases");

	g_bIsTankInPlay = false;
	g_bAnnounceTankDamage = false;
	g_iTankClient = 0;
	ClearTankDamage();

	HookEvent("tank_spawn", Event_TankSpawn);
	HookEvent("player_death", Event_PlayerKilled);
	HookEvent("round_start", Event_RoundStart);
	HookEvent("round_end", Event_RoundEnd);
	HookEvent("player_hurt", Event_PlayerHurt);

	g_hCvarEnabled = CreateConVar("l4d_tankdamage_enabled", "1", "Announce damage done to tanks when enabled", FCVAR_NOTIFY|FCVAR_SPONLY, true, 0.0, true, 1.0);
	g_hCvarRewardPills = CreateConVar("l4d_tankdamage_reward_pills", "0", "Reward the survivor who dealt the most damage to the tank with pain pills on tank death, if their light-health slot is empty (0: Disable, 1: Enable)", FCVAR_NOTIFY, true, 0.0, true, 1.0);

	g_hCvarSurvivorLimit = FindConVar("survivor_limit");
	g_hCvarTankHealth = FindConVar("z_tank_health");
	g_hCvarDifficulty = FindConVar("z_difficulty");

	g_hCvarEnabled.AddChangeHook(Cvar_Enabled);
	g_hCvarSurvivorLimit.AddChangeHook(Cvar_SurvivorLimit);
	g_hCvarTankHealth.AddChangeHook(Cvar_TankHealth);
	g_hCvarDifficulty.AddChangeHook(Cvar_TankHealth);
	FindConVar("mp_gamemode").AddChangeHook(Cvar_TankHealth);

	g_bEnabled = g_hCvarEnabled.BoolValue;
	g_iSurvivorLimit = g_hCvarSurvivorLimit.IntValue;
	CalculateTankHealth();

	g_iOffset_Incapacitated = FindSendPropInfo("Tank", "m_isIncapacitated");
	g_fwdOnTankDeath = new GlobalForward("OnTankDeath", ET_Event);
}

public void OnMapStart()
{
	// In cases where a tank spawns and map is changed manually, bypassing round end
	ClearTankDamage();
}

public void OnClientDisconnect_Post(int client)
{
	if (!g_bIsTankInPlay || client != g_iTankClient) {
		return;
	}
	CreateTimer(0.1, Timer_CheckTank, client); // Use a delayed timer due to bugs where the tank passes to another player
}

void Cvar_Enabled(ConVar convar, const char[] oldValue, const char[] newValue)
{
	g_bEnabled = convar.BoolValue;
}

void Cvar_SurvivorLimit(ConVar convar, const char[] oldValue, const char[] newValue)
{
	g_iSurvivorLimit = convar.IntValue;
}

void Cvar_TankHealth(ConVar convar, const char[] oldValue, const char[] newValue)
{
	CalculateTankHealth();
}

void CalculateTankHealth()
{
	char sGameMode[32];
	FindConVar("mp_gamemode").GetString(sGameMode, sizeof(sGameMode));

	g_fMaxTankHealth = g_hCvarTankHealth.FloatValue;
	if (g_fMaxTankHealth <= 0.0) {
		g_fMaxTankHealth = 1.0;
	}

	// Versus or Realism Versus
	if (StrEqual(sGameMode, "versus") || StrEqual(sGameMode, "mutation12")) {
		g_fMaxTankHealth *= 1.5;
	}
	// Anything else (should be fine...?)
	else {
		g_fMaxTankHealth = g_hCvarTankHealth.FloatValue;

		char sDifficulty[16];
		g_hCvarDifficulty.GetString(sDifficulty, sizeof(sDifficulty));

		if (sDifficulty[0] == 'E') {
			g_fMaxTankHealth *= 0.75;     // Easy
		} else if (sDifficulty[0] == 'H' || sDifficulty[0] == 'I') {
			g_fMaxTankHealth *= 2.0;      // Advanced or Expert
		}
	}
}

void Event_PlayerHurt(Event event, const char[] name, bool dontBroadcast)
{
	if (!g_bIsTankInPlay) {
		return; // No tank in play; no damage to record
	}

	int victim = GetClientOfUserId(event.GetInt("userid"));
	if (victim != GetTankClient() ||    // Victim isn't tank; no damage to record
		IsTankDying()                   // Something buggy happens when tank is dying with regards to damage
	) {
		return;
	}

	int attacker = GetClientOfUserId(event.GetInt("attacker"));
	// We only care about damage dealt by survivors, though it can be funny to see
	// claw/self inflicted hittable damage, so maybe in the future we'll do that
	if (attacker == 0 ||                        // Damage from world?
		!IsClientInGame(attacker) ||            // Not sure if this happens
		GetClientTeam(attacker) != TEAM_SURVIVOR
	) {
		return;
	}

	g_iDamage[attacker] += event.GetInt("dmg_health");
	g_iLastTankHealth = event.GetInt("health");
}

void Event_PlayerKilled(Event event, const char[] name, bool dontBroadcast)
{
	if (!g_bIsTankInPlay) {
		return; // No tank in play; no damage to record
	}

	int victim = GetClientOfUserId(event.GetInt("userid"));
	if (victim != g_iTankClient) {
		return;
	}

	// Award the killing blow's damage to the attacker; we don't award
	// damage from player_hurt after the tank has died/is dying
	// If we don't do it this way, we get wonky/inaccurate damage values
	int attacker = GetClientOfUserId(event.GetInt("attacker"));
	if (attacker && IsClientInGame(attacker)) {
		g_iDamage[attacker] += g_iLastTankHealth;
	}

	// Player was Tank
	if (!IsFakeClient(victim)) {
		g_bWasTank[victim] = true;
	} else {
		g_iWasTankAI = 1;
	}
	// Damage announce could probably happen right here...
	CreateTimer(0.1, Timer_CheckTank, victim); // Use a delayed timer due to bugs where the tank passes to another player
}

void Event_TankSpawn(Event event, const char[] name, bool dontBroadcast)
{
	int client = GetClientOfUserId(event.GetInt("userid"));
	g_iTankClient = client;

	if (g_bIsTankInPlay) {
		return; // Tank passed
	}

	// New tank, damage has not been announced
	g_bAnnounceTankDamage = true;
	g_bIsTankInPlay = true;
	// Set health for damage print in case it doesn't get set by player_hurt (aka no one shoots the tank)
	g_iLastTankHealth = GetClientHealth(client);
}

void Event_RoundStart(Event event, const char[] name, bool dontBroadcast)
{
	g_bPrintedHealth = false;
	g_bIsTankInPlay = false;
	g_iTankClient = 0;
	ClearTankDamage(); // Probably redundant
}

// When survivors wipe or juke tank, announce damage
void Event_RoundEnd(Event event, const char[] name, bool dontBroadcast)
{
	// But only if a tank that hasn't been killed exists
	if (g_bAnnounceTankDamage) {
		PrintRemainingHealth();
		PrintTankDamage();
	}
	ClearTankDamage();
}

Action Timer_CheckTank(Handle timer, any oldtankclient)
{
	if (g_iTankClient != oldtankclient) {
		return Plugin_Stop; // Tank passed
	}

	int tankclient = FindTankClient();
	if (tankclient && tankclient != oldtankclient) {
		g_iTankClient = tankclient;
		return Plugin_Stop; // Found tank, done
	}

	if (g_bAnnounceTankDamage) {
		PrintTankDamage();
	}
	ClearTankDamage();
	g_bIsTankInPlay = false; // No tank in play

	Call_StartForward(g_fwdOnTankDeath);
	Call_Finish();

	return Plugin_Stop;
}

bool IsTankDying()
{
	int tankclient = GetTankClient();
	if (!tankclient) {
		return false;
	}
	return view_as<bool>(GetEntData(tankclient, g_iOffset_Incapacitated));
}

void PrintRemainingHealth()
{
	g_bPrintedHealth = true;
	if (!g_bEnabled) {
		return;
	}

	int tankclient = GetTankClient();
	if (!tankclient) {
		return;
	}

	char sName[MAX_NAME_LENGTH];
	if (IsFakeClient(tankclient)) {
		Format(sName, sizeof(sName), "%T", "TankNameAI", LANG_SERVER);
	} else {
		GetClientName(tankclient, sName, sizeof(sName));
	}

	CPrintToChatAll("%t", "TankHealthRemaining", sName, g_iLastTankHealth);
}

void PrintTankDamage()
{
	if (!g_bEnabled) {
		return;
	}

	if (!g_bPrintedHealth) {
		for (int i = 1; i <= MaxClients; i++) {
			if (g_bWasTank[i]) {
				char sName[MAX_NAME_LENGTH];
				GetClientName(i, sName, sizeof(sName));
				CPrintToChatAll("%t", "TankDamageHeader", sName);
				g_bWasTank[i] = false;
			}
			else if (g_iWasTankAI > 0) {
				char sAI[64];
				Format(sAI, sizeof(sAI), "%T", "TankNameAI", LANG_SERVER);
				CPrintToChatAll("%t", "TankDamageHeader", sAI);
			}
			g_iWasTankAI = 0;
		}
	}

	int client;
	int percent_total;      // Accumulated total of calculated percents, for fudging out numbers at the end
	int damage_total;       // Accumulated total damage dealt by survivors, to see if we need to fudge upwards to 100%
	int survivor_index = -1;
	int[] survivor_clients = new int[g_iSurvivorLimit]; // Array to store survivor client indexes in, for the display iteration
	int percent_damage, damage;

	for (client = 1; client <= MaxClients; client++) {
		if (!IsClientInGame(client) || GetClientTeam(client) != TEAM_SURVIVOR || g_iDamage[client] == 0) {
			continue;
		}
		survivor_index++;
		survivor_clients[survivor_index] = client;
		damage = g_iDamage[client];
		damage_total += damage;
		percent_damage = GetDamageAsPercent(damage);
		percent_total += percent_damage;
	}

	SortCustom1D(survivor_clients, g_iSurvivorLimit, SortByDamageDesc);

	int percent_adjustment;
	// Percents add up to less than 100% AND > 99.5% damage was dealt to tank
	if (percent_total < 100 && float(damage_total) > (g_fMaxTankHealth - (g_fMaxTankHealth / 200.0))) {
		percent_adjustment = 100 - percent_total;
	}

	int last_percent = 100; // Used to store the last percent in iteration to make sure an adjusted percent doesn't exceed the previous percent
	int adjusted_percent_damage;
	for (int k = 0; k <= survivor_index; k++) {
		client = survivor_clients[k];
		damage = g_iDamage[client];
		percent_damage = GetDamageAsPercent(damage);
		// Attempt to adjust the top damager's percent, defer adjustment to next player if it's an exact percent
		// e.g. 3000 damage on 6k health tank shouldn't be adjusted
		if (percent_adjustment != 0 &&  // Is there percent to adjust
			damage > 0 &&               // Is damage dealt > 0%
			!IsExactPercent(damage)     // Percent representation is not exact, e.g. 3000 damage on 6k tank = 50%
		) {
			adjusted_percent_damage = percent_damage + percent_adjustment;
			if (adjusted_percent_damage <= last_percent) { // Make sure adjusted percent is not higher than previous percent, order must be maintained
				percent_damage = adjusted_percent_damage;
				percent_adjustment = 0;
			}
		}
		last_percent = percent_damage;

		char sName[MAX_NAME_LENGTH];
		GetClientName(client, sName, sizeof(sName));
		for (int i = 1; i <= MaxClients; i++) {
			if (IsClientInGame(i)) {
				CPrintToChat(i, "%t", "TankDamageLine", damage, percent_damage, sName);
			}
		}
	}

	// NEW: reward the top damager (survivor_clients[0] after the descending sort) with pills.
	if (survivor_index >= 0) {
		RewardTopDamager(survivor_clients[0]);
	}
}

void RewardTopDamager(int client)
{
	if (!g_hCvarRewardPills.BoolValue) {
		return;
	}

	// Only reward a living survivor who is still in game.
	if (client < 1 || client > MaxClients
		|| !IsClientInGame(client)
		|| GetClientTeam(client) != TEAM_SURVIVOR
		|| !IsPlayerAlive(client)
	) {
		return;
	}

	// Don't overwrite an existing light-health item (pills or adrenaline).
	if (GetPlayerWeaponSlot(client, WEAPON_SLOT_LIGHT_HEALTH) != -1) {
		return;
	}

	GivePlayerWeaponByName(client, "weapon_pain_pills");

	char sName[MAX_NAME_LENGTH];
	GetClientName(client, sName, sizeof(sName));
	CPrintToChatAll("%t", "RewardPills", sName);
}

void GivePlayerWeaponByName(int iClient, const char[] sWeaponName)
{
#if (SOURCEMOD_V_MINOR >= 12 || (SOURCEMOD_V_MINOR == 11 && SOURCEMOD_V_REV >= 6754))
	GivePlayerItem(iClient, sWeaponName); // Was fixed in v1.11.6754
#else
	int iEntity = CreateEntityByName(sWeaponName);
	if (iEntity == -1) {
		return;
	}
	DispatchSpawn(iEntity);
	EquipPlayerWeapon(iClient, iEntity);
#endif
}

void ClearTankDamage()
{
	g_iLastTankHealth = 0;
	g_iWasTankAI = 0;
	for (int i = 1; i <= MaxClients; i++) {
		g_iDamage[i] = 0;
		g_bWasTank[i] = false;
	}
	g_bAnnounceTankDamage = false;
}

int GetTankClient()
{
	if (!g_bIsTankInPlay) {
		return 0;
	}

	int tankclient = g_iTankClient;

	if (!IsClientInGame(tankclient)) { // If tank somehow is no longer in the game (kicked, hence events didn't fire)
		tankclient = FindTankClient();  // find the tank client
		if (!tankclient) {
			return 0;
		}
		g_iTankClient = tankclient;
	}

	return tankclient;
}

int FindTankClient()
{
	for (int client = 1; client <= MaxClients; client++) {
		if (!IsClientInGame(client) ||
			GetClientTeam(client) != TEAM_INFECTED ||
			!IsPlayerAlive(client) ||
			GetEntProp(client, Prop_Send, "m_zombieClass") != ZOMBIECLASS_TANK
		) {
			continue;
		}
		return client; // Found tank, return
	}
	return 0;
}

int GetDamageAsPercent(int damage)
{
	return RoundToNearest((damage / g_fMaxTankHealth) * 100.0);
}

// comparing the type of int with the float, how different is it
bool IsExactPercent(int damage)
{
	float fDamageAsPercent = (damage / g_fMaxTankHealth) * 100.0;
	float fDifference = float(GetDamageAsPercent(damage)) - fDamageAsPercent;
	return (FloatAbs(fDifference) < 0.001);
}

int SortByDamageDesc(int elem1, int elem2, const int[] array, Handle hndl)
{
	// By damage, then by client index, descending
	if (g_iDamage[elem1] > g_iDamage[elem2]) return -1;
	else if (g_iDamage[elem2] > g_iDamage[elem1]) return 1;
	else if (elem1 > elem2) return -1;
	else if (elem2 > elem1) return 1;
	return 0;
}

/**
 * Check if the translation file exists, and load it.
 *
 * @param translation	Translation name.
 * @noreturn
 */
stock void LoadTranslation(const char[] translation)
{
	char sPath[PLATFORM_MAX_PATH], sName[64];

	FormatEx(sName, sizeof(sName), "translations/%s.txt", translation);
	BuildPath(Path_SM, sPath, sizeof(sPath), sName);
	if (!FileExists(sPath)) {
		SetFailState("Missing translation file %s.txt", translation);
	}

	LoadTranslations(translation);
}
