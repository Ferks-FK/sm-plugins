/**
 * [L4D2] Rock Bonus
 *
 * Based on plugins from SirPlease/L4D2-Competitive-Rework:
 *   - l4d2_skill_detect          rock skeet detection
 *   - l4d2_penalty_bonus         giving the bonus through vs_defib_penalty, so the
 *                                end of round scoreboard already shows it
 *   - l4d2_hybrid_scoremod_zone  end of round summary format
 */

#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <sdkhooks>
#include <left4dhooks>
#include <colors>
#include <l4d2_rock_bonus>

#define PLUGIN_VERSION "1.0.0"

#define TEAM_SURVIVOR  2
#define MAX_EDICTS     2048

#define ROCK_CHECK_TIME 0.34  // Delay after the rock is destroyed before checking for a skeet

public Plugin myinfo =
{
	name        = "[L4D2] Rock Bonus",
	author      = "Ferks-FK",
	description = "Versus score bonus for breaking Tank rocks",
	version     = PLUGIN_VERSION,
	url         = ""
};

// Translation phrase for each RockBonusEvent (same order as the enum)
static const char g_sEventPhrase[][] =
{
	"Event_Skeet",
	"Event_HitSurvivor"
};

ConVar g_cvEnable;
ConVar g_cvMaxBonus;
ConVar g_cvCountBots;
ConVar g_cvSummary;
ConVar g_cvSkeetPoints;
ConVar g_cvHitPoints;
ConVar g_cvRequireSafe;

GlobalForward g_fwdOnEvent;
GlobalForward g_fwdOnEventPost;

bool g_bRoundLive;
bool g_bBonusInjected;  // Bonus already injected through vs_defib_penalty this round
bool g_bBonusSent;      // Bonus already handed to l4d2_penalty_bonus this round
bool g_bSummaryShown;
bool g_bMadeItKnown;     // L4D2_OnEndVersusModeRound already told us if the survivors made it
bool g_bSurvivorsMadeIt;
bool g_bBonusLost;       // Bonus not given because the survivors didn't make it (l4d2_rock_bonus_require_safe)

// Result of each round of the current map (0 = first half, 1 = second half),
// so the summary at the end of round 2 can show both for comparison
bool g_bResultSaved[2];
bool g_bResultLost[2];
int  g_iResultBonus[2];
int  g_iResultMax[2];
int  g_iResultBroken[2];
int  g_iResultHits[2];

ConVar g_cvDefibPenalty;          // vs_defib_penalty, used to inject the bonus
int    g_iOriginalDefibPenalty;   // Value to restore after we overrode it
bool   g_bDefibPenaltyOverridden;

int g_iRoundBonus;   // Bonus accumulated this round, always within [0, max]
int g_iEventCount[RockBonus_EventCount];

int  g_iLastHitRock[MAXPLAYERS + 1];  // Entity reference of the last rock that hit each survivor

bool g_bRockTracked[MAX_EDICTS + 1];
int  g_iRockSerial[MAX_EDICTS + 1];   // Detects slot reuse between destruction and the delayed check
int  g_iRockDamage[MAX_EDICTS + 1];   // Survivor damage dealt to the rock, -1 = touched something / hit someone
int  g_iRockSkeeter[MAX_EDICTS + 1];  // userid of the last survivor who shot the rock

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

public APLRes AskPluginLoad2(Handle myself, bool late, char[] error, int errMax)
{
	if (GetEngineVersion() != Engine_Left4Dead2)
	{
		strcopy(error, errMax, "This plugin only supports Left 4 Dead 2.");
		return APLRes_SilentFailure;
	}

	CreateNative("RockBonus_GetRoundBonus", Native_GetRoundBonus);
	CreateNative("RockBonus_GetEventCount", Native_GetEventCount);
	RegPluginLibrary("l4d2_rock_bonus");

	g_bRoundLive = late;
	return APLRes_Success;
}

