/**
 * [L4D2] Tank Throws Cars
 *
 * SourceMod plugin based on the "Tank Throws Cars" mod:
 * https://steamcommunity.com/sharedfiles/filedetails/?id=3809650828
 */

#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <sdktools>

#define PLUGIN_VERSION "1.0.0"

#define TEAM_INFECTED       3
#define ZC_TANK             8
#define EF_NODRAW           32
#define MASK_PICKUP         33636363
#define COLLISION_GROUP_DEBRIS 1   // collides with the world only

#define SEQ_THROW_ONEHAND   49
#define SEQ_THROW_UNDERHAND 50
#define SEQ_THROW_OVERHEAD  51

#define ROCK_SEARCH_RADIUS  250.0
#define ROCK_WAIT_TIME      3.0
#define PROP_CONTACT_MARGIN 8.0   // geometry this close to a prop's box doesn't block seeing it

enum
{
	STATE_NONE = 0,
	STATE_WAIT_ROCK,
	STATE_HOLDING
}

public Plugin myinfo =
{
	name        = "[L4D2] Tank Throws Cars",
	author      = "Ferks-FK, Raya",
	description = "Tank picks up and throws punchable props/cars instead of a rock",
	version     = PLUGIN_VERSION,
	url         = ""
};

static const float ZERO_VECTOR[3] = {0.0, 0.0, 0.0};

static const char g_sCarModels[][] =
{
	"airport_baggage_tractor", "airport_catering_truck", "airport_fuel_truck", "ambulance", "army_truck",
	"car001a_hatchback", "car001a_phy", "car001b_hatchback", "car001b_phy",
	"car002a", "car002a_physics", "car002b", "car002b_physics",
	"car003a", "car003a_physics", "car003b", "car003b_physics",
	"car004a", "car004a_physics", "car004b", "car004b_physics",
	"car005a", "car005a_physics", "car005b", "car005b_physics",
	"cara_69sedan", "cara_82hatchback", "cara_82hatchback_wrecked", "cara_84sedan", "cara_95sedan", "cara_95sedan_wrecked",
	"cement_truck01", "deliveryvan", "flatnose_truck", "flatnose_truck_wrecked", "flatnose_truck_wrecked_propercollision",
	"longnose_truck", "news_van", "pickup_4x4", "pickup_dually", "pickup_regcab", "pickup_truck_78", "pickup_truck_2004",
	"police_car", "police_car_city", "police_car_lights_on", "police_car_opentrunk", "police_car_rural",
	"semi_truck3", "suv_2001", "taxi_cab", "taxi_city", "taxi_rural",
	"truck001a", "truck002a_cab", "truck003a", "utility_truck", "van", "van001a", "van001a_physics",
	"zapastl", "zapastl_static"
};

static const float g_fHorizontalAngles[][2] =
{
	{0.0, 0.0}, {0.0, 90.0}, {0.0, -90.0}, {0.0, 180.0},
	{90.0, 90.0}, {90.0, -90.0}, {-90.0, 90.0}, {-90.0, -90.0}
};

ConVar g_cvRangePickup;
ConVar g_cvRangeTryThrow;
ConVar g_cvThrowPower;
ConVar g_cvReleaseForwardDist;
ConVar g_cvAimTolerance;

Handle g_hSDKApplyAbsVelocityImpulse;
Handle g_hSDKApplyLocalAngularVelocityImpulse;

int g_iOffAbilityTimestamp = -1;
int g_iOffStaggerTimestamp = -1;

int g_iState[MAXPLAYERS + 1];
int g_iPropRef[MAXPLAYERS + 1] = {INVALID_ENT_REFERENCE, ...};
int g_iProxyRef[MAXPLAYERS + 1] = {INVALID_ENT_REFERENCE, ...};
int g_iPropOldEffects[MAXPLAYERS + 1];
bool g_bIsCar[MAXPLAYERS + 1];
float g_fWaitRockUntil[MAXPLAYERS + 1];
ArrayList g_hHiddenChildren[MAXPLAYERS + 1];  // [ref, old m_fEffects]

// ============================================================
// Setup
// ============================================================
public void OnPluginStart()
{
	if (GetEngineVersion() != Engine_Left4Dead2)
		SetFailState("This plugin only supports Left 4 Dead 2.");

	// +8: offset of the timer's timestamp
	g_iOffAbilityTimestamp = FindSendPropInfo("CThrow", "m_nextActivationTimer");
	if (g_iOffAbilityTimestamp == -1)
		SetFailState("Could not find CThrow::m_nextActivationTimer.");
	g_iOffAbilityTimestamp += 8;

	g_iOffStaggerTimestamp = FindSendPropInfo("CTerrorPlayer", "m_staggerTimer");
	if (g_iOffStaggerTimestamp != -1)
		g_iOffStaggerTimestamp += 8;

	GameData gameData = new GameData("tank_throws_cars");
	if (gameData == null)
		SetFailState("Could not load gamedata/tank_throws_cars.txt.");
	g_hSDKApplyAbsVelocityImpulse = PrepImpulseCall(gameData, "CBaseEntity::ApplyAbsVelocityImpulse");
	g_hSDKApplyLocalAngularVelocityImpulse = PrepImpulseCall(gameData, "CBaseEntity::ApplyLocalAngularVelocityImpulse");
	delete gameData;

	CreateConVar("tank_throws_cars_version", PLUGIN_VERSION, "Plugin version.", FCVAR_NOTIFY | FCVAR_DONTRECORD);
	g_cvRangePickup        = CreateConVar("tank_throws_cars_range_pickup", "50", "Max distance (on the ground, from the tank's feet) to the spot of the prop the tank is aiming at.", _, true, 1.0, true, 1000.0);
	g_cvRangeTryThrow      = CreateConVar("tank_throws_cars_range_try_throw", "50", "Bot Tanks only. -1 = bots never pick up props (always a normal rock). >= 0 = a bot aiming at a throwable prop this close throws it.", _, true, -1.0, true, 500.0);
	g_cvAimTolerance       = CreateConVar("tank_throws_cars_aim_tolerance", "0", "How many units the aim may miss a prop's edge and still pick it up.", _, true, 0.0, true, 64.0);
	g_cvThrowPower         = CreateConVar("tank_throws_cars_throw_power", "1200", "Force used when the tank throws.", _, true, 100.0, true, 10000.0);
	g_cvReleaseForwardDist = CreateConVar("tank_throws_cars_release_forward_dist", "50", "How far in front of the tank the prop is released.", _, true, 0.0, true, 500.0);
	AutoExecConfig(true, "tank_throws_cars");

	HookEvent("round_start", Event_RoundStart, EventHookMode_PostNoCopy);
	HookEvent("tank_spawn", Event_TankSpawn);
	HookEvent("ability_use", Event_AbilityUse);
	HookEvent("tank_frustrated", Event_TankFrustrated);
}

