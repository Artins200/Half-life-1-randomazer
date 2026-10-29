#include "hlsdk_mini.h"
#include "chaos_events.h"
#include "death_counter.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <vector>
#include <string>
#include <map>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <unistd.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

extern "C" {
#ifdef _WIN32
void __stdcall GiveFnptrsToDll(enginefuncs_t *pengfuncsFromEngine, globalvars_t *pGlobals);
#else
void GiveFnptrsToDll(enginefuncs_t *pengfuncsFromEngine, globalvars_t *pGlobals);
#endif
int GetEntityAPI2(DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion);
}

// ============================================================================
// Mock GoldSrc Engine State
// ============================================================================

static char          g_stringPool[65536];
static int           g_stringPoolUsed = 1; // offset 0 = empty string
static edict_t       g_edicts[128];
static int           g_numEdicts = 0;
static globalvars_t  g_mockGlobals;
static enginefuncs_t g_mockEngfuncs;
static char          g_testGameDir[256];

static std::map<std::string, cvar_t *>        g_cvars;
static std::map<std::string, void (*)(void)>  g_srvCmds;
static std::vector<std::string>               g_cmdArgs;

struct RecordedNetMsg {
    int dest;
    int type;
    int byteCount;
    int firstByte;
    std::vector<std::string> strings;
};

static bool                        g_inMessage = false;
static RecordedNetMsg              g_curMsg;
static std::vector<RecordedNetMsg> g_allMessages;
static int                         g_msgIdShake = 0;
static int                         g_msgIdFade  = 0;
static int                         g_msgIdFov   = 0;
static int                         g_msgIdSay   = 0;

static int Mock_AllocString(const char *str) {
    if (!str) return 0;
    int cur = 1;
    while (cur < g_stringPoolUsed) {
        if (strcmp(&g_stringPool[cur], str) == 0) {
            return cur;
        }
        cur += (int)strlen(&g_stringPool[cur]) + 1;
    }
    int len = (int)strlen(str) + 1;
    assert(g_stringPoolUsed + len < (int)sizeof(g_stringPool));
    int off = g_stringPoolUsed;
    memcpy(&g_stringPool[off], str, len);
    g_stringPoolUsed += len;
    return off;
}

static int Mock_PrecacheModel(char *s) { (void)s; return 1; }
static int Mock_PrecacheSound(char *s) { (void)s; return 1; }
static void Mock_SetModel(edict_t *e, const char *m) {
    if (e && m) e->v.model = Mock_AllocString(m);
}
static void Mock_SetSize(edict_t *e, const float *rgflMin, const float *rgflMax) {
    if (!e) return;
    e->v.mins = Vec3(rgflMin[0], rgflMin[1], rgflMin[2]);
    e->v.maxs = Vec3(rgflMax[0], rgflMax[1], rgflMax[2]);
}
static void Mock_SetOrigin(edict_t *e, const float *rgflOrigin) {
    if (!e) return;
    e->v.origin = Vec3(rgflOrigin[0], rgflOrigin[1], rgflOrigin[2]);
}
static void Mock_EmitSound(edict_t *entity, int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch) {
    (void)entity; (void)channel; (void)sample; (void)volume; (void)attenuation; (void)fFlags; (void)pitch;
}

static void Mock_TraceLine(const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr) {
    (void)fNoMonsters; (void)pentToSkip;
    memset((void *)ptr, 0, sizeof(*ptr));
    ptr->flFraction = 1.0f;
    ptr->vecEndPos = Vec3(v2[0], v2[1], v2[2]);
    (void)v1;
}

static void Mock_TraceHull(const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr) {
    (void)fNoMonsters; (void)hullNumber; (void)pentToSkip;
    memset((void *)ptr, 0, sizeof(*ptr));
    // Simulate floor at z = 0
    if (v2[2] < 0.0f && v1[2] >= 0.0f) {
        ptr->flFraction = v1[2] / (v1[2] - v2[2]);
        ptr->vecEndPos = Vec3(v1[0], v1[1], 0.0f);
    } else {
        ptr->flFraction = 1.0f;
        ptr->vecEndPos = Vec3(v2[0], v2[1], v2[2]);
    }
}

static int Mock_DropToFloor(edict_t *e) {
    if (e && e->v.origin.z > 0.0f) e->v.origin.z = 0.0f;
    return 1;
}

