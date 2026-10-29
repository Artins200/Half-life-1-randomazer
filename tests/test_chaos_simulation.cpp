#include "hlsdk_mini.h"
#include "chaos_events.h"
#include "chaos_features.h"
#include "death_counter.h"

// Assertions are part of the test even in Release builds.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define MKDIR(p) _mkdir(p)
#define RMDIR(p) _rmdir(p)
#else
#include <unistd.h>
#define MKDIR(p) mkdir(p, 0755)
#define RMDIR(p) rmdir(p)
#endif

extern "C" {
#ifdef _WIN32
void __stdcall GiveFnptrsToDll(enginefuncs_t *pengfuncsFromEngine, globalvars_t *pGlobals);
#else
void GiveFnptrsToDll(enginefuncs_t *pengfuncsFromEngine, globalvars_t *pGlobals);
#endif
int GetEntityAPI2(DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion);
const enginefuncs_t *ChaosTestHookedEngine();
}

// ============================================================================
// Mock GoldSrc Engine State
// ============================================================================

static char          g_stringPool[65536];
static int           g_stringPoolUsed = 1; // offset 0 = empty string
static int           g_allocStringCalls = 0;
static edict_t       g_edicts[128];
static int           g_numEdicts = 0;
static globalvars_t  g_mockGlobals;
static enginefuncs_t g_mockEngfuncs;
static char          g_testGameDir[256];

static std::map<std::string, cvar_t *>        g_cvars;
static std::map<std::string, void (*)(void)>  g_srvCmds;
static std::vector<std::string>               g_cmdArgs;
static std::vector<std::string> g_serverCommands, g_consoleOutput, g_receivedItems;
static std::set<std::string> g_precachedClasses, g_precachedSounds, g_inventory;
static bool g_loadingMap = false;
static bool g_wallTrace = false;
static bool g_blockHull = false;
static bool g_blockPath = false;
static bool g_dangerAtFeet = false;
static int g_pointContents = CONTENTS_EMPTY;
static bool g_mapValid = true;
static bool g_precacheCues = true;
static int g_emittedSounds = 0;
static const enginefuncs_t *g_testHookedEngine = nullptr;
static entvars_t g_savedPlayerVars;
static bool g_mutateFlagsDuringSave = false;
// Valve's CBasePlayer stores fall speed in its own private member, not in
// entvars.flFallVelocity. Model that original PreThink/PostThink behavior.
static float g_origFallVelocity = 0.0f;

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
    ++g_allocStringCalls;
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
static int Mock_PrecacheSound(char *s) { if (s) g_precachedSounds.insert(s); return 1; }
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
    (void)entity; (void)channel; (void)volume; (void)attenuation; (void)fFlags; (void)pitch;
    assert(sample && g_precachedSounds.count(sample) && "emitted sound was not precached by the map");
    ++g_emittedSounds;
}

static void Mock_TraceLine(const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr) {
    (void)fNoMonsters; (void)pentToSkip;
    memset((void *)ptr, 0, sizeof(*ptr));
    if (g_wallTrace) {
        ptr->flFraction = 0.25f;
        ptr->vecEndPos = Vec3(v1[0] + (v2[0]-v1[0])*0.25f, v1[1] + (v2[1]-v1[1])*0.25f, v1[2]);
        ptr->vecPlaneNormal = Vec3(-1, 0, 0);
        ptr->pHit = &g_edicts[0];
    } else {
        ptr->flFraction = 1.0f;
        ptr->vecEndPos = Vec3(v2[0], v2[1], v2[2]);
    }
}

static void Mock_TraceHull(const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr) {
    (void)fNoMonsters; (void)hullNumber; (void)pentToSkip;
    memset((void *)ptr, 0, sizeof(*ptr));
    if (g_blockHull && v1[0] == v2[0] && v1[1] == v2[1] && v1[2] == v2[2]) {
        ptr->fStartSolid = TRUE;
        return;
    }
    if (g_blockPath && (v1[0] != v2[0] || v1[1] != v2[1])) {
        ptr->flFraction = 0.4f; // a closed door across a horizontal path
        return;
    }
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
    if (g_dangerAtFeet && rgflVector[2] < 0.0f) return CONTENTS_WATER;
    return g_pointContents;
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
                g_edicts[i].v.movetype = MOVETYPE_STEP;
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

static void Mock_ServerPrint(const char *msg) { if (msg) g_consoleOutput.push_back(msg); }
static void Mock_ClientPrintf(edict_t *pEdict, PRINT_TYPE ptype, const char *msg) {
    (void)pEdict; (void)ptype; if (msg) g_consoleOutput.push_back(msg);
}
static void Mock_ServerCommand(char *str) {
    if (!str) return;
    if (strcmp(str, "exec chaos.cfg\n") == 0)
        assert(g_cvars.count("chaos_enabled") && g_cvars.count("chaos_interval") &&
               g_cvars.count("chaos_monsters") && "config must be queued after cvar registration");
    g_serverCommands.push_back(str);
}
static int Mock_IsMapValid(char *name) { return g_mapValid && name && strcmp(name, "c1a1") == 0; }
static edict_t *Mock_FindEntityInSphere(edict_t *start, const float *org, float radius) {
    int first = start ? (int)(start - g_edicts) + 1 : 1;
    Vec3 center(org[0], org[1], org[2]);
    for (int i = first; i < g_numEdicts; ++i)
        if (!g_edicts[i].free && (g_edicts[i].v.origin - center).Length() <= radius) return &g_edicts[i];
    return nullptr;
}
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

static bool ValidUtf8(const std::string &s) {
    for (size_t i = 0; i < s.size();) {
        unsigned char ch = (unsigned char)s[i];
        int n = ch < 0x80 ? 1 : ((ch & 0xe0) == 0xc0 ? 2 : ((ch & 0xf0) == 0xe0 ? 3 : ((ch & 0xf8) == 0xf0 ? 4 : 0)));
        if (!n || i + (size_t)n > s.size()) return false;
        for (int j = 1; j < n; ++j) if (((unsigned char)s[i+j] & 0xc0) != 0x80) return false;
        i += n;
    }
    return true;
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
    } else if (g_curMsg.type == g_msgIdSay) {
        assert(g_curMsg.byteCount <= 191 && "SayText exceeds bounded user-message payload");
        assert(g_curMsg.strings.size() == 1 && ValidUtf8(g_curMsg.strings[0]));
    }
    assert(g_curMsg.byteCount <= 255 && "Network message is too large");

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
    static cvar_t friction = {"sv_friction", "4", 0, 4, nullptr};
    static cvar_t air = {"sv_airaccelerate", "10", 0, 10, nullptr};
    static cvar_t accel = {"sv_accelerate", "10", 0, 10, nullptr};
    Mock_CVarRegister(&friction); Mock_CVarRegister(&air); Mock_CVarRegister(&accel);
    g_msgIdShake = Mock_RegUserMsg("ScreenShake", 6);
    g_msgIdFade  = Mock_RegUserMsg("ScreenFade", 10);
    g_msgIdFov   = Mock_RegUserMsg("SetFOV", 1);
    g_msgIdSay   = Mock_RegUserMsg("SayText", -1);
}