public void OnMapEnd()
{
	for (int i = 1; i <= MaxClients; i++)
		ResetState(i);
}

// Unloading mid-throw would leave the real prop invisible and frozen
public void OnPluginEnd()
{
	for (int client = 1; client <= MaxClients; client++)
	{
		if (g_iState[client] != STATE_HOLDING)
			continue;

		int proxy = EntRefToEntIndex(g_iProxyRef[client]);
		if (proxy != INVALID_ENT_REFERENCE)
			RemoveEntity(proxy);

		int prop = EntRefToEntIndex(g_iPropRef[client]);
		if (prop == INVALID_ENT_REFERENCE)
			continue;

		SetEntProp(prop, Prop_Send, "m_fEffects", g_iPropOldEffects[client]);
		ArrayList children = g_hHiddenChildren[client];
		int count = children ? children.Length : 0;
		for (int i = 0; i < count; i++)
		{
			int child = EntRefToEntIndex(children.Get(i, 0));
			if (child == INVALID_ENT_REFERENCE)
				continue;
			if (HasEntProp(child, Prop_Send, "m_fEffects"))
				SetEntProp(child, Prop_Send, "m_fEffects", children.Get(i, 1));
			AcceptEntityInput(child, "EnableCollision");
		}

		SetPropIntAny(prop, "m_hasTankGlow", 1);
		AcceptEntityInput(prop, "EnableMotion");
		AcceptEntityInput(prop, "EnableCollision");
		AcceptEntityInput(prop, "Wake");
	}
}

public void OnClientDisconnect(int client)
{
	if (g_iState[client] == STATE_HOLDING)
		DropHeldProp(client);
	else
		ResetState(client);
}

void Event_RoundStart(Event event, const char[] name, bool dontBroadcast)
{
	for (int i = 1; i <= MaxClients; i++)
		ResetState(i);
}

// Tank being passed to another player: drop the prop from the hand now,
// before this player turns into a ghost and takes the hand with him
void Event_TankFrustrated(Event event, const char[] name, bool dontBroadcast)
{
	int client = GetClientOfUserId(event.GetInt("userid"));
	if (!client)
		return;

	if (g_iState[client] == STATE_HOLDING)
		DropHeldProp(client);
	else
		ResetState(client);
}

// ============================================================
// Bot tank: throw the prop it is aiming at
// ============================================================
void Event_TankSpawn(Event event, const char[] name, bool dontBroadcast)
{
	int userid = event.GetInt("userid");
	int client = GetClientOfUserId(userid);
	if (!client)
		return;

	ResetState(client);

	if (g_cvRangeTryThrow.FloatValue >= 0.0)
		CreateTimer(0.1, Timer_TankActiveChecker, userid, TIMER_REPEAT | TIMER_FLAG_NO_MAPCHANGE);
}

Action Timer_TankActiveChecker(Handle timer, int userid)
{
	int client = GetClientOfUserId(userid);
	if (!IsValidTank(client) || !IsFakeClient(client))
		return Plugin_Stop;

	// The tank has left its idle state once it sees a target or starts moving
	float vel[3];
	GetEntPropVector(client, Prop_Data, "m_vecAbsVelocity", vel);

	if (GetPropIntAny(client, "m_hasVisibleThreats") || GetVectorLength(vel) > 1.0)
	{
		CreateTimer(2.0, Timer_EnableTankChecker, userid, TIMER_FLAG_NO_MAPCHANGE);
		return Plugin_Stop;
	}
	return Plugin_Continue;
}

Action Timer_EnableTankChecker(Handle timer, int userid)
{
	int client = GetClientOfUserId(userid);
	if (IsValidTank(client) && IsFakeClient(client))
		CreateTimer(0.5, Timer_PropThrowControl, userid, TIMER_REPEAT | TIMER_FLAG_NO_MAPCHANGE);
	return Plugin_Stop;
}

