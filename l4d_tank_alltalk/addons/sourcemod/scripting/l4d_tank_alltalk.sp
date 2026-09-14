#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <sdktools>
#include <colors>

#define TEAM_INFECTED       3
#define ZOMBIECLASS_TANK    8       // Zombie class of the tank, used to find tank after it's been passed to another player

#define SOUND_ALLTALK        "ui/pickup_secret01.wav"

bool
	g_bEnabled              = true,
	g_bAnnounce             = true,
	g_bIsTankInPlay         = false,    // Whether or not the tank is active
	g_bAlltalkForced        = false,    // Whether the plugin currently has sv_alltalk forced on
	g_bSelfChangingCvar     = false,    // Guard to ignore our own sv_alltalk changes
	g_bRoundEndHandled      = false;    // Guard: round_end refires during the map transition, only handle it once per round

int
	g_iTankClient           = 0,        // Which client is currently playing as tank
	g_iTankKillerUserId     = 0,        // Userid of whoever landed the killing blow, captured at player_death
	g_iOriginalAlltalk      = 0;        // sv_alltalk value before the plugin forced it on

Handle
	g_hRevertTimer          = null;

ConVar
	g_hCvarEnabled          = null,
	g_hCvarDuration         = null,
	g_hCvarAnnounce         = null,
	g_hCvarSound            = null,
	g_hCvarSvAlltalk        = null;

public Plugin myinfo =
{
	name = "Tank AllTalk L4D2",
	author = "Ferks-FK",
	description = "Enables sv_alltalk for a while after the Tank dies",
	version = "1.0.0",
	url = ""
};

