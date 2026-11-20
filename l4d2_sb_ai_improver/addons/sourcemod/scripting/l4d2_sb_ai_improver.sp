#pragma newdecls required
#pragma semicolon 1

#include <sourcemod>
#include <sdktools>
#include <sdkhooks>
#include <dhooks>
#include <left4dhooks>

#undef REQUIRE_EXTENSIONS
#include <actions>
#define REQUIRE_EXTENSIONS

public Plugin myinfo = 
{
	name 		= "[L4D2] Survivor Bot AI Improver",
	author 		= "Emana202, Ferks-FK",
	description = "Attempt at improving survivor bots' AI and behaviour as much as possible.",
	version 	= "1.6",
	url 		= "https://forums.alliedmods.net/showthread.php?t=342872"
}

#define MAXENTITIES 				2048

#define BOT_WITCH_CHECK_DIST_SQR		4000000.0
#define BOT_BOOMER_AVOID_RADIUS_SQR		40000.0
#define BOT_GRENADE_CHECK_RADIUS_SQR	90000.0
#define BOT_TANK_KEEP_AWAY_DIST			384.0
#define BOT_TANK_KEEP_AWAY_DIST_SQR		(BOT_TANK_KEEP_AWAY_DIST*BOT_TANK_KEEP_AWAY_DIST)
#define BOT_TANK_CHASE_DIST_SQR			2250000.0
#define BOT_TANK_SUPPORT_CLOSE_DIST_SQR	40000.0
#define BOT_CMD_MOVE_INTERVAL 		0.8
#define BOT_TANK_FLEE_AIM_TIME		2.0
#define BOT_TANK_RETREAT_INTERVAL	0.5
#define BOT_TANK_LOOK_MEMORY_TIME	1.5
#define BOT_TANK_LOOK_THREAT_DIST_SQR	90000.0
#define BOT_TANK_ROUTE_RETRY_TIME	1.0
#define BOT_TANK_ROUTE_LEG_DIST_SQR	2250000.0
#define BOT_TANK_ROUTE_ARRIVE_DIST	100.0
#define BOT_TANK_ROUTE_FAIL_RETRY_TIME	2.0
#define BOT_TANK_ROUTE_MIN_LEG_DIST_SQR	250000.0
#define BOT_TANK_ROUTE_NEAR_AREAS	64
#define BOT_TANK_ROUTE_NEAR_CHECKS	5
#define BOT_TANK_ROUTE_GOAL_FLOW	3000.0
#define BOT_TANK_ROUTE_MAX_PATH	6000.0
#define BOT_TANK_ROUTE_FLOW_BUCKETS	512
#define BOT_PATH_LIMIT_MARGIN	1.25
#define BOT_FAR_PATH_SEARCH_MAX	8000.0
#define BOT_CHARGE_EVADE_SEARCH_MAX	480.0
#define BOT_TANK_PATH_NEAR_DIST_SQR	562500.0
#define BOT_TANK_PATH_NEAR_SEARCH_MAX	940.0
#define BOT_ITEM_SEARCH_MULT	3.0
#define BOT_ESCAPE_SEARCH_INTERVAL	0.5
#define BOT_ESCAPE_SEARCH_MAX	1500.0
#define BOT_ESCAPE_SEARCH_TRIES	6
#define BOT_ESCAPE_EXTRA_TIME	0.5
#define BOT_TANK_ROUTE_GOAL_SEARCHES	2
#define BOT_APPROACH_CHECK_INTERVAL	0.25
#define BOT_GRENADE_CHECK_INTERVAL	0.1
#define BOT_FIRE_CHECK_INTERVAL	0.05
#define BOT_TANK_SIGHT_CACHE_TIME	0.05
#define BOT_TANK_AIM_CACHE_TICKS	2
#define BOT_INFECTED_SNAP_TIME	0.1
#define BOT_INFECTED_SIGHT_MISSES	10
#define BOT_INFECTED_CLOSE_SIGHT_DIST_SQR	90000.0
#define BOT_ENTITY_SCAN_FAST_TIME	0.1
#define BOT_ENTITY_SCAN_SLOW_TIME	0.5
#define BOT_ENTITY_SCAN_MAX	64
#define BOT_TANK_ROUTE_GOAL_FLOW_STEP	600.0
#define BOT_TANK_ROUTE_GOAL_TRIES	4
#define BOT_AIM_PART_CACHE_TIME	0.05
#define BOT_VISION_RECHECK_TIME	0.05
#define BOT_CYCLE_STALL_TIME	0.5
#define BOT_MELEE_BONE_MARGIN	128.0
#define BOT_AMMO_UPDATE_INTERVAL	0.1
#define BOT_SURVIVOR_TYPE_CACHE_TIME	1.0
#define BOT_ENTITY_SIGHT_CACHE_TIME	0.1
#define BOT_TANK_RESCUE_FAR_RANGE_MULT_SQR	4.0
#define BOT_TANK_ROUTE_FLIP_MARGIN	150.0
#define BOT_TANK_ROUTE_MOVE_NAME	"TankFleeRoute"
#define BOT_TANK_BLIND_MOVE_NAME	"TankFleeBlind"
#define BOT_TANK_FLEE_STOP_MARGIN	50.0
#define BOT_TANK_KEEP_AWAY_STOP_DIST_SQR	((BOT_TANK_KEEP_AWAY_DIST+BOT_TANK_FLEE_STOP_MARGIN)*(BOT_TANK_KEEP_AWAY_DIST+BOT_TANK_FLEE_STOP_MARGIN))

#define HUMAN_HEIGHT				71.0
#define HUMAN_HALF_HEIGHT			(HUMAN_HEIGHT*0.5)

enum
{
	L4D_SURVIVOR_NICK			= 1,
	L4D_SURVIVOR_ROCHELLE 		= 2,
	L4D_SURVIVOR_COACH			= 3,
	L4D_SURVIVOR_ELLIS			= 4,
	L4D_SURVIVOR_BILL			= 5,
	L4D_SURVIVOR_ZOEY			= 6,
	L4D_SURVIVOR_FRANCIS		= 7,
	L4D_SURVIVOR_LOUIS 			= 8
}

enum
{
	L4D_WEAPON_PREFERENCE_ASSAULTRIFLE 	= 1,
	L4D_WEAPON_PREFERENCE_SHOTGUN		= 2,
	L4D_WEAPON_PREFERENCE_SNIPERRIFLE	= 3,
	L4D_WEAPON_PREFERENCE_SMG			= 4,
	L4D_WEAPON_PREFERENCE_SECONDARY		= 5
}

/*============ IN-GAME CONVARS =======================================================================*/
ConVar g_hCvar_GameDifficulty; 
ConVar g_hCvar_SurvivorLimpHealth;
ConVar g_hCvar_TankRockHealth;
ConVar g_hCvar_GasCanUseDuration;
ConVar g_hCvar_ChaseBileRange;
ConVar g_hCvar_ServerGravity;
ConVar g_hCvar_BileCoverDuration_Bot;
ConVar g_hCvar_BileCoverDuration_PZ;

ConVar g_hCvar_MaxMeleeSurvivors; 
ConVar g_hCvar_BotsShootThrough;
ConVar g_hCvar_BotsFriendlyFire;
ConVar g_hCvar_BotsDisabled;
ConVar g_hCvar_BotsDontShoot;
ConVar g_hCvar_BotsVomitBlindTime;
ConVar g_hCvar_Debug;

char g_sCvar_GameDifficulty[12];
int g_iCvar_SurvivorLimpHealth; 
int g_iCvar_TankRockHealth; 
float g_fCvar_GasCanUseDuration; 
float g_fCvar_ChaseBileRange; 
float g_fCvar_ServerGravity; 
float g_fCvar_BileCoverDuration_Bot;
float g_fCvar_BileCoverDuration_PZ;

int g_iCvar_MaxMeleeSurvivors; 
bool g_bCvar_BotsShootThrough;
bool g_bCvar_BotsFriendlyFire;
bool g_bCvar_BotsDisabled;
bool g_bCvar_BotsDontShoot;
float g_fCvar_BotsVomitBlindTime;
bool g_bCvar_Debug;

/*============ MELEE RELATED CONVARS =================================================================*/
ConVar g_hCvar_MaxAmmo_Pistol;
ConVar g_hCvar_MaxAmmo_AssaultRifle;
ConVar g_hCvar_MaxAmmo_SMG;
ConVar g_hCvar_MaxAmmo_M60;
ConVar g_hCvar_MaxAmmo_Shotgun;
ConVar g_hCvar_MaxAmmo_AutoShotgun;
ConVar g_hCvar_MaxAmmo_HuntRifle;
ConVar g_hCvar_MaxAmmo_SniperRifle;
ConVar g_hCvar_MaxAmmo_PipeBomb;
ConVar g_hCvar_MaxAmmo_Molotov;
ConVar g_hCvar_MaxAmmo_VomitJar;
ConVar g_hCvar_MaxAmmo_PainPills;
ConVar g_hCvar_MaxAmmo_GrenLauncher;
ConVar g_hCvar_MaxAmmo_Adrenaline;
ConVar g_hCvar_MaxAmmo_Chainsaw;
ConVar g_hCvar_MaxAmmo_AmmoPack;
ConVar g_hCvar_MaxAmmo_Medkit;

int g_iCvar_MaxAmmo_Pistol;
int g_iCvar_MaxAmmo_AssaultRifle;
int g_iCvar_MaxAmmo_SMG;
int g_iCvar_MaxAmmo_M60;
int g_iCvar_MaxAmmo_Shotgun;
int g_iCvar_MaxAmmo_AutoShotgun;
int g_iCvar_MaxAmmo_HuntRifle;
int g_iCvar_MaxAmmo_SniperRifle;
int g_iCvar_MaxAmmo_PipeBomb;
int g_iCvar_MaxAmmo_Molotov;
int g_iCvar_MaxAmmo_VomitJar;
int g_iCvar_MaxAmmo_PainPills;
int g_iCvar_MaxAmmo_GrenLauncher;
int g_iCvar_MaxAmmo_Adrenaline;
int g_iCvar_MaxAmmo_Chainsaw;
int g_iCvar_MaxAmmo_AmmoPack;
int g_iCvar_MaxAmmo_Medkit;

/*============ MELEE RELATED CONVARS =================================================================*/
ConVar g_hCvar_ImprovedMelee_MaxCount;

ConVar g_hCvar_ImprovedMelee_Enabled;
ConVar g_hCvar_ImprovedMelee_SwitchCount;
ConVar g_hCvar_ImprovedMelee_SwitchRange;
ConVar g_hCvar_ImprovedMelee_ApproachRange;
ConVar g_hCvar_ImprovedMelee_AimRange;
ConVar g_hCvar_ImprovedMelee_AttackRange;
ConVar g_hCvar_ImprovedMelee_ShoveChance;

bool g_bCvar_ImprovedMelee_Enabled;
int g_iCvar_ImprovedMelee_SwitchCount; 
int g_iCvar_ImprovedMelee_ShoveChance; 
float g_fCvar_ImprovedMelee_SwitchRange; 
float g_fCvar_ImprovedMelee_ApproachRange;
float g_fCvar_ImprovedMelee_AimRange;
float g_fCvar_ImprovedMelee_AttackRange;

ConVar g_hCvar_ImprovedMelee_ChainsawLimit;
ConVar g_hCvar_ImprovedMelee_SwitchCount2;

int g_iCvar_ImprovedMelee_ChainsawLimit;
int g_iCvar_ImprovedMelee_SwitchCount2;
/*============ TARGET SELECTION CONVARS ==================================================================*/
ConVar g_hCvar_TargetSelection_Enabled;
ConVar g_hCvar_TargetSelection_ShootRange;
ConVar g_hCvar_TargetSelection_ShootRange2;
ConVar g_hCvar_TargetSelection_ShootRange3;
ConVar g_hCvar_TargetSelection_ShootRange4;
ConVar g_hCvar_TargetSelection_IgnoreDociles;

bool g_bCvar_TargetSelection_Enabled;
float g_fCvar_TargetSelection_ShootRange;
float g_fCvar_TargetSelection_ShootRange2;
float g_fCvar_TargetSelection_ShootRange3;
float g_fCvar_TargetSelection_ShootRange4;
bool g_bCvar_TargetSelection_IgnoreDociles;
/*----------------------------------------------------------------------------------------------------*/
ConVar g_hCvar_Vision_FieldOfView;
float g_fCvar_Vision_FieldOfView;

ConVar g_hCvar_Vision_NoticeTimeScale;
float g_fCvar_Vision_NoticeTimeScale;

/*============ TANK RELATED CONVARS ==================================================================*/
ConVar g_hCvar_TankRock_ShootEnabled;
ConVar g_hCvar_TankRock_ShootRange;
bool g_bCvar_TankRock_ShootEnabled;
float g_fCvar_TankRock_ShootRange;
/*============ SAVE RELATED CONVARS ==================================================================*/
ConVar g_hCvar_AutoShove_Enabled;
int g_iCvar_AutoShove_Enabled;
/*----------------------------------------------------------------------------------------------------*/
ConVar g_hCvar_FireBash_Chance1;
ConVar g_hCvar_FireBash_Chance2;
int g_iCvar_FireBash_Chance1;
int g_iCvar_FireBash_Chance2;
/*----------------------------------------------------------------------------------------------------*/
ConVar g_hCvar_HelpPinnedFriend_Enabled;
ConVar g_hCvar_HelpPinnedFriend_ShootRange;
ConVar g_hCvar_HelpPinnedFriend_ShoveRange;
int g_iCvar_HelpPinnedFriend_Enabled;
float g_fCvar_HelpPinnedFriend_ShootRange;
float g_fCvar_HelpPinnedFriend_ShoveRange;
/*============ WEAPON RELATED CONVARS ================================================================*/
ConVar g_hCvar_BotWeaponPreference_ForceMagnum;
bool g_bCvar_BotWeaponPreference_ForceMagnum;

ConVar g_hCvar_BotWeaponPreference_Rochelle;
ConVar g_hCvar_BotWeaponPreference_Zoey; 
ConVar g_hCvar_BotWeaponPreference_Ellis; 
ConVar g_hCvar_BotWeaponPreference_Coach; 
ConVar g_hCvar_BotWeaponPreference_Francis; 
ConVar g_hCvar_BotWeaponPreference_Nick;
ConVar g_hCvar_BotWeaponPreference_Louis; 
ConVar g_hCvar_BotWeaponPreference_Bill;

int g_iCvar_BotWeaponPreference_Rochelle;
int g_iCvar_BotWeaponPreference_Zoey;
int g_iCvar_BotWeaponPreference_Ellis; 
int g_iCvar_BotWeaponPreference_Coach;
int g_iCvar_BotWeaponPreference_Francis;
int g_iCvar_BotWeaponPreference_Nick;
int g_iCvar_BotWeaponPreference_Louis;
int g_iCvar_BotWeaponPreference_Bill;
/*----------------------------------------------------------------------------------------------------*/
ConVar g_hCvar_SwapSameTypePrimaries;
bool g_bCvar_SwapSameTypePrimaries;
/*----------------------------------------------------------------------------------------------------*/
ConVar g_hCvar_MaxWeaponTier3_M60;
ConVar g_hCvar_MaxWeaponTier3_GLauncher;

int g_iCvar_MaxWeaponTier3_M60;
int g_iCvar_MaxWeaponTier3_GLauncher;

/*============ GRENADE RELATED CONVARS ===============================================================*/
ConVar g_hCvar_GrenadeThrow_Enabled;
ConVar g_hCvar_GrenadeThrow_GrenadeTypes;
ConVar g_hCvar_GrenadeThrow_ThrowRange;
ConVar g_hCvar_GrenadeThrow_HordeSize; 
ConVar g_hCvar_GrenadeThrow_NextThrowTime1;
ConVar g_hCvar_GrenadeThrow_NextThrowTime2;
/*----------------------------------------------------------------------------------------------------*/
bool g_bCvar_GrenadeThrow_Enabled;
int g_iCvar_GrenadeThrow_GrenadeTypes; 
float g_fCvar_GrenadeThrow_ThrowRange; 
float g_fCvar_GrenadeThrow_HordeSize; 
float g_fCvar_GrenadeThrow_NextThrowTime1;
float g_fCvar_GrenadeThrow_NextThrowTime2;
/*----------------------------------------------------------------------------------------------------*/
ConVar g_hCvar_SwapSameTypeGrenades;
bool g_bCvar_SwapSameTypeGrenades;