Action Timer_PropThrowControl(Handle timer, int userid)
{
	int client = GetClientOfUserId(userid);
	if (!IsValidTank(client) || !IsFakeClient(client))
		return Plugin_Stop;

	if (g_iState[client] != STATE_NONE || IsThrowSequence(GetEntProp(client, Prop_Data, "m_nSequence")))
		return Plugin_Continue;

	int ability = GetEntPropEnt(client, Prop_Send, "m_customAbility");
	if (ability <= MaxClients || !IsValidEntity(ability))
		return Plugin_Continue;

	if (GetGameTime() < GetEntDataFloat(ability, g_iOffAbilityTimestamp))
		return Plugin_Continue;

	float rangeTry = g_cvRangeTryThrow.FloatValue;
	if (rangeTry < 0.0)
		return Plugin_Stop;

	if (FindAimedProp(client, rangeTry) != -1)
	{
		// Add the button instead of replacing the field, other plugins may be forcing buttons too
		SetEntProp(client, Prop_Data, "m_afButtonForced", GetEntProp(client, Prop_Data, "m_afButtonForced") | IN_ATTACK2);
		RequestFrame(Frame_ClearForcedAttack2, userid);
	}
	return Plugin_Continue;
}

void Frame_ClearForcedAttack2(int userid)
{
	int client = GetClientOfUserId(userid);
	if (client && IsClientInGame(client))
		SetEntProp(client, Prop_Data, "m_afButtonForced", GetEntProp(client, Prop_Data, "m_afButtonForced") & ~IN_ATTACK2);
}

// ============================================================
// Throw: replace the rock with a prop
// ============================================================
void Event_AbilityUse(Event event, const char[] name, bool dontBroadcast)
{
	char ability[32];
	event.GetString("ability", ability, sizeof(ability));
	if (!StrEqual(ability, "ability_throw"))
		return;

	int client = GetClientOfUserId(event.GetInt("userid"));
	if (!client || !IsClientInGame(client) || g_iState[client] != STATE_NONE)
		return;

	// range_try_throw -1: bots never swap their rock for a prop
	if (IsFakeClient(client) && g_cvRangeTryThrow.FloatValue < 0.0)
		return;

	int prop = FindAimedProp(client, g_cvRangePickup.FloatValue);
	if (prop == -1)
		return;

	g_iState[client] = STATE_WAIT_ROCK;
	g_iPropRef[client] = EntIndexToEntRef(prop);
	g_fWaitRockUntil[client] = GetGameTime() + ROCK_WAIT_TIME;

	ProcessWaitRock(client);
}

public void OnGameFrame()
{
	for (int client = 1; client <= MaxClients; client++)
	{
		switch (g_iState[client])
		{
			case STATE_WAIT_ROCK: ProcessWaitRock(client);
			case STATE_HOLDING:   ProcessHolding(client);
		}
	}
}

void ProcessWaitRock(int client)
{
	// Give up if another tank grabbed this prop meanwhile (it would end up invisible)
	int prop = EntRefToEntIndex(g_iPropRef[client]);
	if (!IsValidTank(client) || GetGameTime() > g_fWaitRockUntil[client]
		|| prop == INVALID_ENT_REFERENCE || !IsPropObject(prop))
	{
		ResetState(client);
		return;
	}

	float tankOrigin[3], rockOrigin[3];
	GetClientAbsOrigin(client, tankOrigin);

	int nearestRock = -1;
	float nearestDist = ROCK_SEARCH_RADIUS;
	int rock = -1;
	while ((rock = FindEntityByClassname(rock, "tank_rock")) != -1)
	{
		GetEntPropVector(rock, Prop_Data, "m_vecAbsOrigin", rockOrigin);
		float dist = GetVectorDistance(rockOrigin, tankOrigin);
		if (dist <= nearestDist)
		{
			nearestDist = dist;
			nearestRock = rock;
		}
	}

	if (nearestRock != -1)
		StartHolding(client, prop, nearestRock);
}

void StartHolding(int client, int prop, int rock)
{
	char model[PLATFORM_MAX_PATH];
	GetEntPropString(prop, Prop_Data, "m_ModelName", model, sizeof(model));

	float origin[3], angles[3];
	GetEntPropVector(prop, Prop_Send, "m_vecOrigin", origin);
	GetEntPropVector(prop, Prop_Send, "m_angRotation", angles);

	// Display-only copy attached to the tank's hand; the real prop stays hidden
	int proxy = CreateEntityByName("prop_dynamic_override");
	if (proxy == -1)
	{
		ResetState(client);
		return;
	}
	DispatchKeyValue(proxy, "model", model);
	DispatchKeyValue(proxy, "solid", "0");
	DispatchKeyValueVector(proxy, "origin", origin);
	DispatchKeyValueVector(proxy, "angles", angles);
	DispatchSpawn(proxy);

	StopThrowFlyLoop(rock);
	RemoveEntity(rock);

	// Like a car punched by the tank, a car alarm the tank grabbed can no longer go off
	char classname[64];
	GetEntityClassname(prop, classname, sizeof(classname));
	if (StrEqual(classname, "prop_car_alarm"))
	{
		AcceptEntityInput(prop, "Disable");
		RemoveAlarmLights(prop);
	}

	AcceptEntityInput(prop, "DisableCollision");
	AcceptEntityInput(prop, "DisableMotion");
	TeleportEntity(prop, NULL_VECTOR, NULL_VECTOR, ZERO_VECTOR);
	SetPropIntAny(prop, "m_hasTankGlow", 0);

	g_iPropOldEffects[client] = GetEntProp(prop, Prop_Send, "m_fEffects");
	SetEntProp(prop, Prop_Send, "m_fEffects", g_iPropOldEffects[client] | EF_NODRAW);
	HideChildren(client, prop);

	SetEntProp(proxy, Prop_Send, "m_nSkin", GetEntProp(prop, Prop_Send, "m_nSkin"));
	SetEntProp(proxy, Prop_Send, "m_nBody", GetEntProp(prop, Prop_Send, "m_nBody"));
	SetEntProp(proxy, Prop_Send, "m_clrRender", GetEntProp(prop, Prop_Send, "m_clrRender"));

	AcceptEntityInput(proxy, "DisableCollision");
	SetVariantString("!activator");
	AcceptEntityInput(proxy, "SetParent", client);
	SetVariantString("rhand");
	AcceptEntityInput(proxy, "SetParentAttachment", client);

	g_iProxyRef[client] = EntIndexToEntRef(proxy);
	g_bIsCar[client] = ModelInList(model, g_sCarModels, sizeof(g_sCarModels));
	// Animation is checked from the next frame on, once the throw sequence is set
	g_iState[client] = STATE_HOLDING;
}

