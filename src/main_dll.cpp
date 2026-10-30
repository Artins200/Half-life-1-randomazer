#include "hlsdk_mini.h"
#include "dll_bridge.h"
#include "chaos_events.h"
#include "death_counter.h"

#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define HL_EXPORT_C     extern "C" __declspec(dllexport)
#define HL_GIVEFNPTRS   extern "C" void __stdcall
#else
#define HL_EXPORT_C     extern "C" __attribute__((visibility("default")))
#define HL_GIVEFNPTRS   extern "C" __attribute__((visibility("default"))) void
#endif

enginefuncs_t g_engfuncs;
globalvars_t *gpGlobals = nullptr;
DLL_FUNCTIONS g_origDllFuncs;
qboolean      g_bHasOrigDllFuncs = FALSE;

static enginefuncs_t g_hookedEngfuncs;
#ifdef CHAOS_STANDALONE_SIMULATION
extern "C" const enginefuncs_t *ChaosTestHookedEngine() { return &g_hookedEngfuncs; }
#endif

// ============================================================================
// Engine callback hooks passed to the selected original Valve or Gearbox DLL
// ============================================================================

static int Hook_PrecacheSound(char *s) {
    if (!g_engfuncs.pfnPrecacheSound) return 0;
    int result = g_engfuncs.pfnPrecacheSound(s);
    if (result > 0) g_chaos.OnTrackPrecacheSound(s);
    return result;
}

static edict_t *Hook_CreateNamedEntity(int classname) {
    edict_t *entity = g_engfuncs.pfnCreateNamedEntity ? g_engfuncs.pfnCreateNamedEntity(classname) : nullptr;
    g_chaos.OnWorldEntityCreated(classname, entity);
    return entity;
}

static int Hook_RegUserMsg(const char *pszName, int iSize) {
    int msgId = g_engfuncs.pfnRegUserMsg ? g_engfuncs.pfnRegUserMsg(pszName, iSize) : 0;
    g_chaos.OnRegUserMsg(pszName, msgId, iSize);
    return msgId;
}

static uint32_t Hook_FunctionFromName(const char *pName) {
#ifndef CHAOS_STANDALONE_SIMULATION
    uint32_t resolved = Bridge_ResolveFunctionFromName(pName);
    if (resolved != 0) {
        return resolved;
    }
#endif
    if (g_engfuncs.pfnFunctionFromName) {
        return g_engfuncs.pfnFunctionFromName(pName);
    }
    return 0;
}

static const char *Hook_NameForFunction(uint32_t function) {
#ifndef CHAOS_STANDALONE_SIMULATION
    const char *name = Bridge_ResolveNameForFunction(function);
    if (name != nullptr) {
        return name;
    }
#endif
    if (g_engfuncs.pfnNameForFunction) {
        return g_engfuncs.pfnNameForFunction(function);
    }
    return nullptr;
}

// ============================================================================
// Game DLL table hooks returned to GoldSrc engine
// ============================================================================

static void Hook_GameInit(void) {
    if (g_origDllFuncs.pfnGameInit) {
        g_origDllFuncs.pfnGameInit();
    }
    g_chaos.RegisterCvarsAndCommands();
}

static int Hook_Spawn(edict_t *pent) {
    char classname[64] = {0};
    if (pent && gpGlobals && gpGlobals->pStringBase && pent->v.classname)
        snprintf(classname, sizeof(classname), "%s", STRING(pent->v.classname));
    const bool isWorld = strcmp(classname, "worldspawn") == 0;
    if (isWorld) {
        g_chaos.OnMapPrecacheReset();
        g_chaos.OnWorldPrecacheBegin();
    }
    int result = g_origDllFuncs.pfnSpawn ? g_origDllFuncs.pfnSpawn(pent) : -1;
    if (isWorld) g_chaos.OnWorldPrecacheEnd();
    else g_chaos.OnMapEntitySpawned(classname, pent, result);
    return result;
}

static void Hook_Save(edict_t *entity, SAVERESTOREDATA *data) {
    if (!g_origDllFuncs.pfnSave) return;
    ChaosManager::PlayerSaveSnapshot snapshot;
    bool restore = g_chaos.BeforePlayerSave(entity, snapshot);
    g_origDllFuncs.pfnSave(entity, data);
    if (restore) g_chaos.AfterPlayerSave(entity, snapshot);
}