/*============ DEFIB RELATED CONVARS =================================================================*/
ConVar g_hCvar_DefibRevive_Enabled; 
ConVar g_hCvar_DefibRevive_ScanDist; 

bool g_bCvar_DefibRevive_Enabled; 
float g_fCvar_DefibRevive_ScanDist;

/*============ ITEM SCAVENGE RELATED CONVARS =========================================================*/
ConVar g_hCvar_ItemScavenge_Items; 
ConVar g_hCvar_ItemScavenge_ApproachRange; 
ConVar g_hCvar_ItemScavenge_ApproachVisibleRange; 
ConVar g_hCvar_ItemScavenge_PickupRange; 
ConVar g_hCvar_ItemScavenge_MapSearchRange; 
ConVar g_hCvar_ItemScavenge_NoHumansRangeMultiplier; 

int g_iCvar_ItemScavenge_Items;
float g_fCvar_ItemScavenge_ApproachRange;
float g_fCvar_ItemScavenge_ApproachVisibleRange;
float g_fCvar_ItemScavenge_PickupRange;
float g_fCvar_ItemScavenge_MapSearchRange;
float g_fCvar_ItemScavenge_NoHumansRangeMultiplier;

/*============ WITCH RELATED CONVARS =========================================================*/
ConVar g_hCvar_WitchBehavior_WalkWhenNearby;
ConVar g_hCvar_WitchBehavior_AllowCrowning;

float g_fCvar_WitchBehavior_WalkWhenNearby;
int g_iCvar_WitchBehavior_AllowCrowning;

/*============ PERFOMANCE RELATED CONVARS =========================================================*/
ConVar g_hCvar_NextProcessTime;
float g_fCvar_NextProcessTime;

/*============ MISC CONVARS =========================================================*/
ConVar g_hCvar_AlwaysCarryProp;
ConVar g_hCvar_SpitterAcidEvasion;
ConVar g_hCvar_SwitchOffCSSWeapons;
ConVar g_hCvar_KeepMovingInCombat;
ConVar g_hCvar_ChargerEvasion;
ConVar g_hCvar_DeployUpgradePacks;
ConVar g_hCvar_DontSwitchToPistol;
ConVar g_hCvar_TakeCoverFromRocks;
ConVar g_hCvar_TankTeamLeash;
ConVar g_hCvar_TankRescue_Range;
ConVar g_hCvar_TankRescue_SafeDist;
ConVar g_hCvar_TankRescue_AbortDist;
ConVar g_hCvar_TankSupport_Range;
ConVar g_hCvar_TankSupport_MinDist;
ConVar g_hCvar_TankFleeDist;
ConVar g_hCvar_TankFleeBlindDist;
ConVar g_hCvar_TankFleeMapRoute;
ConVar g_hCvar_AvoidTanksWithProp;
ConVar g_hCvar_NoFallDmgOnLadderFail;

bool g_bCvar_SpitterAcidEvasion;
bool g_bCvar_AlwaysCarryProp;
bool g_bCvar_SwitchOffCSSWeapons;
bool g_bCvar_ChargerEvasion;
bool g_bCvar_DeployUpgradePacks;
bool g_bCvar_DontSwitchToPistol;
bool g_bCvar_TakeCoverFromRocks;
float g_fCvar_TankTeamLeash;
float g_fCvar_TankRescue_RangeSqr;
float g_fCvar_TankRescue_SafeDistSqr;
float g_fCvar_TankRescue_AbortDistSqr;
float g_fCvar_TankSupport_RangeSqr;
float g_fCvar_TankSupport_MinDist;
float g_fCvar_TankFleeDistSqr;
float g_fCvar_TankFleeStopDistSqr;
float g_fCvar_TankFleeBlindDistSqr;
float g_fCvar_TankFleeBlindStopDistSqr;
bool g_bCvar_TankFleeMapRoute;
bool g_bCvar_AvoidTanksWithProp;
bool g_bCvar_NoFallDmgOnLadderFail;

/*============ VARIABLES =========================================================*/
float g_fSurvivorBot_NextPressAttackTime[MAXPLAYERS+1];

int g_iSurvivorBot_TargetInfected[MAXPLAYERS+1];
int g_iSurvivorBot_IncapacitatedFriend[MAXPLAYERS+1];
int g_iSurvivorBot_PinnedFriend[MAXPLAYERS+1];

int g_iSurvivorBot_WitchTarget[MAXPLAYERS+1];
bool g_bSurvivorBot_IsWitchHarasser[MAXPLAYERS+1];

// Keeps a moving witch from replacing the bot's move order before it can run.
float g_fSurvivorBot_NextWitchMoveUpdateTime[MAXPLAYERS+1];

bool g_bSurvivorBot_PreventFire[MAXPLAYERS+1];
float g_fSurvivorBot_TankFleeUntil[MAXPLAYERS+1];
float g_fSurvivorBot_TankLastSeenTime[MAXPLAYERS+1];
int g_iSurvivorBot_TankRouteDirection[MAXPLAYERS+1];
float g_fSurvivorBot_NextTankRouteTime[MAXPLAYERS+1];
bool g_bTankRoute_AreasFound;
int g_iTankRoute_StartArea;
int g_iTankRoute_EndArea;
float g_fTankRoute_MaxFlow;
int g_iTankRoute_FlowAreas[BOT_TANK_ROUTE_FLOW_BUCKETS];
float g_fSurvivorBot_NextEscapeSearchTime[MAXPLAYERS+1];
float g_fSurvivorBot_NextDefibCheckTime[MAXPLAYERS+1];
float g_fSurvivorBot_NextDefibDamageCheckTime[MAXPLAYERS+1];
bool g_bSurvivorBot_DefibPosDamaging[MAXPLAYERS+1];
float g_fClient_NextAmmoUpdateTime[MAXPLAYERS+1];
float g_fClient_SurvivorTypeUntil[MAXPLAYERS+1];
int g_iClient_SurvivorType[MAXPLAYERS+1];
float g_fSurvivorBot_AimPartUntil[MAXPLAYERS+1];
int g_iSurvivorBot_AimPartTarget[MAXPLAYERS+1];
float g_fSurvivorBot_AimPartPos[MAXPLAYERS+1][3];
float g_fSurvivorBot_MeleeBoneUntil[MAXPLAYERS+1];
int g_iSurvivorBot_MeleeBoneTarget[MAXPLAYERS+1];
float g_fSurvivorBot_MeleeBonePos[MAXPLAYERS+1][3];
char g_sDebug_RouteFailReason[32];
int g_iTankRoute_Searches;
float g_fSurvivorBot_SightUntil[MAXPLAYERS+1];
int g_iSurvivorBot_SightEntity[MAXPLAYERS+1];
bool g_bSurvivorBot_Sight[MAXPLAYERS+1];
float g_fSurvivorBot_ScavengeScanTime[MAXPLAYERS+1];
bool g_bSurvivorBot_ScavengeUseExtended[MAXPLAYERS+1];
float g_fSurvivorBot_NextApproachCheckTime[MAXPLAYERS+1];
float g_fSurvivorBot_NextScavengeCheckTime[MAXPLAYERS+1];
float g_fSurvivorBot_NextGrenadeCheckTime[MAXPLAYERS+1];
float g_fSurvivorBot_NextGrenadeTargetTime[MAXPLAYERS+1];
int g_iSurvivorBot_GrenadeFarTarget[MAXPLAYERS+1];
float g_fSurvivorBot_NextFireCheckTime[MAXPLAYERS+1];
bool g_bSurvivorBot_FireCheckResult[MAXPLAYERS+1];
bool g_bSurvivorBot_VisionMemoryUsed[MAXPLAYERS+1];
float g_fTankSight_Until[MAXPLAYERS+1][MAXPLAYERS+1];
bool g_bTankSight_Visible[MAXPLAYERS+1][MAXPLAYERS+1];
int g_iTankAimBone_Tick[MAXPLAYERS+1];
bool g_bTankAimBone_Valid[MAXPLAYERS+1];
float g_fTankAimBone_Pos[MAXPLAYERS+1][3];
float g_fSurvivorBot_TankAimSightUntil[MAXPLAYERS+1];
bool g_bSurvivorBot_TankAimSight[MAXPLAYERS+1];
int g_iBotProcessing_LastTick = -1;
float g_fEntityScan_NextFastTime;
float g_fEntityScan_NextSlowTime;
int g_iEntityScan_Rocks[BOT_ENTITY_SCAN_MAX], g_iEntityScan_RockCount;
int g_iEntityScan_Props[BOT_ENTITY_SCAN_MAX], g_iEntityScan_PropCount;
int g_iEntityScan_DeathModels[BOT_ENTITY_SCAN_MAX], g_iEntityScan_DeathModelCount;
float g_fDebug_NextFleeAimLogTime[MAXPLAYERS+1];

bool g_bClient_IsLookingAtPosition[MAXPLAYERS+1];
bool g_bClient_IsFiringWeapon[MAXPLAYERS+1];

int g_iSurvivorBot_ScavengeItem[MAXPLAYERS+1];
float g_fSurvivorBot_NextUsePressTime[MAXPLAYERS+1];
float g_fSurvivorBot_NextScavengeItemScanTime[MAXPLAYERS+1];

float g_fSurvivorBot_VomitBlindedTime[MAXPLAYERS+1];

float g_fSurvivorBot_NextMoveCommandTime[MAXPLAYERS+1];

float g_fSurvivorBot_BlockWeaponSwitchTime[MAXPLAYERS+1];
float g_fSurvivorBot_BlockWeaponReloadTime[MAXPLAYERS+1];

float g_fSurvivorBot_MeleeApproachTime[MAXPLAYERS+1];
float g_fSurvivorBot_MeleeAttackTime[MAXPLAYERS+1];

float g_fSurvivorBot_TimeSinceLeftLadder[MAXPLAYERS+1];

// Path danger cache: the path search is too heavy to run on every think of every bot while a tank is alive.
// Regroup, rescue and team each keep their own answer, since their goals differ.
#define LBI_PDSLOT_REGROUP        0
#define LBI_PDSLOT_RESCUE         1
#define LBI_PDSLOT_TEAM           2
#define LBI_PATHDANGER_SLOTS      3

// A neutral answer can be kept longer, but one that stops or turns the bot back must expire quickly.
#define LBI_PATHDANGER_COOLDOWN   0.35    // neutral answer
#define LBI_PATHDANGER_CD_BLOCK   0.12    // answer that stops or turns the bot back
#define LBI_PATHDANGER_GOALTOL    16384.0 // 128 units, squared

float g_fPathDanger_NextCheck[MAXPLAYERS+1][LBI_PATHDANGER_SLOTS];
int   g_iPathDanger_Cached[MAXPLAYERS+1][LBI_PATHDANGER_SLOTS];
float g_fPathDanger_CachedGoal[MAXPLAYERS+1][LBI_PATHDANGER_SLOTS][3];
bool  g_bPathDanger_HasCache[MAXPLAYERS+1][LBI_PATHDANGER_SLOTS];

// The same for every player, so these are looked up once.
int g_iOffs_vecMins = -1;
int g_iOffs_vecMaxs = -1;

// Alive common infected, refreshed a few times per second and shared by all bots.
float g_fInfectedSnap_NextTime;
int g_iInfectedSnap_Slot[MAXENTITIES+1];
int g_iInfectedSnap_Count;
int g_iInfectedSnap_Entity[MAXENTITIES+1];
bool g_bInfectedSnap_Attacking[MAXENTITIES+1];
float g_fInfectedSnap_Origin[MAXENTITIES+1][3];
float g_fInfectedSnap_Center[MAXENTITIES+1][3];
int g_iInfectedSnap_PipeBomb = -1;
float g_fInfectedSnap_PipeBombPos[3];
bool g_bInfectedSnap_BileThrown;

int g_iTeamItemCount_Tick = -1;
int g_iTeamItemCount[view_as<int>(L4D2WeaponId_MAX)];

// Reset when the entity index is created or destroyed. 0 = unknown, 1 = yes, 2 = no.
int g_iEntCache_IsDoor[MAXENTITIES+1];
bool g_bEntCache_IsWeapon[MAXENTITIES+1];
int g_iEntCache_WeaponTier[MAXENTITIES+1];
int g_iEntCache_WeaponClass[MAXENTITIES+1];

enum
{
	WPNCLASS_UNKNOWN = 0,
	WPNCLASS_OTHER,
	WPNCLASS_SMG,
	WPNCLASS_RIFLE,
	WPNCLASS_SHOTGUN_T1,
	WPNCLASS_SHOTGUN_T2,
	WPNCLASS_SNIPER_T1,
	WPNCLASS_SNIPER_T2,
	WPNCLASS_GRENADE_LAUNCHER,
	WPNCLASS_M60,
	WPNCLASS_MELEE,
	WPNCLASS_CHAINSAW,
	WPNCLASS_PISTOL,
	WPNCLASS_MAGNUM,
	WPNCLASS_PIPE_BOMB,
	WPNCLASS_MOLOTOV,
	WPNCLASS_VOMITJAR,
	WPNCLASS_FIRST_AID_KIT,
	WPNCLASS_DEFIBRILLATOR,
	WPNCLASS_UPGRADE_PACK
}

// Let OnEntityDestroyed skip entities that never entered the item lists or the vision memory.
bool g_bEntityInItemList[MAXENTITIES+1];
bool g_bEntityHasVisionMemory[MAXENTITIES+1];

bool g_bHasAliveSurvivorBots;
float g_fHasAliveSurvivorBots_NextCheck;

// Valid until the next SetMoveToPosition, since the goal position cannot change before that.
float g_fSurvivorBot_NextTankCoverSearchTime[MAXPLAYERS+1];
float g_fSurvivorBot_NextTeamRegroupTime[MAXPLAYERS+1];
float g_fSurvivorBot_NextTankSupportTime[MAXPLAYERS+1];
int g_iSurvivorBot_SupportVictim[MAXPLAYERS+1];
float g_fSurvivorBot_SupportVictimUntil[MAXPLAYERS+1];
bool g_bSurvivorBot_SupportingVictim[MAXPLAYERS+1];
int g_iTeamAnchor;
int g_iTeamAnchor_Tick = -1;

int g_iSurvivorBot_RescueFriend[MAXPLAYERS+1];
int g_iSurvivorBot_RescueReachArea[MAXPLAYERS+1];
float g_fSurvivorBot_RescueReachUntil[MAXPLAYERS+1];
bool g_bSurvivorBot_RescueReachable[MAXPLAYERS+1];
float g_fSurvivorBot_RescueUntil[MAXPLAYERS+1];
bool g_bSurvivorBot_RetreatCommanded[MAXPLAYERS+1];
float g_fSurvivorBot_NextRetreatTime[MAXPLAYERS+1];
int g_iDebug_RetreatActionCount[MAXPLAYERS+1];
float g_fDebug_NextRetreatActionLogTime[MAXPLAYERS+1];
int g_iDebug_RegroupSuspendCount[MAXPLAYERS+1];
float g_fDebug_NextRegroupSuspendLogTime[MAXPLAYERS+1];
int g_iDebug_RegroupResumeCount[MAXPLAYERS+1];
float g_fDebug_NextRegroupResumeLogTime[MAXPLAYERS+1];
float g_fDebug_NextRetreatStopLogTime[MAXPLAYERS+1];
float g_fDebug_NextRescueLogTime[MAXPLAYERS+1];
float g_fDebug_NextRescueAbandonLogTime[MAXPLAYERS+1];

bool g_bSurvivorBot_MovePos_AreasCached[MAXPLAYERS+1];
int g_iSurvivorBot_MovePosReach_Start[MAXPLAYERS+1];
int g_iSurvivorBot_MovePosReach_Goal[MAXPLAYERS+1];
float g_fSurvivorBot_MovePosReach_Until[MAXPLAYERS+1];
bool g_bSurvivorBot_MovePosReach_Result[MAXPLAYERS+1];
int g_iSurvivorBot_MovePos_Area[MAXPLAYERS+1];
int g_iSurvivorBot_MovePos_ReachArea[MAXPLAYERS+1];