void ProcessHolding(int client)
{
	int prop = EntRefToEntIndex(g_iPropRef[client]);
	if (prop == INVALID_ENT_REFERENCE)
	{
		int proxy = EntRefToEntIndex(g_iProxyRef[client]);
		if (proxy != INVALID_ENT_REFERENCE)
			RemoveEntity(proxy);
		ResetState(client);
		return;
	}

	// Lost the tank without tank_frustrated: the hand may already be elsewhere,
	// so put the prop back where it was
	if (!IsTankBody(client))
	{
		CancelHold(client);
		return;
	}

	if (!IsValidTank(client))
	{
		DropHeldProp(client);
		return;
	}

	int seq = GetEntProp(client, Prop_Data, "m_nSequence");
	bool weirdAct = !IsThrowSequence(seq) || IsStaggering(client);

	float cycle = GetEntPropFloat(client, Prop_Data, "m_flCycle");
	bool released = (seq == SEQ_THROW_ONEHAND && cycle >= 0.75)
		|| (seq == SEQ_THROW_UNDERHAND && cycle >= 0.75)
		|| (seq == SEQ_THROW_OVERHEAD && cycle >= 0.82);

	if (!released && !weirdAct)
		return;

	if (weirdAct)
	{
		DropHeldProp(client);
		return;
	}

	float eyeAngles[3], force[3];
	GetClientEyeAngles(client, eyeAngles);
	GetAngleVectors(eyeAngles, force, NULL_VECTOR, NULL_VECTOR);
	ScaleVector(force, g_cvThrowPower.FloatValue);

	float releaseOrigin[3];
	GetClientEyePosition(client, releaseOrigin);
	if (seq == SEQ_THROW_ONEHAND)
		releaseOrigin[2] += 30.0;
	else if (seq == SEQ_THROW_OVERHEAD)
		releaseOrigin[2] += 50.0;

	float forwardDist = g_cvReleaseForwardDist.FloatValue;
	if (forwardDist > 0.0)
	{
		float absAngles[3], fwd[3];
		GetClientAbsAngles(client, absAngles);
		GetAngleVectors(absAngles, fwd, NULL_VECTOR, NULL_VECTOR);
		ScaleVector(fwd, forwardDist);
		AddVectors(releaseOrigin, fwd, releaseOrigin);
	}

	float eyePos[3];
	GetClientEyePosition(client, eyePos);
	ScheduleRelease(client, releaseOrigin, ZERO_VECTOR, false, force, eyePos);
}

// Tank died, staggered or got interrupted: drop the prop where the hand was
void DropHeldProp(int client)
{
	int prop = EntRefToEntIndex(g_iPropRef[client]);
	int proxy = EntRefToEntIndex(g_iProxyRef[client]);
	if (prop == INVALID_ENT_REFERENCE)
	{
		if (proxy != INVALID_ENT_REFERENCE)
			RemoveEntity(proxy);
		ResetState(client);
		return;
	}

	float origin[3], angles[3];
	if (proxy != INVALID_ENT_REFERENCE)
	{
		// Detach from the hand to read its position in the world
		AcceptEntityInput(proxy, "ClearParent");
		GetEntPropVector(proxy, Prop_Send, "m_vecOrigin", origin);
		GetEntPropVector(proxy, Prop_Send, "m_angRotation", angles);
	}
	else
	{
		GetEntPropVector(prop, Prop_Send, "m_vecOrigin", origin);
		GetEntPropVector(prop, Prop_Send, "m_angRotation", angles);
	}

	ScheduleRelease(client, origin, angles, true, ZERO_VECTOR, ZERO_VECTOR);
}

// Throw cancelled: the real prop stayed hidden at its original spot, so bring it back there
void CancelHold(int client)
{
	int prop = EntRefToEntIndex(g_iPropRef[client]);
	if (prop == INVALID_ENT_REFERENCE)
	{
		int proxy = EntRefToEntIndex(g_iProxyRef[client]);
		if (proxy != INVALID_ENT_REFERENCE)
			RemoveEntity(proxy);
		ResetState(client);
		return;
	}

	float origin[3], angles[3];
	GetEntPropVector(prop, Prop_Send, "m_vecOrigin", origin);
	GetEntPropVector(prop, Prop_Send, "m_angRotation", angles);
	ScheduleRelease(client, origin, angles, true, ZERO_VECTOR, ZERO_VECTOR);
}