static void Hook_ServerActivate(edict_t *pEdictList, int edictCount, int clientMax) {
    if (g_origDllFuncs.pfnServerActivate) {
        g_origDllFuncs.pfnServerActivate(pEdictList, edictCount, clientMax);
    }
    g_chaos.RegisterCvarsAndCommands();
    g_chaos.OnServerActivate(pEdictList, edictCount, clientMax);
}

static void Hook_ServerDeactivate(void) {
    g_chaos.OnServerDeactivate();
    if (g_origDllFuncs.pfnServerDeactivate) {
        g_origDllFuncs.pfnServerDeactivate();
    }
}

static void Hook_ClientPutInServer(edict_t *pEntity) {
    if (g_origDllFuncs.pfnClientPutInServer) {
        g_origDllFuncs.pfnClientPutInServer(pEntity);
    }
    g_chaos.OnClientPutInServer(pEntity);
}

static void Hook_ClientDisconnect(edict_t *pEntity) {
    // Stop our sends before the engine tears down this client's buffers.
    g_chaos.OnClientDisconnect(pEntity);
    if (g_origDllFuncs.pfnClientDisconnect) {
        g_origDllFuncs.pfnClientDisconnect(pEntity);
    }
}

static void Hook_ClientKill(edict_t *pEntity) {
    g_deathCounter.OnClientKill();
    if (g_origDllFuncs.pfnClientKill) {
        g_origDllFuncs.pfnClientKill(pEntity);
    }
}

static void Hook_ClientCommand(edict_t *pEntity) {
    if (g_chaos.OnClientCommand(pEntity)) {
        return;
    }
    if (g_origDllFuncs.pfnClientCommand) {
        g_origDllFuncs.pfnClientCommand(pEntity);
    }
}

static void Hook_StartFrame(void) {
    if (g_origDllFuncs.pfnStartFrame) {
        g_origDllFuncs.pfnStartFrame();
    }
    g_chaos.OnStartFrame();
}

static void Hook_PlayerMove(struct playermove_s *move, qboolean server) {
    if (!g_origDllFuncs.pfnPM_Move) return;
    ChaosManager::MovementOverride state;
    g_chaos.BeforePlayerMove(move, server, state);
    g_origDllFuncs.pfnPM_Move(move, server);
    g_chaos.AfterPlayerMove(move, state);
}

static void Hook_PlayerPreThink(edict_t *pEntity) {
    g_chaos.OnPlayerPreThink(pEntity);
    if (g_origDllFuncs.pfnPlayerPreThink) {
        g_origDllFuncs.pfnPlayerPreThink(pEntity);
    }
}

static void Hook_PlayerPostThink(edict_t *pEntity) {
    if (g_origDllFuncs.pfnPlayerPostThink) {
        g_origDllFuncs.pfnPlayerPostThink(pEntity);
    }
    g_chaos.OnPlayerPostThink(pEntity);
}

static const char *Hook_GetGameDescription(void) {
    return "HL1: Randomizer Chaos";
}

static void InstallDllHooks(DLL_FUNCTIONS *pFunctionTable) {
    memcpy(&g_origDllFuncs, pFunctionTable, sizeof(DLL_FUNCTIONS));
    g_bHasOrigDllFuncs = TRUE;

    pFunctionTable->pfnGameInit           = Hook_GameInit;
    pFunctionTable->pfnSpawn              = Hook_Spawn;
    pFunctionTable->pfnSave               = Hook_Save;
    pFunctionTable->pfnServerActivate     = Hook_ServerActivate;
    pFunctionTable->pfnServerDeactivate   = Hook_ServerDeactivate;
    pFunctionTable->pfnClientPutInServer  = Hook_ClientPutInServer;
    pFunctionTable->pfnClientDisconnect   = Hook_ClientDisconnect;
    pFunctionTable->pfnClientKill         = Hook_ClientKill;
    pFunctionTable->pfnClientCommand      = Hook_ClientCommand;
    pFunctionTable->pfnStartFrame         = Hook_StartFrame;
    pFunctionTable->pfnPlayerPreThink     = Hook_PlayerPreThink;
    pFunctionTable->pfnPlayerPostThink    = Hook_PlayerPostThink;
    if (g_origDllFuncs.pfnPM_Move) pFunctionTable->pfnPM_Move = Hook_PlayerMove;
    pFunctionTable->pfnGetGameDescription = Hook_GetGameDescription;
}

