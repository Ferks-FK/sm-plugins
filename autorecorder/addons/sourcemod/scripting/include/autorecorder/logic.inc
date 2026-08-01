#if defined _autorecorder_logic_included
  #endinput
#endif
#define _autorecorder_logic_included

#include <@shqke/util/format>
#include <@shqke/util/convars>
#include <@shqke/util/files>
#include <@shqke/util/clients>
#include <colors>
#include <ripext>

ConVar hostip;
ConVar hostport          = null;
ConVar mp_gamemode       = null;
ConVar sm_server_uid     = null;

ConVar sm_autorecord_match_id = null;
ConVar sm_autorecord_enable          = null;
ConVar sm_autorecord_minplayers      = null;
ConVar sm_autorecord_minplayersdelay = null;
ConVar sm_autorecord_ignorebots      = null;
ConVar sm_autorecord_roundsplit      = null;
ConVar sm_autorecord_sizesplit       = null;
ConVar sm_autorecord_lengthsplit     = null;
ConVar sm_autorecord_allowoverwrites = null;
ConVar sm_autorecord_pathfmt         = null;
ConVar sm_autorecord_upload_enable   = null;
ConVar sm_autorecord_upload_url      = null;
ConVar sm_autorecord_upload_apikey   = null;
ConVar sm_autorecord_recover_enable  = null;

float g_flLastMinPlayersTest = -1.0;
bool  g_bOrphanScanDone      = false;

char g_szMatchId[16];
char g_szCurrentCampaign[16];

char g_szDemoPath[PLATFORM_MAX_PATH];
char g_szModFolder[PLATFORM_MAX_PATH];
char g_szDemoMapName[PLATFORM_MAX_PATH];
char g_szLevelName[PLATFORM_MAX_PATH];

char g_szPendingUploadPath[PLATFORM_MAX_PATH];
char g_szPendingUploadMatchId[16];
char g_szPendingUploadMapName[PLATFORM_MAX_PATH];

// ─── Retry queue ─────────────────────────────────────────────────────────────
ArrayList g_hRetryQueue = null;

enum struct RetryEntry {
    char path[PLATFORM_MAX_PATH];
    char matchId[16];
    char mapName[PLATFORM_MAX_PATH];
    int  attempts;
}

#define MAX_RETRY_ATTEMPTS 5
#define RETRY_INTERVAL    15.0 // 15 Seconds
#define MAX_ORPHAN_SCAN_DEPTH 6

// ─── Custom log file ─────────────────────────────────────────────────────────
char g_szLogFile[PLATFORM_MAX_PATH];

void AR_Log_Init()
{
    BuildPath(Path_SM, g_szLogFile, sizeof(g_szLogFile), "logs/autorecorder.log");
}

void AR_WriteLog(const char[] message)
{
    Handle file = OpenFile(g_szLogFile, "a");
    if (file == null) {
        PrintToServer("[autorecorder] Unable to open log file: %s", g_szLogFile);
        return;
    }

    char date[64];
    FormatTime(date, sizeof(date), "%m/%d/%Y - %H:%M:%S", GetTime());

    WriteFileLine(file, "%s: %s", date, message);
    CloseHandle(file);
}

void AR_LogMessage(const char[] format, any ...)
{
    char buffer[1024];
    VFormat(buffer, sizeof(buffer), format, 2);
    AR_WriteLog(buffer);
}

void AR_LogError(const char[] format, any ...)
{
    char buffer[1024];
    VFormat(buffer, sizeof(buffer), format, 2);

    char full[1064];
    Format(full, sizeof(full), "ERROR: %s", buffer);
    AR_WriteLog(full);
}

// ─── Upload ──────────────────────────────────────────────────────────────────