int g_iSurvivorBot_DefibTarget[MAXPLAYERS+1];

int g_iSurvivorBot_Grenade_ThrowTarget[MAXPLAYERS+1];
float g_fSurvivorBot_Grenade_ThrowPos[MAXPLAYERS+1][3];
float g_fSurvivorBot_Grenade_AimPos[MAXPLAYERS+1][3];

float g_fSurvivorBot_Grenade_NextThrowTime;
float g_fSurvivorBot_Grenade_NextThrowTime_Molotov;

float g_fSurvivorBot_LookPosition[MAXPLAYERS+1][3];
float g_fSurvivorBot_LookPosition_Duration[MAXPLAYERS+1];

float g_fSurvivorBot_MovePos_Position[MAXPLAYERS+1][3];
float g_fSurvivorBot_MovePos_Duration[MAXPLAYERS+1];
int g_iSurvivorBot_MovePos_Priority[MAXPLAYERS+1];
float g_fSurvivorBot_MovePos_Tolerance[MAXPLAYERS+1];
bool g_bSurvivorBot_MovePos_IgnoreDamaging[MAXPLAYERS+1];
char g_sSurvivorBot_MovePos_Name[MAXPLAYERS+1][512];

bool g_bSurvivorBot_ForceSwitchWeapon[MAXPLAYERS+1];
bool g_bSurvivorBot_ForceBash[MAXPLAYERS+1];

float g_fSurvivorBot_PinnedReactTime[MAXPLAYERS+1];

int g_iSurvivorBot_NearbyInfectedCount[MAXPLAYERS+1]; 
int g_iSurvivorBot_NearestInfectedCount[MAXPLAYERS+1]; 
int g_iSurvivorBot_ThreatInfectedCount[MAXPLAYERS+1]; 
int g_iSurvivorBot_GrenadeInfectedCount[MAXPLAYERS+1];

int g_iSurvivorBot_VisionMemory_State[MAXPLAYERS+1][MAXENTITIES+1];
int g_iSurvivorBot_VisionMemory_State_FOV[MAXPLAYERS+1][MAXENTITIES+1];

float g_fSurvivorBot_VisionMemory_Time[MAXPLAYERS+1][MAXENTITIES+1];
float g_fSurvivorBot_VisionMemory_Time_FOV[MAXPLAYERS+1][MAXENTITIES+1];

float g_fSurvivorBot_NextWeaponRangeSwitchTime[MAXPLAYERS+1];

bool g_bSurvivorBot_ForceWeaponReload[MAXPLAYERS+1];

int g_iSurvivorBot_NearbyFriends[MAXPLAYERS+1];

int g_iBotProcessing_ProcessedCount;
bool g_bBotProcessing_IsProcessed[MAXPLAYERS+1];
float g_fBotProcessing_NextProcessTime;

// Per-bot results of the periodic recompute, so one bot never reads another's.
int g_iSurvivorBot_TeamLeader[MAXPLAYERS+1];
int g_iSurvivorBot_NearestTeammate[MAXPLAYERS+1];
int g_iSurvivorBot_GameDifficulty[MAXPLAYERS+1];
int g_iSurvivorBot_NearestDeathModel[MAXPLAYERS+1];
int g_iSurvivorBot_TeamCount[MAXPLAYERS+1];
int g_iSurvivorBot_AlivePlayers[MAXPLAYERS+1];
int g_iSurvivorBot_TankRock[MAXPLAYERS+1];
int g_iSurvivorBot_TankTarget[MAXPLAYERS+1];
int g_iSurvivorBot_WitchHarasser[MAXPLAYERS+1];
int g_iSurvivorBot_TankProp[MAXPLAYERS+1];
bool g_bSurvivorBot_FriendIsNearBoomer[MAXPLAYERS+1];
bool g_bSurvivorBot_FriendIsNearThrowArea[MAXPLAYERS+1];
bool g_bSurvivorBot_TeamHasHumanPlayer[MAXPLAYERS+1];

float g_fDebug_LastStallWarnTime;

// How far the last recompute of this bot got, so CYCLE-STALLED can tell where it stops.
int g_iDebug_LastCheckpoint[MAXPLAYERS+1];

// Last logged state, so the recompute log only fires when something changed.
int g_iDebug_Last_TankTarget[MAXPLAYERS+1] = {-999, ...};
int g_iDebug_Last_WitchHarasser[MAXPLAYERS+1] = {-999, ...};
int g_iDebug_Last_TeamCount[MAXPLAYERS+1] = {-999, ...};
int g_iDebug_Last_AlivePlayers[MAXPLAYERS+1] = {-999, ...};
int g_iDebug_Last_HumanOnTeam[MAXPLAYERS+1] = {-999, ...};

int g_iDebug_Last_TankCheckTarget[MAXPLAYERS+1] = {-999, ...};
int g_iDebug_Last_WitchCheckTarget[MAXPLAYERS+1] = {-999, ...};
int g_iDebug_Last_WitchCheckHarasser[MAXPLAYERS+1] = {-999, ...};
float g_fDebug_NextScavengeLogTime[MAXPLAYERS+1];
float g_fDebug_NextMovePosLogTime[MAXPLAYERS+1];

int g_iInfectedBot_CurrentVictim[MAXPLAYERS+1];
bool g_bInfectedBot_IsThrowing[MAXPLAYERS+1];
float g_fInfectedBot_CoveredInVomitTime[MAXPLAYERS+1];

bool g_bMapStarted;
bool g_bCutsceneIsPlaying;
char g_sCurrentMapName[128];

// ----------------------------------------------------------------------------------------------------
// CLIENT GLOBAL DATA
// ----------------------------------------------------------------------------------------------------
float g_fClientEyePos[MAXPLAYERS+1][3];
float g_fClientEyeAng[MAXPLAYERS+1][3];
float g_fClientAbsOrigin[MAXPLAYERS+1][3];
float g_fClientCenteroid[MAXPLAYERS+1][3];
int g_iClientNavArea[MAXPLAYERS+1];
int g_iClientInventory[MAXPLAYERS+1][6];

// ----------------------------------------------------------------------------------------------------
// WEAPON GLOBAL DATA
// ----------------------------------------------------------------------------------------------------
int g_iWeapon_Clip1[MAXENTITIES+1];
int g_iWeapon_MaxAmmo[MAXENTITIES+1]; 
int g_iWeapon_AmmoLeft[MAXENTITIES+1];
int g_iItem_Used[MAXENTITIES+1]; // To fix bots grabbing same ammo upgrade repeatedly

// ----------------------------------------------------------------------------------------------------
// MELEE WEAPON MODELS
// ----------------------------------------------------------------------------------------------------
char g_sMeleeWeaponMdls[][] =
{
	"models/weapons/melee/w_katana.mdl",
	"models/weapons/melee/w_fireaxe.mdl",
	"models/weapons/melee/w_machete.mdl",
	"models/weapons/melee/w_electric_guitar.mdl",
	"models/weapons/melee/w_tonfa.mdl",
	"models/weapons/melee/w_golfclub.mdl",
	"models/weapons/melee/w_bat.mdl",
	"models/weapons/melee/w_cricket_bat.mdl",
	"models/weapons/melee/w_frying_pan.mdl",
	"models/weapons/melee/w_crowbar.mdl",
	"models/w_models/weapons/w_knife_t.mdl",
	"models/weapons/melee/w_shovel.mdl",
	"models/weapons/melee/w_pitchfork.mdl",
	"models/weapons/melee/w_riotshield.mdl"
};

// ----------------------------------------------------------------------------------------------------
// ENTITY ARRAYLISTS
// ----------------------------------------------------------------------------------------------------
ArrayList g_hMeleeList;
ArrayList g_hPistolList;
ArrayList g_hSMGList;
ArrayList g_hShotgunT1List;
ArrayList g_hShotgunT2List;
ArrayList g_hAssaultRifleList;
ArrayList g_hSniperRifleList;
ArrayList g_hTier3List;
ArrayList g_hGrenadeList;
ArrayList g_hFirstAidKitList;
ArrayList g_hDefibrillatorList;
ArrayList g_hUpgradePackList;
ArrayList g_hPainPillsList;
ArrayList g_hAdrenalineList;

ArrayList g_hAmmopileList;
ArrayList g_hLaserSightList;
ArrayList g_hDeployedAmmoPacks;

ArrayList g_hWitchList;
// ----------------------------------------------------------------------------------------------------
// CHARACTER MODEL BONES
// ----------------------------------------------------------------------------------------------------
char g_sBoneNames_Old[][] =
{
	"ValveBiped.Bip01_Head1", 
	"ValveBiped.Bip01_Spine", 
	"ValveBiped.Bip01_Spine1", 
	"ValveBiped.Bip01_Spine2", 
	"ValveBiped.Bip01_Spine4", 
	"ValveBiped.Bip01_L_UpperArm", 
	"ValveBiped.Bip01_L_Forearm", 
	"ValveBiped.Bip01_L_Hand", 
	"ValveBiped.Bip01_R_UpperArm", 
	"ValveBiped.Bip01_R_Forearm", 
	"ValveBiped.Bip01_R_Hand", 
	"ValveBiped.Bip01_Pelvis", 
	"ValveBiped.Bip01_L_Thigh", 
	"ValveBiped.Bip01_L_Knee", 
	"ValveBiped.Bip01_L_Foot", 
	"ValveBiped.Bip01_R_Thigh", 
	"ValveBiped.Bip01_R_Knee", 
	"ValveBiped.Bip01_R_Foot"
};
char g_sBoneNames_New[][] =
{
	"bip_head",
	"bip_spine_0",
	"bip_spine_1",
	"bip_spine_2",
	"bip_spine_3",
	"bip_upperArm_L",
	"bip_lowerArm_L",
	"bip_hand_L",
	"bip_upperArm_R",
	"bip_lowerArm_R",
	"bip_hand_R",
	"bip_pelvis",
	"bip_hip_L",
	"bip_knee_L",
	"bip_foot_L",
	"bip_hip_R",
	"bip_knee_R",
	"bip_foot_R"
};

bool g_bLateLoad;
bool g_bExtensionActions;

public APLRes AskPluginLoad2(Handle myself, bool late, char[] error, int err_max)
{
	EngineVersion test = GetEngineVersion();
	if (test != Engine_Left4Dead2)
	{
		strcopy(error, err_max, "Plugin only supports Left 4 Dead 2.");
		return APLRes_SilentFailure;
	}

	g_bLateLoad = late;
	return APLRes_Success;
}

public void OnPluginStart()
{
    Handle hGameConfig = LoadGameConfigFile("l4d2_improved_bots");
    if (!hGameConfig)SetFailState("Failed to find 'l4d2_improved_bots.txt' game config.");

    CreateAllSDKCalls(hGameConfig);

    g_iOffs_vecMins = FindSendPropInfo("CTerrorPlayer", "m_vecMins");
    g_iOffs_vecMaxs = FindSendPropInfo("CTerrorPlayer", "m_vecMaxs");
    CreateAllDetours(hGameConfig);

    delete hGameConfig;

    HookEvent("round_start", 			Event_OnRoundStart);
    HookEvent("round_end", 				Event_OnRoundEnd);

    HookEvent("weapon_fire", 			Event_OnWeaponFire);
    HookEvent("player_death", 			Event_OnPlayerDeath);
    HookEvent("player_use",				Event_OnPlayerUse);
    
    HookEvent("lunge_pounce", 			Event_OnSurvivorGrabbed);
    HookEvent("tongue_grab", 			Event_OnSurvivorGrabbed);
    HookEvent("jockey_ride", 			Event_OnSurvivorGrabbed);
    HookEvent("charger_carry_start", 	Event_OnSurvivorGrabbed);

    HookEvent("charger_charge_start",	Event_OnChargeStart);
    
    HookEvent("witch_harasser_set", 	Event_OnWitchHaraserSet);

    HookEvent("gameinstructor_nodraw",  Event_IntroCutsceneBegin, EventHookMode_PostNoCopy);
    HookEvent("gameinstructor_draw",    Event_IntroCutsceneEnd,   EventHookMode_PostNoCopy);

    CreateAndHookConVars();
    AutoExecConfig(true, "l4d2_improved_bots");

    if (g_bLateLoad)
    {
        for (int i = 1; i <= MaxClients; i++)
        {
            if (!IsClientInGame(i))continue;
            OnClientJoinServer(i);
        }
    }

    g_bExtensionActions = LibraryExists("actionslib");
}