public void OnPluginStart()
{
	LoadTranslations("l4d2_rock_bonus.phrases");

	CreateConVar("l4d2_rock_bonus_version", PLUGIN_VERSION, "Plugin version", FCVAR_NOTIFY | FCVAR_DONTRECORD);
	g_cvEnable    = CreateConVar("l4d2_rock_bonus_enable", "1", "Enable the plugin (Versus only)", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_cvMaxBonus  = CreateConVar("l4d2_rock_bonus_max", "400", "Maximum bonus per round (0 = no limit)", FCVAR_NOTIFY, true, 0.0);
	g_cvCountBots = CreateConVar("l4d2_rock_bonus_count_bots", "1", "Events caused by bots award points", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_cvSummary   = CreateConVar("l4d2_rock_bonus_summary", "2", "End of round summary: 0 = off, 1 = always, 2 = only if events happened", FCVAR_NOTIFY, true, 0.0, true, 2.0);
	g_cvRequireSafe = CreateConVar("l4d2_rock_bonus_require_safe", "1", "Only give the bonus if the survivors reach the saferoom (or escape in the finale)", FCVAR_NOTIFY, true, 0.0, true, 1.0);

	g_cvSkeetPoints = CreateConVar("l4d2_rock_bonus_skeet_points", "25", "Points given when a survivor breaks a Tank rock (0 = disabled)", FCVAR_NOTIFY, true, 0.0);
	g_cvHitPoints   = CreateConVar("l4d2_rock_bonus_hit_points", "25", "Points taken when a rock hits a survivor (0 = disabled)", FCVAR_NOTIFY, true, 0.0);

	AutoExecConfig(true, "l4d2_rock_bonus");

	g_fwdOnEvent     = new GlobalForward("RockBonus_OnEvent", ET_Hook, Param_Cell, Param_Cell, Param_CellByRef);
	g_fwdOnEventPost = new GlobalForward("RockBonus_OnEventPost", ET_Ignore, Param_Cell, Param_Cell, Param_Cell, Param_Cell);

	g_cvDefibPenalty = FindConVar("vs_defib_penalty");

	HookEvent("round_start", Event_RoundStart, EventHookMode_PostNoCopy);
	HookEvent("round_end", Event_RoundEnd, EventHookMode_PostNoCopy);

	// Late load: hook players already in game
	for (int i = 1; i <= MaxClients; i++)
	{
		if (IsClientInGame(i))
			OnClientPutInServer(i);
	}

	ResetRound();
}

public void OnPluginEnd()
{
	RestoreDefibPenalty();
}

public void OnMapStart()
{
	RestoreDefibPenalty();
	ResetRound();

	g_bResultSaved[0] = false;
	g_bResultSaved[1] = false;

	// Pending skeet checks are killed on map change (TIMER_FLAG_NO_MAPCHANGE)
	for (int i = 0; i <= MAX_EDICTS; i++)
		g_bRockTracked[i] = false;
}

public void OnClientPutInServer(int client)
{
	SDKHook(client, SDKHook_OnTakeDamagePost, OnPlayerTakeDamagePost);
}

void ResetRound()
{
	g_iRoundBonus   = 0;
	g_bBonusInjected = false;
	g_bBonusSent     = false;
	g_bSummaryShown = false;
	g_bMadeItKnown  = false;
	g_bBonusLost    = false;

	for (RockBonusEvent ev = RockBonus_Skeet; ev < RockBonus_EventCount; ev++)
		g_iEventCount[ev] = 0;

	for (int i = 1; i <= MaxClients; i++)
		g_iLastHitRock[i] = 0;
}

bool IsActive()
{
	return g_cvEnable.BoolValue && g_bRoundLive && L4D_IsVersusMode();
}

void Event_RoundStart(Event event, const char[] name, bool dontBroadcast)
{
	RestoreDefibPenalty();
	ResetRound();
	g_bRoundLive = true;
}

void Event_RoundEnd(Event event, const char[] name, bool dontBroadcast)
{
	g_bRoundLive = false;
}

// ---------------------------------------------------------------------------
// Detection: rock broken (skeet)
//
//   - Rocks are tracked from the moment they are released (left4dhooks).
//   - TraceAttack from a survivor accumulates damage and stores the shooter.
//   - Any Touch, or the rock damaging a survivor, marks it as not skeeted (-1).
//   - When the rock is destroyed, wait ROCK_CHECK_TIME so late damage can
//     still cancel it; damage > 0 at that point means it was skeeted.
// ---------------------------------------------------------------------------

public void L4D_TankRock_OnRelease_Post(int tank, int rock, const float vecPos[3], const float vecAng[3], const float vecVel[3], const float vecRot[3])
{
	if (rock <= MaxClients || rock > MAX_EDICTS)
		return;

	g_bRockTracked[rock] = true;
	g_iRockSerial[rock]++;
	g_iRockDamage[rock]  = 0;
	g_iRockSkeeter[rock] = 0;

	SDKHook(rock, SDKHook_TraceAttack, OnRockTraceAttack);
	SDKHook(rock, SDKHook_Touch, OnRockTouch);
}

Action OnRockTraceAttack(int victim, int &attacker, int &inflictor, float &damage, int &damagetype, int &ammotype, int hitbox, int hitgroup)
{
	if (IsSurvivor(attacker) && g_iRockDamage[victim] >= 0)
	{
		g_iRockDamage[victim] += RoundToFloor(damage);
		g_iRockSkeeter[victim] = GetClientUserId(attacker);
	}

	return Plugin_Continue;
}

void OnRockTouch(int entity, int other)
{
	// The rock touched something: it was not skeeted
	g_iRockDamage[entity] = -1;
	SDKUnhook(entity, SDKHook_Touch, OnRockTouch);
}

public void OnEntityDestroyed(int entity)
{
	if (entity <= MaxClients || entity > MAX_EDICTS || !g_bRockTracked[entity])
		return;

	SDKUnhook(entity, SDKHook_TraceAttack, OnRockTraceAttack);

	DataPack pack;
	CreateDataTimer(ROCK_CHECK_TIME, Timer_CheckRockSkeet, pack, TIMER_FLAG_NO_MAPCHANGE);
	pack.WriteCell(entity);
	pack.WriteCell(g_iRockSerial[entity]);
}

Action Timer_CheckRockSkeet(Handle timer, DataPack pack)
{
	pack.Reset();
	int rock   = pack.ReadCell();
	int serial = pack.ReadCell();

	// Slot was reused by a new rock in the meantime
	if (!g_bRockTracked[rock] || g_iRockSerial[rock] != serial)
		return Plugin_Stop;

	g_bRockTracked[rock] = false;

	// Didn't hit anyone / didn't touch anything: it was shot down
	if (g_iRockDamage[rock] > 0)
	{
		int client = GetClientOfUserId(g_iRockSkeeter[rock]);
		if (IsSurvivor(client))
			TriggerEvent(RockBonus_Skeet, client);
	}

	return Plugin_Stop;
}

// ---------------------------------------------------------------------------
// Detection: rock hit a survivor
// ---------------------------------------------------------------------------

// The rock itself is the damage inflictor, so this also catches the hit that
// incapacitates a survivor, and doesn't depend on which Tank threw it
void OnPlayerTakeDamagePost(int victim, int attacker, int inflictor, float damage, int damagetype)
{
	if (inflictor <= MaxClients || inflictor > MAX_EDICTS || !IsValidEntity(inflictor) || !IsSurvivor(victim))
		return;

	char cls[16];
	GetEntityClassname(inflictor, cls, sizeof(cls));
	if (!StrEqual(cls, "tank_rock"))
		return;

	// The rock landed on someone: it can't count as a skeet
	if (g_bRockTracked[inflictor])
		g_iRockDamage[inflictor] = -1;

	// One rock can deal damage more than once: count it once per survivor
	int ref = EntIndexToEntRef(inflictor);
	if (g_iLastHitRock[victim] == ref)
		return;

	g_iLastHitRock[victim] = ref;
	TriggerEvent(RockBonus_HitSurvivor, victim);
}

// ---------------------------------------------------------------------------
// Core: apply an event's points
// ---------------------------------------------------------------------------

void TriggerEvent(RockBonusEvent event, int client)
{
	if (!IsActive())
		return;

	if (IsFakeClient(client) && !g_cvCountBots.BoolValue)
		return;

	// Counted even when the event gives no points, for the summary
	g_iEventCount[event]++;

	int points = GetEventPoints(event);
	if (points == 0)
		return;

	int changed = points;
	Action result = Plugin_Continue;
	Call_StartForward(g_fwdOnEvent);
	Call_PushCell(client);
	Call_PushCell(event);
	Call_PushCellRef(changed);
	Call_Finish(result);

	if (result >= Plugin_Handled)
		return;
	if (result == Plugin_Changed)
		points = changed;

	int applied = AddRoundBonus(points);

	// Bonus at 0 and nothing to take away: stay silent
	if (applied == 0 && points < 0)
		return;

	// A gain with the bonus already at the max is still announced, as "max"
	AnnounceEvent(event, client, applied);

	if (applied == 0)
		return;

	Call_StartForward(g_fwdOnEventPost);
	Call_PushCell(client);
	Call_PushCell(event);
	Call_PushCell(applied);
	Call_PushCell(g_iRoundBonus);
	Call_Finish();
}

int GetEventPoints(RockBonusEvent event)
{
	switch (event)
	{
		case RockBonus_Skeet:       return g_cvSkeetPoints.IntValue;
		case RockBonus_HitSurvivor: return -g_cvHitPoints.IntValue; // The cvar is the amount taken away
	}

	return 0;
}

/**
 * Adds points to the round bonus, keeping it within [0, max] after every event.
 * Penalties only remove what was earned from rocks, never the points the team
 * already had, and what the chat shows is exactly what the team gets.
 *
 * @return  The change actually applied.
 */
int AddRoundBonus(int points)
{
	int before = g_iRoundBonus;
	int after  = before + points;

	if (after < 0)
		after = 0;

	int max = g_cvMaxBonus.IntValue;
	if (max > 0 && after > max)
		after = max;

	g_iRoundBonus = after;
	return after - before;
}

// ---------------------------------------------------------------------------
// End of round: apply to the Versus score and show the summary
//
// The bonus is handed to the game BEFORE it calculates the round score, through
// the defib penalty (vs_defib_penalty * m_iVersusDefibsUsed is subtracted from
// the score, so a negative penalty adds points). The game then includes it in
// its own calculation, and the end of round scoreboard already shows it.
//
// If l4d2_penalty_bonus is loaded it owns vs_defib_penalty, so the bonus is
// given to it through PBONUS_RequestFinalUpdate instead.
// ---------------------------------------------------------------------------

public Action L4D2_OnEndVersusModeRound(bool countSurvivors)
{
	CloseRound();

	// countSurvivors: true when the survivors made it to the saferoom
	g_bMadeItKnown     = true;
	g_bSurvivorsMadeIt = countSurvivors;

	int bonus = GetPayout();
	if (!g_bBonusInjected && bonus > 0 && !LibraryExists("penaltybonus"))
	{
		g_bBonusInjected = true;
		InjectBonusAsDefibPenalty(bonus);
	}

	return Plugin_Continue;
}

// Called by l4d2_penalty_bonus right before the round score is calculated
public int PBONUS_RequestFinalUpdate(int &update)
{
	CloseRound();

	int bonus = GetPayout();
	if (!g_bBonusSent && bonus > 0)
	{
		g_bBonusSent = true;
		update += bonus;
	}

	return update;
}

public void L4D2_OnEndVersusModeRound_Post()
{
	if (g_bSummaryShown || !g_cvEnable.BoolValue)
		return;

	g_bSummaryShown = true;
	int round = GameRules_GetProp("m_bInSecondHalfOfRound") ? 1 : 0;
	SaveRoundResult(round);
	AnnounceSummary(round);
}

// Bonus the team actually gets, taking l4d2_rock_bonus_require_safe into account
int GetPayout()
{
	if (!g_cvEnable.BoolValue || g_iRoundBonus <= 0)
		return 0;

	if (g_cvRequireSafe.BoolValue && !SurvivorsMadeIt())
	{
		g_bBonusLost = true;
		return 0;
	}

	return g_iRoundBonus;
}

bool SurvivorsMadeIt()
{
	if (g_bMadeItKnown)
		return g_bSurvivorsMadeIt;

	// l4d2_penalty_bonus may ask before our L4D2_OnEndVersusModeRound runs:
	// the round only ends with survivors still standing if they made it
	for (int i = 1; i <= MaxClients; i++)
	{
		if (IsSurvivor(i) && IsPlayerAlive(i) && !L4D_IsPlayerIncapacitated(i))
			return true;
	}

	return false;
}

// From here on the bonus is final: events can't change it anymore
void CloseRound()
{
	g_bRoundLive = false;
}

void InjectBonusAsDefibPenalty(int bonus)
{
	// Logical index of the team that played survivors this round
	int team = GameRules_GetProp("m_bAreTeamsFlipped") ? 1 : 0;

	// Keep the real defib penalty: pretend 1 defib was used, with a penalty
	// equal to (real penalty) - (our bonus)
	int defibs = GameRules_GetProp("m_iVersusDefibsUsed", 4, team);

	if (!g_bDefibPenaltyOverridden)
	{
		g_iOriginalDefibPenalty   = g_cvDefibPenalty.IntValue;
		g_bDefibPenaltyOverridden = true;
	}

	SetDefibPenaltySilent(g_iOriginalDefibPenalty * defibs - bonus);
	GameRules_SetProp("m_iVersusDefibsUsed", 1, 4, team);
}

void RestoreDefibPenalty()
{
	if (!g_bDefibPenaltyOverridden)
		return;

	g_bDefibPenaltyOverridden = false;
	SetDefibPenaltySilent(g_iOriginalDefibPenalty);
}

// Changes the cvar without the "Server cvar changed" chat message
void SetDefibPenaltySilent(int value)
{
	int flags = g_cvDefibPenalty.Flags;
	g_cvDefibPenalty.Flags = flags & ~FCVAR_NOTIFY;
	g_cvDefibPenalty.IntValue = value;
	g_cvDefibPenalty.Flags = flags;
}

// ---------------------------------------------------------------------------
// Chat
// ---------------------------------------------------------------------------

void FormatAmount(int client, int amount, char[] buffer, int maxlen)
{
	if (amount > 0)
		Format(buffer, maxlen, "%T", "Amount_Positive", client, amount);
	else if (amount < 0)
		Format(buffer, maxlen, "%T", "Amount_Negative", client, -amount);
	else
		Format(buffer, maxlen, "%T", "Amount_Max", client); // Only reached when the bonus is already at the max
}

void AnnounceEvent(RockBonusEvent event, int client, int points)
{
	char name[MAX_NAME_LENGTH];
	GetClientName(client, name, sizeof(name));

	char amount[64];
	for (int i = 1; i <= MaxClients; i++)
	{
		if (!IsClientInGame(i) || IsFakeClient(i))
			continue;

		FormatAmount(i, points, amount, sizeof(amount));
		CPrintToChat(i, "%T", g_sEventPhrase[view_as<int>(event)], i, name, amount);
	}
}

void SaveRoundResult(int round)
{
	g_bResultSaved[round]  = true;
	g_bResultLost[round]   = g_bBonusLost;
	g_iResultBonus[round]  = g_bBonusLost ? 0 : g_iRoundBonus;
	g_iResultMax[round]    = g_cvMaxBonus.IntValue;
	g_iResultBroken[round] = g_iEventCount[RockBonus_Skeet];
	g_iResultHits[round]   = g_iEventCount[RockBonus_HitSurvivor];
}

// Shows every round of this map up to lastRound (round 2 also shows round 1)
void AnnounceSummary(int lastRound)
{
	int mode = g_cvSummary.IntValue;
	if (mode == 0)
		return;

	if (mode == 2)
	{
		bool anyEvent = false;
		for (int r = 0; r <= lastRound; r++)
		{
			if (g_bResultSaved[r] && (g_iResultBroken[r] > 0 || g_iResultHits[r] > 0))
				anyEvent = true;
		}

		if (!anyEvent)
			return;
	}

	for (int i = 1; i <= MaxClients; i++)
	{
		if (!IsClientInGame(i) || IsFakeClient(i))
			continue;

		for (int r = 0; r <= lastRound; r++)
		{
			if (g_bResultSaved[r])
				PrintRoundResult(i, r);
		}
	}
}

void PrintRoundResult(int client, int round)
{
	// "150/400", or just "150" without a max
	char value[64];
	if (g_iResultMax[round] > 0)
		FormatEx(value, sizeof(value), "{green}%d{default}/{lightgreen}%d{default}", g_iResultBonus[round], g_iResultMax[round]);
	else
		FormatEx(value, sizeof(value), "{green}%d{default}", g_iResultBonus[round]);

	if (g_bResultLost[round])
	{
		CPrintToChat(client, "%T", "Summary_Lost", client, round + 1, value);
		return;
	}

	char broken[64], hit[64];
	FormatCount(client, g_iResultBroken[round], "Broken_One", "Broken_Many", broken, sizeof(broken));
	FormatCount(client, g_iResultHits[round], "Hit_One", "Hit_Many", hit, sizeof(hit));
	CPrintToChat(client, "%T", "Summary", client, round + 1, value, broken, hit);
}

void FormatCount(int client, int count, const char[] one, const char[] many, char[] buffer, int maxlen)
{
	if (count == 1)
		Format(buffer, maxlen, "%T", one, client, count);
	else
		Format(buffer, maxlen, "%T", many, client, count);
}

// ---------------------------------------------------------------------------
// Natives
// ---------------------------------------------------------------------------

any Native_GetRoundBonus(Handle plugin, int numParams)
{
	return g_iRoundBonus;
}

any Native_GetEventCount(Handle plugin, int numParams)
{
	RockBonusEvent event = GetNativeCell(1);
	if (event < RockBonus_Skeet || event >= RockBonus_EventCount)
		return ThrowNativeError(SP_ERROR_NATIVE, "Invalid event (%d)", event);

	return g_iEventCount[event];
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool IsSurvivor(int client)
{
	return client > 0 && client <= MaxClients && IsClientInGame(client) && GetClientTeam(client) == TEAM_SURVIVOR;
}
