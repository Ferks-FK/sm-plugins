#include <sourcemod>
#include <sdktools>
#include <left4dhooks>

#define PLUGIN_VERSION "1.1.0"

#define L4D2_TEAM_NONE      0
#define L4D2_TEAM_SPECTATOR 1
#define L4D2_TEAM_SURVIVOR  2
#define L4D2_TEAM_INFECTED  3

bool g_MustBeFixTeams = false;
bool g_PluginMoving = false;

Handle g_MustBeFixTimer = INVALID_HANDLE;

ConVar g_CvarSurvivorLimit;
ConVar g_CvarMaxInfected;

StringMap g_WinnersMap;
StringMap g_LosersMap;

public Plugin myinfo =
{
    name        = "L4D2 - Fix Teams",
    author      = "Altair Sossai, Ferks-FK",
    description = "Fix teams shuffling during map switching",
    version     = PLUGIN_VERSION,
    url         = ""
};

public void OnPluginStart()
{
    HookEvent("round_start", Event_RoundStart, EventHookMode_PostNoCopy);
    HookEvent("player_team", Event_PlayerTeam);

    g_WinnersMap = new StringMap();
    g_LosersMap  = new StringMap();

    g_CvarSurvivorLimit = FindConVar("survivor_limit");
    g_CvarMaxInfected   = FindConVar("z_max_player_zombies");
}

public void OnClientDisconnect(int client)
{
    if (IsFakeClient(client)) return;

    // On map transition, all players trigger OnClientDisconnect.
    // Only remove from the map if the fix hasn't been activated yet (real disconnection).
    if (g_MustBeFixTeams) return;

    char steamId[32];
    if (!GetClientAuthId(client, AuthId_Steam2, steamId, sizeof(steamId))) return;

    g_WinnersMap.Remove(steamId);
    g_LosersMap.Remove(steamId);
}

public void OnMapStart()
{
    if (!g_MustBeFixTeams) return;

    // Remove from map who didn't reconnect (real disconnection during transition).
    CreateTimer(5.0, Timer_PurgeDisconnected);
}

Action Timer_PurgeDisconnected(Handle timer)
{
    PurgeDisconnectedFromMap(g_WinnersMap);
    PurgeDisconnectedFromMap(g_LosersMap);
    return Plugin_Stop;
}

void PurgeDisconnectedFromMap(StringMap map)
{
    StringMapSnapshot snapshot = map.Snapshot();
    char steamId[32];

    for (int i = 0; i < snapshot.Length; i++)
    {
        snapshot.GetKey(i, steamId, sizeof(steamId));

        if (FindClientBySteamId(steamId) == -1)
            map.Remove(steamId);
    }

    delete snapshot;
}

public void L4D2_OnEndVersusModeRound_Post()
{
    if (!IsSecondHalfOfRound()) return;

    // Kill the previous timer if it exists, prevents it from resetting g_MustBeFixTeams in the middle of the next transition.
    if (g_MustBeFixTimer != INVALID_HANDLE)
    {
        KillTimer(g_MustBeFixTimer);
        g_MustBeFixTimer = INVALID_HANDLE;
    }

    g_MustBeFixTeams = true;
    SaveTeams();
}

Action Timer_CheckManualSpec(Handle timer, DataPack data)
{
    data.Reset();
    int client = GetClientOfUserId(data.ReadCell());

    if (!IsClientInGame(client) || IsFakeClient(client)) return Plugin_Stop;

    // If the player is still in spectator after 1s, it was a manual choice — remove from the maps.
    if (GetClientTeam(client) != L4D2_TEAM_SPECTATOR) return Plugin_Stop;

    char steamId[32];
    if (!GetClientAuthId(client, AuthId_Steam2, steamId, sizeof(steamId))) return Plugin_Stop;

    g_WinnersMap.Remove(steamId);
    g_LosersMap.Remove(steamId);

    return Plugin_Stop;
}

Action Timer_FixTeams(Handle timer)
{
    FixTeams();

    return Plugin_Stop;
}

Action Timer_DisableFixTeams(Handle timer)
{
    g_MustBeFixTeams = false;
    g_MustBeFixTimer = INVALID_HANDLE;

    bool survivorsAreWinning = SurvivorsAreWinning();

    int winnerTeam = survivorsAreWinning ? L4D2_TEAM_SURVIVOR : L4D2_TEAM_INFECTED;
    int losersTeam = survivorsAreWinning ? L4D2_TEAM_INFECTED : L4D2_TEAM_SURVIVOR;

    MoveSpectatorsToAvailableTeam(g_WinnersMap, winnerTeam);
    MoveSpectatorsToAvailableTeam(g_LosersMap,  losersTeam);

    ClearTeamsData();

    return Plugin_Stop;
}

