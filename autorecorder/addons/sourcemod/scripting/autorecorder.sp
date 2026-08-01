#include <sourcemod>

#define REQUIRE_EXTENSIONS
#include <sourcetvmanager>

#include <autorecorder/logic>
#include <autorecorder/console>

public void OnPluginStart()
{
    LoadTranslations("autorecorder.phrases");

    RegPluginLibrary("autorecorder");

    CreateNative("AR_GetMatchID", Native_AR_GetMatchID);

    AR_Log_Init();
    Logic_Init();
    Console_Init();
}

public int Native_AR_GetMatchID(Handle hPlugin, int numParams)
{
    SetNativeString(1, g_szMatchId, GetNativeCell(2));
    return g_szMatchId[0] != '\0';
}

public void OnLibraryRemoved(const char[] name)
{
    if (strcmp(name, "sourcetvsupport") == 0 && SourceTV_IsRecording()) {
        SourceTV_StopRecording();
    }
}

public APLRes AskPluginLoad2(Handle myself, bool late, char[] error, int maxlen)
{
    switch (GetEngineVersion()) {
        case Engine_Left4Dead2, Engine_Left4Dead:
        {
            return APLRes_Success;
        }
    }

    strcopy(error, maxlen, "Game is not supported.");

    return APLRes_SilentFailure;
}

public Plugin myinfo =
{
    name = "[L4D/2] Automated Demo Recording",
    author = "shqke, Ferks-FK",
    description = "Plugin takes control over demo recording process allowing to record only useful footage",
    version = "1.4",
    url = "https://github.com/shqke/sp_public"
};
