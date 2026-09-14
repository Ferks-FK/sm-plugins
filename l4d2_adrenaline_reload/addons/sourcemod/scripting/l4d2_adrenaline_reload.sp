#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <sdktools>
#include <left4dhooks>

#define PLUGIN_VERSION          "2.1.0"

#define TEAM_SURVIVOR           2
#define WEAPON_PREFIX_LEN       7       // "weapon_"
#define MAX_WEAPON_NAME_LENGTH  64
#define MAX_SANE_RELOAD         10.0    // discard absurd m_flNextPrimaryAttack readings

StringMap g_hDurations = null;

float g_fRate[MAXPLAYERS + 1];          // playback rate applied to the reload in progress
float g_fUntil[MAXPLAYERS + 1];         // GameTime when that sped-up reload ends

public Plugin myinfo =
{
	name = "[L4D2] Adrenaline Reload",
	author = "Ferks-FK",
	description = "Faster reload only while the survivor is under the adrenaline effect.",
	version = PLUGIN_VERSION,
	url = ""
};

public void OnPluginStart()
{
	g_hDurations = new StringMap();

	RegServerCmd("l4d2_adrenaline_reload", Cmd_AdrenalineReload,
		"l4d2_adrenaline_reload <gun> <time> - reload time under adrenaline. Unlisted weapons stay vanilla.");

	HookEvent("weapon_reload", Event_WeaponReload, EventHookMode_Post);
}

public void OnClientPutInServer(int client)
{
	g_fRate[client] = 0.0;
	g_fUntil[client] = 0.0;
}

// =====================================================================
//  Configuration
// =====================================================================

Action Cmd_AdrenalineReload(int args)
{
	if (args != 2) {
		PrintToServer("[AdrenalineReload] Usage: l4d2_adrenaline_reload <gun> <time>");
		return Plugin_Handled;
	}

	char sWeaponName[MAX_WEAPON_NAME_LENGTH];
	GetCmdArg(1, sWeaponName, sizeof(sWeaponName));

	char sValue[32];
	GetCmdArg(2, sValue, sizeof(sValue));
	float fValue = StringToFloat(sValue);

	if (!L4D2_IsValidWeapon(sWeaponName)) {
		PrintToServer("[AdrenalineReload] Invalid weapon: '%s'.", sWeaponName);
		return Plugin_Handled;
	}

	// Shotguns reload shell by shell and never go through the single duration
	// handled here. Use reloaddurationmult from l4d2_weapon_attributes instead.
	if (IsShotgun(sWeaponName)) {
		PrintToServer("[AdrenalineReload] '%s' is a shotgun: per-shell reload, not supported.", sWeaponName);
		return Plugin_Handled;
	}

	if (fValue <= 0.0) {
		PrintToServer("[AdrenalineReload] '%s': time must be greater than zero (got %.3f).", sWeaponName, fValue);
		return Plugin_Handled;
	}

	g_hDurations.SetValue(sWeaponName, fValue);
	PrintToServer("[AdrenalineReload] '%s': %.3f under adrenaline.", sWeaponName, fValue);

	return Plugin_Handled;
}

// =====================================================================
//  Application
// =====================================================================

void Event_WeaponReload(Event hEvent, const char[] sName, bool bDontBroadcast)
{
	int client = GetClientOfUserId(hEvent.GetInt("userid"));

	if (!IsEligibleSurvivor(client)) {
		return;
	}

	int weapon = GetActiveWeapon(client);
	if (weapon == -1) {
		return;
	}

	float fAdrenaline = 0.0;
	if (!GetConfiguredDuration(weapon, fAdrenaline)) {
		return;
	}

	/**
	 * Measure the real duration of the reload the game just started.
	 *
	 * The ReloadDuration field of the WeaponInformationDatabase is useless as
	 * a baseline: for SMGs it reads 0 in vanilla, because the timing comes
	 * from the animation. So it is derived from m_flNextPrimaryAttack, which
	 * at this instant already holds "now + real duration" — no matter whether
	 * that duration comes from the animation, a global override or another
	 * plugin.
	 */
	float fNow = GetGameTime();
	float fBase = GetEntPropFloat(weapon, Prop_Send, "m_flNextPrimaryAttack") - fNow;

	if (fBase <= 0.0 || fBase > MAX_SANE_RELOAD || fAdrenaline >= fBase) {
		return;
	}

	float fRate = fBase / fAdrenaline;

	SetEntPropFloat(weapon, Prop_Send, "m_flNextPrimaryAttack", fNow + fAdrenaline);
	SetEntPropFloat(client, Prop_Send, "m_flNextAttack", fNow + fAdrenaline);
	SetEntPropFloat(weapon, Prop_Send, "m_flPlaybackRate", fRate);

	g_fRate[client] = fRate;
	g_fUntil[client] = fNow + fAdrenaline;
}

/**
 * Shoving resets the weapon's m_flPlaybackRate: without this the animation
 * drops back to normal speed mid-reload while the real timing stays reduced.
 * Adrenaline is not required here — if it expires during the reload, the
 * already accelerated animation still has to stay consistent to the end.
 */
public Action OnPlayerRunCmd(int client, int &buttons)
{
	if (!(buttons & IN_ATTACK2) || g_fRate[client] <= 0.0) {
		return Plugin_Continue;
	}

	if (GetGameTime() >= g_fUntil[client]) {
		g_fRate[client] = 0.0;
		return Plugin_Continue;
	}

	int weapon = GetActiveWeapon(client);
	if (weapon == -1) {
		return Plugin_Continue;
	}

	SetEntPropFloat(weapon, Prop_Send, "m_flPlaybackRate", g_fRate[client]);

	return Plugin_Continue;
}

// =====================================================================
//  Helpers
// =====================================================================

bool IsEligibleSurvivor(int client)
{
	if (client < 1 || client > MaxClients || !IsClientInGame(client) || IsFakeClient(client)) {
		return false;
	}

	if (GetClientTeam(client) != TEAM_SURVIVOR || !IsPlayerAlive(client)) {
		return false;
	}

	return view_as<bool>(GetEntProp(client, Prop_Send, "m_bAdrenalineActive"));
}

// The weapon actually held, which is the one being reloaded or shoved with.
int GetActiveWeapon(int client)
{
	return GetEntPropEnt(client, Prop_Send, "m_hActiveWeapon");
}

bool GetConfiguredDuration(int weapon, float &fAdrenaline)
{
	char sClassname[MAX_WEAPON_NAME_LENGTH];
	if (!GetEntityClassname(weapon, sClassname, sizeof(sClassname))) {
		return false;
	}

	if (strncmp(sClassname, "weapon_", WEAPON_PREFIX_LEN) != 0) {
		return false;
	}

	return (g_hDurations.GetValue(sClassname[WEAPON_PREFIX_LEN], fAdrenaline) && fAdrenaline > 0.0);
}

bool IsShotgun(const char[] sWeaponName)
{
	return (StrEqual(sWeaponName, "pumpshotgun", false)
		|| StrEqual(sWeaponName, "shotgun_chrome", false)
		|| StrEqual(sWeaponName, "autoshotgun", false)
		|| StrEqual(sWeaponName, "shotgun_spas", false));
}