static int Orig_Spawn(edict_t *pent) {
    if (!pent) return -1;
    const char *cls = &g_stringPool[pent->v.classname];
    if (strcmp(cls, "worldspawn") == 0) {
        // Emulate Valve's W_Precache through the same hooked engine table the
        // real original DLL receives. No chaos-owned extra precaches are needed.
        static const char *stock[] = {
            "item_suit", "item_battery", "item_longjump", "weapon_crowbar", "weapon_9mmhandgun",
            "weapon_357", "weapon_shotgun", "weapon_9mmAR", "weapon_crossbow", "weapon_rpg",
            "weapon_gauss", "weapon_egon", "weapon_hornetgun", "weapon_handgrenade", "weapon_satchel",
            "weapon_tripmine", "weapon_snark", "monster_snark", "ammo_9mmclip", "ammo_357", "ammo_buckshot",
            "ammo_9mmAR", "ammo_ARgrenades", "ammo_crossbow", "ammo_rpgclip", "ammo_gaussclip"
        };
        static const char *cues[] = {"fvox/beep.wav", "debris/beamstart7.wav", "items/suitchargeok1.wav",
            "items/medshot4.wav", "items/gunpickup2.wav", "items/weapondrop1.wav",
            "items/suitchargeno1.wav", "debris/zap4.wav", "buttons/button3.wav", "buttons/button10.wav"};
        for (const char *name : stock) {
            int stringId = Mock_AllocString(name);
            edict_t *e = g_testHookedEngine->pfnCreateNamedEntity(stringId);
            assert(e);
            g_precachedClasses.insert(name);
            Mock_RemoveEntity(e);
        }
        if (g_precacheCues) for (const char *cue : cues) g_testHookedEngine->pfnPrecacheSound((char *)cue);
        return 0;
    }
    bool resourceClass = strncmp(cls, "weapon_", 7) == 0 || strncmp(cls, "ammo_", 5) == 0 ||
                         strncmp(cls, "item_", 5) == 0 || strncmp(cls, "monster_", 8) == 0;
    if (resourceClass) {
        if (g_loadingMap) g_precachedClasses.insert(cls);
        else assert(g_precachedClasses.count(cls) && "runtime class did not have its resources precached");
    }
    return 0;
}

static void Orig_Save(edict_t *entity, SAVERESTOREDATA *) {
    if (entity) {
        g_savedPlayerVars = entity->v;
        if (g_mutateFlagsDuringSave) entity->v.flags |= FL_FROZEN;
    }
}
static void Orig_PlayerPreThink(edict_t *player) {
    if (player && !(player->v.flags & FL_ONGROUND)) g_origFallVelocity = -player->v.velocity.z;
}
static void Orig_PlayerPostThink(edict_t *player) {
    if (player && (player->v.flags & FL_ONGROUND)) {
        if (g_origFallVelocity > 580.0f && player->v.health > 0) player->v.health -= 30.0f;
        g_origFallVelocity = 0.0f;
    }
}
static void Orig_Touch(edict_t *item, edict_t *player) {
    if (!item || !player) return;
    assert((item->v.spawnflags & (1 << 30)) != 0 && "dynamic pickups must not respawn");
    const std::string cls = &g_stringPool[item->v.classname];
    g_receivedItems.push_back(cls);
    if (cls == "item_suit") player->v.weapons |= (int)(1u << WEAPON_SUIT);
    if (cls.compare(0, 7, "weapon_") == 0) {
        if (g_inventory.insert(cls).second) {
            item->v.owner = player; item->v.solid = SOLID_NOT;
            return;
        }
    }
    Mock_RemoveEntity(item);
}
static void Orig_Use(edict_t *item, edict_t *player) {
    if (!item || !player) return;
    if (strcmp(&g_stringPool[item->v.classname], "player_weaponstrip") == 0) {
        for (int i = 2; i < g_numEdicts; ++i)
            if (!g_edicts[i].free && g_edicts[i].v.owner == player) Mock_RemoveEntity(&g_edicts[i]);
        g_inventory.clear();
        player->v.weapons &= (int)(1u << WEAPON_SUIT);
    }
}