void UploadDemo(const char[] demoPath, const char[] matchId, const char[] mapName, int attempts = 0)
{
    if (!sm_autorecord_upload_enable.BoolValue) {
        return;
    }

    char url[512];
    sm_autorecord_upload_url.GetString(url, sizeof(url));
    if (url[0] == '\0') {
        AR_LogError("sm_autorecord_upload_url is not set — demo not uploaded");
        return;
    }

    if (!FileExists(demoPath)) {
        AR_LogError("Demo file not found for upload: %s", demoPath);
        return;
    }

    char apiKey[256];
    sm_autorecord_upload_apikey.GetString(apiKey, sizeof(apiKey));

    char serverName[64];
    char serverIp[32];
    char port[8];
    hostip.GetString(serverIp, sizeof(serverIp));
    hostport.GetString(port, sizeof(port));
    Format(serverName, sizeof(serverName), "%s:%s", serverIp, port);

    // Only extract the filename (without path)
    char filename[PLATFORM_MAX_PATH];
    strcopy(filename, sizeof(filename), demoPath);
    for (int i = strlen(filename) - 1; i >= 0; i--) {
        if (filename[i] == '/' || filename[i] == '\\') {
            strcopy(filename, sizeof(filename), filename[i + 1]);
            break;
        }
    }

    // Avoid conflicts between simultaneous uploads
    DataPack pack = new DataPack();
    pack.WriteString(demoPath);
    pack.WriteString(matchId);
    pack.WriteString(mapName);
    pack.WriteCell(attempts);

    HTTPRequest req = new HTTPRequest(url);
    req.SetHeader("Content-Type",    "application/octet-stream");
    req.SetHeader("X-API-Key",       apiKey);
    req.SetHeader("X-Demo-Filename", filename);
    req.SetHeader("X-Server-Name",   serverName);
    req.SetHeader("X-Current-Map",   mapName);
    req.SetHeader("X-Match-Id",      matchId);
    req.Timeout = 180;

    req.UploadFile(demoPath, OnDemoUploaded, pack);

    AR_LogMessage("Uploading demo: %s -> %s (DemoPath: %s)", filename, url, demoPath);
}

public void OnDemoUploaded(HTTPStatus status, any value)
{
    DataPack pack = view_as<DataPack>(value);
    pack.Reset();

    char path[PLATFORM_MAX_PATH];
    char matchId[16];
    char mapName[64];
    pack.ReadString(path,    sizeof(path));
    pack.ReadString(matchId, sizeof(matchId));
    pack.ReadString(mapName, sizeof(mapName));
    int  attempts = pack.ReadCell();
    delete pack;

    if (status == HTTPStatus_Created || status == HTTPStatus_OK) {
        AR_LogMessage("Demo uploaded successfully (HTTP %d): %s", status, path);

        if (DeleteFile(path)) {
            AR_LogMessage("Demo deleted from disk: %s", path);
        } else {
            AR_LogError("Failed to delete demo from disk: %s", path);
        }
    }
    else if (status == HTTPStatus_Forbidden) {
        // Configuration error — no point in retrying
        AR_LogError("Demo upload failed: invalid API key (HTTP 403) — not retrying: %s", path);
    }
    else {
        AR_LogError("Demo upload failed (HTTP %d) — queuing for retry: %s", status, path);
        QueueRetry(path, matchId, mapName, attempts);
    }
}

// ─── Retry queue ─────────────────────────────────────────────────────────────

void QueueRetry(const char[] path, const char[] matchId, const char[] mapName, int attempts = 0)
{
    if (path[0] == '\0') return;

    // Avoid duplicates in the queue
    RetryEntry entry;
    for (int i = 0; i < g_hRetryQueue.Length; i++) {
        g_hRetryQueue.GetArray(i, entry);
        if (StrEqual(entry.path, path)) return;
    }

    RetryEntry newEntry;
    strcopy(newEntry.path,    sizeof(newEntry.path),    path);
    strcopy(newEntry.matchId, sizeof(newEntry.matchId), matchId);
    strcopy(newEntry.mapName, sizeof(newEntry.mapName), mapName);
    newEntry.attempts = attempts;

    g_hRetryQueue.PushArray(newEntry);
    AR_LogMessage("Demo queued for retry: %s (attempt %d/%d, queue size: %d)", path, attempts, MAX_RETRY_ATTEMPTS, g_hRetryQueue.Length);
}

public Action Timer_RetryUploads(Handle timer)
{
    if (g_hRetryQueue.Length == 0) return Plugin_Continue;

    RetryEntry entry;
    g_hRetryQueue.GetArray(0, entry);
    g_hRetryQueue.Erase(0);

    if (!FileExists(entry.path)) {
        AR_LogError("Retry skipped — file no longer exists: %s", entry.path);
        return Plugin_Continue;
    }

    if (entry.attempts >= MAX_RETRY_ATTEMPTS) {
        AR_LogError("Demo retry limit reached (%d attempts), giving up: %s", entry.attempts, entry.path);
        return Plugin_Continue;
    }

    AR_LogMessage("Retrying demo upload (attempt %d/%d): %s", entry.attempts + 1, MAX_RETRY_ATTEMPTS, entry.path);
    UploadDemo(entry.path, entry.matchId, entry.mapName, entry.attempts + 1);

    return Plugin_Continue;
}