void ScheduleRelease(int client, const float origin[3], const float angles[3], bool weirdAct, const float force[3], const float tankEyePos[3])
{
	DataPack pack;
	CreateDataTimer(0.05, Timer_ReleaseProp, pack, TIMER_FLAG_NO_MAPCHANGE);
	pack.WriteCell(g_iPropRef[client]);
	pack.WriteCell(g_iProxyRef[client]);
	pack.WriteCell(g_bIsCar[client]);
	pack.WriteCell(weirdAct);
	pack.WriteCell(g_iPropOldEffects[client]);
	for (int i = 0; i < 3; i++) pack.WriteFloat(origin[i]);
	for (int i = 0; i < 3; i++) pack.WriteFloat(angles[i]);
	for (int i = 0; i < 3; i++) pack.WriteFloat(force[i]);
	for (int i = 0; i < 3; i++) pack.WriteFloat(tankEyePos[i]);

	ArrayList children = g_hHiddenChildren[client];
	int count = children ? children.Length : 0;
	pack.WriteCell(count);
	for (int i = 0; i < count; i++)
	{
		pack.WriteCell(children.Get(i, 0));
		pack.WriteCell(children.Get(i, 1));
	}

	ResetState(client);
}

Action Timer_ReleaseProp(Handle timer, DataPack pack)
{
	pack.Reset();
	int prop = EntRefToEntIndex(pack.ReadCell());
	int proxy = EntRefToEntIndex(pack.ReadCell());
	bool isCar = pack.ReadCell();
	bool weirdAct = pack.ReadCell();
	int oldEffects = pack.ReadCell();

	float origin[3], angles[3], force[3], tankEyePos[3];
	for (int i = 0; i < 3; i++) origin[i] = pack.ReadFloat();
	for (int i = 0; i < 3; i++) angles[i] = pack.ReadFloat();
	for (int i = 0; i < 3; i++) force[i] = pack.ReadFloat();
	for (int i = 0; i < 3; i++) tankEyePos[i] = pack.ReadFloat();

	if (proxy != INVALID_ENT_REFERENCE)
	{
		StopThrowFlyLoop(proxy);
		RemoveEntity(proxy);
	}

	if (prop == INVALID_ENT_REFERENCE)
		return Plugin_Stop;

	TeleportEntity(prop, origin, weirdAct ? angles : NULL_VECTOR, ZERO_VECTOR);

	SetEntProp(prop, Prop_Send, "m_fEffects", oldEffects);

	int count = pack.ReadCell();
	for (int i = 0; i < count; i++)
	{
		int child = EntRefToEntIndex(pack.ReadCell());
		int childEffects = pack.ReadCell();
		if (child == INVALID_ENT_REFERENCE)
			continue;

		if (HasEntProp(child, Prop_Send, "m_fEffects"))
			SetEntProp(child, Prop_Send, "m_fEffects", childEffects);
		AcceptEntityInput(child, "EnableCollision");
	}

	SetPropIntAny(prop, "m_hasTankGlow", 1);

	// Collide with the world right away so it can't fly through walls, but not with
	// players for a moment so it doesn't push the tank away
	DataPack restore;
	CreateDataTimer(0.05, Timer_RestoreCollisionGroup, restore, TIMER_FLAG_NO_MAPCHANGE);
	restore.WriteCell(EntIndexToEntRef(prop));
	restore.WriteCell(GetEntProp(prop, Prop_Send, "m_CollisionGroup"));
	SetEntityCollisionGroup(prop, COLLISION_GROUP_DEBRIS);

	AcceptEntityInput(prop, "EnableCollision");
	AcceptEntityInput(prop, "EnableMotion");
	AcceptEntityInput(prop, "Wake");

	if (!weirdAct)
	{
		if (isCar)
			FixCarAngles(prop, origin);

		KeepPropBeforeWall(prop, tankEyePos, force);

		// Launched through physics only: a TeleportEntity velocity stays stored in the prop
		// and keeps pushing anyone who stands on it after it stops
		SDKCall(g_hSDKApplyAbsVelocityImpulse, prop, force);

		float spin[3];
		for (int i = 0; i < 3; i++)
			spin[i] = float(GetRandomInt(-150, 150));
		SDKCall(g_hSDKApplyLocalAngularVelocityImpulse, prop, spin);
	}

	return Plugin_Stop;
}

Action Timer_RestoreCollisionGroup(Handle timer, DataPack pack)
{
	pack.Reset();
	int ent = EntRefToEntIndex(pack.ReadCell());
	int group = pack.ReadCell();
	if (ent != INVALID_ENT_REFERENCE)
		SetEntityCollisionGroup(ent, group);
	return Plugin_Stop;
}