static void ResetMockWorld(const char *mapName, float initialTime, bool hasSuit) {
    g_precachedClasses.clear(); g_precachedSounds.clear(); g_inventory.clear(); g_receivedItems.clear();
    g_loadingMap = true; g_wallTrace = false; g_blockHull = false; g_blockPath = false;
    g_dangerAtFeet = false; g_pointContents = CONTENTS_EMPTY; g_origFallVelocity = 0.0f;
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
    g_edicts[1].v.movetype = MOVETYPE_WALK;
    g_edicts[1].v.view_ofs = Vec3(0, 0, 28);
    g_edicts[1].v.flags = FL_CLIENT | FL_ONGROUND;
    g_edicts[1].v.deadflag = DEAD_NO;
    g_edicts[1].v.weapons = hasSuit ? ((int)(1u << 31) | (1 << 1) | (1 << 2)) : 0;

    // Edict 2: Nearby non-scripted bullsquid for monster events
    g_edicts[2].free = FALSE;
    g_edicts[2].pvPrivateData = (void *)(uintptr_t)0x3333;
    g_edicts[2].v.pContainingEntity = &g_edicts[2];
    g_edicts[2].v.classname = Mock_AllocString("monster_bullsquid");
    g_edicts[2].v.origin = Vec3(320.0f, 220.0f, 36.0f);
    g_edicts[2].v.health = 40.0f;
    g_edicts[2].v.flags = FL_MONSTER;
    g_edicts[2].v.movetype = MOVETYPE_STEP;
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
    g_edicts[3].v.movetype = MOVETYPE_STEP;
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

static void StartWorld(DLL_FUNCTIONS &dll, const char *map, float time, bool suit, bool monsters = true, bool deactivate = true) {
    if (deactivate) dll.pfnServerDeactivate();
    ResetMockWorld(map, time, suit);
    assert(dll.pfnSpawn(&g_edicts[0]) >= 0);
    if (monsters) {
        static const char *classes[] = {"monster_headcrab", "monster_babycrab", "monster_houndeye", "monster_bullchicken",
            "monster_zombie", "monster_alien_slave", "monster_human_grunt", "monster_alien_grunt",
            "monster_snark", "monster_cockroach", "monster_barney", "monster_scientist", "item_healthkit"};
        for (const char *name : classes) {
            edict_t *e = Mock_CreateNamedEntity(Mock_AllocString(name));
            assert(e && dll.pfnSpawn(e) >= 0);
            Mock_RemoveEntity(e);
        }
    }
    g_loadingMap = false;
    dll.pfnServerActivate(g_edicts, g_numEdicts, 1);
    dll.pfnClientPutInServer(&g_edicts[1]);
}
static void Step(DLL_FUNCTIONS &dll, float seconds, float dt = 0.2f) {
    int steps = (int)roundf(seconds / dt);
    for (int i = 0; i < steps; ++i) {
        g_mockGlobals.time += dt;
        dll.pfnPlayerPreThink(&g_edicts[1]);
        dll.pfnStartFrame();
        dll.pfnPlayerPostThink(&g_edicts[1]);
    }
}
static int CountSpawned() {
    int result = 0;
    for (int i = 2; i < g_numEdicts; ++i)
        if (!g_edicts[i].free && g_edicts[i].v.iuser4 == 0xC4A05) ++result;
    return result;
}
static void RemoveSpawned() {
    for (int i = 2; i < g_numEdicts; ++i)
        if (!g_edicts[i].free && g_edicts[i].v.iuser4 == 0xC4A05) Mock_RemoveEntity(&g_edicts[i]);
}
static bool Contains(const std::vector<std::string> &lines, const char *text) {
    for (const std::string &line : lines) if (line.find(text) != std::string::npos) return true;
    return false;
}
static std::string ReadText(const char *path) {
    FILE *fp = fopen(path, "rb");
    assert(fp);
    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf), fp);
    fclose(fp);
    return std::string(buf, n);
}
static void WriteText(const char *path, const char *text) {
    FILE *fp = fopen(path, "wb"); assert(fp);
    assert(fwrite(text, 1, strlen(text), fp) == strlen(text));
    assert(fclose(fp) == 0);
}