public void OnPluginStart()
{
	LoadTranslation("l4d_tank_alltalk.phrases");

	HookEvent("tank_spawn", Event_TankSpawn);
	HookEvent("player_death", Event_PlayerKilled);
	HookEvent("round_start", Event_RoundStart);
	HookEvent("round_end", Event_RoundEnd);

	g_hCvarEnabled = CreateConVar("l4d_tankalltalk_enabled", "1", "Enable sv_alltalk for a while after the Tank dies", FCVAR_NOTIFY|FCVAR_SPONLY, true, 0.0, true, 1.0);
	g_hCvarDuration = CreateConVar("l4d_tankalltalk_duration", "10.0", "How long (in seconds) sv_alltalk stays enabled after the Tank dies", FCVAR_NOTIFY, true, 0.0);
	g_hCvarAnnounce = CreateConVar("l4d_tankalltalk_announce", "1", "Announce in chat when all talk is enabled/disabled by this plugin", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_hCvarSound = CreateConVar("l4d_tankalltalk_sound", "0", "Play a sound when all talk gets enabled by this plugin", FCVAR_NOTIFY, true, 0.0, true, 1.0);

	g_hCvarSvAlltalk = FindConVar("sv_alltalk");
	if (g_hCvarSvAlltalk == null) {
		SetFailState("Could not find \"sv_alltalk\" convar.");
	}

	g_hCvarEnabled.AddChangeHook(Cvar_Enabled);
	g_hCvarAnnounce.AddChangeHook(Cvar_Announce);
	g_hCvarSvAlltalk.AddChangeHook(Cvar_SvAlltalkChanged);

	g_bEnabled = g_hCvarEnabled.BoolValue;
	g_bAnnounce = g_hCvarAnnounce.BoolValue;

	AutoExecConfig(true, "l4d_tank_alltalk");
}

public void OnMapStart()
{
	PrecacheSound(SOUND_ALLTALK);
	g_bRoundEndHandled = false;
}

public void OnPluginEnd()
{
	RevertAlltalk(false);
}

public void OnClientDisconnect_Post(int client)
{
	if (!g_bIsTankInPlay || client != g_iTankClient) {
		return;
	}
	g_iTankKillerUserId = 0; // Disconnecting isn't a kill
	CreateTimer(0.1, Timer_CheckTank, client); // Use a delayed timer due to bugs where the tank passes to another player
}

void Cvar_Enabled(ConVar convar, const char[] oldValue, const char[] newValue)
{
	g_bEnabled = convar.BoolValue;
}

void Cvar_Announce(ConVar convar, const char[] oldValue, const char[] newValue)
{
	g_bAnnounce = convar.BoolValue;
}

// If an admin (or another plugin) changes sv_alltalk while we're the ones holding it on,
// stop managing it so we don't fight/override their decision when our timer expires.
void Cvar_SvAlltalkChanged(ConVar convar, const char[] oldValue, const char[] newValue)
{
	if (g_bSelfChangingCvar || !g_bAlltalkForced) {
		return;
	}

	g_bAlltalkForced = false;
	if (g_hRevertTimer != null) {
		KillTimer(g_hRevertTimer);
		g_hRevertTimer = null;
	}
}

void Event_TankSpawn(Event event, const char[] name, bool dontBroadcast)
{
	g_iTankClient = GetClientOfUserId(event.GetInt("userid"));
	g_bIsTankInPlay = true;
}

void Event_PlayerKilled(Event event, const char[] name, bool dontBroadcast)
{
	if (!g_bIsTankInPlay) {
		return; // No tank in play
	}

	int victim = GetClientOfUserId(event.GetInt("userid"));
	if (victim != g_iTankClient) {
		return;
	}

	g_iTankKillerUserId = event.GetInt("attacker");
	CreateTimer(0.1, Timer_CheckTank, victim); // Use a delayed timer due to bugs where the tank passes to another player
}

void Event_RoundStart(Event event, const char[] name, bool dontBroadcast)
{
	g_bIsTankInPlay = false;
	g_iTankClient = 0;
	g_iTankKillerUserId = 0;
	g_bRoundEndHandled = false;

	// All talk is only meant for the break between rounds. If the previous round's window is
	// still running (e.g. same-map round swap with little/no delay), cut it here so it never
	// bleeds into this round's live gameplay (opposing teams hearing each other mid-match).
	RevertAlltalk(true);
}

// A Versus round only ends one of two ways: the Tank wins (survivors wiped / time ran out) or
// the survivors win (they escaped). Either way, all talk should turn on — no need to work out
// which one happened.
void Event_RoundEnd(Event event, const char[] name, bool dontBroadcast)
{
	if (g_bRoundEndHandled) {
		return; // round_end refires during the map transition; already handled this round
	}
	g_bRoundEndHandled = true;

	g_bIsTankInPlay = false;
	g_iTankClient = 0;
	g_iTankKillerUserId = 0;

	RoundEnded();
}

Action Timer_CheckTank(Handle timer, any oldtankclient)
{
	if (g_iTankClient != oldtankclient) {
		return Plugin_Stop; // Tank already passed to someone else, nothing to do
	}

	int tankclient = FindTankClient();
	if (tankclient && tankclient != oldtankclient) {
		g_iTankClient = tankclient;
		return Plugin_Stop; // Found tank, it was just passed, not killed
	}

	// No tank found in play; the last one is truly dead
	g_bIsTankInPlay = false;
	TankDied(GetClientOfUserId(g_iTankKillerUserId));

	return Plugin_Stop;
}

void TankDied(int killer)
{
	if (!g_bEnabled) {
		return;
	}

	float duration = g_hCvarDuration.FloatValue;
	if (duration <= 0.0) {
		return;
	}

	EnableAlltalk(duration);

	if (g_bAnnounce) {
		int roundedDuration = RoundToNearest(duration);
		if (killer > 0 && IsClientInGame(killer)) {
			char sName[MAX_NAME_LENGTH];
			GetClientName(killer, sName, sizeof(sName));
			CPrintToChatAll("%t", "AllTalkEnabledByKiller", sName, roundedDuration);
		} else {
			CPrintToChatAll("%t", "AllTalkEnabledGeneric", roundedDuration);
		}
	}
}

void RoundEnded()
{
	if (!g_bEnabled) {
		return;
	}

	float duration = g_hCvarDuration.FloatValue;
	if (duration <= 0.0) {
		return;
	}

	EnableAlltalk(duration);

	if (g_bAnnounce) {
		CPrintToChatAll("%t", "AllTalkEnabledRoundEnd", RoundToNearest(duration));
	}
}

void EnableAlltalk(float duration)
{
	if (!g_bAlltalkForced) {
		g_iOriginalAlltalk = g_hCvarSvAlltalk.IntValue;
		g_bAlltalkForced = true;
	}

	SetAlltalk(1);

	if (g_hRevertTimer != null) {
		KillTimer(g_hRevertTimer);
	}
	// No TIMER_FLAG_NO_MAPCHANGE here: that flag makes SM silently kill the timer on a map
	// change instead of calling our callback, which would leave g_hRevertTimer pointing at an
	// already-destroyed handle (crashes KillTimer next time). Letting it carry over is safe;
	// Timer_RevertAlltalk nulls the handle itself whenever it actually fires.
	g_hRevertTimer = CreateTimer(duration, Timer_RevertAlltalk);

	if (g_hCvarSound.BoolValue) {
		EmitSoundToAll(SOUND_ALLTALK, _, SNDCHAN_AUTO, SNDLEVEL_NORMAL, SND_NOFLAGS, 1.0);
	}
}

Action Timer_RevertAlltalk(Handle timer)
{
	g_hRevertTimer = null;
	RevertAlltalk(true);
	return Plugin_Stop;
}

void RevertAlltalk(bool announce)
{
	if (!g_bAlltalkForced) {
		return;
	}

	if (g_hRevertTimer != null) {
		KillTimer(g_hRevertTimer);
		g_hRevertTimer = null;
	}

	SetAlltalk(g_iOriginalAlltalk);
	g_bAlltalkForced = false;

	if (announce && g_bAnnounce) {
		CPrintToChatAll("%t", "AllTalkDisabled");
	}
}

void SetAlltalk(int value)
{
	g_bSelfChangingCvar = true;

	// sv_alltalk has FCVAR_NOTIFY, which makes the engine broadcast a "server cvar changed"
	// message to everyone on every change. Drop the flag for the duration of our own change
	// so only our chat message shows up, then restore it so manual/other changes still notify.
	int flags = GetConVarFlags(g_hCvarSvAlltalk);
	SetConVarFlags(g_hCvarSvAlltalk, flags & ~FCVAR_NOTIFY);
	g_hCvarSvAlltalk.IntValue = value;
	SetConVarFlags(g_hCvarSvAlltalk, flags);

	g_bSelfChangingCvar = false;
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