// The release point can end up past a wall the tank is facing:
// pull the prop back so it hits the wall instead of going through it
void KeepPropBeforeWall(int prop, const float tankEyePos[3], const float force[3])
{
	float origin[3], angles[3], mins[3], maxs[3];
	GetEntPropVector(prop, Prop_Send, "m_vecOrigin", origin);
	GetEntPropVector(prop, Prop_Send, "m_angRotation", angles);
	GetEntPropVector(prop, Prop_Send, "m_vecMins", mins);
	GetEntPropVector(prop, Prop_Send, "m_vecMaxs", maxs);

	float dir[3];
	NormalizeVector(force, dir);

	float fwd[3], right[3], up[3];
	GetAngleVectors(angles, fwd, right, up);
	float dotFwd = GetVectorDotProduct(fwd, dir);
	float dotLeft = -GetVectorDotProduct(right, dir);
	float dotUp = GetVectorDotProduct(up, dir);
	float reach = (dotFwd > 0.0 ? maxs[0] : mins[0]) * dotFwd
		+ (dotLeft > 0.0 ? maxs[1] : mins[1]) * dotLeft
		+ (dotUp > 0.0 ? maxs[2] : mins[2]) * dotUp;
	if (reach < 0.0)
		reach = 0.0;

	float front[3];
	front = dir;
	ScaleVector(front, reach);
	AddVectors(origin, front, front);

	Handle trace = TR_TraceRayFilterEx(tankEyePos, front, MASK_SOLID, RayType_EndPoint, TraceFilter_World, prop);
	bool blocked = TR_DidHit(trace) && !TR_StartSolid(trace);
	float hitPos[3];
	TR_GetEndPosition(hitPos, trace);
	delete trace;

	if (!blocked)
		return;

	float back[3];
	SubtractVectors(front, tankEyePos, back);
	NormalizeVector(back, back);
	ScaleVector(back, reach + 4.0);
	SubtractVectors(hitPos, back, origin);
	TeleportEntity(prop, origin, NULL_VECTOR, NULL_VECTOR);
}

// Ignores players and the thrown prop (plus anything parented to it)
bool TraceFilter_World(int entity, int contentsMask, any prop)
{
	if (entity >= 1 && entity <= MaxClients)
		return false;
	return entity != prop && GetRootMoveParent(entity) != prop;
}

// Snap cars to the nearest "lying flat" orientation so they don't land standing on their nose
void FixCarAngles(int prop, const float origin[3])
{
	float ang[3];
	GetEntPropVector(prop, Prop_Send, "m_angRotation", ang);

	int best = 0;
	float bestDiff = 999999.0;
	for (int i = 0; i < sizeof(g_fHorizontalAngles); i++)
	{
		float dx = NormalizeAngle(ang[0] - g_fHorizontalAngles[i][0]);
		float dy = NormalizeAngle(ang[1] - g_fHorizontalAngles[i][1]);
		float diff = dx * dx + dy * dy;
		if (diff < bestDiff)
		{
			bestDiff = diff;
			best = i;
		}
	}

	float newAngles[3], newOrigin[3];
	newAngles[0] = g_fHorizontalAngles[best][0];
	newAngles[1] = g_fHorizontalAngles[best][1];
	newAngles[2] = ang[2];

	// A ±180 yaw flip puts the model ~80 units lower
	newOrigin = origin;
	newOrigin[2] += FloatAbs(newAngles[1]) / 180.0 * 80.0;

	TeleportEntity(prop, newOrigin, newAngles, NULL_VECTOR);
}

// ============================================================
// Prop search: only the prop the tank is aiming at
// ============================================================
// Uses each prop's box instead of its model, so thin parts or gaps don't make the aim miss
int FindAimedProp(int tank, float range)
{
	int bestProp = -1;
	float bestDist = 999999.0, dist;

	int maxEnts = GetMaxEntities();
	for (int ent = MaxClients + 1; ent < maxEnts; ent++)
	{
		if (IsValidEdict(ent) && IsAimedProp(tank, ent, range, dist) && dist < bestDist)
		{
			bestDist = dist;
			bestProp = ent;
		}
	}
	return bestProp;
}

bool IsAimedProp(int tank, int ent, float range, float &dist)
{
	if (!IsPropObject(ent))
		return false;

	// Picking up the prop he stands on would fling the tank along with it
	int ground = GetEntPropEnt(tank, Prop_Send, "m_hGroundEntity");
	if (ground > MaxClients && (ground == ent || GetRootMoveParent(ground) == ent))
		return false;

	float eyePos[3], eyeAngles[3], dir[3];
	GetClientEyePosition(tank, eyePos);
	GetClientEyeAngles(tank, eyeAngles);
	GetAngleVectors(eyeAngles, dir, NULL_VECTOR, NULL_VECTOR);

	float tolerance = g_cvAimTolerance.FloatValue;
	float entry;
	if (!RayHitsBox(ent, eyePos, dir, tolerance, entry))
		return false;

	float aimPoint[3];
	aimPoint = dir;
	ScaleVector(aimPoint, entry);
	AddVectors(eyePos, aimPoint, aimPoint);

	// Measured on the ground from the tank's feet
	float tankOrigin[3];
	GetClientAbsOrigin(tank, tankOrigin);
	float dx = aimPoint[0] - tankOrigin[0];
	float dy = aimPoint[1] - tankOrigin[1];
	// Whole units, so a prop right at the limit ("50 of 50") is in range
	float reach = float(RoundToNearest(SquareRoot(dx * dx + dy * dy)));
	if (reach > range)
		return false;

	// A wall between the tank and the prop blocks it; the floor or curb the prop
	// rests on (right against its box) doesn't
	Handle trace = TR_TraceRayFilterEx(eyePos, aimPoint, MASK_PICKUP, RayType_EndPoint, TraceFilter_Ignore, tank);
	bool blocked = false;
	if (TR_DidHit(trace))
	{
		int hit = TR_GetEntityIndex(trace);
		float hitPos[3];
		TR_GetEndPosition(hitPos, trace);
		blocked = hit != ent && GetRootMoveParent(hit) != ent
			&& GetDistanceToBox(ent, hitPos) > tolerance + PROP_CONTACT_MARGIN;
	}
	delete trace;

	if (blocked)
		return false;

	dist = entry;
	return true;
}