// ============================================================================
// Exported GoldSrc DLL Entry Points
// ============================================================================

HL_GIVEFNPTRS GiveFnptrsToDll(enginefuncs_t *pengfuncsFromEngine, globalvars_t *pGlobals) {
    memset(&g_engfuncs, 0, sizeof(g_engfuncs));
    if (pengfuncsFromEngine) {
        // Copy only the legacy prefix; newer engines may append callbacks.
        // Reading past an older table (even with IsBadReadPtr) is undefined.
        const size_t safeLegacySize = offsetof(enginefuncs_t, pfnPEntityOfEntIndexAllEntities);
        memcpy(&g_engfuncs, pengfuncsFromEngine, safeLegacySize);
        g_engfuncs.pfnPEntityOfEntIndexAllEntities = g_engfuncs.pfnPEntityOfEntIndex;
    }
    gpGlobals = pGlobals;

    memcpy(&g_hookedEngfuncs, &g_engfuncs, sizeof(enginefuncs_t));
    g_hookedEngfuncs.pfnCreateNamedEntity = Hook_CreateNamedEntity;
    g_hookedEngfuncs.pfnPrecacheSound    = Hook_PrecacheSound;
    g_hookedEngfuncs.pfnRegUserMsg       = Hook_RegUserMsg;
    g_hookedEngfuncs.pfnFunctionFromName = Hook_FunctionFromName;
    g_hookedEngfuncs.pfnNameForFunction  = Hook_NameForFunction;

#ifndef CHAOS_STANDALONE_SIMULATION
    if (Bridge_LoadOriginalGameDll()) {
        Bridge_ForwardGiveFnptrsToDll(&g_hookedEngfuncs, pGlobals);
    }
#endif
}

HL_EXPORT_C int GetEntityAPI(DLL_FUNCTIONS *pFunctionTable, int interfaceVersion) {
    if (!pFunctionTable || interfaceVersion != INTERFACE_VERSION) {
        return FALSE;
    }
#ifndef CHAOS_STANDALONE_SIMULATION
    if (!Bridge_LoadOriginalGameDll()) {
        return FALSE;
    }
    if (!Bridge_ForwardGetEntityAPI(pFunctionTable, interfaceVersion)) {
        return FALSE;
    }
#endif
    InstallDllHooks(pFunctionTable);
    return TRUE;
}

HL_EXPORT_C int GetEntityAPI2(DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion) {
    if (!pFunctionTable || !interfaceVersion) {
        return FALSE;
    }
    if (*interfaceVersion != INTERFACE_VERSION) {
        *interfaceVersion = INTERFACE_VERSION;
        return FALSE;
    }
#ifndef CHAOS_STANDALONE_SIMULATION
    if (!Bridge_LoadOriginalGameDll()) {
        return FALSE;
    }
    if (!Bridge_ForwardGetEntityAPI2(pFunctionTable, interfaceVersion)) {
        return FALSE;
    }
#endif
    InstallDllHooks(pFunctionTable);
    return TRUE;
}

HL_EXPORT_C int GetNewDLLFunctions(NEW_DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion) {
    if (!pFunctionTable || !interfaceVersion) {
        return FALSE;
    }
#ifndef CHAOS_STANDALONE_SIMULATION
    if (!Bridge_LoadOriginalGameDll()) {
        return FALSE;
    }
    return Bridge_ForwardGetNewDLLFunctions(pFunctionTable, interfaceVersion);
#else
    return FALSE;
#endif
}

HL_EXPORT_C int Server_GetBlendingInterface(int version, void **ppinterface, void *pstudio, float *rotationmatrix, float *bonetransform) {
#ifndef CHAOS_STANDALONE_SIMULATION
    if (!Bridge_LoadOriginalGameDll()) {
        return FALSE;
    }
    return Bridge_ForwardServerGetBlendingInterface(version, ppinterface, pstudio, rotationmatrix, bonetransform);
#else
    (void)version; (void)ppinterface; (void)pstudio; (void)rotationmatrix; (void)bonetransform;
    return FALSE;
#endif
}