static int Mock_PointContents(const float *rgflVector) {
    (void)rgflVector;
    return CONTENTS_EMPTY;
}

static edict_t *Mock_CreateNamedEntity(int className) {
    const char *cls = &g_stringPool[className];
    for (int i = 2; i < 128; ++i) {
        if (g_edicts[i].free) {
            memset((void *)&g_edicts[i], 0, sizeof(edict_t));
            g_edicts[i].free = FALSE;
            g_edicts[i].pvPrivateData = (void *)(uintptr_t)0xDEADBEEF;
            g_edicts[i].v.pContainingEntity = &g_edicts[i];
            g_edicts[i].v.classname = className;
            if (strncmp(cls, "monster_", 8) == 0) {
                g_edicts[i].v.flags = FL_MONSTER;
                g_edicts[i].v.health = 60.0f;
                g_edicts[i].v.takedamage = DAMAGE_YES;
                g_edicts[i].v.solid = SOLID_SLIDEBOX;
            } else if (strncmp(cls, "weapon_", 7) == 0 || strncmp(cls, "ammo_", 5) == 0 || strncmp(cls, "item_", 5) == 0) {
                g_edicts[i].v.solid = SOLID_TRIGGER;
            }
            if (i >= g_numEdicts) g_numEdicts = i + 1;
            return &g_edicts[i];
        }
    }
    return nullptr;
}

static void Mock_RemoveEntity(edict_t *e) {
    if (!e) return;
    int idx = (int)(e - g_edicts);
    if (idx >= 2 && idx < 128) {
        memset((void *)e, 0, sizeof(*e));
        e->free = TRUE;
    }
}

static int Mock_NumberOfEntities(void) {
    int count = 0;
    for (int i = 0; i < g_numEdicts; ++i) {
        if (!g_edicts[i].free) count++;
    }
    return count;
}

static int Mock_EntOffsetOfPEntity(const edict_t *pEdict) {
    if (!pEdict) return 0;
    return (int)((const char *)pEdict - (const char *)g_edicts);
}

static edict_t *Mock_PEntityOfEntIndex(int iEntIndex) {
    if (iEntIndex < 0 || iEntIndex >= g_numEdicts) return nullptr;
    if (g_edicts[iEntIndex].free) return nullptr;
    return &g_edicts[iEntIndex];
}

static int Mock_IndexOfEdict(const edict_t *pEdict) {
    if (!pEdict) return 0;
    return (int)(pEdict - g_edicts);
}

static edict_t *Mock_FindEntityByString(edict_t *pStart, const char *pszField, const char *pszValue) {
    int startIdx = pStart ? ((int)(pStart - g_edicts) + 1) : 1;
    for (int i = startIdx; i < g_numEdicts; ++i) {
        if (g_edicts[i].free) continue;
        if (strcmp(pszField, "classname") == 0) {
            const char *cls = &g_stringPool[g_edicts[i].v.classname];
            if (strcmp(cls, pszValue) == 0) {
                return &g_edicts[i];
            }
        }
    }
    return nullptr;
}

static void Mock_ServerPrint(const char *szMsg) { (void)szMsg; }
static void Mock_ClientPrintf(edict_t *pEdict, PRINT_TYPE ptype, const char *szMsg) {
    (void)pEdict; (void)ptype; (void)szMsg;
}
static void Mock_ServerCommand(char *str) { (void)str; }
static void Mock_ServerExecute(void) {}
static void Mock_ClientCommand(edict_t *pEdict, char *szFmt, ...) { (void)pEdict; (void)szFmt; }

static void Mock_MessageBegin(int msg_dest, int msg_type, const float *pOrigin, edict_t *ed) {
    (void)pOrigin; (void)ed;
    assert(!g_inMessage && "Nested MessageBegin without MessageEnd!");
    g_inMessage = true;
    g_curMsg = RecordedNetMsg();
    g_curMsg.dest = msg_dest;
    g_curMsg.type = msg_type;
    g_curMsg.byteCount = 0;
    g_curMsg.firstByte = -1;
}