// ─── Orphaned demo recovery ─────────────────────────────────────────────────
// Recovers demos left behind by a crash/kill (SourceTV_OnStopRecording never
// fires). Only possible if sm_autorecord_pathfmt uses %m as its own path
// segment (e.g. "demos/%m/%l"), since that's the only way to know which
// match a leftover demo belongs to.
// Resolves the directory to scan: sm_autorecord_pathfmt up to the %m segment.
// Only %f, %i, %P, %L and %% are resolved there, since they're stable across
// restarts - any other specifier means the prefix can't be reconstructed.
bool GetOrphanScanRoot(char[] root, int maxlength)
{
    char fmt[PLATFORM_MAX_PATH];
    sm_autorecord_pathfmt.GetString(fmt, sizeof(fmt));

    if (fmt[0] == '\0') {
        AR_LogMessage("Orphan demo recovery: sm_autorecord_pathfmt is empty - skipping scan");
        return false;
    }

    int idx = StrContains(fmt, "%m", true);
    if (idx == -1) {
        AR_LogMessage("Orphan demo recovery: sm_autorecord_pathfmt (\"%s\") does not use %%m - skipping scan", fmt);
        return false;
    }

    // %m must be its own path segment: bounded by '/' (or string edges)
    bool precededOk = (idx > 0) && (fmt[idx - 1] == '/');
    bool followedOk = (fmt[idx + 2] == '\0') || (fmt[idx + 2] == '/');
    if (!precededOk || !followedOk) {
        AR_LogMessage("Orphan demo recovery: %%m in sm_autorecord_pathfmt (\"%s\") is not its own path segment - skipping scan", fmt);
        return false;
    }

    char prefixFmt[PLATFORM_MAX_PATH];
    strcopy(prefixFmt, sizeof(prefixFmt), fmt);
    prefixFmt[idx] = '\0';

    for (int i = 0; prefixFmt[i] != '\0'; i++) {
        if (prefixFmt[i] != '%') continue;

        char specifier = prefixFmt[i + 1];
        if (specifier != 'f' && specifier != 'i' && specifier != 'P' && specifier != 'L' && specifier != '%') {
            AR_LogMessage("Orphan demo recovery: sm_autorecord_pathfmt prefix (\"%s\") before %%m has an unstable specifier (%%%c) - skipping scan", prefixFmt, specifier);
            return false;
        }
        i++;
    }

    int rootlen = FormatPathString(root, maxlength, prefixFmt);
    if (rootlen == 0) {
        AR_LogMessage("Orphan demo recovery: resolved root path is empty (prefix: \"%s\") - skipping scan", prefixFmt);
        return false;
    }

    // Drop the trailing '/' so later concatenation doesn't produce "//"
    if (root[rootlen - 1] == '/') {
        root[rootlen - 1] = '\0';
    }

    if (root[0] == '\0') {
        AR_LogMessage("Orphan demo recovery: resolved root path is empty (prefix: \"%s\") - skipping scan", prefixFmt);
        return false;
    }

    return true;
}

int ScanMatchDirForDemos(const char[] dirPath, const char[] matchId, int depth)
{
    if (depth > MAX_ORPHAN_SCAN_DEPTH) {
        AR_LogMessage("Orphan demo recovery: max scan depth reached at \"%s\" - skipping", dirPath);
        return 0;
    }

    DirectoryListing dir = OpenDirectory(dirPath);
    if (dir == null) {
        AR_LogMessage("Orphan demo recovery: failed to open directory \"%s\"", dirPath);
        return 0;
    }

    int found = 0;
    char entry[PLATFORM_MAX_PATH];
    FileType type;

    while (dir.GetNext(entry, sizeof(entry), type)) {
        if (StrEqual(entry, ".") || StrEqual(entry, "..")) continue;

        char fullPath[PLATFORM_MAX_PATH];
        Format(fullPath, sizeof(fullPath), "%s/%s", dirPath, entry);

        if (type == FileType_Directory) {
            found += ScanMatchDirForDemos(fullPath, matchId, depth + 1);
            continue;
        }

        int len = strlen(entry);
        if (len <= 4 || !StrEqual(entry[len - 4], ".dem")) continue;

        // Never touch the demo that's actively being recorded right now
        if (SourceTV_IsRecording() && StrEqual(fullPath, g_szDemoPath)) continue;

        char mapName[PLATFORM_MAX_PATH];
        ExtractMapNameFromDemoFile(entry, mapName, sizeof(mapName));

        AR_LogMessage("Found orphaned demo: %s (match: %s, map: %s) — queuing for upload", fullPath, matchId, mapName);
        QueueRetry(fullPath, matchId, mapName);
        found++;
    }

    delete dir;
    return found;
}