int main() {
    srand(19981119); // Half-Life 1 release date seed!

#ifdef _WIN32
    const char *tmpBase = getenv("TEMP");
    snprintf(g_testGameDir, sizeof(g_testGameDir), "%s\\hl_chaos_sim_%d", (tmpBase && tmpBase[0]) ? tmpBase : ".", _getpid());
#else
    snprintf(g_testGameDir, sizeof(g_testGameDir), "/tmp/hl_chaos_sim_%ld", (long)getpid());
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
    g_mockEngfuncs.pfnFindEntityInSphere = Mock_FindEntityInSphere;
    g_mockEngfuncs.pfnIsMapValid          = Mock_IsMapValid;
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
    g_testHookedEngine = ChaosTestHookedEngine();

    DLL_FUNCTIONS dllTable;
    memset(&dllTable, 0, sizeof(dllTable));
    dllTable.pfnGameInit = Orig_GameInit;
    dllTable.pfnSpawn    = Orig_Spawn;
    dllTable.pfnSave     = Orig_Save;
    dllTable.pfnTouch    = Orig_Touch;
    dllTable.pfnUse      = Orig_Use;
    dllTable.pfnPlayerPreThink = Orig_PlayerPreThink;
    dllTable.pfnPlayerPostThink = Orig_PlayerPostThink;

    int ifaceVer = INTERFACE_VERSION;
    int ok = GetEntityAPI2(&dllTable, &ifaceVer);
    assert(ok == TRUE);

    dllTable.pfnGameInit();
    assert(g_serverCommands.size() == 1 && g_serverCommands[0] == "exec chaos.cfg\n");
    StartWorld(dllTable, "c1a2", 1.0f, true, true, false);
    assert(g_serverCommands.size() == 1); // no repeated exec on ServerActivate

    printf("[1] 477 event definitions and unique recipe IDs...\n");
    assert(g_chaos.GetEventCount() == 477);
    std::set<std::string> recipeTitles;
    for (int i = 0; i < g_chaos.GetEventCount(); ++i) {
        const ChaosEventDef *ev = g_chaos.GetEventDef(i);
        assert(ev && ev->id == i && ev->titleEn && *ev->titleEn && ev->titleRu && *ev->titleRu && ev->descRu && *ev->descRu);
        if ((i >= CHAOS_RECIPE_START && i < CHAOS_RECIPE_END) || i >= CHAOS_EXTENSION_START)
            assert(recipeTitles.insert(ev->titleEn).second);
    }
    assert(recipeTitles.size() == 432);
    assert(ChaosRecipeTheme(428) == -1 && ChaosRecipeVariant(428) == -1);
    assert(ChaosRecipeTheme(429) == 0 && ChaosRecipeVariant(429) == 16);
    assert(ChaosRecipeTheme(476) == 23 && ChaosRecipeVariant(476) == 17);
    assert(!g_chaos.GetEventDef(-1) && !g_chaos.GetEventDef(477));

    printf("[2] Automatic timer, bounded HUD and strict command parsing...\n");
    int before = g_deathCounter.GetTotalEvents();
    Step(dllTable, 32.5f, 0.05f);
    assert(g_deathCounter.GetTotalEvents() == before + 3);
    bool foundHud = false;
    for (const auto &m : g_allMessages)
        if (m.type == SVC_TEMPENTITY && m.firstByte == TE_TEXTMESSAGE && !m.strings.empty() &&
            m.strings[0].find("DEATHS:") != std::string::npos && m.strings[0].find("NEXT:") != std::string::npos) foundHud = true;
    assert(foundHud);
    before = g_deathCounter.GetTotalEvents();
    g_cmdArgs = {"chaos_trigger", "not-a-number"};
    dllTable.pfnClientCommand(&g_edicts[1]);
    assert(g_deathCounter.GetTotalEvents() == before);
    g_consoleOutput.clear();
    g_cmdArgs = {"chaos_list", "20"};
    dllTable.pfnClientCommand(&g_edicts[1]);
    assert(g_consoleOutput.size() <= 27 && Contains(g_consoleOutput, "476"));

    printf("[3] Chaos starts on the tram and before HEV; resource guards remain active...\n");
    Mock_CVarSetFloat("chaos_enabled", 1);
    Mock_CVarSetFloat("chaos_interval", 10);
    StartWorld(dllTable, "c0a0", 1, false);
    g_edicts[1].v.flags |= FL_ONTRAIN;
    g_allMessages.clear();
    before = g_deathCounter.GetTotalEvents();
    std::set<int> tramEvents;
    for (int i = 0; i < 4; ++i) {
        Step(dllTable, 10.4f);
        assert(g_deathCounter.GetTotalEvents() == before + i + 1);
        const ChaosEventDef *ev = g_chaos.GetEventDef(g_chaos.GetLastEventId());
        assert(ev && !ev->requiresSuit && !ev->skipOnTram);
        tramEvents.insert(ev->id);
    }
    assert(tramEvents.size() == 4); // The introductory pool is not a repeated placeholder.
    bool tramHud = false;
    for (const auto &m : g_allMessages) if (!m.strings.empty()) {
        if (m.strings[0].find("NEXT:") != std::string::npos &&
            m.strings[0].find("INTRO/VEHICLE: VISUAL EVENTS") != std::string::npos) tramHud = true;
        assert(m.strings[0].find("STORY / VEHICLE SAFE MODE") == std::string::npos);
    }
    assert(tramHud && g_edicts[1].v.weapons == 0);
    Mock_CVarSetFloat("chaos_enabled", 0);
    before = g_deathCounter.GetTotalEvents();
    g_chaos.CmdTriggerById(12); // physics stays blocked on a running tram
    assert(g_deathCounter.GetTotalEvents() == before && Mock_CVarGetFloat("sv_gravity") == 800);
    g_chaos.CmdTriggerById(33); // camera events actually stay active on the tram
    assert(g_deathCounter.GetTotalEvents() == before + 1 && g_edicts[1].v.fov == 130);
    Step(dllTable, 2); assert(g_edicts[1].v.fov == 130);
    Step(dllTable, 9); assert(g_edicts[1].v.fov == 0);
    g_allMessages.clear();
    g_chaos.CmdTriggerById(428); Step(dllTable, 3);
    bool lastAnimeFrame = false;
    for (const auto &m : g_allMessages) if (!m.strings.empty() &&
        m.strings[0].find("JUST A MEME!") != std::string::npos) lastAnimeFrame = true;
    assert(lastAnimeFrame);

    // Check the entire intro pool, including combinations: only effects that
    // leave player movement, inventory and engine physics untouched run here.
    StartWorld(dllTable, "c0a0", 1, false); g_edicts[1].v.flags |= FL_ONTRAIN;
    g_receivedItems.clear(); g_serverCommands.clear();
    int safeCount = 0;
    for (int id = 0; id < g_chaos.GetEventCount(); ++id) {
        const ChaosEventDef *ev = g_chaos.GetEventDef(id);
        before = g_deathCounter.GetTotalEvents(); g_chaos.CmdTriggerById(id);
        const bool allowed = !ev->requiresSuit && !ev->skipOnTram;
        assert(g_deathCounter.GetTotalEvents() == before + (allowed ? 1 : 0));
        if (allowed) ++safeCount;
        Step(dllTable, 0.4f);
        assert(g_edicts[1].v.health == 100 && g_edicts[1].v.weapons == 0 &&
               g_edicts[1].v.velocity.Length() == 0 && g_edicts[1].v.maxspeed == 320);
        assert(Mock_CVarGetFloat("sv_gravity") == 800 && Mock_CVarGetFloat("sv_maxspeed") == 320);
        assert(g_receivedItems.empty() && g_serverCommands.empty() && CountSpawned() == 0);
    }
    assert(safeCount > CHAOS_HISTORY_SIZE);

    Mock_CVarSetFloat("chaos_enabled", 1);
    StartWorld(dllTable, "c0a0c", 1, false); // map prefix without FL_ONTRAIN
    before = g_deathCounter.GetTotalEvents(); Step(dllTable, 11);
    assert(g_deathCounter.GetTotalEvents() == before + 1);
    assert(!g_chaos.GetEventDef(g_chaos.GetLastEventId())->skipOnTram);

    StartWorld(dllTable, "c1a0", 1, false);
    before = g_deathCounter.GetTotalEvents();
    g_chaos.CmdTriggerById(14); // real physics effects work before the HEV suit
    assert(g_deathCounter.GetTotalEvents() == before + 1 && g_edicts[1].v.maxspeed == 720);
    g_chaos.CmdTriggerById(23); // but don't give/strip weapons before HEV
    assert(g_deathCounter.GetTotalEvents() == before + 1 && g_edicts[1].v.weapons == 0);
    Step(dllTable, 30.8f);
    assert(g_deathCounter.GetTotalEvents() == before + 4);
    assert(!g_chaos.GetEventDef(g_chaos.GetLastEventId())->requiresSuit);
    g_edicts[1].v.weapons |= (int)(1u << WEAPON_SUIT);
    g_chaos.CmdTriggerById(23); // inventory events become available immediately
    assert(g_deathCounter.GetTotalEvents() == before + 5);

    // A time rewind (as on save-load) must restart the countdown, not freeze it.
    StartWorld(dllTable, "c1a0a", 40, false);
    before = g_deathCounter.GetTotalEvents(); Step(dllTable, 11);
    assert(g_deathCounter.GetTotalEvents() == before + 1);
    g_mockGlobals.time = 2; Step(dllTable, 11);
    assert(g_deathCounter.GetTotalEvents() == before + 2);

    // Empty network slots are not additional players. Joining a second client
    // or enabling deathmatch/coop still suspends global physics changes.
    StartWorld(dllTable, "c1a0", 1, false);
    g_mockGlobals.maxClients = 2;
    before = g_deathCounter.GetTotalEvents(); Step(dllTable, 21);
    assert(g_deathCounter.GetTotalEvents() == before + 2);
    g_edicts[2].v.flags |= FL_CLIENT; before = g_deathCounter.GetTotalEvents();
    Step(dllTable, 15); assert(g_deathCounter.GetTotalEvents() == before);
    g_edicts[2].v.flags &= ~FL_CLIENT; Step(dllTable, 11);
    assert(g_deathCounter.GetTotalEvents() == before + 1);
    g_mockGlobals.deathmatch = 1; before = g_deathCounter.GetTotalEvents();
    Step(dllTable, 11); assert(g_deathCounter.GetTotalEvents() == before);
    g_mockGlobals.deathmatch = 0; g_mockGlobals.coop = 1;
    Step(dllTable, 11); assert(g_deathCounter.GetTotalEvents() == before);
    g_mockGlobals.coop = 0;
    StartWorld(dllTable, "c1a0", 1, false); g_mockGlobals.maxClients = 0;
    before = g_deathCounter.GetTotalEvents(); Step(dllTable, 11);
    assert(g_deathCounter.GetTotalEvents() == before + 1); // use ServerActivate's slot count

    StartWorld(dllTable, "c1a2", 1, true, false);
    Mock_CVarSetFloat("chaos_monsters", 1);
    before = g_deathCounter.GetTotalEvents();
    g_chaos.CmdTriggerById(0); // headcrab not precached on this map
    assert(g_deathCounter.GetTotalEvents() == before && CountSpawned() == 0);
    g_edicts[1].v.flags |= FL_ONTRAIN;
    g_chaos.CmdTriggerById(14);
    assert(g_deathCounter.GetTotalEvents() == before);
    g_edicts[1].v.flags &= ~FL_ONTRAIN;
    g_edicts[1].v.movetype = MOVETYPE_FLY;
    g_chaos.CmdTriggerById(14);
    assert(g_deathCounter.GetTotalEvents() == before);
    g_edicts[1].v.movetype = MOVETYPE_WALK;
    Mock_CVarSetFloat("chaos_enabled", 0);
    Mock_CVarSetFloat("chaos_spawn_limit", 2);
    g_chaos.CmdTriggerById(8); // snark is part of stock weapon precache
    assert(CountSpawned() == 2);
    Step(dllTable, 0.4f);
    before = g_deathCounter.GetTotalEvents();
    g_chaos.CmdTriggerById(8);
    assert(g_deathCounter.GetTotalEvents() == before && CountSpawned() == 2);
    RemoveSpawned();
    int originalMax = g_mockGlobals.maxEntities;
    g_mockGlobals.maxEntities = Mock_NumberOfEntities() + 16;
    g_chaos.CmdTriggerById(8);
    assert(CountSpawned() == 0);
    g_mockGlobals.maxEntities = originalMax;
    g_precacheCues = false;
    StartWorld(dllTable, "c1a2", 1, true, false);
    int soundBefore = g_emittedSounds;
    g_chaos.CmdTriggerById(14);
    Step(dllTable, 1);
    assert(g_emittedSounds == soundBefore);
    g_precacheCues = true;

    printf("[4] Exercise all 477 events and expire every modifier...\n");
    StartWorld(dllTable, "c1a2", 10, true);
    Mock_CVarSetFloat("chaos_enabled", 0);
    g_receivedItems.clear();
    g_chaos.CmdTriggerById(23);
    assert(g_receivedItems.empty()); // arsenal is queued, not a single-frame flood
    Step(dllTable, 0.2f);
    assert(g_receivedItems.size() <= 2);
    Step(dllTable, 4);
    assert(g_receivedItems.size() >= 15);
    Step(dllTable, 2); // finish the bounded pickup queue
    int allocationsAfterFirstArsenal = g_allocStringCalls;
    g_chaos.CmdTriggerById(23); Step(dllTable, 6);
    assert(g_allocStringCalls == allocationsAfterFirstArsenal && "runtime class IDs must be reused on a map");
    StartWorld(dllTable, "c1a2", 100, true);
    Mock_CVarSetFloat("chaos_enabled", 0);
    Mock_CVarSetFloat("chaos_monsters", 1);
    Mock_CVarSetFloat("chaos_spawn_limit", 8);
    Step(dllTable, 8);
    Vec3 scientist = g_edicts[3].v.origin;
    for (int id = 0; id < g_chaos.GetEventCount(); ++id) {
        RemoveSpawned();
        g_edicts[1].v.health = 25;
        g_edicts[1].v.deadflag = DEAD_NO;
        g_chaos.CmdTriggerById(id);
        if (g_chaos.GetLastEventId() != id) fprintf(stderr, "Event %d was unexpectedly rejected\n", id);
        assert(g_chaos.GetLastEventId() == id);
        assert(g_edicts[1].v.health >= 15 && g_edicts[1].v.deadflag == DEAD_NO);
        assert(g_edicts[3].v.origin.x == scientist.x && g_edicts[3].v.origin.y == scientist.y && g_edicts[3].v.health == 20);
        Step(dllTable, 20);
        assert(fabsf(Mock_CVarGetFloat("sv_gravity") - 800) < 0.01f);
        assert(fabsf(g_edicts[1].v.fov) < 0.1f);
        assert(CountSpawned() <= 8);
    }
    // The appended combinations have concrete behavior, not just new names.
    StartWorld(dllTable, "c1a2", 1, true); Mock_CVarSetFloat("chaos_enabled", 0);
    g_chaos.CmdTriggerById(429); assert(g_edicts[1].v.maxspeed == 520); // TOTEM + B-HOP
    Step(dllTable, 0.4f); g_edicts[1].v.health = 20; g_edicts[1].v.armorvalue = 0;
    g_chaos.CmdTriggerById(CHAOS_EXTENSION_START + 7 * CHAOS_EXTRA_VARIANT_COUNT + 1); // BEDROCK + LAST STAND
    assert(g_edicts[1].v.health == 50 && g_edicts[1].v.armorvalue == 25 && (g_edicts[1].v.flags & FL_GODMODE));
    Step(dllTable, 5); assert(!(g_edicts[1].v.flags & FL_GODMODE));

    printf("[5] Preserve custom cvars, FOV, speed, god mode and overlapping effects...\n");
    StartWorld(dllTable, "c1a2", 1, true);
    Mock_CVarSetFloat("sv_gravity", 600); Mock_CVarSetFloat("sv_maxspeed", 275);
    Mock_CVarSetFloat("sv_accelerate", 9); Mock_CVarSetFloat("sv_friction", 6); Mock_CVarSetFloat("sv_airaccelerate", 12);
    g_edicts[1].v.maxspeed = 245; g_edicts[1].v.fov = 75;
    g_edicts[1].v.flags |= FL_GODMODE;
    g_edicts[1].v.renderfx = 2; g_edicts[1].v.renderamt = 17; g_edicts[1].v.rendercolor = Vec3(1, 2, 3);
    g_chaos.CmdTriggerById(12); Step(dllTable, 0.4f); g_chaos.CmdTriggerById(13);
    Step(dllTable, 9);
    assert(fabsf(Mock_CVarGetFloat("sv_gravity") - 600) < 0.01f);
    g_chaos.CmdTriggerById(14); Step(dllTable, 0.4f); g_chaos.CmdTriggerById(16);
    Step(dllTable, 0.4f); g_chaos.CmdTriggerById(33); Step(dllTable, 0.4f); g_chaos.CmdTriggerById(32);
    assert(g_edicts[1].v.maxspeed == 720 && g_edicts[1].v.fov == 130);
    g_mutateFlagsDuringSave = true; dllTable.pfnSave(&g_edicts[1], nullptr); g_mutateFlagsDuringSave = false;
    assert(g_savedPlayerVars.maxspeed == 245 && g_savedPlayerVars.fov == 75);
    assert((g_savedPlayerVars.flags & FL_GODMODE) && g_savedPlayerVars.renderfx == 2);
    assert(g_edicts[1].v.flags & FL_FROZEN); // the original save's unrelated flag change survives
    g_edicts[1].v.flags &= ~FL_FROZEN;
    assert(g_edicts[1].v.maxspeed == 720 && g_edicts[1].v.fov == 130 && g_edicts[1].v.renderfx == kRenderFxGlowShell);
    Step(dllTable, 15);
    assert(Mock_CVarGetFloat("sv_maxspeed") == 275 && Mock_CVarGetFloat("sv_accelerate") == 9);
    assert(Mock_CVarGetFloat("sv_friction") == 6 && Mock_CVarGetFloat("sv_airaccelerate") == 12);
    assert(g_edicts[1].v.maxspeed == 245 && g_edicts[1].v.fov == 75);
    assert((g_edicts[1].v.flags & FL_GODMODE) && g_edicts[1].v.renderfx == 2 && g_edicts[1].v.renderamt == 17);
    g_chaos.CmdTriggerById(12);
    Mock_CVarSetFloat("sv_gravity", 950); // user changed it while effect was active
    Step(dllTable, 11);
    assert(Mock_CVarGetFloat("sv_gravity") == 950);
    g_chaos.CmdRecover();
    assert(Mock_CVarGetFloat("sv_gravity") == 800 && g_edicts[1].v.maxspeed == 320 && g_edicts[1].v.fov == 0);
    assert(!(g_edicts[1].v.flags & FL_GODMODE));
    Mock_CVarSetFloat("sv_gravity", 950);
    dllTable.pfnServerDeactivate();
    assert(Mock_CVarGetFloat("sv_gravity") == 950);
    Mock_CVarSetFloat("sv_gravity", 800); Mock_CVarSetFloat("sv_maxspeed", 320);
    Mock_CVarSetFloat("sv_accelerate", 10); Mock_CVarSetFloat("sv_friction", 4); Mock_CVarSetFloat("sv_airaccelerate", 10);

    printf("[5a] Cancel active effects on trains, ladders and multiplayer; preserve script changes...\n");
    StartWorld(dllTable, "c1a2", 1, true, true, false);
    Mock_CVarSetFloat("chaos_enabled", 0);
    g_edicts[1].v.maxspeed = 0; // a valid engine default must not become 320
    g_chaos.CmdTriggerById(14); Step(dllTable, 11);
    assert(g_edicts[1].v.maxspeed == 0);
    g_edicts[1].v.maxspeed = 245; g_edicts[1].v.fov = 75;
    g_chaos.CmdTriggerById(12); Step(dllTable, 0.4f);
    g_chaos.CmdTriggerById(14); Step(dllTable, 0.4f);
    g_chaos.CmdTriggerById(33); Step(dllTable, 0.4f);
    g_chaos.CmdTriggerById(32); Step(dllTable, 0.4f);
    g_chaos.CmdTriggerById(18); Step(dllTable, 0.4f);
    g_chaos.CmdTriggerById(27); Step(dllTable, 0.4f);
    g_chaos.CmdTriggerById(428); Step(dllTable, 0.4f);
    g_chaos.CmdTriggerById(23); // leave the arsenal queued as the player enters a train
    g_receivedItems.clear(); g_allMessages.clear();
    g_edicts[1].v.flags |= FL_ONTRAIN;
    g_edicts[1].v.maxspeed = 87; g_edicts[1].v.fov = 55;
    g_edicts[1].v.renderfx = kRenderFxGlowShell;
    g_edicts[1].v.renderamt = 12; g_edicts[1].v.rendercolor = Vec3(3, 4, 5);
    g_edicts[1].v.velocity = Vec3(0, 0, 0);
    dllTable.pfnSave(&g_edicts[1], nullptr);
    assert(g_savedPlayerVars.maxspeed == 87 && g_savedPlayerVars.fov == 55);
    assert(g_savedPlayerVars.renderamt == 12 && g_savedPlayerVars.rendercolor.x == 3);
    dllTable.pfnPlayerPreThink(&g_edicts[1]);
    assert(Mock_CVarGetFloat("sv_gravity") == 800 && Mock_CVarGetFloat("sv_maxspeed") == 320);
    assert(g_edicts[1].v.maxspeed == 87 && g_edicts[1].v.fov == 55);
    assert(g_edicts[1].v.renderamt == 12 && g_edicts[1].v.rendercolor.x == 3);
    assert(!(g_edicts[1].v.flags & FL_GODMODE));
    Step(dllTable, 4);
    assert(g_receivedItems.empty() && g_edicts[1].v.velocity.Length() == 0);
    for (const auto &m : g_allMessages)
        for (const auto &s : m.strings) assert(s.find("JUST A MEME!") == std::string::npos);
    g_edicts[1].v.flags &= ~FL_ONTRAIN;
    g_mockGlobals.maxClients = 2; g_edicts[2].v.flags |= FL_CLIENT; g_serverCommands.clear();
    before = g_deathCounter.GetTotalEvents();
    g_chaos.CmdTriggerById(12); g_chaos.CmdC1A1(true);
    assert(g_deathCounter.GetTotalEvents() == before && g_serverCommands.empty());
    g_edicts[2].v.flags &= ~FL_CLIENT; g_mockGlobals.maxClients = 1;

    printf("[5b] Overlapping movement effects do not shorten fall protection...\n");
    StartWorld(dllTable, "c1a2", 1, true);
    g_chaos.CmdTriggerById(17); Step(dllTable, 0.4f);
    g_chaos.CmdTriggerById(19); Step(dllTable, 15); // nominal window passed, still airborne
    g_edicts[1].v.velocity.z = -950; g_edicts[1].v.flFallVelocity = 600;
    dllTable.pfnPlayerPreThink(&g_edicts[1]);
    assert(g_edicts[1].v.velocity.z == -400 && g_edicts[1].v.flFallVelocity == 400);
    assert(g_origFallVelocity == 400); // original player code sees a safe speed
    g_edicts[1].v.flags |= FL_ONGROUND; dllTable.pfnPlayerPostThink(&g_edicts[1]);
    assert(g_edicts[1].v.health == 100);
    Step(dllTable, 0.4f); // protection ends on landing
    g_edicts[1].v.flags &= ~FL_ONGROUND; g_edicts[1].v.velocity.z = -950;
    dllTable.pfnPlayerPreThink(&g_edicts[1]); assert(g_origFallVelocity == 950);
    g_edicts[1].v.flags |= FL_ONGROUND; dllTable.pfnPlayerPostThink(&g_edicts[1]);
    assert(g_edicts[1].v.health == 70); // not permanent invulnerability

    printf("[6] Totem activates before death, never resurrects a dead player...\n");
    StartWorld(dllTable, "c1a2", 1, true, true, false);
    g_chaos.CmdTriggerById(32);
    g_mutateFlagsDuringSave = true; dllTable.pfnSave(&g_edicts[1], nullptr); g_mutateFlagsDuringSave = false;
    assert(!(g_savedPlayerVars.flags & FL_GODMODE) && (g_edicts[1].v.flags & FL_GODMODE));
    assert(g_edicts[1].v.flags & FL_FROZEN);
    g_edicts[1].v.flags &= ~FL_FROZEN;
    Step(dllTable, 9);
    g_chaos.CmdTotem();
    g_edicts[1].v.health = 26; Step(dllTable, 0.2f); assert(g_edicts[1].v.health == 26);
    g_edicts[1].v.health = 25; Step(dllTable, 0.2f);
    assert(g_edicts[1].v.health >= 60 && (g_edicts[1].v.flags & FL_GODMODE));
    Step(dllTable, 4); assert(!(g_edicts[1].v.flags & FL_GODMODE));
    g_chaos.CmdTotem();
    g_edicts[1].v.health = -1; g_edicts[1].v.deadflag = DEAD_DYING;
    dllTable.pfnPlayerPreThink(&g_edicts[1]);
    assert(g_edicts[1].v.health < 0 && g_edicts[1].v.deadflag != DEAD_NO);
    // ClientKill counts before the dead frame, but must still clear the totem.
    StartWorld(dllTable, "c1a2", 1, true); Step(dllTable, 0.2f);
    g_chaos.CmdTotem(); dllTable.pfnClientKill(&g_edicts[1]);
    g_edicts[1].v.health = -1; g_edicts[1].v.deadflag = DEAD_DYING; Step(dllTable, 0.2f);
    g_edicts[1].v.health = 25; g_edicts[1].v.deadflag = DEAD_NO; Step(dllTable, 0.2f);
    assert(g_edicts[1].v.health == 25 && !(g_edicts[1].v.flags & FL_GODMODE));

    printf("[7] Portal pair: safe destination, blocked exit and no map command...\n");
    StartWorld(dllTable, "c1a2", 1, true);
    g_serverCommands.clear(); g_wallTrace = true;
    g_chaos.CmdPortalGun();
    g_edicts[1].v.origin = Vec3(100, 200, 36); g_chaos.CmdPortalPlace(0);
    g_edicts[1].v.origin = Vec3(500, 200, 36); g_chaos.CmdPortalPlace(1);
    g_edicts[1].v.origin = Vec3(254, 200, 1);
    g_blockHull = true; g_chaos.CmdPortalEnter(); assert(g_edicts[1].v.origin.x == 254);
    g_blockHull = false; g_dangerAtFeet = true;
    g_chaos.CmdPortalEnter(); assert(g_edicts[1].v.origin.x == 254); // centre empty, feet in water
    g_dangerAtFeet = false; g_chaos.CmdPortalEnter(); assert(g_edicts[1].v.origin.x == 654);
    assert(g_serverCommands.empty());
    // Aperture-themed recipes must actually place different blue/orange slots.
    StartWorld(dllTable, "c1a2", 1, true); g_wallTrace = true;
    g_edicts[1].v.origin = Vec3(100, 200, 36);
    g_chaos.CmdTriggerById(CHAOS_RECIPE_START + 21 * CHAOS_VARIANT_COUNT + 13);
    Step(dllTable, 0.4f); g_edicts[1].v.origin = Vec3(500, 200, 36);
    g_chaos.CmdTriggerById(CHAOS_RECIPE_START + 22 * CHAOS_VARIANT_COUNT + 13);
    g_edicts[1].v.origin = Vec3(254, 200, 1); g_chaos.CmdPortalEnter();
    assert(g_edicts[1].v.origin.x == 654);

    printf("[7a] Rewind never passes a newly closed door...\n");
    StartWorld(dllTable, "c1a2", 1, true);
    Mock_CVarSetFloat("chaos_enabled", 0); Step(dllTable, 4);
    g_edicts[1].v.origin = Vec3(500, 200, 36);
    g_blockPath = true; g_chaos.CmdTriggerById(20);
    assert(g_edicts[1].v.origin.x == 500);
    g_blockPath = false; Step(dllTable, 0.4f); g_chaos.CmdTriggerById(20);
    assert(g_edicts[1].v.origin.x == 100);

    printf("[8] c1a1 confirmation, non-overwriting save slots and intro prank...\n");
    char saveDir[512], manualPath[512];
    snprintf(saveDir, sizeof(saveDir), "%s/SAVE", g_testGameDir); MKDIR(saveDir);
    // GoldSrc can age <slot>.sav into <slot>01.sav. Even a backup-only
    // collision must prevent this name from being used.
    char slot[64], agedCollision[512], baseCollision[512], badDir[512];
    assert(ChaosFindUnusedSaveSlot(g_testGameDir, 42, slot, sizeof(slot)) == CHAOS_SAVE_SLOT_OK);
    assert(strcmp(slot, "chaos_c1a1_42_00") == 0);
    snprintf(agedCollision, sizeof(agedCollision), "%s/SAVE/%s01.sav", g_testGameDir, slot);
    WriteText(agedCollision, "old-aged-save");
    assert(ChaosFindUnusedSaveSlot(g_testGameDir, 42, slot, sizeof(slot)) == CHAOS_SAVE_SLOT_OK);
    assert(strcmp(slot, "chaos_c1a1_42_01") == 0);
    snprintf(baseCollision, sizeof(baseCollision), "%s/SAVE/%s.sav", g_testGameDir, slot);
    WriteText(baseCollision, "old-base-save");
    assert(ChaosFindUnusedSaveSlot(g_testGameDir, 42, slot, sizeof(slot)) == CHAOS_SAVE_SLOT_OK);
    assert(strcmp(slot, "chaos_c1a1_42_02") == 0);
#ifndef _WIN32
    char dangling[512];
    snprintf(dangling, sizeof(dangling), "%s/SAVE/%s01.sav", g_testGameDir, slot);
    assert(symlink("missing-target", dangling) == 0);
    assert(ChaosFindUnusedSaveSlot(g_testGameDir, 42, slot, sizeof(slot)) == CHAOS_SAVE_SLOT_OK);
    assert(strcmp(slot, "chaos_c1a1_42_03") == 0);
#endif
    assert(ReadText(agedCollision) == "old-aged-save" && ReadText(baseCollision) == "old-base-save");
    char tinySlot[4];
    assert(ChaosFindUnusedSaveSlot(g_testGameDir, 42, tinySlot, sizeof(tinySlot)) == CHAOS_SAVE_SLOT_CHECK_FAILED);
    snprintf(badDir, sizeof(badDir), "%s/not_a_directory", g_testGameDir); WriteText(badDir, "file");
    assert(ChaosFindUnusedSaveSlot(badDir, 42, slot, sizeof(slot)) == CHAOS_SAVE_SLOT_CHECK_FAILED);
    snprintf(manualPath, sizeof(manualPath), "%s/SAVE/my_old_game.sav", g_testGameDir);
    WriteText(manualPath, "manual-save-sentinel");
    g_serverCommands.clear();
    g_chaos.CmdC1A1(false); assert(g_serverCommands.empty());
    g_mapValid = false; g_chaos.CmdC1A1(true); assert(g_serverCommands.empty());
    g_mapValid = true; g_mockGlobals.coop = 1;
    g_chaos.CmdC1A1(true); assert(g_serverCommands.empty());
    g_mockGlobals.coop = 0; g_chaos.CmdC1A1(true);
    assert(g_serverCommands.size() == 1 && g_serverCommands[0] == "map c1a1\n");
    StartWorld(dllTable, "c1a1", 1, false);
    Step(dllTable, 5);
    assert(g_serverCommands.size() == 2 && g_serverCommands.back().find("save chaos_c1a1_") == 0);
    assert((g_edicts[1].v.weapons & (int)(1u << WEAPON_SUIT)) != 0);
    std::string firstSave = g_serverCommands.back().substr(5);
    if (!firstSave.empty() && firstSave.back() == '\n') firstSave.pop_back();
    char checkpointPath[512];
    snprintf(checkpointPath, sizeof(checkpointPath), "%s/SAVE/%s.sav", g_testGameDir, firstSave.c_str());
    WriteText(checkpointPath, "checkpoint-sentinel");
    g_chaos.CmdC1A1(true);
    StartWorld(dllTable, "c1a1", 1, false);
    Step(dllTable, 5);
    assert(g_serverCommands.size() == 4 && g_serverCommands.back().find("save chaos_c1a1_") == 0);
    assert(g_serverCommands.back() != std::string("save ") + firstSave + "\n");
    assert(ReadText(manualPath) == "manual-save-sentinel" && ReadText(checkpointPath) == "checkpoint-sentinel");
    g_serverCommands.clear(); g_allMessages.clear();
    StartWorld(dllTable, "c0a0", 1, false);
    Step(dllTable, 3);
    bool prank = false;
    for (const auto &m : g_allMessages) if (!m.strings.empty() && m.strings[0].find("JUST KIDDING") != std::string::npos) prank = true;
    assert(prank && g_serverCommands.empty() && ReadText(manualPath) == "manual-save-sentinel");

    printf("[8a] CS 1.6 anime meme has four bounded HUD frames, with no file commands...\n");
    StartWorld(dllTable, "c1a2", 1, true);
    Mock_CVarSetFloat("chaos_enabled", 0); g_allMessages.clear(); g_serverCommands.clear();
    g_chaos.CmdTriggerById(428); Step(dllTable, 3);
    const char *frames[] = {"CS 1.6 UPDATE...\n[ DOWNLOADING ]", "ANIME IN CS 1.6?!\n( o_o )",
                            "( ^_^ )\nRUSH B!", "JUST A MEME!\nNO FILES CHANGED."};
    int seenFrames[4] = {};
    for (const auto &m : g_allMessages) {
        if (m.type != SVC_TEMPENTITY || m.firstByte != TE_TEXTMESSAGE || m.strings.empty()) continue;
        for (int i = 0; i < 4; ++i) if (m.strings[0] == frames[i]) {
            assert(m.dest == MSG_ONE_UNRELIABLE);
            ++seenFrames[i];
        }
    }
    for (int count : seenFrames) assert(count == 1);
    assert(g_serverCommands.empty() && ReadText(manualPath) == "manual-save-sentinel");

    printf("[9] Death persistence, save reload, deferred atomic stats and speedrun timer...\n");
    StartWorld(dllTable, "c1a2", 42, true);
    Mock_CVarSetFloat("chaos_enabled", 0);
    g_deathCounter.ResetDeaths();
    Step(dllTable, 0.2f);
    g_edicts[1].v.health = -12; g_edicts[1].v.deadflag = DEAD_DYING;
    Step(dllTable, 0.2f); assert(g_deathCounter.GetTotalDeaths() == 1);
    StartWorld(dllTable, "c1a2", 2, true);
    Step(dllTable, 0.2f); assert(g_deathCounter.GetTotalDeaths() == 1);
    dllTable.pfnClientKill(&g_edicts[1]);
    g_edicts[1].v.health = -1; g_edicts[1].v.deadflag = DEAD_DYING;
    Step(dllTable, 0.2f); assert(g_deathCounter.GetTotalDeaths() == 2);
    DeathCounter fresh; fresh.Init(g_testGameDir); assert(fresh.GetTotalDeaths() == 2);
    StartWorld(dllTable, "c1a2", 1, true);
    g_chaos.CmdSpeedrun(); g_allMessages.clear(); Step(dllTable, 2);
    bool runHud = false;
    for (const auto &m : g_allMessages) if (!m.strings.empty() && m.strings[0].find("RUN:") != std::string::npos) runHud = true;
    assert(runHud);
    g_chaos.CmdTriggerById(40); Step(dllTable, 16);
    DeathCounter afterFlush; afterFlush.Init(g_testGameDir);
    assert(afterFlush.GetTotalEvents() == g_deathCounter.GetTotalEvents());
    assert(afterFlush.GetTotalDeaths() == 2);
    std::string oldStats = ReadText(statsPath);
    char blockedTemp[512];
    snprintf(blockedTemp, sizeof(blockedTemp), "%s/chaos_stats.cfg.tmp", g_testGameDir);
    assert(MKDIR(blockedTemp) == 0); // opening the temporary file must fail
    g_deathCounter.ResetDeaths();
    assert(ReadText(statsPath) == oldStats); // existing file was never truncated
    assert(RMDIR(blockedTemp) == 0);
    g_deathCounter.FlushNow();
    DeathCounter afterRetry; afterRetry.Init(g_testGameDir);
    assert(afterRetry.GetTotalDeaths() == 0);

    printf("\nALL 9 GOLDSRC CHAOS SIMULATION SUITES PASSED!\n");
    return 0;
}