static void Mock_MessageEnd(void) {
    assert(g_inMessage && "MessageEnd without MessageBegin!");
    g_inMessage = false;

    // Validate exact GoldSrc network wire format!
    if (g_curMsg.type == SVC_TEMPENTITY) {
        if (g_curMsg.firstByte == TE_TEXTMESSAGE) {
            assert(g_curMsg.strings.size() == 1);
            int expectedBytes = 21 + (int)g_curMsg.strings[0].size() + 1;
            assert(g_curMsg.byteCount == expectedBytes && "TE_TEXTMESSAGE wire byte count mismatch!");
            // Verify ASCII-only on TE_TEXTMESSAGE (GoldSrc HUD sprites lack UTF-8)
            for (unsigned char ch : g_curMsg.strings[0]) {
                assert(ch < 128 && "Non-ASCII byte sent to TE_TEXTMESSAGE!");
            }
        } else if (g_curMsg.firstByte == TE_DLIGHT) {
            assert(g_curMsg.byteCount == 13 && "TE_DLIGHT must be exactly 13 bytes!");
        }
    } else if (g_curMsg.type == g_msgIdShake) {
        assert(g_curMsg.byteCount == 6 && "ScreenShake must be 6 bytes!");
    } else if (g_curMsg.type == g_msgIdFade) {
        assert(g_curMsg.byteCount == 10 && "ScreenFade must be 10 bytes!");
    } else if (g_curMsg.type == g_msgIdFov) {
        assert(g_curMsg.byteCount == 1 && "SetFOV must be 1 byte!");
    }

    g_allMessages.push_back(g_curMsg);
}

static void Mock_WriteByte(int iValue) {
    assert(g_inMessage);
    if (g_curMsg.byteCount == 0) g_curMsg.firstByte = (iValue & 0xFF);
    g_curMsg.byteCount += 1;
}
static void Mock_WriteChar(int iValue) { (void)iValue; assert(g_inMessage); g_curMsg.byteCount += 1; }
static void Mock_WriteShort(int iValue) { (void)iValue; assert(g_inMessage); g_curMsg.byteCount += 2; }
static void Mock_WriteLong(int iValue) { (void)iValue; assert(g_inMessage); g_curMsg.byteCount += 4; }
static void Mock_WriteAngle(float flValue) { (void)flValue; assert(g_inMessage); g_curMsg.byteCount += 1; }
static void Mock_WriteCoord(float flValue) { (void)flValue; assert(g_inMessage); g_curMsg.byteCount += 2; }
static void Mock_WriteString(const char *sz) {
    assert(g_inMessage);
    std::string s = sz ? sz : "";
    g_curMsg.strings.push_back(s);
    g_curMsg.byteCount += (int)s.size() + 1;
}
static void Mock_WriteEntity(int iValue) { (void)iValue; assert(g_inMessage); g_curMsg.byteCount += 2; }

static void Mock_CVarRegister(cvar_t *pCvar) {
    if (!pCvar || !pCvar->name) return;
    pCvar->value = (float)atof(pCvar->string ? pCvar->string : "0");
    g_cvars[pCvar->name] = pCvar;
}
static float Mock_CVarGetFloat(const char *szVarName) {
    auto it = g_cvars.find(szVarName);
    return (it != g_cvars.end()) ? it->second->value : 0.0f;
}
static const char *Mock_CVarGetString(const char *szVarName) {
    auto it = g_cvars.find(szVarName);
    return (it != g_cvars.end()) ? it->second->string : "";
}
static void Mock_CVarSetFloat(const char *szVarName, float flValue) {
    auto it = g_cvars.find(szVarName);
    if (it != g_cvars.end()) {
        it->second->value = flValue;
    }
}
static void Mock_CVarSetString(const char *szVarName, const char *szValue) {
    auto it = g_cvars.find(szVarName);
    if (it != g_cvars.end()) {
        it->second->value = (float)atof(szValue);
    }
}
static cvar_t *Mock_CVarGetPointer(const char *szVarName) {
    auto it = g_cvars.find(szVarName);
    return (it != g_cvars.end()) ? it->second : nullptr;
}

static void Mock_AddServerCommand(char *cmd_name, void (*function)(void)) {
    if (cmd_name && function) {
        g_srvCmds[cmd_name] = function;
    }
}

static const char *Mock_Cmd_Argv(int argc) {
    if (argc >= 0 && argc < (int)g_cmdArgs.size()) {
        return g_cmdArgs[argc].c_str();
    }
    return "";
}
static int Mock_Cmd_Argc(void) {
    return (int)g_cmdArgs.size();
}