// Strips ".dem" and the ".N" overwrite-protection suffix, e.g. "c1m1_hotel.2.dem" -> "c1m1_hotel"
void ExtractMapNameFromDemoFile(const char[] filename, char[] mapName, int maxlength)
{
    strcopy(mapName, maxlength, filename);

    int len = strlen(mapName);
    if (len > 4 && StrEqual(mapName[len - 4], ".dem")) {
        mapName[len - 4] = '\0';
        len -= 4;
    }

    int dot = FindCharInString(mapName, '.', true);
    if (dot != -1 && dot + 1 < len) {
        bool numeric = true;
        for (int i = dot + 1; i < len; i++) {
            if (!IsCharNumeric(mapName[i])) { numeric = false; break; }
        }
        if (numeric) {
            mapName[dot] = '\0';
        }
    }
}

void ScanForOrphanedDemos()
{
    if (!sm_autorecord_recover_enable.BoolValue) {
        AR_LogMessage("Orphan demo recovery: sm_autorecord_recover_enable is disabled - skipping scan");
        return;
    }
    if (!sm_autorecord_upload_enable.BoolValue) {
        AR_LogMessage("Orphan demo recovery: sm_autorecord_upload_enable is disabled - skipping scan");
        return;
    }

    char root[PLATFORM_MAX_PATH];
    if (!GetOrphanScanRoot(root, sizeof(root))) return;

    if (!DirExists(root)) {
        AR_LogMessage("Orphan demo recovery: root directory \"%s\" does not exist - skipping scan", root);
        return;
    }

    DirectoryListing dir = OpenDirectory(root);
    if (dir == null) {
        AR_LogMessage("Orphan demo recovery: failed to open root directory \"%s\"", root);
        return;
    }

    AR_LogMessage("Orphan demo recovery: scanning \"%s\" for leftover demos", root);

    int found = 0;
    int matchDirs = 0;
    char entry[PLATFORM_MAX_PATH];
    FileType type;

    while (dir.GetNext(entry, sizeof(entry), type)) {
        if (type != FileType_Directory) continue;
        if (StrEqual(entry, ".") || StrEqual(entry, "..")) continue;

        matchDirs++;

        char matchDir[PLATFORM_MAX_PATH];
        Format(matchDir, sizeof(matchDir), "%s/%s", root, entry);

        found += ScanMatchDirForDemos(matchDir, entry, 0);
    }

    delete dir;

    AR_LogMessage("Orphan demo recovery: scanned %d match folder(s) in \"%s\", queued %d leftover demo(s) for upload", matchDirs, root, found);
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

bool HaveEnoughPlayers()
{
    return Util_ClientsConnected(!sm_autorecord_ignorebots.BoolValue) >= sm_autorecord_minplayers.IntValue;
}

void GenerateMatchId()
{
    Format(g_szMatchId, sizeof(g_szMatchId), "%08x", GetURandomInt());
    sm_autorecord_match_id.SetString(g_szMatchId);
}

/**
 * Extracts the campaign identifier from the map name.
 * Example: "c8m1_apartment" → "c8"
 *          "c1m4_atrium"    → "c1"
 *          "c10m2_swamp"    → "c10"
 */
void ExtractCampaignName(const char[] mapname, char[] campaign, int maxlength)
{
    campaign[0] = '\0';

    int len = strlen(mapname);
    for (int i = 0; i < len - 1; i++) {
        if (mapname[i] == 'c' && IsCharNumeric(mapname[i + 1])) {
            int pos = 0;
            campaign[pos++] = 'c';

            int j = i + 1;
            while (j < len && IsCharNumeric(mapname[j])) {
                if (pos < maxlength - 1) {
                    campaign[pos++] = mapname[j];
                }
                j++;
            }

            if (j < len && mapname[j] == 'm') {
                campaign[pos] = '\0';
                return;
            }

            campaign[0] = '\0';
        }
    }
}

int FormatPathString(char[] buffer, int maxlength, const char[] format)
{
    bool expectSpecifier = false;
    int pos = 0;

    for (int i = 0; i < maxlength && pos < maxlength && format[i] != '\0'; i++) {
        if (format[i] == '%' && (expectSpecifier = !expectSpecifier)) {
            continue;
        }

        if (!expectSpecifier) {
            buffer[pos++] = format[i];
            continue;
        }

        switch (format[i]) {
            case 's': { pos += Format(buffer[pos], maxlength - pos, "%u", GetTime()); }
            case 'q': { pos += Format(buffer[pos], maxlength - pos, "%u", GetURandomInt() % 100000); }
            case 'f': { pos += strcopy(buffer[pos], maxlength - pos, g_szModFolder); }
            case 'l': { pos += strcopy(buffer[pos], maxlength - pos, g_szLevelName); }
            case 'i': { pos += Util_ConVarGetStringValue(sm_server_uid, buffer[pos], maxlength - pos); }
            case 'P': { pos += Util_ConVarGetStringValue(hostport,      buffer[pos], maxlength - pos, "27015"); }
            case 'L': { pos += Util_ConVarGetStringValue(mp_gamemode,   buffer[pos], maxlength - pos, "coop"); }
            case 'm': { pos += strcopy(buffer[pos], maxlength - pos, g_szMatchId); }
            default: {
                if (Util_IsCharTimeSpecifier(format[i])) {
                    static char s_format[3] = "%%";
                    s_format[1] = format[i];

                    static char s_timeBuffer[128];
                    FormatTime(s_timeBuffer, sizeof(s_timeBuffer), s_format);

                    pos += strcopy(buffer[pos], maxlength - pos, s_timeBuffer);
                }
            }
        }

        expectSpecifier = false;
    }

    buffer[pos] = '\0';
    return pos;
}

bool TryStartRecording(const char[] format, char[] error, int maxlength)
{
    if (!LibraryExists("sourcetvsupport")) {
        strcopy(error, maxlength, "Missing extension SourceTV Support");
        return false;
    }

    if (!SourceTV_IsMasterProxy()) {
        strcopy(error, maxlength, "Only SourceTV master proxy can record demos instantly");
        return false;
    }

    int pathlen = 0;
    char path[PLATFORM_MAX_PATH + 1] = "";

    if (format[0] != '\0') {
        pathlen = FormatPathString(path, sizeof(path), format);
    }

    if (path[0] == '\0') {
        char cvarfmt[PLATFORM_MAX_PATH];
        sm_autorecord_pathfmt.GetString(cvarfmt, sizeof(cvarfmt));
        pathlen = FormatPathString(path, sizeof(path), cvarfmt);
    }

    if (path[0] == '\0') {
        pathlen = FormatPathString(path, sizeof(path), "auto-%Y%m%d-%H%M%S-%l-%q");
    }

    if (pathlen == sizeof(path) - 1) {
        Format(error, maxlength, "Buffer overflow (path: \"%s\")", path);
        return false;
    }

    if (!Util_IsValidPath(path)) {
        Format(error, maxlength, "Invalid path \"%s\"", path);
        return false;
    }

    if (!Util_CreateDirHierarchy(path, 0x1FD)) {
        Format(error, maxlength, "Failed to create directory hierarchy (path: \"%s\")", path);
        return false;
    }

    bool bAllowOverwrites = sm_autorecord_allowoverwrites.BoolValue;
    pathlen = Util_StripKnownExtension(path, ".dem");

    for (int i = 0; ; i++) {
        if (i == 10000) {
            path[pathlen] = '\0';
            Format(error, maxlength, "Failed to form a unique file name - ran out of retries (path: \"%s.dem\")", path);
            return false;
        }

        int written = Format(path[pathlen], sizeof(path) - pathlen, (i == 0) ? ".dem" : ".%d.dem", i);
        if (pathlen + written == sizeof(path) - 1) {
            path[pathlen] = '\0';
            if (i == 0) {
                Format(error, maxlength, "Failed to form a file name - buffer overflow (path: \"%s.dem\")", path);
            } else {
                Format(error, maxlength, "Failed to form a unique file name - buffer overflow (path: \"%s.%d.dem\")", path, i);
            }
            return false;
        }

        if (bAllowOverwrites || !FileExists(path)) {
            break;
        }
    }

    if (!SourceTV_StartRecording(path)) {
        Format(error, maxlength, "Failed to start a recording (path: \"%s\")", path);
        return false;
    }

    return true;
}

void TryStartAutoRecording(const char[] reason)
{
    if (!sm_autorecord_enable.BoolValue)  return;
    if (!SourceTV_IsActive())             return;
    if (!SourceTV_IsMasterProxy())        return;
    if (SourceTV_IsRecording())           return;
    if (!HaveEnoughPlayers())             return;
    if (GetGameTime() == 1.0)             return;

    char error[512];
    if (TryStartRecording(NULL_STRING, error, sizeof(error))) {
        AR_LogMessage("Started autorecording (reason: \"%s\")", reason);
    } else {
        AR_LogError("Unable to start autorecording (error: \"%s\")", error);
    }
}

// ─── Frames / Timers ─────────────────────────────────────────────────────────

public void Handler_Frame_CheckStopRecording(any data)
{
    if (!sm_autorecord_enable.BoolValue) return;
    if (!SourceTV_IsRecording())         return;

    if (g_flLastMinPlayersTest != -1.0 && g_flLastMinPlayersTest + sm_autorecord_minplayersdelay.FloatValue < GetEngineTime()) {
        SourceTV_StopRecording();
        return;
    }

    if (g_flLastMinPlayersTest == -1.0 && !HaveEnoughPlayers()) {
        if (sm_autorecord_minplayersdelay.FloatValue <= 0.0) {
            SourceTV_StopRecording();
            return;
        } else {
            g_flLastMinPlayersTest = GetEngineTime();
        }
    }

    int maxsize = sm_autorecord_sizesplit.IntValue;
    if (maxsize > 0) {
        int bytes = FileSize(g_szDemoPath);
        if (bytes / 1000 / 1000 >= maxsize) {
            char szSize[64];
            Util_FormatBytes(szSize, sizeof(szSize), bytes);
            AR_LogMessage("Restarting demo recording (size: %s)", szSize);
            SourceTV_StopRecording();
            TryStartAutoRecording("Max demo file size reached");
            return;
        }
    }

    int maxlength = sm_autorecord_lengthsplit.IntValue;
    if (maxlength > 0) {
        int seconds = RoundToCeil(GetTickInterval() * SourceTV_GetRecordingTick());
        if (seconds / 60 >= maxlength) {
            char szLength[64];
            Util_FormatSeconds(szLength, sizeof(szLength), seconds);
            AR_LogMessage("Restarting demo recording (length: %s)", szLength);
            SourceTV_StopRecording();
            TryStartAutoRecording("Max demo length reached");
            return;
        }
    }
}

public Action RunThink(Handle timer)
{
    Handler_Frame_CheckStopRecording(0);
    return Plugin_Continue;
}

public Action Timer_UploadDemo(Handle timer)
{
    if (g_szPendingUploadPath[0] != '\0') {
        UploadDemo(g_szPendingUploadPath, g_szPendingUploadMatchId, g_szPendingUploadMapName);
        g_szPendingUploadPath[0]    = '\0';
        g_szPendingUploadMatchId[0] = '\0';
        g_szPendingUploadMapName[0] = '\0';
    }
    return Plugin_Stop;
}

// ─── Forwards ────────────────────────────────────────────────────────────────

public void OnClientPutInServer(int client)
{
    TryStartAutoRecording("Client joined");
}

public void OnClientConnected(int client)
{
    if (g_flLastMinPlayersTest != -1.0 && HaveEnoughPlayers()) {
        g_flLastMinPlayersTest = -1.0;
    }
}

public void OnClientDisconnect_Post(int client)
{
    RequestFrame(Handler_Frame_CheckStopRecording);
}

public void Event_PlayerTeam(Event event, const char[] name, bool dontBroadcast)
{
    int client  = GetClientOfUserId(event.GetInt("userid"));
    int oldTeam = event.GetInt("oldteam");

    if (!client || IsFakeClient(client)) return;

    // If the player was already on a team, ignore
    if (oldTeam != 0) return;

    if (SourceTV_IsRecording()) {
        CPrintToChat(client, "%t", "Recording Started", g_szDemoMapName, g_szMatchId);
    }
}

public void Event_round_start(Event event, const char[] name, bool dontBroadcast)
{
    if (sm_autorecord_enable.BoolValue && sm_autorecord_roundsplit.BoolValue) {
        SourceTV_StopRecording();
        TryStartAutoRecording("Round started");
    }
}

// ─── SourceTV Manager ────────────────────────────────────────────────────────

public int SourceTV_OnStartRecording(int instance, const char[] filename)
{
    SourceTV_GetDemoFileName(g_szDemoPath, sizeof(g_szDemoPath));
    strcopy(g_szDemoMapName, sizeof(g_szDemoMapName), g_szLevelName);

    RequestFrame(Handler_Frame_CheckStopRecording);

    return 0;
}

public int SourceTV_OnStopRecording(int instance, const char[] filename, int recordingtick)
{
    g_flLastMinPlayersTest = -1.0;

    // Snapshot of the path and context at the exact moment of stop
    // The engine has not yet finalized the file — a 2s timer ensures the .dem
    // is completely recorded before upload
    strcopy(g_szPendingUploadPath,    sizeof(g_szPendingUploadPath),    g_szDemoPath);
    strcopy(g_szPendingUploadMatchId, sizeof(g_szPendingUploadMatchId), g_szMatchId);
    strcopy(g_szPendingUploadMapName, sizeof(g_szPendingUploadMapName), g_szDemoMapName);

    CreateTimer(2.0, Timer_UploadDemo);

    return 0;
}

public void OnConfigsExecuted()
{
    TryStartAutoRecording("Config executed");

    // Waits until here (rather than OnPluginStart) since sm_autorecord_pathfmt
    // is set via server.cfg and isn't available yet at plugin load.
    if (!g_bOrphanScanDone) {
        g_bOrphanScanDone = true;
        ScanForOrphanedDemos();
    }
}

public void Handler_Frame_MapStart(any data)
{
    TryStartAutoRecording("Map started");
}

public void OnMapStart()
{
    char path[PLATFORM_MAX_PATH];
    GetCurrentMap(path, sizeof(path));
    Util_UnqualifiedFileName(g_szLevelName, sizeof(g_szLevelName), path);

    // Detects campaign change and generates a new match ID immediately
    // The pending upload already has its own snapshot of the previous ID
    char newCampaign[16];
    ExtractCampaignName(g_szLevelName, newCampaign, sizeof(newCampaign));

    if (newCampaign[0] != '\0') {
        if (g_szCurrentCampaign[0] == '\0' || !StrEqual(g_szCurrentCampaign, newCampaign)) {
            strcopy(g_szCurrentCampaign, sizeof(g_szCurrentCampaign), newCampaign);
            GenerateMatchId();
            AR_LogMessage("Campaign changed to %s - New match ID: %s", newCampaign, g_szMatchId);
        }
    }

    RequestFrame(Handler_Frame_MapStart);
}

public void Handler_ConVar_Change(ConVar convar, const char[] oldValue, const char[] newValue)
{
    if (SourceTV_IsRecording()) {
        RequestFrame(Handler_Frame_CheckStopRecording);
    } else {
        TryStartAutoRecording("ConVar change condition trigger");
    }
}

// ─── Init ────────────────────────────────────────────────────────────────────

void Logic_Init()
{
    hostip     = FindConVar("net_public_adr");
    hostport   = FindConVar("hostport");
    mp_gamemode = FindConVar("mp_gamemode");

    sm_server_uid = CreateConVar("sm_server_uid", "", "Unique server ID string");

    sm_autorecord_match_id        = CreateConVar("sm_autorecord_match_id",        "", "Current match ID (auto-managed)", FCVAR_ARCHIVE);
    sm_autorecord_enable          = CreateConVar("sm_autorecord_enable",          "1",   "Enable autorecording features");
    sm_autorecord_minplayers      = CreateConVar("sm_autorecord_minplayers",      "1",   "Minimum players required to allow demo recording");
    sm_autorecord_minplayersdelay = CreateConVar("sm_autorecord_minplayersdelay", "30.0","Delay before stopping a demo, in seconds", FCVAR_NONE, true, 0.0, true, 300.0);
    sm_autorecord_ignorebots      = CreateConVar("sm_autorecord_ignorebots",      "1",   "Ignore bots when counting players");
    sm_autorecord_roundsplit      = CreateConVar("sm_autorecord_roundsplit",      "1",   "Restart recording on every round_start");
    sm_autorecord_sizesplit       = CreateConVar("sm_autorecord_sizesplit",       "0",   "Restart recording if demo file has reached max size, in megabytes");
    sm_autorecord_lengthsplit     = CreateConVar("sm_autorecord_lengthsplit",     "0",   "Restart recording after time of footage, in minutes");
    sm_autorecord_allowoverwrites = CreateConVar("sm_autorecord_allowoverwrites", "0",   "Allow file overwrites, or append .i.dem to the end of path");
    sm_autorecord_pathfmt         = CreateConVar("sm_autorecord_pathfmt",         "",
        "Format string specifying a path where to record demo files.\n"
        ... "	Available specifiers:\n"
        ... "	%s - unix timestamp of when recording has started\n"
        ... "	%q - randomly generated number in range [0-99999]\n"
        ... "	%f - game folder (e.g. left4dead2)\n"
        ... "	%l - level name without extension (e.g. c8m1_apartment)\n"
        ... "	%i - unique id (from convar sm_server_uid)\n"
        ... "   %m - current match ID\n"
        ... "	%P - server game port (from convar hostport)\n"
        ... "	%L - game mode (from convar mp_gamemode)\n"
        ... "	%% - a % sign\n"
        ... "	Single character specifiers from https://www.cplusplus.com/reference/ctime/strftime/\n");

    sm_autorecord_upload_enable  = CreateConVar("sm_autorecord_upload_enable",  "1", "Enable automatic demo upload after recording stops");
    sm_autorecord_upload_url     = CreateConVar("sm_autorecord_upload_url",     "",  "URL of the demo upload endpoint (e.g., https://mysite.com/api/demos/upload)");
    sm_autorecord_upload_apikey  = CreateConVar("sm_autorecord_upload_apikey",  "",  "API key for demo upload endpoint");
    sm_autorecord_recover_enable = CreateConVar("sm_autorecord_recover_enable", "1",
        "On startup, scan the demo folder for leftover .dem files (e.g. from a crash) and upload them.\n"
        ... "	Requires sm_autorecord_pathfmt to use %m as its own path segment (e.g. \"demos/%m/%l\"), otherwise there's no way to know which match a leftover demo belongs to.");

    sm_autorecord_minplayers.AddChangeHook(Handler_ConVar_Change);
    sm_autorecord_ignorebots.AddChangeHook(Handler_ConVar_Change);
    sm_autorecord_minplayersdelay.AddChangeHook(Handler_ConVar_Change);
    sm_autorecord_sizesplit.AddChangeHook(Handler_ConVar_Change);
    sm_autorecord_lengthsplit.AddChangeHook(Handler_ConVar_Change);

    HookEvent("round_start",  Event_round_start, EventHookMode_PostNoCopy);
    HookEvent("player_team",  Event_PlayerTeam);

    g_hRetryQueue = new ArrayList(sizeof(RetryEntry));

    char savedId[16];
    sm_autorecord_match_id.GetString(savedId, sizeof(savedId));

    if (savedId[0] != '\0') {
        strcopy(g_szMatchId, sizeof(g_szMatchId), savedId);
        AR_LogMessage("Restored match ID: %s", g_szMatchId);
    } else {
        GenerateMatchId();
    }

    char currentMap[PLATFORM_MAX_PATH];
    GetCurrentMap(currentMap, sizeof(currentMap));

    char mapName[PLATFORM_MAX_PATH];
    Util_UnqualifiedFileName(mapName, sizeof(mapName), currentMap);

    ExtractCampaignName(mapName, g_szCurrentCampaign, sizeof(g_szCurrentCampaign));

    CreateTimer(5.0,           RunThink,          .flags = TIMER_REPEAT);
    CreateTimer(RETRY_INTERVAL,Timer_RetryUploads, .flags = TIMER_REPEAT);

    GetGameFolderName(g_szModFolder, sizeof(g_szModFolder));

    if (SourceTV_IsRecording()) {
        SourceTV_OnStartRecording(0, NULL_STRING);
    }
}