void CreateAndHookConVars()
{
	g_hCvar_GameDifficulty 							= FindConVar("z_difficulty");
	g_hCvar_SurvivorLimpHealth 						= FindConVar("survivor_limp_health");
	g_hCvar_TankRockHealth 							= FindConVar("z_tank_throw_health");
	g_hCvar_GasCanUseDuration 						= FindConVar("gas_can_use_duration");
	g_hCvar_ChaseBileRange							= FindConVar("z_notice_it_range");
	g_hCvar_ServerGravity							= FindConVar("sv_gravity");
	g_hCvar_BileCoverDuration_Bot					= FindConVar("vomitjar_duration_infected_bot");
	g_hCvar_BileCoverDuration_PZ					= FindConVar("vomitjar_duration_infected_pz");

	g_hCvar_MaxMeleeSurvivors 						= FindConVar("sb_max_team_melee_weapons");
	g_hCvar_BotsShootThrough 						= FindConVar("sb_allow_shoot_through_survivors");
	g_hCvar_BotsFriendlyFire 						= FindConVar("sb_friendlyfire");
	g_hCvar_BotsDisabled 							= FindConVar("sb_stop");
	g_hCvar_BotsDontShoot 							= FindConVar("sb_dont_shoot");
	g_hCvar_BotsVomitBlindTime 						= FindConVar("sb_vomit_blind_time");

	g_hCvar_MaxAmmo_Pistol							= FindConVar("ammo_pistol_max");
	g_hCvar_MaxAmmo_AssaultRifle					= FindConVar("ammo_assaultrifle_max");
	g_hCvar_MaxAmmo_SMG								= FindConVar("ammo_smg_max");
	g_hCvar_MaxAmmo_M60								= FindConVar("ammo_m60_max");
	g_hCvar_MaxAmmo_Shotgun							= FindConVar("ammo_shotgun_max");
	g_hCvar_MaxAmmo_AutoShotgun						= FindConVar("ammo_autoshotgun_max");
	g_hCvar_MaxAmmo_HuntRifle						= FindConVar("ammo_huntingrifle_max");
	g_hCvar_MaxAmmo_SniperRifle						= FindConVar("ammo_sniperrifle_max");
	g_hCvar_MaxAmmo_PipeBomb						= FindConVar("ammo_pipebomb_max");
	g_hCvar_MaxAmmo_Molotov							= FindConVar("ammo_molotov_max");
	g_hCvar_MaxAmmo_VomitJar						= FindConVar("ammo_vomitjar_max");
	g_hCvar_MaxAmmo_PainPills						= FindConVar("ammo_painpills_max");
	g_hCvar_MaxAmmo_GrenLauncher					= FindConVar("ammo_grenadelauncher_max");
	g_hCvar_MaxAmmo_Adrenaline						= FindConVar("ammo_adrenaline_max");
	g_hCvar_MaxAmmo_Chainsaw						= FindConVar("ammo_chainsaw_max");
	g_hCvar_MaxAmmo_AmmoPack						= FindConVar("ammo_ammo_pack_max");
	g_hCvar_MaxAmmo_Medkit							= FindConVar("ammo_firstaid_max");

	g_hCvar_ImprovedMelee_Enabled 					= CreateConVar("l4d2_improvedbots_melee_enabled", "1", "Enables survivor bots' improved melee behaviour.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_hCvar_ImprovedMelee_MaxCount 					= CreateConVar("l4d2_improvedbots_melee_max_team", "1", "The total number of melee weapons allowed on the team. <0: Bots never use melee>", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_ImprovedMelee_SwitchCount 				= CreateConVar("l4d2_improvedbots_melee_switch_count", "3", "The nearby infected count required for bot to switch to their melee weapon.", FCVAR_NOTIFY, true, 1.0);
	g_hCvar_ImprovedMelee_SwitchRange 				= CreateConVar("l4d2_improvedbots_melee_switch_range", "250", "Range at which bot's target should be to switch to melee weapon.", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_ImprovedMelee_ApproachRange				= CreateConVar("l4d2_improvedbots_melee_approach_range", "400", "Range at which bot's target should be to approach it. <0: Disable Approaching>", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_ImprovedMelee_AimRange 					= CreateConVar("l4d2_improvedbots_melee_aim_range", "125", "Range at which bot's target should be to start taking aim at it.", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_ImprovedMelee_AttackRange 				= CreateConVar("l4d2_improvedbots_melee_attack_range", "70", "Range at which bot's target should be to start attacking it.", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_ImprovedMelee_ShoveChance 				= CreateConVar("l4d2_improvedbots_melee_shove_chance", "4", "Chance for bot to bash target instead of attacking with melee. <0: Disable Bashing>", FCVAR_NOTIFY, true, 0.0);

	g_hCvar_ImprovedMelee_ChainsawLimit 			= CreateConVar("l4d2_improvedbots_melee_chainsaw_limit", "1", "The total number of chainsaws allowed on the team. <0: Bots never use chainsaw>", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_ImprovedMelee_SwitchCount2 				= CreateConVar("l4d2_improvedbots_melee_chainsaw_switch_count", "6", "The nearby infected count required for bot to switch to chainsaw.", FCVAR_NOTIFY, true, 1.0);

	g_hCvar_TargetSelection_Enabled					= CreateConVar("l4d2_improvedbots_targetselection_enabled", "1", "Enables survivor bots' improved target selection.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_hCvar_TargetSelection_ShootRange				= CreateConVar("l4d2_improvedbots_targetselection_shootrange", "2000", "Range at which target need to be for bots to start firing at it.", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_TargetSelection_ShootRange2				= CreateConVar("l4d2_improvedbots_targetselection_shootrange_shotgun", "750", "Range at which target need to be for bots to start firing at it with shotgun.", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_TargetSelection_ShootRange3				= CreateConVar("l4d2_improvedbots_targetselection_shootrange_sniperrifle", "3000", "Range at which target need to be for bots to start firing at it with sniper rifle.", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_TargetSelection_ShootRange4				= CreateConVar("l4d2_improvedbots_targetselection_shootrange_pistol", "1500", "Range at which target need to be for bots to start firing at it with secondary weapon.", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_TargetSelection_IgnoreDociles			= CreateConVar("l4d2_improvedbots_targetselection_ignoredociles", "1", "If bots shouldn't target common infected that are currently not attacking survivors.", FCVAR_NOTIFY, true, 0.0, true, 1.0);

	g_hCvar_GrenadeThrow_Enabled 					= CreateConVar("l4d2_improvedbots_grenadethrowing_enabled", "1", "Enables survivor bots throwing grenades.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_hCvar_GrenadeThrow_GrenadeTypes				= CreateConVar("l4d2_improvedbots_grenadethrowing_grenadetypes", "7", "What grenades should survivor bots throw? <1: Pipe-Bomb, 2: Molotov, 4: Bile Bomb. Add numbers together.>", FCVAR_NOTIFY, true, 1.0, true, 7.0);
	g_hCvar_GrenadeThrow_ThrowRange					= CreateConVar("l4d2_improvedbots_grenadethrowing_throw_range", "1500", "Range at which target needs to be for bot to throw grenade at it.", FCVAR_NOTIFY);
	g_hCvar_GrenadeThrow_HordeSize 					= CreateConVar("l4d2_improvedbots_grenadethrowing_horde_size_multiplier", "3.75", "Infected count required to throw grenade Multiplier (Value * SurvivorCount).", FCVAR_NOTIFY, true, 1.0);
	g_hCvar_GrenadeThrow_NextThrowTime1 			= CreateConVar("l4d2_improvedbots_grenadethrowing_next_throw_time_min", "20", "First number to pick to randomize next grenade throw time.", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_GrenadeThrow_NextThrowTime2 			= CreateConVar("l4d2_improvedbots_grenadethrowing_next_throw_time_max", "30", "Second number to pick to randomize next grenade throw time.", FCVAR_NOTIFY, true, 0.0);

	g_hCvar_TankRock_ShootEnabled 					= CreateConVar("l4d2_improvedbots_shootattankrocks_enabled", "1", "Enables survivor bots shooting tank's thrown rocks.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_hCvar_TankRock_ShootRange 					= CreateConVar("l4d2_improvedbots_shootattankrocks_range", "1500", "Range at which rock needs to be for bot to start shooting at it.", FCVAR_NOTIFY, true, 0.0);

	g_hCvar_AutoShove_Enabled						= CreateConVar("l4d2_improvedbots_autoshove_enabled", "1", "Makes survivor bots automatically shove every nearby infected. <0: Disabled, 1: All infected, 2: Only if infected is behind them>", FCVAR_NOTIFY, true, 0.0, true, 2.0);

	g_hCvar_HelpPinnedFriend_Enabled				= CreateConVar("l4d2_improvedbots_help_pinnedfriend_enabled", "3", "Makes survivor bots force attack pinned survivor's SI if possible. <0: Disabled, 1: Shoot at attacker, 2: Shove the attacker if close enough. Add numbers together.>", FCVAR_NOTIFY, true, 0.0, true, 3.0);
	g_hCvar_HelpPinnedFriend_ShootRange				= CreateConVar("l4d2_improvedbots_help_pinnedfriend_shootrange", "2000", "Range at which bots will start firing at SI.", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_HelpPinnedFriend_ShoveRange				= CreateConVar("l4d2_improvedbots_help_pinnedfriend_shoverange", "75", "Range at which bots will start to bash SI.", FCVAR_NOTIFY), true, 0.0;

	g_hCvar_DefibRevive_Enabled						= CreateConVar("l4d2_improvedbots_defib_revive_enabled", "1", "Enable bots reviving dead players with defibrillators if they have one available.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_hCvar_DefibRevive_ScanDist 					= CreateConVar("l4d2_improvedbots_defib_revive_distance", "2000", "Range at which survivor's dead body should be for bot to consider it reviveable.", FCVAR_NOTIFY, true, 0.0);

	g_hCvar_FireBash_Chance1						= CreateConVar("l4d2_improvedbots_fireshove_chance_pumpshotguns", "4", "Chance at which survivor bot may shove after firing a pump-action shotgun. <0: Disabled, 1: Always>", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_FireBash_Chance2						= CreateConVar("l4d2_improvedbots_fireshove_chance_css_sniperrifles", "3", "Chance at which survivor bot may shove after firing a bolt-action sniper rifle. <0: Disabled, 1: Always>", FCVAR_NOTIFY, true, 0.0);
	
	g_hCvar_ItemScavenge_Items 						= CreateConVar("l4d2_improvedbots_itemscavenge_enabled", "16383", "Enable improved bot item scavenging for specified items. <0: Disable, 1: Pipe Bomb, 2: Molotov, 4: Bile Bomb, 8: Medkit, 16: Defibrillator, 32: UpgradePack, 64: Pain Pills, 128: Adrenaline, 256: Laser Sights, 512: Ammopack, 1024: Ammopile, 2048: Chainsaw, 4096: Secondary Weapons, 8192: Primary Weapons. Add numbers together>", FCVAR_NOTIFY, true, 0.0, true, 16383.0);
	g_hCvar_ItemScavenge_ApproachRange 				= CreateConVar("l4d2_improvedbots_itemscavenge_scavenge_distance", "300", "Distance at which item should be for bot to move it.", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_ItemScavenge_ApproachVisibleRange 		= CreateConVar("l4d2_improvedbots_itemscavenge_scavenge_visible_distance", "600", "Distance at which a visible item should be for bot to move it.", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_ItemScavenge_PickupRange 				= CreateConVar("l4d2_improvedbots_itemscavenge_pickup_distance", "100", "Distance at which item should be for bot to able to pick it up.", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_ItemScavenge_MapSearchRange 			= CreateConVar("l4d2_improvedbots_itemscavenge_mapsearchdistance", "2500", "How close should the item be to the survivor bot to able to count it when searching?", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_ItemScavenge_NoHumansRangeMultiplier	= CreateConVar("l4d2_improvedbots_itemscavenge_nohumans_rangemultiplier", "3.0", "The bots' scavenge distance is multiplied to this value when there's no human players left in the team.", FCVAR_NOTIFY, true, 0.0);

	g_hCvar_BotWeaponPreference_Nick 				= CreateConVar("l4d2_improvedbots_weapon_preference_nick", "1", "Bot Nick's weapon preference. <0: Default, 1: Assault Rifle, 2: Shotgun, 3: Sniper Rifle, 4: SMG, 5: Secondary Weapon>", FCVAR_NOTIFY, true, 0.0, true, 5.0);
	g_hCvar_BotWeaponPreference_Rochelle 			= CreateConVar("l4d2_improvedbots_weapon_preference_rochelle", "1", "Bot Rochelle's weapon preference. <0: Default, 1: Assault Rifle, 2: Shotgun, 3: Sniper Rifle, 4: SMG, 5: Secondary Weapon>", FCVAR_NOTIFY, true, 0.0, true, 5.0);
	g_hCvar_BotWeaponPreference_Coach 				= CreateConVar("l4d2_improvedbots_weapon_preference_coach", "2", "Bot Coach's weapon preference. <0: Default, 1: Assault Rifle, 2: Shotgun, 3: Sniper Rifle, 4: SMG, 5: Secondary Weapon>", FCVAR_NOTIFY, true, 0.0, true, 5.0);
	g_hCvar_BotWeaponPreference_Ellis 				= CreateConVar("l4d2_improvedbots_weapon_preference_ellis", "3", "Bot Ellis's weapon preference. <0: Default, 1: Assault Rifle, 2: Shotgun, 3: Sniper Rifle, 4: SMG, 5: Secondary Weapon>", FCVAR_NOTIFY, true, 0.0, true, 5.0);
	g_hCvar_BotWeaponPreference_Bill  				= CreateConVar("l4d2_improvedbots_weapon_preference_bill", "1", "Bot Bill's weapon preference. <0: Default, 1: Assault Rifle, 2: Shotgun, 3: Sniper Rifle, 4: SMG, 5: Secondary Weapon>", FCVAR_NOTIFY, true, 0.0, true, 5.0);
	g_hCvar_BotWeaponPreference_Zoey 				= CreateConVar("l4d2_improvedbots_weapon_preference_zoey", "3", "Bot Zoey's weapon preference. <0: Default, 1: Assault Rifle, 2: Shotgun, 3: Sniper Rifle, 4: SMG, 5: Secondary Weapon>", FCVAR_NOTIFY, true, 0.0, true, 5.0);
	g_hCvar_BotWeaponPreference_Francis 			= CreateConVar("l4d2_improvedbots_weapon_preference_francis", "2", "Bot Francis's weapon preference. <0: Default, 1: Assault Rifle, 2: Shotgun, 3: Sniper Rifle, 4: SMG, 5: Secondary Weapon>", FCVAR_NOTIFY, true, 0.0, true, 5.0);
	g_hCvar_BotWeaponPreference_Louis 				= CreateConVar("l4d2_improvedbots_weapon_preference_louis", "1", "Bot Louis's weapon preference. <0: Default, 1: Assault Rifle, 2: Shotgun, 3: Sniper Rifle, 4: SMG, 5: Secondary Weapon>", FCVAR_NOTIFY, true, 0.0, true, 5.0);
	g_hCvar_BotWeaponPreference_ForceMagnum 		= CreateConVar("l4d2_improvedbots_weapon_preference_magnums_only", "0", "If every survivor bot should only use magnum instead of regular pistol if possible.", FCVAR_NOTIFY, true, 0.0, true, 1.0);

	g_hCvar_SwapSameTypePrimaries 					= CreateConVar("l4d2_improvedbots_changeweaponiftoomanysubtype_primaries", "1", "Makes survivor bots change their primary weapon subtype if there's too much of the same one, Ex. change AK-47 to M16 or SPAS-12 to Autoshotgun.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_hCvar_SwapSameTypeGrenades 					= CreateConVar("l4d2_improvedbots_changeweaponiftoomanysubtype_grenades", "1", "Makes survivor bots change their grenade type if there's too much of the same one, Ex. Pipe-Bomb to Molotov.", FCVAR_NOTIFY, true, 0.0, true, 1.0);

	g_hCvar_MaxWeaponTier3_M60 						= CreateConVar("l4d2_improvedbots_tier3weaponlimit_m60", "1", "The total number of M60s allowed on the team. <0: Bots never use M60>", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_MaxWeaponTier3_GLauncher 				= CreateConVar("l4d2_improvedbots_tier3weaponlimit_grenadelauncher", "1", "The total number of grenade launchers allowed on the team. <0: Bots never use grenade launcher>", FCVAR_NOTIFY, true, 0.0);
	
	g_hCvar_Vision_FieldOfView 						= CreateConVar("l4d2_improvedbots_vision_fieldofview", "60.0", "The field of view of survivor bots.", FCVAR_NOTIFY, true, 0.0, true, 180.0);
	g_hCvar_Vision_NoticeTimeScale 					= CreateConVar("l4d2_improvedbots_vision_noticetimescale", "1.1", "The time required for bots to notice enemy target is multiplied to this value.", FCVAR_NOTIFY, true, 0.0, true, 4.0);
	
	g_hCvar_SpitterAcidEvasion						= CreateConVar("l4d2_improvedbots_evadespitteracids", "1", "Enables survivor bots' improved spitter acid evasion", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_hCvar_AlwaysCarryProp							= CreateConVar("l4d2_improvedbots_alwayscarryprop", "1", "If enabled, survivor bot will keep holding the prop it currently has unless it's swarmed by a mob, every teammate needs help, or it wants to use an item.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_hCvar_KeepMovingInCombat						= CreateConVar("l4d2_improvedbots_keepmovingincombat", "1", "If bots shouldn't stop moving in combat when there's no human players in team.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_hCvar_SwitchOffCSSWeapons						= CreateConVar("l4d2_improvedbots_switchoffcssweapon", "1", "If bots should change their primary weapon to other one if they're using CSS weapons.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_hCvar_ChargerEvasion							= CreateConVar("l4d2_improvedbots_chargerevasion", "1", "Enables survivor bots's charger dodging behavior.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_hCvar_DeployUpgradePacks						= CreateConVar("l4d2_improvedbots_deployupgradepacks", "1", "If bots should deploy their upgrade pack when available and not in combat.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_hCvar_DontSwitchToPistol						= CreateConVar("l4d2_improvedbots_dontswitchtopistol", "1", "If bots shouldn't switch to their pistol while they have sniper rifle equiped.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_hCvar_TakeCoverFromRocks						= CreateConVar("l4d2_improvedbots_takecoverfromtankrocks", "1", "If bots should take cover from tank's thrown rocks.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_hCvar_TankTeamLeash							= CreateConVar("l4d2_improvedbots_tank_team_leash", "600", "How far from the team a bot may be while a tank is alive. Farther than this, it regroups instead of fleeing. <0: Disable>", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_TankRescue_Range						= CreateConVar("l4d2_improvedbots_tank_rescue_range", "800", "How close a downed survivor must be for a bot to go help it while a tank is alive. Up to double this if no tank is on the path. <0: Disable>", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_TankRescue_SafeDist						= CreateConVar("l4d2_improvedbots_tank_rescue_safe_distance", "400", "A rescue only starts if the tank is farther than this from both the bot and the downed survivor.", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_TankRescue_AbortDist						= CreateConVar("l4d2_improvedbots_tank_rescue_abort_distance", "200", "An ongoing rescue is dropped if the tank gets closer than this to the bot or the downed survivor.", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_TankSupport_Range						= CreateConVar("l4d2_improvedbots_tank_support_range", "450", "How close a bot tries to stay to the survivor a tank is chasing. <0: Disable>", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_TankSupport_MinDist						= CreateConVar("l4d2_improvedbots_tank_support_min_distance", "800", "A bot supporting a chased survivor stops advancing this far from the tank. Keep it above l4d2_improvedbots_tank_flee_distance, or the bot flees before it stops.", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_TankFleeDist							= CreateConVar("l4d2_improvedbots_tank_flee_distance", "650", "A bot that sees a tank flees from it at this distance.", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_TankFleeMapRoute						= CreateConVar("l4d2_improvedbots_tank_flee_map_route", "0", "A bot fleeing a tank follows the main route of the map, toward the end if it is ahead of the tank and toward the start if it is behind. Falls back to the game retreat when no route is usable.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_hCvar_TankFleeBlindDist						= CreateConVar("l4d2_improvedbots_tank_flee_blind_distance", "600", "A bot with no line of sight to a tank flees from it at this distance. A retreat only starts within 1024, the range at which an unseen tank is tracked, and a bot always flees within 384.", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_AvoidTanksWithProp						= CreateConVar("l4d2_improvedbots_avoidtanksnearpunchableprops", "1", "If bots should avoid and retreat from tanks that are nearby punchable props like cars.", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	g_hCvar_NoFallDmgOnLadderFail					= CreateConVar("l4d2_improvedbots_nofalldmgonladderfail", "1", "If enabled, survivor bots won't take fall damage if they were climbing a ladder just before that.", FCVAR_NOTIFY, true, 0.0, true, 1.0);

	g_hCvar_WitchBehavior_WalkWhenNearby			= CreateConVar("l4d2_improvedbots_witchbehavior_walkwhennearby", "500", "Survivor bots will start walking near witch if they're this range near her and she's not disturbed. <0: Disabled>", FCVAR_NOTIFY, true, 0.0);
	g_hCvar_WitchBehavior_AllowCrowning				= CreateConVar("l4d2_improvedbots_witchbehavior_allowcrowning", "1", "Allows survivor bots to crown witch on their path if they're holding any shotgun type weapon. <0: Disabled; 1: Only if survivor team doesn't have any human players; 2:Enabled>", FCVAR_NOTIFY, true, 0.0, true, 2.0);

	g_hCvar_NextProcessTime 						= CreateConVar("l4d2_improvedbots_process_time", "0.1", "Bots' data computing time delay (infected count, nearby friends, etc). Increasing the value might help increasing the game performance, but slow down bots.", FCVAR_NOTIFY, true, 0.033);

	g_hCvar_Debug									= CreateConVar("l4d2_improvedbots_debug", "0", "Logs the bots' shared tactical state (team leader, tank target, witch harasser, team count) and key AI decisions (tank reaction, scavenge leash) to diagnose freezing/wandering bots. 0=Off, 1=On.", FCVAR_NOTIFY, true, 0.0, true, 1.0);

	g_hCvar_GameDifficulty.AddChangeHook(OnConVarChanged);
	g_hCvar_SurvivorLimpHealth.AddChangeHook(OnConVarChanged);
	g_hCvar_TankRockHealth.AddChangeHook(OnConVarChanged);
	g_hCvar_GasCanUseDuration.AddChangeHook(OnConVarChanged);
	g_hCvar_ChaseBileRange.AddChangeHook(OnConVarChanged);
	g_hCvar_ServerGravity.AddChangeHook(OnConVarChanged);
	g_hCvar_BileCoverDuration_Bot.AddChangeHook(OnConVarChanged);
	g_hCvar_BileCoverDuration_PZ.AddChangeHook(OnConVarChanged);
	
	g_hCvar_MaxAmmo_Pistol.AddChangeHook(OnConVarChanged);
	g_hCvar_MaxAmmo_AssaultRifle.AddChangeHook(OnConVarChanged);
	g_hCvar_MaxAmmo_SMG.AddChangeHook(OnConVarChanged);
	g_hCvar_MaxAmmo_M60.AddChangeHook(OnConVarChanged);
	g_hCvar_MaxAmmo_Shotgun.AddChangeHook(OnConVarChanged);
	g_hCvar_MaxAmmo_AutoShotgun.AddChangeHook(OnConVarChanged);
	g_hCvar_MaxAmmo_HuntRifle.AddChangeHook(OnConVarChanged);
	g_hCvar_MaxAmmo_SniperRifle.AddChangeHook(OnConVarChanged);
	g_hCvar_MaxAmmo_PipeBomb.AddChangeHook(OnConVarChanged);
	g_hCvar_MaxAmmo_Molotov.AddChangeHook(OnConVarChanged);
	g_hCvar_MaxAmmo_VomitJar.AddChangeHook(OnConVarChanged);
	g_hCvar_MaxAmmo_PainPills.AddChangeHook(OnConVarChanged);
	g_hCvar_MaxAmmo_GrenLauncher.AddChangeHook(OnConVarChanged);
	g_hCvar_MaxAmmo_Adrenaline.AddChangeHook(OnConVarChanged);
	g_hCvar_MaxAmmo_Chainsaw.AddChangeHook(OnConVarChanged);
	g_hCvar_MaxAmmo_AmmoPack.AddChangeHook(OnConVarChanged);
	g_hCvar_MaxAmmo_Medkit.AddChangeHook(OnConVarChanged);

	g_hCvar_MaxMeleeSurvivors.AddChangeHook(OnConVarChanged);
	g_hCvar_BotsShootThrough.AddChangeHook(OnConVarChanged);
	g_hCvar_BotsFriendlyFire.AddChangeHook(OnConVarChanged);
	g_hCvar_BotsDisabled.AddChangeHook(OnConVarChanged);
	g_hCvar_BotsDontShoot.AddChangeHook(OnConVarChanged);
	g_hCvar_BotsVomitBlindTime.AddChangeHook(OnConVarChanged);

	g_hCvar_ImprovedMelee_MaxCount.AddChangeHook(OnConVarChanged);

	g_hCvar_ImprovedMelee_Enabled.AddChangeHook(OnConVarChanged);
	g_hCvar_ImprovedMelee_SwitchCount.AddChangeHook(OnConVarChanged);
	g_hCvar_ImprovedMelee_SwitchRange.AddChangeHook(OnConVarChanged);
	g_hCvar_ImprovedMelee_ApproachRange.AddChangeHook(OnConVarChanged);
	g_hCvar_ImprovedMelee_AimRange.AddChangeHook(OnConVarChanged);
	g_hCvar_ImprovedMelee_AttackRange.AddChangeHook(OnConVarChanged);
	g_hCvar_ImprovedMelee_ShoveChance.AddChangeHook(OnConVarChanged);
	
	g_hCvar_ImprovedMelee_ChainsawLimit.AddChangeHook(OnConVarChanged);
	g_hCvar_ImprovedMelee_SwitchCount2.AddChangeHook(OnConVarChanged);

	g_hCvar_TargetSelection_Enabled.AddChangeHook(OnConVarChanged);
	g_hCvar_TargetSelection_ShootRange.AddChangeHook(OnConVarChanged);
	g_hCvar_TargetSelection_ShootRange2.AddChangeHook(OnConVarChanged);
	g_hCvar_TargetSelection_ShootRange3.AddChangeHook(OnConVarChanged);
	g_hCvar_TargetSelection_ShootRange4.AddChangeHook(OnConVarChanged);
	g_hCvar_TargetSelection_IgnoreDociles.AddChangeHook(OnConVarChanged);

	g_hCvar_GrenadeThrow_Enabled.AddChangeHook(OnConVarChanged);
	g_hCvar_GrenadeThrow_GrenadeTypes.AddChangeHook(OnConVarChanged);
	g_hCvar_GrenadeThrow_ThrowRange.AddChangeHook(OnConVarChanged);
	g_hCvar_GrenadeThrow_HordeSize.AddChangeHook(OnConVarChanged);
	g_hCvar_GrenadeThrow_NextThrowTime1.AddChangeHook(OnConVarChanged);
	g_hCvar_GrenadeThrow_NextThrowTime2.AddChangeHook(OnConVarChanged);

	g_hCvar_TankRock_ShootEnabled.AddChangeHook(OnConVarChanged);
	g_hCvar_TankRock_ShootRange.AddChangeHook(OnConVarChanged);

	g_hCvar_AutoShove_Enabled.AddChangeHook(OnConVarChanged);
	
	g_hCvar_HelpPinnedFriend_Enabled.AddChangeHook(OnConVarChanged);
	g_hCvar_HelpPinnedFriend_ShootRange.AddChangeHook(OnConVarChanged);
	g_hCvar_HelpPinnedFriend_ShoveRange.AddChangeHook(OnConVarChanged);

	g_hCvar_DefibRevive_Enabled.AddChangeHook(OnConVarChanged);
	g_hCvar_DefibRevive_ScanDist.AddChangeHook(OnConVarChanged);
	
	g_hCvar_FireBash_Chance1.AddChangeHook(OnConVarChanged);
	g_hCvar_FireBash_Chance2.AddChangeHook(OnConVarChanged);
	
	g_hCvar_ItemScavenge_Items.AddChangeHook(OnConVarChanged);
	g_hCvar_ItemScavenge_ApproachRange.AddChangeHook(OnConVarChanged);
	g_hCvar_ItemScavenge_ApproachVisibleRange.AddChangeHook(OnConVarChanged);
	g_hCvar_ItemScavenge_PickupRange.AddChangeHook(OnConVarChanged);
	g_hCvar_ItemScavenge_MapSearchRange.AddChangeHook(OnConVarChanged);
	g_hCvar_ItemScavenge_NoHumansRangeMultiplier.AddChangeHook(OnConVarChanged);

	g_hCvar_BotWeaponPreference_ForceMagnum.AddChangeHook(OnConVarChanged);
	
	g_hCvar_BotWeaponPreference_Nick.AddChangeHook(OnConVarChanged);
	g_hCvar_BotWeaponPreference_Louis.AddChangeHook(OnConVarChanged);
	g_hCvar_BotWeaponPreference_Bill.AddChangeHook(OnConVarChanged);
	g_hCvar_BotWeaponPreference_Rochelle.AddChangeHook(OnConVarChanged);
	g_hCvar_BotWeaponPreference_Zoey.AddChangeHook(OnConVarChanged);
	g_hCvar_BotWeaponPreference_Ellis.AddChangeHook(OnConVarChanged);
	g_hCvar_BotWeaponPreference_Coach.AddChangeHook(OnConVarChanged);
	g_hCvar_BotWeaponPreference_Francis.AddChangeHook(OnConVarChanged);
	
	g_hCvar_SwapSameTypePrimaries.AddChangeHook(OnConVarChanged);
	g_hCvar_SwapSameTypeGrenades.AddChangeHook(OnConVarChanged);

	g_hCvar_MaxWeaponTier3_M60.AddChangeHook(OnConVarChanged);
	g_hCvar_MaxWeaponTier3_GLauncher.AddChangeHook(OnConVarChanged);
	
	g_hCvar_Vision_FieldOfView.AddChangeHook(OnConVarChanged);
	g_hCvar_Vision_NoticeTimeScale.AddChangeHook(OnConVarChanged);
	
	g_hCvar_SpitterAcidEvasion.AddChangeHook(OnConVarChanged);
	g_hCvar_AlwaysCarryProp.AddChangeHook(OnConVarChanged);
	g_hCvar_KeepMovingInCombat.AddChangeHook(OnConVarChanged);
	g_hCvar_SwitchOffCSSWeapons.AddChangeHook(OnConVarChanged);
	g_hCvar_ChargerEvasion.AddChangeHook(OnConVarChanged);
	g_hCvar_DeployUpgradePacks.AddChangeHook(OnConVarChanged);
	g_hCvar_DontSwitchToPistol.AddChangeHook(OnConVarChanged);
	g_hCvar_TakeCoverFromRocks.AddChangeHook(OnConVarChanged);
	g_hCvar_TankTeamLeash.AddChangeHook(OnConVarChanged);
	g_hCvar_TankRescue_Range.AddChangeHook(OnConVarChanged);
	g_hCvar_TankRescue_SafeDist.AddChangeHook(OnConVarChanged);
	g_hCvar_TankRescue_AbortDist.AddChangeHook(OnConVarChanged);
	g_hCvar_TankSupport_Range.AddChangeHook(OnConVarChanged);
	g_hCvar_TankSupport_MinDist.AddChangeHook(OnConVarChanged);
	g_hCvar_TankFleeDist.AddChangeHook(OnConVarChanged);
	g_hCvar_TankFleeMapRoute.AddChangeHook(OnConVarChanged);
	g_hCvar_TankFleeBlindDist.AddChangeHook(OnConVarChanged);
	g_hCvar_AvoidTanksWithProp.AddChangeHook(OnConVarChanged);
	g_hCvar_NoFallDmgOnLadderFail.AddChangeHook(OnConVarChanged);

	g_hCvar_WitchBehavior_WalkWhenNearby.AddChangeHook(OnConVarChanged);
	g_hCvar_WitchBehavior_AllowCrowning.AddChangeHook(OnConVarChanged);
	
	g_hCvar_NextProcessTime.AddChangeHook(OnConVarChanged);
	g_hCvar_Debug.AddChangeHook(OnConVarChanged);
}

public void OnAllPluginsLoaded()
{
	UpdateConVarValues();
}

public void OnConfigsExecuted()
{
	UpdateConVarValues();

	if (g_bCvar_Debug)DebugLog("PLUGIN-LOADED build=%s %s targetSelection=%d", __DATE__, __TIME__, g_bCvar_TargetSelection_Enabled);
}

// Written directly instead of through LogToFile, which also echoes every line to the server console.
void DebugLog(const char[] sFormat, any ...)
{
	static char sMessage[1024];
	VFormat(sMessage, sizeof(sMessage), sFormat, 2);

	char sDate[16], sTime[32], sPath[PLATFORM_MAX_PATH];
	FormatTime(sDate, sizeof(sDate), "%Y%m%d");
	FormatTime(sTime, sizeof(sTime), "%m/%d/%Y - %H:%M:%S");
	BuildPath(Path_SM, sPath, sizeof(sPath), "logs/sb_ai_improver_%s.log", sDate);

	File hFile = OpenFile(sPath, "a");
	if (hFile == null)return;

	hFile.WriteLine("%s: %s", sTime, sMessage);
	delete hFile;
}

void OnConVarChanged(Handle convar, const char[] oldValue, const char[] newValue)
{
	UpdateConVarValues();
}

void UpdateConVarValues()
{	
	g_hCvar_GameDifficulty.GetString(g_sCvar_GameDifficulty, sizeof(g_sCvar_GameDifficulty));
	g_iCvar_SurvivorLimpHealth 							= g_hCvar_SurvivorLimpHealth.IntValue;
	g_iCvar_TankRockHealth 								= g_hCvar_TankRockHealth.IntValue;
	g_fCvar_GasCanUseDuration 							= g_hCvar_GasCanUseDuration.FloatValue;
	g_fCvar_ChaseBileRange 								= g_hCvar_ChaseBileRange.FloatValue;
	g_fCvar_ServerGravity 								= g_hCvar_ServerGravity.FloatValue;
	g_fCvar_BileCoverDuration_Bot 						= g_hCvar_BileCoverDuration_Bot.FloatValue;
	g_fCvar_BileCoverDuration_PZ 						= g_hCvar_BileCoverDuration_PZ.FloatValue;

	g_iCvar_MaxAmmo_Pistol								= g_hCvar_MaxAmmo_Pistol.IntValue;
	g_iCvar_MaxAmmo_AssaultRifle						= g_hCvar_MaxAmmo_AssaultRifle.IntValue;
	g_iCvar_MaxAmmo_SMG									= g_hCvar_MaxAmmo_SMG.IntValue;
	g_iCvar_MaxAmmo_M60									= g_hCvar_MaxAmmo_M60.IntValue;
	g_iCvar_MaxAmmo_Shotgun								= g_hCvar_MaxAmmo_Shotgun.IntValue;
	g_iCvar_MaxAmmo_AutoShotgun							= g_hCvar_MaxAmmo_AutoShotgun.IntValue;
	g_iCvar_MaxAmmo_HuntRifle							= g_hCvar_MaxAmmo_HuntRifle.IntValue;	
	g_iCvar_MaxAmmo_SniperRifle							= g_hCvar_MaxAmmo_SniperRifle.IntValue;
	g_iCvar_MaxAmmo_PipeBomb							= g_hCvar_MaxAmmo_PipeBomb.IntValue;	
	g_iCvar_MaxAmmo_Molotov								= g_hCvar_MaxAmmo_Molotov.IntValue;
	g_iCvar_MaxAmmo_VomitJar							= g_hCvar_MaxAmmo_VomitJar.IntValue;
	g_iCvar_MaxAmmo_PainPills							= g_hCvar_MaxAmmo_PainPills.IntValue;
	g_iCvar_MaxAmmo_GrenLauncher						= g_hCvar_MaxAmmo_GrenLauncher.IntValue;
	g_iCvar_MaxAmmo_Adrenaline							= g_hCvar_MaxAmmo_Adrenaline.IntValue;
	g_iCvar_MaxAmmo_Chainsaw							= g_hCvar_MaxAmmo_Chainsaw.IntValue;
	g_iCvar_MaxAmmo_AmmoPack							= g_hCvar_MaxAmmo_AmmoPack.IntValue;
	g_iCvar_MaxAmmo_Medkit								= g_hCvar_MaxAmmo_Medkit.IntValue;

	g_bCvar_BotsShootThrough 							= g_hCvar_BotsShootThrough.BoolValue;
	g_bCvar_BotsFriendlyFire 							= g_hCvar_BotsFriendlyFire.BoolValue;
	g_bCvar_BotsDisabled								= g_hCvar_BotsDisabled.BoolValue;
	g_bCvar_BotsDontShoot								= g_hCvar_BotsDontShoot.BoolValue;
	g_fCvar_BotsVomitBlindTime							= g_hCvar_BotsVomitBlindTime.FloatValue;

	g_hCvar_MaxMeleeSurvivors.IntValue					= g_hCvar_ImprovedMelee_MaxCount.IntValue;
	g_iCvar_MaxMeleeSurvivors 							= g_hCvar_MaxMeleeSurvivors.IntValue;

	g_bCvar_ImprovedMelee_Enabled 						= g_hCvar_ImprovedMelee_Enabled.BoolValue;
	g_iCvar_ImprovedMelee_SwitchCount 					= g_hCvar_ImprovedMelee_SwitchCount.IntValue;
	g_iCvar_ImprovedMelee_ShoveChance 					= g_hCvar_ImprovedMelee_ShoveChance.IntValue;
	g_fCvar_ImprovedMelee_SwitchRange 					= (g_hCvar_ImprovedMelee_SwitchRange.FloatValue*g_hCvar_ImprovedMelee_SwitchRange.FloatValue);
	g_fCvar_ImprovedMelee_ApproachRange 				= (g_hCvar_ImprovedMelee_ApproachRange.FloatValue*g_hCvar_ImprovedMelee_ApproachRange.FloatValue);
	g_fCvar_ImprovedMelee_AimRange 						= (g_hCvar_ImprovedMelee_AimRange.FloatValue*g_hCvar_ImprovedMelee_AimRange.FloatValue);
	g_fCvar_ImprovedMelee_AttackRange 					= g_hCvar_ImprovedMelee_AttackRange.FloatValue;
	
	g_iCvar_ImprovedMelee_ChainsawLimit 				= g_hCvar_ImprovedMelee_ChainsawLimit.IntValue;
	g_iCvar_ImprovedMelee_SwitchCount2 					= g_hCvar_ImprovedMelee_SwitchCount2.IntValue;

	g_bCvar_TargetSelection_Enabled						= g_hCvar_TargetSelection_Enabled.BoolValue;
	g_fCvar_TargetSelection_ShootRange					= g_hCvar_TargetSelection_ShootRange.FloatValue;
	g_fCvar_TargetSelection_ShootRange2					= g_hCvar_TargetSelection_ShootRange2.FloatValue;
	g_fCvar_TargetSelection_ShootRange3					= g_hCvar_TargetSelection_ShootRange3.FloatValue;
	g_fCvar_TargetSelection_ShootRange4					= g_hCvar_TargetSelection_ShootRange4.FloatValue;
	g_bCvar_TargetSelection_IgnoreDociles				= g_hCvar_TargetSelection_IgnoreDociles.BoolValue;

	g_bCvar_BotWeaponPreference_ForceMagnum 			= g_hCvar_BotWeaponPreference_ForceMagnum.BoolValue;
	
	g_iCvar_BotWeaponPreference_Nick 					= g_hCvar_BotWeaponPreference_Nick.IntValue;
	g_iCvar_BotWeaponPreference_Louis 					= g_hCvar_BotWeaponPreference_Louis.IntValue;
	g_iCvar_BotWeaponPreference_Bill 					= g_hCvar_BotWeaponPreference_Bill.IntValue;
	g_iCvar_BotWeaponPreference_Rochelle 				= g_hCvar_BotWeaponPreference_Rochelle.IntValue;
	g_iCvar_BotWeaponPreference_Zoey 					= g_hCvar_BotWeaponPreference_Zoey.IntValue;
	g_iCvar_BotWeaponPreference_Ellis 					= g_hCvar_BotWeaponPreference_Ellis.IntValue;
	g_iCvar_BotWeaponPreference_Coach 					= g_hCvar_BotWeaponPreference_Coach.IntValue;
	g_iCvar_BotWeaponPreference_Francis 				= g_hCvar_BotWeaponPreference_Francis.IntValue;

	g_bCvar_GrenadeThrow_Enabled 						= g_hCvar_GrenadeThrow_Enabled.BoolValue;
	g_iCvar_GrenadeThrow_GrenadeTypes 					= g_hCvar_GrenadeThrow_GrenadeTypes.IntValue;
	g_fCvar_GrenadeThrow_ThrowRange 					= g_hCvar_GrenadeThrow_ThrowRange.FloatValue;
	g_fCvar_GrenadeThrow_HordeSize 						= g_hCvar_GrenadeThrow_HordeSize.FloatValue;
	g_fCvar_GrenadeThrow_NextThrowTime1 				= g_hCvar_GrenadeThrow_NextThrowTime1.FloatValue;
	g_fCvar_GrenadeThrow_NextThrowTime2 				= g_hCvar_GrenadeThrow_NextThrowTime2.FloatValue;

	g_bCvar_TankRock_ShootEnabled						= g_hCvar_TankRock_ShootEnabled.BoolValue;
	g_fCvar_TankRock_ShootRange							= (g_hCvar_TankRock_ShootRange.FloatValue*g_hCvar_TankRock_ShootRange.FloatValue);

	g_bCvar_DefibRevive_Enabled 						= g_hCvar_DefibRevive_Enabled.BoolValue;
	g_fCvar_DefibRevive_ScanDist 						= g_hCvar_DefibRevive_ScanDist.FloatValue;

	g_iCvar_FireBash_Chance1 							= g_hCvar_FireBash_Chance1.IntValue;
	g_iCvar_FireBash_Chance2 							= g_hCvar_FireBash_Chance2.IntValue;

	g_iCvar_AutoShove_Enabled 							= g_hCvar_AutoShove_Enabled.IntValue;
	
	g_iCvar_HelpPinnedFriend_Enabled 					= g_hCvar_HelpPinnedFriend_Enabled.IntValue;
	g_fCvar_HelpPinnedFriend_ShootRange 				= g_hCvar_HelpPinnedFriend_ShootRange.FloatValue;
	g_fCvar_HelpPinnedFriend_ShoveRange 				= g_hCvar_HelpPinnedFriend_ShoveRange.FloatValue;
	
	g_iCvar_ItemScavenge_Items 							= g_hCvar_ItemScavenge_Items.IntValue;
	g_fCvar_ItemScavenge_ApproachRange 					= g_hCvar_ItemScavenge_ApproachRange.FloatValue;
	g_fCvar_ItemScavenge_ApproachVisibleRange 			= g_hCvar_ItemScavenge_ApproachVisibleRange.FloatValue;
	g_fCvar_ItemScavenge_PickupRange 					= g_hCvar_ItemScavenge_PickupRange.FloatValue;
	g_fCvar_ItemScavenge_MapSearchRange 				= (g_hCvar_ItemScavenge_MapSearchRange.FloatValue*g_hCvar_ItemScavenge_MapSearchRange.FloatValue);
	g_fCvar_ItemScavenge_NoHumansRangeMultiplier 		= g_hCvar_ItemScavenge_NoHumansRangeMultiplier.FloatValue;

	g_bCvar_SwapSameTypePrimaries 						= g_hCvar_SwapSameTypePrimaries.BoolValue;
	g_bCvar_SwapSameTypeGrenades 						= g_hCvar_SwapSameTypeGrenades.BoolValue;
	
	g_iCvar_MaxWeaponTier3_M60 							= g_hCvar_MaxWeaponTier3_M60.IntValue;
	g_iCvar_MaxWeaponTier3_GLauncher 					= g_hCvar_MaxWeaponTier3_GLauncher.IntValue;
	
	g_fCvar_Vision_FieldOfView 							= g_hCvar_Vision_FieldOfView.FloatValue;
	g_fCvar_Vision_NoticeTimeScale 						= g_hCvar_Vision_NoticeTimeScale.FloatValue;
	
	g_bCvar_SpitterAcidEvasion							= g_hCvar_SpitterAcidEvasion.BoolValue;
	g_bCvar_AlwaysCarryProp								= g_hCvar_AlwaysCarryProp.BoolValue;
	g_bCvar_SwitchOffCSSWeapons							= g_hCvar_SwitchOffCSSWeapons.BoolValue;
	g_bCvar_ChargerEvasion								= g_hCvar_ChargerEvasion.BoolValue;
	g_bCvar_DeployUpgradePacks							= g_hCvar_DeployUpgradePacks.BoolValue;
	g_bCvar_DontSwitchToPistol							= g_hCvar_DontSwitchToPistol.BoolValue;
	g_bCvar_TakeCoverFromRocks							= g_hCvar_TakeCoverFromRocks.BoolValue;
	g_fCvar_TankTeamLeash								= g_hCvar_TankTeamLeash.FloatValue;
	g_fCvar_TankRescue_RangeSqr							= (g_hCvar_TankRescue_Range.FloatValue*g_hCvar_TankRescue_Range.FloatValue);
	g_fCvar_TankRescue_SafeDistSqr						= (g_hCvar_TankRescue_SafeDist.FloatValue*g_hCvar_TankRescue_SafeDist.FloatValue);
	g_fCvar_TankRescue_AbortDistSqr						= (g_hCvar_TankRescue_AbortDist.FloatValue*g_hCvar_TankRescue_AbortDist.FloatValue);
	g_fCvar_TankSupport_RangeSqr						= (g_hCvar_TankSupport_Range.FloatValue*g_hCvar_TankSupport_Range.FloatValue);
	g_fCvar_TankSupport_MinDist							= g_hCvar_TankSupport_MinDist.FloatValue;
	g_fCvar_TankFleeDistSqr								= (g_hCvar_TankFleeDist.FloatValue*g_hCvar_TankFleeDist.FloatValue);
	g_fCvar_TankFleeStopDistSqr							= ((g_hCvar_TankFleeDist.FloatValue + BOT_TANK_FLEE_STOP_MARGIN)*(g_hCvar_TankFleeDist.FloatValue + BOT_TANK_FLEE_STOP_MARGIN));
	g_fCvar_TankFleeBlindDistSqr						= (g_hCvar_TankFleeBlindDist.FloatValue*g_hCvar_TankFleeBlindDist.FloatValue);
	g_bCvar_TankFleeMapRoute							= g_hCvar_TankFleeMapRoute.BoolValue;
	g_fCvar_TankFleeBlindStopDistSqr					= ((g_hCvar_TankFleeBlindDist.FloatValue + BOT_TANK_FLEE_STOP_MARGIN)*(g_hCvar_TankFleeBlindDist.FloatValue + BOT_TANK_FLEE_STOP_MARGIN));
	g_bCvar_AvoidTanksWithProp							= g_hCvar_AvoidTanksWithProp.BoolValue;
	g_bCvar_NoFallDmgOnLadderFail						= g_hCvar_NoFallDmgOnLadderFail.BoolValue;

	if (g_bMapStarted)
	{
		char sShouldHurryCode[64]; FormatEx(sShouldHurryCode, sizeof(sShouldHurryCode), "DirectorScript.GetDirectorOptions().cm_ShouldHurry <- %i", g_hCvar_KeepMovingInCombat.IntValue);
		L4D2_ExecVScriptCode(sShouldHurryCode);
	}

	g_fCvar_WitchBehavior_WalkWhenNearby 				= g_hCvar_WitchBehavior_WalkWhenNearby.FloatValue;
	g_iCvar_WitchBehavior_AllowCrowning 				= g_hCvar_WitchBehavior_AllowCrowning.IntValue;

	g_fCvar_NextProcessTime 							= g_hCvar_NextProcessTime.FloatValue;
	g_bCvar_Debug										= g_hCvar_Debug.BoolValue;
}

Handle g_hCalcAbsolutePosition;

Handle g_hLookupBone; 
Handle g_hGetBonePosition; 

Handle g_hGetMaxClip1;
Handle g_hIsInCombat;

Handle g_hIsReachableNavArea; 
Handle g_hIsAvailable; 

Handle g_hMarkNavAreaAsBlocked;

ActionConstructor g_hSurvivorLegsRetreat;

int g_iNavArea_Center;
int g_iNavArea_Parent;
int g_iNavArea_NWCorner;
int g_iNavArea_SECorner;
int g_iNavArea_InvDXCorners;
int g_iNavArea_InvDYCorners;
int g_iNavArea_DamagingTickCount;

void CreateAllSDKCalls(Handle hGameData)
{
	if ((g_iNavArea_Center = GameConfGetOffset(hGameData, "CNavArea::m_center")) == -1)
		SetFailState("Failed to get CNavArea::m_center offset.");
	if ((g_iNavArea_Parent = GameConfGetOffset(hGameData, "CNavArea::m_parent")) == -1)
		SetFailState("Failed to get CNavArea::m_parent offset.");
	if ((g_iNavArea_NWCorner = GameConfGetOffset(hGameData, "CNavArea::m_nwCorner")) == -1)
		SetFailState("Failed to get CNavArea::m_nwCorner offset.");
	if ((g_iNavArea_SECorner = GameConfGetOffset(hGameData, "CNavArea::m_seCorner")) == -1)
		SetFailState("Failed to get CNavArea::m_seCorner offset.");
	if ((g_iNavArea_InvDXCorners = GameConfGetOffset(hGameData, "CNavArea::m_invDxCorners")) == -1)
		SetFailState("Failed to get CNavArea::m_invDxCorners offset.");
	if ((g_iNavArea_InvDYCorners = GameConfGetOffset(hGameData, "CNavArea::m_invDyCorners")) == -1)
		SetFailState("Failed to get CNavArea::m_invDyCorners offset.");
	if ((g_iNavArea_DamagingTickCount = GameConfGetOffset(hGameData, "CNavArea::m_damagingTickCount")) == -1)
		SetFailState("Failed to get CNavArea::m_damagingTickCount offset.");

	StartPrepSDKCall(SDKCall_Entity);
	PrepSDKCall_SetFromConf(hGameData, SDKConf_Signature, "CBaseEntity::CalcAbsolutePosition");
	if ((g_hCalcAbsolutePosition = EndPrepSDKCall()) == null) 
		SetFailState("Failed to create SDKCall for CBaseEntity::CalcAbsolutePosition signature!");

	StartPrepSDKCall(SDKCall_Entity);
	PrepSDKCall_SetFromConf(hGameData, SDKConf_Signature, "CBaseAnimating::GetBonePosition");
	PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
	PrepSDKCall_AddParameter(SDKType_Vector, SDKPass_ByRef, _, VENCODE_FLAG_COPYBACK);
	PrepSDKCall_AddParameter(SDKType_QAngle, SDKPass_ByRef, _, VENCODE_FLAG_COPYBACK);
	if ((g_hGetBonePosition = EndPrepSDKCall()) == null) 
		SetFailState("Failed to create SDKCall for CBaseAnimating::GetBonePosition signature!");

	StartPrepSDKCall(SDKCall_Entity);
	PrepSDKCall_SetFromConf(hGameData, SDKConf_Signature, "CBaseAnimating::LookupBone");
	PrepSDKCall_AddParameter(SDKType_String, SDKPass_Pointer);
	PrepSDKCall_SetReturnInfo(SDKType_PlainOldData, SDKPass_Plain);
	if ((g_hLookupBone = EndPrepSDKCall()) == null) 
		SetFailState("Failed to create SDKCall for CBaseAnimating::LookupBone signature!");

	StartPrepSDKCall(SDKCall_Entity);
	PrepSDKCall_SetFromConf(hGameData, SDKConf_Virtual, "CBaseCombatWeapon::GetMaxClip1");
	PrepSDKCall_SetReturnInfo(SDKType_PlainOldData, SDKPass_Plain);
	if ((g_hGetMaxClip1 = EndPrepSDKCall()) == null)
		SetFailState("Failed to create SDKCall for CBaseCombatWeapon::GetMaxClip1 virtual!");

	StartPrepSDKCall(SDKCall_Player);
	PrepSDKCall_SetFromConf(hGameData, SDKConf_Signature, "SurvivorBot::IsReachable<CNavArea>");
	PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
	PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
	PrepSDKCall_SetReturnInfo(SDKType_Bool, SDKPass_Plain);
	if ((g_hIsReachableNavArea = EndPrepSDKCall()) == null)
		SetFailState("Failed to create SDKCall for SurvivorBot::IsReachable<CNavArea> signature!");

	StartPrepSDKCall(SDKCall_Player);
	PrepSDKCall_SetFromConf(hGameData, SDKConf_Signature, "SurvivorBot::IsAvailable");
	PrepSDKCall_SetReturnInfo(SDKType_Bool, SDKPass_Plain);
	if ((g_hIsAvailable = EndPrepSDKCall()) == null)
		SetFailState("Failed to create SDKCall for SurvivorBot::IsAvailable signature!");

	StartPrepSDKCall(SDKCall_Player);
	PrepSDKCall_SetFromConf(hGameData, SDKConf_Signature, "CTerrorPlayer::IsInCombat");
	PrepSDKCall_AddParameter(SDKType_Bool, SDKPass_Plain);
	PrepSDKCall_SetReturnInfo(SDKType_Bool, SDKPass_Plain);
	if ((g_hIsInCombat = EndPrepSDKCall()) == null)
		SetFailState("Failed to create SDKCall for CTerrorPlayer::IsInCombat signature!");	

	StartPrepSDKCall(SDKCall_Raw);
	PrepSDKCall_SetFromConf(hGameData, SDKConf_Signature, "CNavArea::MarkAsBlocked");
	PrepSDKCall_AddParameter(SDKType_PlainOldData, SDKPass_Plain);
	PrepSDKCall_AddParameter(SDKType_CBaseEntity, SDKPass_Pointer, VDECODE_FLAG_ALLOWNULL);
	PrepSDKCall_AddParameter(SDKType_Bool, SDKPass_Plain);
	PrepSDKCall_SetReturnInfo(SDKType_Bool, SDKPass_Plain);
	if ((g_hMarkNavAreaAsBlocked = EndPrepSDKCall()) == null)
		SetFailState("Failed to create SDKCall for CNavArea::MarkAsBlocked signature!");

	// Actions 4.x only accepts actions it built itself, and its size setter doesn't work, so the retreat is set up from the gamedata.
	g_hSurvivorLegsRetreat = ActionConstructor.SetupFromConf(hGameData, "SBAI_SurvivorLegsRetreat");
	if (g_hSurvivorLegsRetreat == null)
		SetFailState("Failed to create ActionConstructor \"SBAI_SurvivorLegsRetreat\" from gamedata!");

}

Handle g_hOnFindUseEntity;
Handle g_hOnInfernoTouchNavArea;
Handle g_hOnGetAvoidRange;

void CreateAllDetours(Handle hGameData)
{
	g_hOnFindUseEntity = DHookCreateDetour(Address_Null, CallConv_THISCALL, ReturnType_CBaseEntity, ThisPointer_CBaseEntity);
	if (!g_hOnFindUseEntity)SetFailState("Failed to setup detour for CTerrorPlayer::FindUseEntity");
	if (!DHookSetFromConf(g_hOnFindUseEntity, hGameData, SDKConf_Signature, "CTerrorPlayer::FindUseEntity"))
		SetFailState("Failed to load CTerrorPlayer::FindUseEntity signature from gamedata");
	DHookAddParam(g_hOnFindUseEntity, HookParamType_Float);
	DHookAddParam(g_hOnFindUseEntity, HookParamType_Float);
	DHookAddParam(g_hOnFindUseEntity, HookParamType_Float);
	DHookAddParam(g_hOnFindUseEntity, HookParamType_Bool);
	DHookAddParam(g_hOnFindUseEntity, HookParamType_Bool);
	if (!DHookEnableDetour(g_hOnFindUseEntity, true, DTR_OnFindUseEntity))
		SetFailState("Failed to detour CTerrorPlayer::FindUseEntity.");

	g_hOnInfernoTouchNavArea = DHookCreateDetour(Address_Null, CallConv_THISCALL, ReturnType_Bool, ThisPointer_CBaseEntity);
	if (!g_hOnInfernoTouchNavArea)SetFailState("Failed to setup detour for CInferno::IsTouching<CNavArea>");
	if (!DHookSetFromConf(g_hOnInfernoTouchNavArea, hGameData, SDKConf_Signature, "CInferno::IsTouching<CNavArea>"))
		SetFailState("Failed to load CInferno::IsTouching<CNavArea> signature from gamedata");
	DHookAddParam(g_hOnInfernoTouchNavArea, HookParamType_Int);
	if (!DHookEnableDetour(g_hOnInfernoTouchNavArea, true, DTR_OnInfernoTouchNavArea))
		SetFailState("Failed to detour CInferno::IsTouching<CNavArea>.");

	g_hOnGetAvoidRange = DHookCreateDetour(Address_Null, CallConv_THISCALL, ReturnType_Float, ThisPointer_CBaseEntity);
	if (!g_hOnGetAvoidRange)SetFailState("Failed to setup detour for SurvivorBot::GetAvoidRange");
	if (!DHookSetFromConf(g_hOnGetAvoidRange, hGameData, SDKConf_Signature, "SurvivorBot::GetAvoidRange"))
		SetFailState("Failed to load SurvivorBot::GetAvoidRange signature from gamedata");
	DHookAddParam(g_hOnGetAvoidRange, HookParamType_CBaseEntity);
	if (!DHookEnableDetour(g_hOnGetAvoidRange, true, DTR_OnSurvivorBotGetAvoidRange))
		SetFailState("Failed to detour SurvivorBot::GetAvoidRange.");	
}

void Event_OnRoundStart(Event hEvent, const char[] sName, bool bBroadcast)
{
	ResetDataOnRoundChange();
}

void Event_OnRoundEnd(Event hEvent, const char[] sName, bool bBroadcast)
{
	ResetDataOnRoundChange();
}

float g_fClient_ThinkFunctionDelay[MAXPLAYERS+1];
void ResetDataOnRoundChange()
{
	g_iBotProcessing_ProcessedCount = 0;
	g_fDebug_LastStallWarnTime = 0.0;
	g_fBotProcessing_NextProcessTime = GetGameTime() + g_fCvar_NextProcessTime;
	g_fSurvivorBot_Grenade_NextThrowTime = g_fSurvivorBot_Grenade_NextThrowTime_Molotov = GetGameTime() + 5.0;

	for (int i = 1; i <= MaxClients; i++)
	{
		g_fClient_ThinkFunctionDelay[i] = GetGameTime() + 5.0;
		if (!IsClientInGame(i))continue;
		ResetClientPluginVariables(i);
	}
}

public void OnClientPutInServer(int iClient)
{
	OnClientJoinServer(iClient);
}

void OnClientJoinServer(int iClient)
{
	SDKHook(iClient, SDKHook_WeaponSwitch, OnSurvivorSwitchWeapon);
	SDKHook(iClient, SDKHook_OnTakeDamage, OnSurvivorTakeDamage);
	g_fClient_ThinkFunctionDelay[iClient] = GetGameTime() + 5.0;
	ResetClientPluginVariables(iClient);
}

public void OnClientDisconnect(int iClient)
{
	SDKUnhook(iClient, SDKHook_WeaponSwitch, OnSurvivorSwitchWeapon);
	SDKUnhook(iClient, SDKHook_OnTakeDamage, OnSurvivorTakeDamage);
	g_fClient_ThinkFunctionDelay[iClient] = GetGameTime() + 5.0;
	ResetClientPluginVariables(iClient);
}

void ResetClientPluginVariables(int iClient)
{
	g_bBotProcessing_IsProcessed[iClient] = false;
	// Only an AI infected sets this, and a player taking the slot later would inherit the old victim.
	g_iInfectedBot_CurrentVictim[iClient] = 0;
	g_fSurvivorBot_RescueReachUntil[iClient] = 0.0;
	g_fSurvivorBot_MovePosReach_Until[iClient] = 0.0;
	g_fSurvivorBot_NextTankCoverSearchTime[iClient] = 0.0;
	g_fSurvivorBot_NextTeamRegroupTime[iClient] = 0.0;
	g_iSurvivorBot_RescueFriend[iClient] = 0;
	g_fSurvivorBot_RescueUntil[iClient] = 0.0;
	g_bSurvivorBot_RetreatCommanded[iClient] = false;
	g_fSurvivorBot_NextRetreatTime[iClient] = 0.0;
	g_fSurvivorBot_NextEscapeSearchTime[iClient] = 0.0;
	g_fSurvivorBot_NextDefibCheckTime[iClient] = 0.0;
	g_fSurvivorBot_NextDefibDamageCheckTime[iClient] = 0.0;
	g_fSurvivorBot_SightUntil[iClient] = 0.0;
	g_fInfectedBot_CoveredInVomitTime[iClient] = 0.0;
	g_iSurvivorBot_SightEntity[iClient] = -1;
	g_fSurvivorBot_ScavengeScanTime[iClient] = 0.0;
	g_bSurvivorBot_ScavengeUseExtended[iClient] = false;
	g_fClient_NextAmmoUpdateTime[iClient] = 0.0;
	g_fClient_SurvivorTypeUntil[iClient] = 0.0;
	g_fSurvivorBot_AimPartUntil[iClient] = 0.0;
	g_iSurvivorBot_AimPartTarget[iClient] = -1;
	g_fSurvivorBot_MeleeBoneUntil[iClient] = 0.0;
	g_iSurvivorBot_MeleeBoneTarget[iClient] = -1;
	g_fSurvivorBot_NextWitchMoveUpdateTime[iClient] = 0.0;
	g_fDebug_NextScavengeLogTime[iClient] = 0.0;
	g_fDebug_NextMovePosLogTime[iClient] = 0.0;
	g_fSurvivorBot_NextApproachCheckTime[iClient] = 0.0;
	g_fSurvivorBot_NextScavengeCheckTime[iClient] = 0.0;
	g_fSurvivorBot_NextGrenadeCheckTime[iClient] = 0.0;
	g_fSurvivorBot_NextGrenadeTargetTime[iClient] = 0.0;
	g_iSurvivorBot_GrenadeFarTarget[iClient] = -1;
	g_fSurvivorBot_NextFireCheckTime[iClient] = 0.0;
	g_bSurvivorBot_FireCheckResult[iClient] = true;
	g_fSurvivorBot_TankAimSightUntil[iClient] = 0.0;
	g_iDebug_RetreatActionCount[iClient] = 0;
	g_fDebug_NextRetreatActionLogTime[iClient] = 0.0;
	g_iDebug_RegroupSuspendCount[iClient] = 0;
	g_fDebug_NextRegroupSuspendLogTime[iClient] = 0.0;
	g_iDebug_RegroupResumeCount[iClient] = 0;
	g_fDebug_NextRegroupResumeLogTime[iClient] = 0.0;
	g_fDebug_NextRetreatStopLogTime[iClient] = 0.0;
	g_fDebug_NextRescueLogTime[iClient] = 0.0;
	g_fDebug_NextRescueAbandonLogTime[iClient] = 0.0;
	g_fSurvivorBot_NextTankSupportTime[iClient] = 0.0;
	g_iSurvivorBot_SupportVictim[iClient] = 0;
	g_fSurvivorBot_SupportVictimUntil[iClient] = 0.0;
	g_bSurvivorBot_SupportingVictim[iClient] = false;

	g_iSurvivorBot_TargetInfected[iClient] = -1;	
	g_iSurvivorBot_WitchTarget[iClient] = -1;	
	g_iSurvivorBot_ThreatInfectedCount[iClient] = 0;
	g_iSurvivorBot_NearbyInfectedCount[iClient] = 0;
	g_iSurvivorBot_NearestInfectedCount[iClient] = 0;
	g_iSurvivorBot_GrenadeInfectedCount[iClient] = 0;
	g_iSurvivorBot_ScavengeItem[iClient] = -1;
	g_iSurvivorBot_DefibTarget[iClient] = -1;
	g_iSurvivorBot_Grenade_ThrowTarget[iClient] = -1;
	g_iSurvivorBot_MovePos_Priority[iClient] = 0;
	for (int iSlot = 0; iSlot < LBI_PATHDANGER_SLOTS; iSlot++)
	{
		g_bPathDanger_HasCache[iClient][iSlot] = false;
		g_fPathDanger_NextCheck[iClient][iSlot] = 0.0;
	}
	
	g_sSurvivorBot_MovePos_Name[iClient][0] = 0;
	g_fSurvivorBot_MovePos_Tolerance[iClient] = -1.0;

	g_bSurvivorBot_IsWitchHarasser[iClient] = false;
	g_fSurvivorBot_TankFleeUntil[iClient] = 0.0;
	g_fSurvivorBot_TankLastSeenTime[iClient] = 0.0;
	g_iSurvivorBot_TankRouteDirection[iClient] = 0;
	g_fSurvivorBot_NextTankRouteTime[iClient] = 0.0;
	g_fDebug_NextFleeAimLogTime[iClient] = 0.0;
	g_bSurvivorBot_ForceWeaponReload[iClient] = false;
	g_bSurvivorBot_ForceSwitchWeapon[iClient] = false;
	g_bSurvivorBot_ForceBash[iClient] = false;
	g_bSurvivorBot_PreventFire[iClient] = false;
	g_bSurvivorBot_MovePos_IgnoreDamaging[iClient] = false;

	g_fSurvivorBot_PinnedReactTime[iClient] = GetGameTime();
	g_fSurvivorBot_NextUsePressTime[iClient] = GetGameTime() + 0.33;
	g_fSurvivorBot_NextScavengeItemScanTime[iClient] = GetGameTime() + 1.0;
	g_fSurvivorBot_BlockWeaponReloadTime[iClient] = GetGameTime();
	g_fSurvivorBot_BlockWeaponSwitchTime[iClient] = GetGameTime();
	g_fSurvivorBot_VomitBlindedTime[iClient] = GetGameTime();
	g_fSurvivorBot_TimeSinceLeftLadder[iClient] = GetGameTime();
	g_fSurvivorBot_MeleeApproachTime[iClient] = GetGameTime();
	g_fSurvivorBot_MeleeAttackTime[iClient] = GetGameTime();
	g_fSurvivorBot_NextMoveCommandTime[iClient] = GetGameTime() + BOT_CMD_MOVE_INTERVAL;
	g_fSurvivorBot_LookPosition_Duration[iClient] = GetGameTime();
	g_fSurvivorBot_NextPressAttackTime[iClient] = GetGameTime();
	g_fSurvivorBot_MovePos_Duration[iClient] = GetGameTime();
	g_fSurvivorBot_NextWeaponRangeSwitchTime[iClient] = GetGameTime();

	SetVectorToZero(g_fSurvivorBot_Grenade_ThrowPos[iClient]);
	SetVectorToZero(g_fSurvivorBot_Grenade_AimPos[iClient]);
	SetVectorToZero(g_fSurvivorBot_LookPosition[iClient]);
	SetVectorToZero(g_fSurvivorBot_MovePos_Position[iClient]);

	// Only a bot that looked at something has anything to clear.
	if (g_bSurvivorBot_VisionMemoryUsed[iClient])
	{
		g_bSurvivorBot_VisionMemoryUsed[iClient] = false;
		for (int i = 0; i < MAXENTITIES; i++)
		{
			g_iSurvivorBot_VisionMemory_State[iClient][i] = g_iSurvivorBot_VisionMemory_State_FOV[iClient][i] = 0;
			g_fSurvivorBot_VisionMemory_Time[iClient][i] = g_fSurvivorBot_VisionMemory_Time_FOV[iClient][i] = 0.0;
		}
	}

	if (!IsValidClient(iClient) || !IsFakeClient(iClient) || GetClientTeam(iClient) != 2)
		return;

	L4D2_CommandABot(iClient, 0, BOT_CMD_RESET);
}

void Event_OnWeaponFire(Event hEvent, const char[] sName, bool bBroadcast)
{
	int iClient = GetClientOfUserId(hEvent.GetInt("userid"));
	if (!iClient || !IsFakeClient(iClient))return;

	static char sWeaponName[64]; 
	hEvent.GetString("weapon", sWeaponName, sizeof(sWeaponName));

	if (GetRandomInt(1, g_iCvar_FireBash_Chance1) == 1 && (strcmp(sWeaponName, "shotgun_chrome") == 0 || strcmp(sWeaponName, "pumpshotgun") == 0))
	{
		g_bSurvivorBot_ForceBash[iClient] = true;
	}
	else if (GetRandomInt(1, g_iCvar_FireBash_Chance2) == 1 && (strcmp(sWeaponName, "sniper_awp") == 0 || strcmp(sWeaponName, "sniper_scout") == 0))
	{
		g_bSurvivorBot_ForceBash[iClient] = true;
	}
}

void Event_OnPlayerDeath(Event hEvent, const char[] sName, bool bBroadcast)
{
	int iVictim = GetClientOfUserId(hEvent.GetInt("userid"));
	int iAttacker = GetClientOfUserId(hEvent.GetInt("attacker"));
	int iInfected = hEvent.GetInt("entityid");

	if (g_iSurvivorBot_TargetInfected[iAttacker] == iVictim || g_iSurvivorBot_TargetInfected[iAttacker] == iInfected)
	{
		g_iSurvivorBot_TargetInfected[iAttacker] = -1;
	}

	for (int i = 1; i <= MaxClients; i++)
	{
		g_iSurvivorBot_VisionMemory_State[i][iVictim] = g_iSurvivorBot_VisionMemory_State_FOV[i][iVictim] = 0;
		g_fSurvivorBot_VisionMemory_Time[i][iVictim] = g_fSurvivorBot_VisionMemory_Time_FOV[i][iVictim] = 0.0;
	}

	g_bInfectedBot_IsThrowing[iVictim] = false;
	g_fInfectedBot_CoveredInVomitTime[iVictim] = GetGameTime();
}

void Event_OnPlayerUse(Event hEvent, const char[] sName, bool bBroadcast)
{
	int iClient = GetClientOfUserId(hEvent.GetInt("userid"));
	int iEntity = hEvent.GetInt("targetid");

	int iUsedMask = (g_iItem_Used[iEntity] | 0);
	iUsedMask |= ( 1 << (iClient - 1) );
	g_iItem_Used[iEntity] = iUsedMask;
}

void Event_OnSurvivorGrabbed(Event hEvent, const char[] sName, bool bBroadcast)
{
	int iVictim = GetClientOfUserId(hEvent.GetInt("victim"));
	if (!IsValidClient(iVictim))return;
	
	for (int i = 1; i <= MaxClients; i++)
	{
		if (!IsClientSurvivor(i) || !IsFakeClient(i))continue;
		float fReactTime = GetRandomFloat(0.33, 0.66); if (i == iVictim)fReactTime *= 0.5;
		g_fSurvivorBot_PinnedReactTime[i] = GetGameTime() + fReactTime;
	}
}

void Event_OnChargeStart(Event hEvent, const char[] sName, bool bBroadcast)
{
	if (!g_bCvar_ChargerEvasion)return;

	int iCharger = GetClientOfUserId(hEvent.GetInt("userid"));
	if (!IsValidClient(iCharger))return;

	float fChargerForward[3]; GetClientAbsAngles(iCharger, fChargerForward);
	GetAngleVectors(fChargerForward, fChargerForward, NULL_VECTOR, NULL_VECTOR);
	NormalizeVector(fChargerForward, fChargerForward);

	int iMoveArea;
	float fChargeDist, fChargeHitDist;
	float fChargeHitPos[3], fClientRight[3], fMovePos[3];
	for (int i = 1; i <= MaxClients; i++)
	{
		if (!IsClientSurvivor(i) || !IsFakeClient(i) || !FEntityInViewAngle(iCharger, i, 5.0) || GetClientDistance(i, iCharger, true) <= 36864.0 || !IsVisibleEntity(iCharger, i, MASK_PLAYERSOLID))
			continue;

		MakeVectorFromPoints(g_fClientAbsOrigin[i], g_fClientAbsOrigin[iCharger], fClientRight);
		GetAngleVectors(fClientRight, NULL_VECTOR, fClientRight, NULL_VECTOR);
		NormalizeVector(fClientRight, fClientRight);

		fChargeDist = GetVectorDistance(g_fClientCenteroid[i], g_fClientCenteroid[iCharger]);
		fChargeHitPos[0] = g_fClientCenteroid[iCharger][0] + (fChargerForward[0] * fChargeDist);
		fChargeHitPos[1] = g_fClientCenteroid[iCharger][1] + (fChargerForward[1] * fChargeDist);
		fChargeHitPos[2] = g_fClientCenteroid[iCharger][2] + (fChargerForward[2] * fChargeDist);

		fChargeHitDist = GetVectorDistance(g_fClientCenteroid[i], fChargeHitPos);
		for (int k = 1; k <= 2; k++)
		{
			fMovePos[0] = g_fClientAbsOrigin[i][0] + (fClientRight[0] * ((k == 1 ? 256.0 : -256.0) - fChargeHitDist));
			fMovePos[1] = g_fClientAbsOrigin[i][1] + (fClientRight[1] * ((k == 1 ? 256.0 : -256.0) - fChargeHitDist));
			fMovePos[2] = g_fClientAbsOrigin[i][2] + (fClientRight[2] * ((k == 1 ? 256.0 : -256.0) - fChargeHitDist));

			iMoveArea = L4D_GetNearestNavArea(fMovePos);
			if (iMoveArea)
			{
				LBI_GetClosestPointOnNavArea(iMoveArea, fMovePos, fMovePos);
				if (!FVectorInViewAngle(iCharger, fMovePos, 5.0) && LBI_IsReachableNavArea(i, iMoveArea))
				{
					float fMoveDist = GetClientTravelDistance(i, fMovePos, true, BOT_CHARGE_EVADE_SEARCH_MAX);
					if (fMoveDist != -1.0 && fMoveDist <= 147456.0)
					{
						SetMoveToPosition(i, fMovePos, 3, "EvadeCharge");
						break;
					}
				}
			}

			if (k == 2)TakeCoverFromEntity(i, iCharger, 512.0);
		}
	}
}

void Event_OnWitchHaraserSet(Event hEvent, const char[] sName, bool bBroadcast)
{
	int iClient = GetClientOfUserId(hEvent.GetInt("userid"));
	if (!IsValidClient(iClient) || !IsPlayerAlive(iClient) || GetClientTeam(iClient) != 2)return;

	int iWitch = hEvent.GetInt("witchid");

	int iWitchRef;
	for (int i = 0; i < g_hWitchList.Length; i++)
	{
		iWitchRef = EntRefToEntIndex(g_hWitchList.Get(i));
		if (iWitchRef == INVALID_ENT_REFERENCE || !IsEntityExists(iWitchRef))
		{
			g_hWitchList.Erase(i--);
			continue;
		}
		if (iWitchRef == iWitch)
		{
			g_hWitchList.Set(i, hEvent.GetInt("userid"), 1);
			break;
		}
	}
}

public Action OnPlayerRunCmd(int iClient, int &iButtons, int &iImpulse, float fVel[3], float fAngles[3])
{
	// Only survivor bots read this data, and never for dead players or spectators.
	if (!HasAliveSurvivorBots() || !IsPlayerAlive(iClient))
		return Plugin_Continue;

	GetClientEyePosition(iClient, g_fClientEyePos[iClient]);
	g_fClientEyeAng[iClient] = fAngles;
	GetClientAbsOrigin(iClient, g_fClientAbsOrigin[iClient]);
	GetPlayerCenteroidFast(iClient, g_fClientAbsOrigin[iClient], g_fClientCenteroid[iClient]);
	g_iClientNavArea[iClient] = L4D_GetLastKnownArea(iClient);

	if (!IsClientSurvivor(iClient))
		return Plugin_Continue;

	g_bClient_IsFiringWeapon[iClient] = false;
	g_bClient_IsLookingAtPosition[iClient] = false;

	int iWpnSlot, iWpnSlots[6]; 
	for (int i = 0; i <= 5; i++)
	{
		iWpnSlot = GetPlayerWeaponSlot(iClient, i);
		iWpnSlots[i] = (!IsEntityWeaponCached(iWpnSlot) ? -1 : iWpnSlot);
	}
	g_iClientInventory[iClient] = iWpnSlots;

	// Only the item and weapon choices use these, and they are slow to read.
	if (GetGameTime() >= g_fClient_NextAmmoUpdateTime[iClient])
	{
		g_fClient_NextAmmoUpdateTime[iClient] = GetGameTime() + BOT_AMMO_UPDATE_INTERVAL;

		if (iWpnSlots[0] != -1)
		{
			g_iWeapon_Clip1[iWpnSlots[0]] = GetWeaponClip1(iWpnSlots[0]);
			g_iWeapon_MaxAmmo[iWpnSlots[0]] = GetWeaponMaxAmmo(iWpnSlots[0]);
			g_iWeapon_AmmoLeft[iWpnSlots[0]] = GetClientPrimaryAmmo(iClient);
		}
		if (iWpnSlots[1] != -1)
		{
			g_iWeapon_Clip1[iWpnSlots[1]] = GetWeaponClip1(iWpnSlots[1]);
		}
	}

	if (g_bCvar_BotsDisabled || g_bCutsceneIsPlaying || L4D_IsInIntro() != 0 || !g_iClientNavArea[iClient] || GetGameTime() <= g_fClient_ThinkFunctionDelay[iClient] || !IsFakeClient(iClient))
		return Plugin_Continue;

	SurvivorBotThink(iClient, iButtons, iWpnSlots);

	bool bPastDeadline = (GetGameTime() > g_fBotProcessing_NextProcessTime);
	int iAliveBots = (bPastDeadline ? GetTeamPlayerCount(2, true, true) : 0);
	// A bot that can't think, as one off the nav mesh, would keep the others from ever recomputing.
	bool bCycleStalled = (bPastDeadline && g_iBotProcessing_ProcessedCount < iAliveBots && GetGameTime() > (g_fBotProcessing_NextProcessTime + BOT_CYCLE_STALL_TIME));
	if (bCycleStalled && g_bCvar_Debug && GetGameTime() > g_fDebug_LastStallWarnTime)
	{
		g_fDebug_LastStallWarnTime = GetGameTime() + 1.0;

		char sStalledBots[256]; sStalledBots[0] = '\0';
		for (int i = 1; i <= MaxClients; i++)
		{
			if (IsClientInGame(i) && IsPlayerAlive(i) && GetClientTeam(i) == 2 && IsFakeClient(i) && !g_bBotProcessing_IsProcessed[i])
			{
				// Shows which check keeps this bot from thinking.
				char sReason[64];
				if (!g_iClientNavArea[i]) Format(sReason, sizeof(sReason), "navArea=0(stuck/off-mesh?)");
				else if (GetGameTime() <= g_fClient_ThinkFunctionDelay[i]) Format(sReason, sizeof(sReason), "thinkDelay=%.1f(now=%.1f)", g_fClient_ThinkFunctionDelay[i], GetGameTime());
				else Format(sReason, sizeof(sReason), "gateOpen,lastCheckpoint=%d", g_iDebug_LastCheckpoint[i]);

				Format(sStalledBots, sizeof(sStalledBots), "%s%d(%N)[%s] ", sStalledBots, i, i, sReason);
			}
		}
		DebugLog("CYCLE-STALLED processed=%d/%d notYetProcessed=%s", g_iBotProcessing_ProcessedCount, iAliveBots, sStalledBots);
	}

	if (bCycleStalled || bPastDeadline && g_iBotProcessing_ProcessedCount >= iAliveBots)
	{
		g_iBotProcessing_ProcessedCount = 0;

		// The bots recompute one per tick, and those ticks are part of the interval.
		float fNextProcess = (g_fCvar_NextProcessTime - (float(iAliveBots - 1) * GetTickInterval()));
		g_fBotProcessing_NextProcessTime = GetGameTime() + (fNextProcess > 0.0 ? fNextProcess : 0.0);
		for (int i = 1; i <= MaxClients; i++)g_bBotProcessing_IsProcessed[i] = false;
	}

	return Plugin_Continue;
}

bool HasAliveSurvivorBots()
{
	float fNow = GetGameTime();
	if (fNow >= g_fHasAliveSurvivorBots_NextCheck)
	{
		g_bHasAliveSurvivorBots = (GetTeamPlayerCount(2, true, true) > 0);
		g_fHasAliveSurvivorBots_NextCheck = fNow + 0.25;
	}
	return g_bHasAliveSurvivorBots;
}

void Event_IntroCutsceneBegin(Event event, const char[] name, bool dontBroadcast)
{
    g_bCutsceneIsPlaying = true;
}

void Event_IntroCutsceneEnd(Event event, const char[] name, bool dontBroadcast)
{
    g_bCutsceneIsPlaying = false;
}

int GetClientWeaponInventory(int iClient, int iSlot)
{
	// The cached weapon can be removed before the cache updates; callers treat -1 as an empty slot.
	int iWeapon = g_iClientInventory[iClient][iSlot];
	return ((iWeapon > 0 && IsValidEdict(iWeapon)) ? iWeapon : -1);
}

public void L4D_OnCThrowActivate_Post(int iAbility)
{
	int iOwner = GetEntityOwner(iAbility);
	if (IsValidClient(iOwner))g_bInfectedBot_IsThrowing[iOwner] = true;
}

public void L4D_TankRock_OnRelease_Post(int iTank, int iRock, const float fVecPos[3], const float fVecAng[3], const float fVecVel[3], const float fVecRot[3])
{
	if (IsValidClient(iTank))g_bInfectedBot_IsThrowing[iTank] = false;
}

public void L4D2_OnStagger_Post(int iTarget, int iSource)
{
	g_bInfectedBot_IsThrowing[iTarget] = false;
}

stock void VScript_DebugDrawLine(float fStartPos[3], float fEndPos[3], int iColorR = 255, int iColorG = 255, int iColorB = 255, bool bZTest = false, float fDrawTime = 1.0)
{
	static char sScriptCode[256]; FormatEx(sScriptCode, sizeof(sScriptCode), "DebugDrawLine(Vector(%f, %f, %f), Vector(%f, %f, %f), %i, %i, %i, %s, %f)",
		fStartPos[0], fStartPos[1], fStartPos[2], fEndPos[0], fEndPos[1], fEndPos[2], iColorR, iColorG, iColorB, (bZTest ? "true" : "false"), fDrawTime);
	L4D2_ExecVScriptCode(sScriptCode);
}

#include "l4d2_sb_ai_improver/think.inc"
#include "l4d2_sb_ai_improver/tank.inc"
#include "l4d2_sb_ai_improver/movement.inc"
#include "l4d2_sb_ai_improver/combat.inc"
#include "l4d2_sb_ai_improver/items.inc"
#include "l4d2_sb_ai_improver/entities.inc"
#include "l4d2_sb_ai_improver/util.inc"
#include "l4d2_sb_ai_improver/perception.inc"
#include "l4d2_sb_ai_improver/inventory.inc"
#include "l4d2_sb_ai_improver/navigation.inc"
#include "l4d2_sb_ai_improver/actions.inc"