static void Mock_MakeVectors(const float *rgflVector) {
    float pitch = rgflVector[0] * (3.14159265f / 180.0f);
    float yaw   = rgflVector[1] * (3.14159265f / 180.0f);
    g_mockGlobals.v_forward = Vec3(cosf(yaw) * cosf(pitch), sinf(yaw) * cosf(pitch), -sinf(pitch));
    g_mockGlobals.v_right   = Vec3(sinf(yaw), -cosf(yaw), 0.0f);
    g_mockGlobals.v_up      = Vec3(0.0f, 0.0f, 1.0f);
}

static int Mock_RegUserMsg(const char *pszName, int iSize) {
    (void)iSize;
    static int nextId = 64;
    int id = ++nextId;
    g_chaos.OnRegUserMsg(pszName, id);
    return id;
}

static void Mock_GetGameDir(char *szGetGameDir) {
    strcpy(szGetGameDir, g_testGameDir);
}

static int32_t Mock_RandomLong(int32_t lLow, int32_t lHigh) {
    if (lHigh <= lLow) return lLow;
    return lLow + (rand() % (lHigh - lLow + 1));
}
static float Mock_RandomFloat(float flLow, float flHigh) {
    if (flHigh <= flLow) return flLow;
    float t = (float)rand() / (float)RAND_MAX;
    return flLow + t * (flHigh - flLow);
}

// Original game DLL mock callbacks
static void Orig_GameInit(void) {
    static cvar_t sv_grav = {"sv_gravity", "800", 0, 800.0f, nullptr};
    static cvar_t sv_maxspd = {"sv_maxspeed", "320", 0, 320.0f, nullptr};
    Mock_CVarRegister(&sv_grav);
    Mock_CVarRegister(&sv_maxspd);
    g_msgIdShake = Mock_RegUserMsg("ScreenShake", 6);
    g_msgIdFade  = Mock_RegUserMsg("ScreenFade", 10);
    g_msgIdFov   = Mock_RegUserMsg("SetFOV", 1);
    g_msgIdSay   = Mock_RegUserMsg("SayText", -1);
}

static int Orig_Spawn(edict_t *pent) {
    if (!pent) return -1;
    // Simulate weapon/item pickup touching player immediately if spawned at player origin
    const char *cls = &g_stringPool[pent->v.classname];
    if (strncmp(cls, "weapon_", 7) == 0 || strncmp(cls, "ammo_", 5) == 0 || strncmp(cls, "item_", 5) == 0) {
        pent->v.solid = SOLID_NOT;
        pent->v.owner = &g_edicts[1];
    }
    return 0;
}

static void Orig_Touch(edict_t *pentTouched, edict_t *pentOther) {
    if (!pentTouched || !pentOther) return;
    const char *cls = &g_stringPool[pentTouched->v.classname];
    if (strcmp(cls, "player_weaponstrip") == 0 && pentOther == &g_edicts[1]) {
        pentOther->v.weapons = (1 << 31); // Keep WEAPON_SUIT only
    }
}