Action Timer_CheckFixTeams(Handle timer)
{
    if (!g_MustBeFixTeams || TeamsDataIsEmpty())
        return Plugin_Stop;

    if (g_MustBeFixTimer == INVALID_HANDLE)
        g_MustBeFixTimer = CreateTimer(30.0, Timer_DisableFixTeams);

    CreateTimer(1.0, Timer_FixTeams);

    return Plugin_Stop;
}

void Event_RoundStart(Event event, const char[] name, bool dontBroadcast)
{
    if (L4D_HasMapStarted() && IsNewGame())
    {
        g_MustBeFixTeams = false;
        g_MustBeFixTimer = INVALID_HANDLE;
        ClearTeamsData();
        return;
    }

    if (!g_MustBeFixTeams) return;

    CreateTimer(2.0, Timer_CheckFixTeams);
}

void Event_PlayerTeam(Event event, const char[] name, bool dontBroadcast)
{
    if (!L4D_HasMapStarted()) return;

    int client = GetClientOfUserId(event.GetInt("userid"));

    if (!IsClientInGame(client) || IsFakeClient(client) || event.GetBool("disconnect")) return;

    if (IsNewGame())
    {
        ClearTeamsData();
        return;
    }

    // Plugin is moving players...
    if (g_PluginMoving) return;

    // Player might have gone to spectator during the fix — check after 1s if they really stayed in spectator.
    if (g_MustBeFixTeams && event.GetInt("team") == L4D2_TEAM_SPECTATOR)
    {
        DataPack data;
        CreateDataTimer(1.0, Timer_CheckManualSpec, data, TIMER_DATA_HNDL_CLOSE);
        data.WriteCell(GetClientUserId(client));
        return;
    }

    CreateTimer(1.0, Timer_FixTeams);
}

// ---------------------------------------------------------------------------
// Core
// ---------------------------------------------------------------------------
void FixTeams()
{
    if (!MustBeFixTeams()) return;

    bool survivorsAreWinning = SurvivorsAreWinning();

    int winnerTeam = survivorsAreWinning ? L4D2_TEAM_SURVIVOR : L4D2_TEAM_INFECTED;
    int losersTeam = survivorsAreWinning ? L4D2_TEAM_INFECTED : L4D2_TEAM_SURVIVOR;

    MoveToSpectatorWhoIsNotInTheTeam(g_WinnersMap, winnerTeam);
    MoveToSpectatorWhoIsNotInTheTeam(g_LosersMap,  losersTeam);

    MoveSpectatorsToTheCorrectTeam(g_WinnersMap, winnerTeam);
    MoveSpectatorsToTheCorrectTeam(g_LosersMap,  losersTeam);
}

// ---------------------------------------------------------------------------
// Save SteamIDs of players in each team
// ---------------------------------------------------------------------------
void SaveTeams()
{
    ClearTeamsData();

    bool survivorsAreWinning = SurvivorsAreWinning();

    int winnerTeam = survivorsAreWinning ? L4D2_TEAM_SURVIVOR : L4D2_TEAM_INFECTED;
    int losersTeam = survivorsAreWinning ? L4D2_TEAM_INFECTED : L4D2_TEAM_SURVIVOR;

    CopyClientsToMap(g_WinnersMap, winnerTeam);
    CopyClientsToMap(g_LosersMap,  losersTeam);
}

void CopyClientsToMap(StringMap map, int team)
{
    char steamId[32];

    for (int client = 1; client <= MaxClients; client++)
    {
        if (!IsClientInGame(client) || IsFakeClient(client) || GetClientTeam(client) != team)
            continue;

        if (!GetClientAuthId(client, AuthId_Steam2, steamId, sizeof(steamId)))
            continue;

        map.SetValue(steamId, true);

        PrintToServer("[Fix Teams] Salvando %s no time %d.", steamId, team);
    }
}

// ---------------------------------------------------------------------------
// Resolve SteamID -> client index at runtime
// ---------------------------------------------------------------------------
int FindClientBySteamId(const char[] steamId)
{
    char clientSteam[32];

    for (int client = 1; client <= MaxClients; client++)
    {
        if (!IsClientConnected(client) || IsFakeClient(client)) continue;

        if (GetClientAuthId(client, AuthId_Steam2, clientSteam, sizeof(clientSteam)) && StrEqual(clientSteam, steamId))
            return client;
    }

    return -1;
}