float GetDistanceToBox(int ent, const float point[3])
{
	float origin[3], angles[3], mins[3], maxs[3];
	GetEntPropVector(ent, Prop_Data, "m_vecAbsOrigin", origin);
	GetEntPropVector(ent, Prop_Data, "m_angAbsRotation", angles);
	GetEntPropVector(ent, Prop_Send, "m_vecMins", mins);
	GetEntPropVector(ent, Prop_Send, "m_vecMaxs", maxs);

	float delta[3], fwd[3], right[3], up[3], local[3];
	SubtractVectors(point, origin, delta);
	GetAngleVectors(angles, fwd, right, up);
	local[0] = GetVectorDotProduct(delta, fwd);
	local[1] = -GetVectorDotProduct(delta, right);
	local[2] = GetVectorDotProduct(delta, up);

	float distSq = 0.0;
	for (int i = 0; i < 3; i++)
	{
		float d = 0.0;
		if (local[i] < mins[i])
			d = mins[i] - local[i];
		else if (local[i] > maxs[i])
			d = local[i] - maxs[i];
		distSq += d * d;
	}
	return SquareRoot(distSq);
}

// entry: distance along the ray to the box
bool RayHitsBox(int ent, const float start[3], const float dir[3], float grow, float &entry)
{
	float origin[3], angles[3], mins[3], maxs[3];
	GetEntPropVector(ent, Prop_Data, "m_vecAbsOrigin", origin);
	GetEntPropVector(ent, Prop_Data, "m_angAbsRotation", angles);
	GetEntPropVector(ent, Prop_Send, "m_vecMins", mins);
	GetEntPropVector(ent, Prop_Send, "m_vecMaxs", maxs);

	float fwd[3], right[3], up[3], delta[3];
	GetAngleVectors(angles, fwd, right, up);
	SubtractVectors(start, origin, delta);

	float o[3], d[3];
	o[0] = GetVectorDotProduct(delta, fwd);
	o[1] = -GetVectorDotProduct(delta, right);
	o[2] = GetVectorDotProduct(delta, up);
	d[0] = GetVectorDotProduct(dir, fwd);
	d[1] = -GetVectorDotProduct(dir, right);
	d[2] = GetVectorDotProduct(dir, up);

	float tMin = 0.0, tMax = 100000.0;
	for (int i = 0; i < 3; i++)
	{
		float lo = mins[i] - grow;
		float hi = maxs[i] + grow;

		if (FloatAbs(d[i]) < 0.000001)
		{
			if (o[i] < lo || o[i] > hi)
				return false;
			continue;
		}

		float t1 = (lo - o[i]) / d[i];
		float t2 = (hi - o[i]) / d[i];
		if (t1 > t2)
		{
			float tmp = t1;
			t1 = t2;
			t2 = tmp;
		}
		if (t1 > tMin)
			tMin = t1;
		if (t2 < tMax)
			tMax = t2;
		if (tMin > tMax)
			return false;
	}

	entry = tMin;
	return true;
}

bool IsPropObject(int ent)
{
	char classname[64];
	GetEntityClassname(ent, classname, sizeof(classname));

	bool isPhysics = StrEqual(classname, "prop_physics");
	if (!isPhysics && !StrEqual(classname, "prop_physics_multiplayer") && !StrEqual(classname, "prop_car_alarm"))
		return false;

	if (GetPropIntAny(ent, "m_spawnflags") & 4 || GetPropIntAny(ent, "m_hasTankGlow") != 1)
		return false;

	// Invisible map props: grabbing one would eat the rock and throw nothing
	if (GetPropIntAny(ent, "m_fEffects") & EF_NODRAW)
		return false;

	return !isPhysics || GetPropIntAny(ent, "m_breakableType") != 2;
}

// The blinking alarm light lives on parts attached to the car (sprites/lights and the
// "_alarm" glass model), which disabling the car itself doesn't turn off
void RemoveAlarmLights(int car)
{
	char classname[64], model[PLATFORM_MAX_PATH];
	int maxEnts = GetMaxEntities();
	for (int ent = MaxClients + 1; ent < maxEnts; ent++)
	{
		if (ent == car || !IsValidEntity(ent) || GetRootMoveParent(ent) != car)
			continue;

		GetEntityClassname(ent, classname, sizeof(classname));
		if (StrContains(classname, "sprite") != -1 || StrContains(classname, "light") != -1 || StrContains(classname, "glow") != -1)
		{
			RemoveEntity(ent);
			continue;
		}

		if (!HasEntProp(ent, Prop_Data, "m_ModelName"))
			continue;

		// e.g. cara_95sedan_glass_alarm.mdl -> cara_95sedan_glass.mdl
		GetEntPropString(ent, Prop_Data, "m_ModelName", model, sizeof(model));
		int pos = StrContains(model, "_alarm.mdl", false);
		if (pos == -1)
			continue;

		strcopy(model[pos], sizeof(model) - pos, ".mdl");
		if (IsModelPrecached(model) || FileExists(model, true))
		{
			PrecacheModel(model);
			SetEntityModel(ent, model);
		}
	}
}