static void ResetMockWorld(const char *mapName, float initialTime, bool hasSuit) {
    memset((void *)g_edicts, 0, sizeof(g_edicts));
    for (int i = 0; i < 128; ++i) g_edicts[i].free = TRUE;

    // Edict 0: worldspawn
    g_edicts[0].free = FALSE;
    g_edicts[0].pvPrivateData = (void *)(uintptr_t)0x1111;
    g_edicts[0].v.pContainingEntity = &g_edicts[0];
    g_edicts[0].v.classname = Mock_AllocString("worldspawn");

    // Edict 1: Gordon Freeman (player)
    g_edicts[1].free = FALSE;
    g_edicts[1].pvPrivateData = (void *)(uintptr_t)0x2222;
    g_edicts[1].v.pContainingEntity = &g_edicts[1];
    g_edicts[1].v.classname = Mock_AllocString("player");
    g_edicts[1].v.origin = Vec3(100.0f, 200.0f, 36.0f);
    g_edicts[1].v.health = 100.0f;
    g_edicts[1].v.max_health = 100.0f;
    g_edicts[1].v.armorvalue = 15.0f;
    g_edicts[1].v.gravity = 1.0f;
    g_edicts[1].v.friction = 1.0f;
    g_edicts[1].v.maxspeed = 320.0f;
    g_edicts[1].v.fov = 0.0f; // 0 = default_fov in GoldSrc CBasePlayer
    g_edicts[1].v.flags = FL_CLIENT | FL_ONGROUND;
    g_edicts[1].v.deadflag = DEAD_NO;
    g_edicts[1].v.weapons = hasSuit ? ((1 << 31) | (1 << 1) | (1 << 2)) : 0;

    // Edict 2: Nearby non-scripted bullsquid for monster events
    g_edicts[2].free = FALSE;
    g_edicts[2].pvPrivateData = (void *)(uintptr_t)0x3333;
    g_edicts[2].v.pContainingEntity = &g_edicts[2];
    g_edicts[2].v.classname = Mock_AllocString("monster_bullsquid");
    g_edicts[2].v.origin = Vec3(320.0f, 220.0f, 36.0f);
    g_edicts[2].v.health = 40.0f;
    g_edicts[2].v.flags = FL_MONSTER;
    g_edicts[2].v.takedamage = DAMAGE_YES;
    g_edicts[2].v.solid = SOLID_SLIDEBOX;

    // Edict 3: Scripted story scientist (has targetname -> MUST NOT belaunched/mutated!)
    g_edicts[3].free = FALSE;
    g_edicts[3].pvPrivateData = (void *)(uintptr_t)0x4444;
    g_edicts[3].v.pContainingEntity = &g_edicts[3];
    g_edicts[3].v.classname = Mock_AllocString("monster_scientist");
    g_edicts[3].v.targetname = Mock_AllocString("dr_kleiner_door_opener");
    g_edicts[3].v.origin = Vec3(180.0f, 200.0f, 36.0f);
    g_edicts[3].v.health = 20.0f;
    g_edicts[3].v.flags = FL_MONSTER;
    g_edicts[3].v.takedamage = DAMAGE_YES;
    g_edicts[3].v.solid = SOLID_SLIDEBOX;

    g_numEdicts = 4;

    g_mockGlobals.time = initialTime;
    g_mockGlobals.frametime = 0.05f;
    g_mockGlobals.maxClients = 1;
    g_mockGlobals.maxEntities = 128;
    g_mockGlobals.pStringBase = g_stringPool;
    g_mockGlobals.mapname = Mock_AllocString(mapName);
}