// ---------------------------------------------------------------------------
// Move to spectator players who are in a team but NOT saved in it
// ---------------------------------------------------------------------------
void MoveToSpectatorWhoIsNotInTheTeam(StringMap map, int team)
{
    char steamId[32];

    for (int client = 1; client <= MaxClients; client++)
    {
        if (!IsClientInGame(client) || IsFakeClient(client) || GetClientTeam(client) != team)
            continue;

        if (!GetClientAuthId(client, AuthId_Steam2, steamId, sizeof(steamId)))
            continue;

        bool dummy;
        if (!map.GetValue(steamId, dummy))
        {
            MovePlayerToTeam(client, L4D2_TEAM_SPECTATOR);
        }
    }
}

// ---------------------------------------------------------------------------
// Move spectators to the correct team if their SteamID is saved in that team
// ---------------------------------------------------------------------------
void MoveSpectatorsToTheCorrectTeam(StringMap map, int team)
{
    StringMapSnapshot snapshot = map.Snapshot();
    int keyCount = snapshot.Length;

    char steamId[32];

    for (int i = 0; i < keyCount; i++)
    {
        snapshot.GetKey(i, steamId, sizeof(steamId));

        int client = FindClientBySteamId(steamId);

        if (client == -1) continue;

        if (!IsClientInGame(client)) continue; // Still loading...

        if (GetClientTeam(client) != L4D2_TEAM_SPECTATOR) continue;

        MovePlayerToTeam(client, team);
    }

    delete snapshot;
}

// ---------------------------------------------------------------------------
// After the timeout: move spectators who were NOT saved to an available team
// ---------------------------------------------------------------------------
void MoveSpectatorsToAvailableTeam(StringMap map, int team)
{
    char steamId[32];

    for (int client = 1; client <= MaxClients; client++)
    {
        if (!IsClientInGame(client) || IsFakeClient(client) || GetClientTeam(client) != L4D2_TEAM_SPECTATOR)
            continue;

        if (!GetClientAuthId(client, AuthId_Steam2, steamId, sizeof(steamId)))
            continue;

        bool dummy;
        if (!map.GetValue(steamId, dummy))
        {
            // Not saved in any team → try to place in an available team
            MovePlayerToTeam(client, team);
        }
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
bool SurvivorsAreWinning()
{
    int flipped = AreTeamsFlipped();

    int survivorIndex = flipped ? 1 : 0;
    int infectedIndex = flipped ? 0 : 1;

    int survivorScore = L4D2Direct_GetVSCampaignScore(survivorIndex);
    int infectedScore = L4D2Direct_GetVSCampaignScore(infectedIndex);

    return survivorScore >= infectedScore;
}

int AreTeamsFlipped()
{
    return GameRules_GetProp("m_bAreTeamsFlipped");
}

int IsSecondHalfOfRound()
{
    return GameRules_GetProp("m_bInSecondHalfOfRound");
}

void ClearTeamsData()
{
    g_WinnersMap.Clear();
    g_LosersMap.Clear();
}

bool TeamsDataIsEmpty()
{
    return g_WinnersMap.Size == 0 && g_LosersMap.Size == 0;
}

bool MustBeFixTeams()
{
    return g_MustBeFixTeams && !TeamsDataIsEmpty();
}

bool IsNewGame()
{
    int teamAScore = L4D2Direct_GetVSCampaignScore(0);
    int teamBScore = L4D2Direct_GetVSCampaignScore(1);

    return teamAScore == 0 && teamBScore == 0;
}

int MovePlayerToTeam(int client, int team)
{
    if (team != L4D2_TEAM_SPECTATOR && NumberOfPlayersInTheTeam(team) >= TeamSize(team))
        return L4D2_TEAM_NONE;

    g_PluginMoving = true;

    int result = L4D2_TEAM_NONE;

    switch (team)
    {
        case L4D2_TEAM_SPECTATOR:
        {
            ChangeClientTeam(client, L4D2_TEAM_SPECTATOR);
            result = L4D2_TEAM_SPECTATOR;
        }
        case L4D2_TEAM_SURVIVOR:
        {
            FakeClientCommand(client, "jointeam 2");
            result = L4D2_TEAM_SURVIVOR;
        }
        case L4D2_TEAM_INFECTED:
        {
            ChangeClientTeam(client, L4D2_TEAM_INFECTED);
            result = L4D2_TEAM_INFECTED;
        }
    }

    g_PluginMoving = false;

    return result;
}

int NumberOfPlayersInTheTeam(int team)
{
    int count = 0;

    for (int client = 1; client <= MaxClients; client++)
    {
        if (!IsClientInGame(client) || IsFakeClient(client) || GetClientTeam(client) != team)
            continue;

        count++;
    }

    return count;
}

int TeamSize(int team)
{
    if (team == L4D2_TEAM_SURVIVOR)
        return g_CvarSurvivorLimit.IntValue;
    else if (team == L4D2_TEAM_INFECTED)
        return g_CvarMaxInfected.IntValue;

    return 0;
}