void HideChildren(int client, int prop)
{
	delete g_hHiddenChildren[client];
	g_hHiddenChildren[client] = new ArrayList(2);

	char classname[64];
	int maxEnts = GetMaxEntities();
	for (int ent = MaxClients + 1; ent < maxEnts; ent++)
	{
		if (ent == prop || !IsValidEntity(ent) || GetRootMoveParent(ent) != prop)
			continue;

		GetEntityClassname(ent, classname, sizeof(classname));
		if (StrEqual(classname, "info_particle_system"))
			continue;

		int effects = 0;
		if (HasEntProp(ent, Prop_Send, "m_fEffects"))
		{
			effects = GetEntProp(ent, Prop_Send, "m_fEffects");
			SetEntProp(ent, Prop_Send, "m_fEffects", effects | EF_NODRAW);
		}
		AcceptEntityInput(ent, "DisableCollision");

		int index = g_hHiddenChildren[client].Push(EntIndexToEntRef(ent));
		g_hHiddenChildren[client].Set(index, effects, 1);
	}
}

// ============================================================
// Helpers
// ============================================================
void ResetState(int client)
{
	g_iState[client] = STATE_NONE;
	g_iPropRef[client] = INVALID_ENT_REFERENCE;
	g_iProxyRef[client] = INVALID_ENT_REFERENCE;
	g_bIsCar[client] = false;
	delete g_hHiddenChildren[client];
}

// Still controls the tank, alive or not (false after losing it, becoming a ghost, etc.)
bool IsTankBody(int client)
{
	return client > 0 && client <= MaxClients && IsClientInGame(client)
		&& GetClientTeam(client) == TEAM_INFECTED
		&& GetEntProp(client, Prop_Send, "m_zombieClass") == ZC_TANK
		&& !GetEntProp(client, Prop_Send, "m_isGhost");
}

bool IsValidTank(int client)
{
	return IsTankBody(client) && IsPlayerAlive(client)
		&& !GetEntProp(client, Prop_Send, "m_isIncapacitated");
}

// CBaseEntity::Apply*Impulse(const Vector &impulse)
Handle PrepImpulseCall(GameData gameData, const char[] name)
{
	StartPrepSDKCall(SDKCall_Entity);
	if (!PrepSDKCall_SetFromConf(gameData, SDKConf_Signature, name))
		SetFailState("Could not find signature \"%s\" in gamedata/tank_throws_cars.txt.", name);
	PrepSDKCall_AddParameter(SDKType_Vector, SDKPass_ByRef);

	Handle call = EndPrepSDKCall();
	if (call == null)
		SetFailState("Could not create SDKCall \"%s\".", name);
	return call;
}

bool IsThrowSequence(int seq)
{
	return seq == SEQ_THROW_ONEHAND || seq == SEQ_THROW_UNDERHAND || seq == SEQ_THROW_OVERHEAD;
}

bool IsStaggering(int client)
{
	return g_iOffStaggerTimestamp != -1 && GetEntDataFloat(client, g_iOffStaggerTimestamp) > GetGameTime();
}

int GetRootMoveParent(int ent)
{
	for (int i = 0; i < 16; i++)
	{
		int parent = GetEntPropEnt(ent, Prop_Data, "m_pParent");
		if (parent <= 0)
			break;
		ent = parent;
	}
	return ent;
}

// Skips the tank and invisible map blockers (e.g. on rooftops) that would hide props from him
bool TraceFilter_Ignore(int entity, int contentsMask, any data)
{
	if (entity == data)
		return false;
	if (entity <= MaxClients)
		return true;

	char classname[64];
	GetEntityClassname(entity, classname, sizeof(classname));
	return !StrEqual(classname, "env_physics_blocker")
		&& !StrEqual(classname, "env_player_blocker")
		&& StrContains(classname, "func_playerinfected_clip") != 0
		&& StrContains(classname, "func_playerghostinfected_clip") != 0
		&& !StrEqual(classname, "func_clip_vphysics");
}

bool ModelInList(const char[] model, const char[][] list, int size)
{
	static const char prefix[] = "models/props_vehicles/";
	if (StrContains(model, prefix, false) != 0)
		return false;

	char name[PLATFORM_MAX_PATH];
	strcopy(name, sizeof(name), model[sizeof(prefix) - 1]);
	int len = strlen(name);
	if (len > 4 && StrEqual(name[len - 4], ".mdl", false))
		name[len - 4] = '\0';

	for (int i = 0; i < size; i++)
	{
		if (StrEqual(name, list[i], false))
			return true;
	}
	return false;
}

int GetPropIntAny(int ent, const char[] prop)
{
	if (HasEntProp(ent, Prop_Send, prop))
		return GetEntProp(ent, Prop_Send, prop);
	if (HasEntProp(ent, Prop_Data, prop))
		return GetEntProp(ent, Prop_Data, prop);
	return 0;
}

void SetPropIntAny(int ent, const char[] prop, int value)
{
	if (HasEntProp(ent, Prop_Send, prop))
		SetEntProp(ent, Prop_Send, prop, value);
	else if (HasEntProp(ent, Prop_Data, prop))
		SetEntProp(ent, Prop_Data, prop, value);
}

float NormalizeAngle(float angle)
{
	while (angle > 180.0) angle -= 360.0;
	while (angle < -180.0) angle += 360.0;
	return angle;
}

void StopThrowFlyLoop(int ent)
{
	char sample[PLATFORM_MAX_PATH];
	int channel, level, pitch;
	float volume;
	if (GetGameSoundParams("HulkZombie.Throw.FlyLoop", channel, level, volume, pitch, sample, sizeof(sample), ent))
		StopSound(ent, channel, sample);

	for (int ch = SNDCHAN_AUTO; ch <= SNDCHAN_STATIC; ch++)
		StopSound(ent, ch, "player/tank/attack/thrown_missile_loop_1.wav");
}