int main() {
    srand(19981119); // Half-Life 1 release date seed!

#ifdef _WIN32
    const char *tmpBase = getenv("TEMP");
    snprintf(g_testGameDir, sizeof(g_testGameDir), "%s\\hl_chaos_sim_dir", (tmpBase && tmpBase[0]) ? tmpBase : ".");
#else
    snprintf(g_testGameDir, sizeof(g_testGameDir), "/tmp/hl_chaos_sim_dir");
#endif
    MKDIR(g_testGameDir);
    char statsPath[512], deathsTxtPath[512];
    snprintf(statsPath, sizeof(statsPath), "%s/chaos_stats.cfg", g_testGameDir);
    snprintf(deathsTxtPath, sizeof(deathsTxtPath), "%s/deaths.txt", g_testGameDir);
    remove(statsPath);
    remove(deathsTxtPath);

    memset(&g_mockEngfuncs, 0, sizeof(g_mockEngfuncs));
    g_mockEngfuncs.pfnPrecacheModel       = Mock_PrecacheModel;
    g_mockEngfuncs.pfnPrecacheSound       = Mock_PrecacheSound;
    g_mockEngfuncs.pfnSetModel            = Mock_SetModel;
    g_mockEngfuncs.pfnSetSize             = Mock_SetSize;
    g_mockEngfuncs.pfnSetOrigin           = Mock_SetOrigin;
    g_mockEngfuncs.pfnEmitSound           = Mock_EmitSound;
    g_mockEngfuncs.pfnTraceLine           = Mock_TraceLine;
    g_mockEngfuncs.pfnTraceHull           = Mock_TraceHull;
    g_mockEngfuncs.pfnDropToFloor         = Mock_DropToFloor;
    g_mockEngfuncs.pfnPointContents       = Mock_PointContents;
    g_mockEngfuncs.pfnCreateNamedEntity   = Mock_CreateNamedEntity;
    g_mockEngfuncs.pfnRemoveEntity        = Mock_RemoveEntity;
    g_mockEngfuncs.pfnNumberOfEntities    = Mock_NumberOfEntities;
    g_mockEngfuncs.pfnEntOffsetOfPEntity  = Mock_EntOffsetOfPEntity;
    g_mockEngfuncs.pfnPEntityOfEntIndex   = Mock_PEntityOfEntIndex;
    g_mockEngfuncs.pfnIndexOfEdict        = Mock_IndexOfEdict;
    g_mockEngfuncs.pfnFindEntityByString  = Mock_FindEntityByString;
    g_mockEngfuncs.pfnServerPrint         = Mock_ServerPrint;
    g_mockEngfuncs.pfnClientPrintf        = Mock_ClientPrintf;
    g_mockEngfuncs.pfnServerCommand       = Mock_ServerCommand;
    g_mockEngfuncs.pfnServerExecute       = Mock_ServerExecute;
    g_mockEngfuncs.pfnClientCommand       = Mock_ClientCommand;
    g_mockEngfuncs.pfnMessageBegin        = Mock_MessageBegin;
    g_mockEngfuncs.pfnMessageEnd          = Mock_MessageEnd;
    g_mockEngfuncs.pfnWriteByte           = Mock_WriteByte;
    g_mockEngfuncs.pfnWriteChar           = Mock_WriteChar;
    g_mockEngfuncs.pfnWriteShort          = Mock_WriteShort;
    g_mockEngfuncs.pfnWriteLong           = Mock_WriteLong;
    g_mockEngfuncs.pfnWriteAngle          = Mock_WriteAngle;
    g_mockEngfuncs.pfnWriteCoord          = Mock_WriteCoord;
    g_mockEngfuncs.pfnWriteString         = Mock_WriteString;
    g_mockEngfuncs.pfnWriteEntity         = Mock_WriteEntity;
    g_mockEngfuncs.pfnCVarRegister        = Mock_CVarRegister;
    g_mockEngfuncs.pfnCVarGetFloat        = Mock_CVarGetFloat;
    g_mockEngfuncs.pfnCVarGetString       = Mock_CVarGetString;
    g_mockEngfuncs.pfnCVarSetFloat        = Mock_CVarSetFloat;
    g_mockEngfuncs.pfnCVarSetString       = Mock_CVarSetString;
    g_mockEngfuncs.pfnCVarGetPointer      = Mock_CVarGetPointer;
    g_mockEngfuncs.pfnAddServerCommand    = Mock_AddServerCommand;
    g_mockEngfuncs.pfnCmd_Argv            = Mock_Cmd_Argv;
    g_mockEngfuncs.pfnCmd_Argc            = Mock_Cmd_Argc;
    g_mockEngfuncs.pfnMakeVectors         = Mock_MakeVectors;
    g_mockEngfuncs.pfnRegUserMsg          = Mock_RegUserMsg;
    g_mockEngfuncs.pfnGetGameDir          = Mock_GetGameDir;
    g_mockEngfuncs.pfnRandomLong          = Mock_RandomLong;
    g_mockEngfuncs.pfnRandomFloat         = Mock_RandomFloat;
    g_mockEngfuncs.pfnAllocString         = Mock_AllocString;

    ResetMockWorld("c1a2", 1.0f, true);

    GiveFnptrsToDll(&g_mockEngfuncs, &g_mockGlobals);

    DLL_FUNCTIONS dllTable;
    memset(&dllTable, 0, sizeof(dllTable));
    dllTable.pfnGameInit = Orig_GameInit;
    dllTable.pfnSpawn    = Orig_Spawn;
    dllTable.pfnTouch    = Orig_Touch;

    int ifaceVer = INTERFACE_VERSION;
    int ok = GetEntityAPI2(&dllTable, &ifaceVer);
    assert(ok == TRUE);

    dllTable.pfnGameInit();
    dllTable.pfnSpawn(&g_edicts[0]); // worldspawn -> triggers precache!
    dllTable.pfnServerActivate(g_edicts, g_numEdicts, 1);
    dllTable.pfnClientPutInServer(&g_edicts[1]);

    printf("[TEST 1] Verifying 38 Chaos events registered (CHAOS_EVENT_COUNT >= 30)...\n");
    assert(g_chaos.GetEventCount() == 38);
    for (int i = 0; i < g_chaos.GetEventCount(); ++i) {
        const ChaosEventDef *ev = g_chaos.GetEventDef(i);
        assert(ev != nullptr);
        assert(ev->id == i);
        assert(ev->titleEn != nullptr && strlen(ev->titleEn) > 0);
        assert(ev->titleRu != nullptr && strlen(ev->titleRu) > 0);
        assert(ev->descRu  != nullptr && strlen(ev->descRu)  > 0);
    }

    printf("[TEST 2] Verifying 10-second automatic Chaos timer & HUD updates...\n");
    int eventsBefore = g_deathCounter.GetTotalEvents();
    for (int step = 0; step < 650; ++step) { // 650 * 0.05s = 32.5 seconds -> 3 automatic events!
        g_mockGlobals.time += 0.05f;
        dllTable.pfnPlayerPreThink(&g_edicts[1]);
        dllTable.pfnStartFrame();
        dllTable.pfnPlayerPostThink(&g_edicts[1]);
    }
    int firedAuto = g_deathCounter.GetTotalEvents() - eventsBefore;
    printf("         Auto-triggered events in 32.5s: %d (expected 3)\n", firedAuto);
    assert(firedAuto == 3);

    // Check that HUD messages with DEATHS and NEXT were emitted
    bool foundHudPanel = false;
    for (const auto &m : g_allMessages) {
        if (m.type == SVC_TEMPENTITY && m.firstByte == TE_TEXTMESSAGE && !m.strings.empty()) {
            if (m.strings[0].find("DEATHS:") != std::string::npos &&
                m.strings[0].find("NEXT:") != std::string::npos) {
                foundHudPanel = true;
                break;
            }
        }
    }
    assert(foundHudPanel && "HUD panel with DEATHS and NEXT timer must be rendered!");

    printf("[TEST 3] Stress-testing ALL 38 Chaos events for non-lethality & state cleanup...\n");
    Mock_CVarSetFloat("chaos_enabled", 0.0f); // Isolate manual event triggers from auto-timer
    ResetMockWorld("c1a2", 100.0f, true);
    dllTable.pfnSpawn(&g_edicts[0]);
    dllTable.pfnServerActivate(g_edicts, g_numEdicts, 1);
    dllTable.pfnClientPutInServer(&g_edicts[1]);

    // Record breadcrumbs first
    for (int b = 0; b < 40; ++b) {
        g_mockGlobals.time += 0.2f;
        g_edicts[1].v.origin.x += 5.0f;
        dllTable.pfnStartFrame();
    }

    const Vec3 scriptedSciPos = g_edicts[3].v.origin;

    for (int evId = 0; evId < g_chaos.GetEventCount(); ++evId) {
        // Test even at low starting HP (25 HP) to prove ZERO instant-death / one-shot traps!
        g_edicts[1].v.health = 25.0f;
        g_edicts[1].v.deadflag = DEAD_NO;

        g_chaos.CmdTriggerById(evId);
        assert(g_chaos.GetLastEventId() == evId);

        // Player must NEVER die directly from an event!
        assert(g_edicts[1].v.health >= 10.0f && "Chaos event dropped low-HP player below safe threshold!");
        assert(g_edicts[1].v.deadflag == DEAD_NO);

        // Scripted NPC (targetname != 0) must NEVER have been moved or killed!
        assert(fabsf(g_edicts[3].v.origin.x - scriptedSciPos.x) < 0.01f);
        assert(fabsf(g_edicts[3].v.origin.y - scriptedSciPos.y) < 0.01f);
        assert(g_edicts[3].v.health == 20.0f);

        // Advance 20 seconds so all timed modifiers expire cleanly
        for (int step = 0; step < 100; ++step) {
            g_mockGlobals.time += 0.2f;
            dllTable.pfnPlayerPreThink(&g_edicts[1]);
            dllTable.pfnStartFrame();
            dllTable.pfnPlayerPostThink(&g_edicts[1]);
        }

        // Verify physics/camera restored after timed modifiers expire (fov == 0 means default_fov in HL1)
        assert(fabsf(Mock_CVarGetFloat("sv_gravity") - 800.0f) < 1.0f);
        assert(fabsf(g_edicts[1].v.fov) < 0.1f);
    }

    Mock_CVarSetFloat("chaos_enabled", 1.0f);
    g_deathCounter.ResetDeaths();
    printf("[TEST 4] Verifying persistent Death Counter across Save Reload (Quickload/Autosave)...\n");
    assert(g_deathCounter.GetTotalDeaths() == 0);

    // Death #1: killed by monster at t = 950.0s
    g_edicts[1].v.health = -12.0f;
    g_edicts[1].v.deadflag = DEAD_DYING;
    g_mockGlobals.time += 0.1f;
    dllTable.pfnStartFrame();
    assert(g_deathCounter.GetTotalDeaths() == 1);
    assert(g_deathCounter.GetMapDeaths() == 1);

    // Simulate GoldSrc Quickload / Autosave reload:
    // ServerDeactivate -> ServerActivate with OLDER gpGlobals->time (e.g. t = 42.0s) and alive player!
    dllTable.pfnServerDeactivate();
    ResetMockWorld("c1a2", 42.0f, true);
    dllTable.pfnSpawn(&g_edicts[0]);
    dllTable.pfnServerActivate(g_edicts, g_numEdicts, 1);
    dllTable.pfnClientPutInServer(&g_edicts[1]);

    // Death count MUST STILL BE 1 after reloading save!
    assert(g_deathCounter.GetTotalDeaths() == 1 && "Death counter must NOT reset on save reload!");

    // Verify timer still runs properly even though gpGlobals->time jumped backwards to 42.0s!
    int evBeforeReloadTick = g_deathCounter.GetTotalEvents();
    for (int step = 0; step < 250; ++step) { // 12.5 seconds
        g_mockGlobals.time += 0.05f;
        dllTable.pfnStartFrame();
    }
    assert(g_deathCounter.GetTotalEvents() == evBeforeReloadTick + 1 && "Timer must recover after backward save-load time jump!");

    // Death #2 after reload -> Total Deaths MUST become 2!
    g_edicts[1].v.health = 0.0f;
    g_edicts[1].v.deadflag = DEAD_DEAD;
    g_mockGlobals.time += 0.1f;
    dllTable.pfnStartFrame();
    assert(g_deathCounter.GetTotalDeaths() == 2);

    // Respawn / reload again, then test 'kill' console command (ClientKill) -> Total Deaths = 3 without double count!
    g_edicts[1].v.health = 100.0f;
    g_edicts[1].v.deadflag = DEAD_NO;
    g_mockGlobals.time += 0.1f;
    dllTable.pfnStartFrame();

    dllTable.pfnClientKill(&g_edicts[1]);
    g_edicts[1].v.health = -1.0f;
    g_edicts[1].v.deadflag = DEAD_DYING;
    g_mockGlobals.time += 0.1f;
    dllTable.pfnStartFrame();
    assert(g_deathCounter.GetTotalDeaths() == 3 && "ClientKill must count once without double counting!");

    // Verify chaos_stats.cfg and deaths.txt exist on disk and persist across full game restart!
    DeathCounter freshInstanceCheck;
    freshInstanceCheck.Init();
    freshInstanceCheck.OnMapStart("c2a1");
    assert(freshInstanceCheck.GetTotalDeaths() == 3 && "Deaths must persist across full hl.exe restart!");

    // Test chaos_reset_deaths client command
    g_cmdArgs = {"chaos_reset_deaths"};
    dllTable.pfnClientCommand(&g_edicts[1]);
    assert(g_deathCounter.GetTotalDeaths() == 0);

    printf("[TEST 5] Verifying pre-HEV suit (c1a0) softlock protection...\n");
    ResetMockWorld("c1a0", 10.0f, false); // No HEV suit!
    dllTable.pfnSpawn(&g_edicts[0]);
    dllTable.pfnServerActivate(g_edicts, g_numEdicts, 1);
    dllTable.pfnClientPutInServer(&g_edicts[1]);
    g_chaos.CmdTriggerById(22); // RANDOM WEAPON GIFT! -> must not grant guns when suitless!
    g_chaos.CmdTriggerById(23); // FULL ARSENAL JACKPOT! -> must not grant guns when suitless!
    assert(g_edicts[1].v.weapons == 0 && "Must not grant weapons before player puts on HEV Suit in c1a0!");

    printf("\nALL 5 GOLDSRC CHAOS SIMULATION SUITES PASSED SUCCESSFULLY!\n");
    return 0;
}
