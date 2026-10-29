#ifdef NDEBUG
#undef NDEBUG
#endif
#include "hlsdk_mini.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static void *OpenModule(const char *path) { return (void *)LoadLibraryA(path); }
static void *FindModuleSymbol(void *module, const char *name) { return (void *)GetProcAddress((HMODULE)module, name); }
static void CloseModule(void *module) { FreeLibrary((HMODULE)module); }
#define GIVE_CALL __stdcall
#else
#include <dlfcn.h>
static void *OpenModule(const char *path) { return dlopen(path, RTLD_NOW | RTLD_LOCAL); }
static void *FindModuleSymbol(void *module, const char *name) { return dlsym(module, name); }
static void CloseModule(void *module) { dlclose(module); }
#define GIVE_CALL
#endif

typedef void (GIVE_CALL *GiveFn)(enginefuncs_t *, globalvars_t *);
typedef int (*GetApi2Fn)(DLL_FUNCTIONS *, int *);
typedef void (*EntityFn)(entvars_t *);
typedef int (*ProbeFn)();

static int soundCalls = 0;
static edict_t *activePlayer = nullptr;
static int eventBanners = 0, countdowns = 0, oldSafeMode = 0, fixedVersionHud = 0;
static int PrecacheSound(char *) { ++soundCalls; return 1; }
static edict_t *EntityOfIndex(int index) { return index == 1 ? activePlayer : nullptr; }
static int RegisterUserMsg(const char *name, int) {
    if (!strcmp(name, "SetFOV")) return 1;
    if (!strcmp(name, "ScreenFade")) return 2;
    if (!strcmp(name, "ScreenShake")) return 3;
    return 4;
}
static float CVarGetFloat(const char *name) {
    if (!strcmp(name, "chaos_enabled")) return 1;
    if (!strcmp(name, "chaos_interval")) return 10;
    return 0;
}
static void MessageBegin(int, int, const float *, edict_t *) {}
static void MessageEnd() {}
static void WriteInt(int) {}
static void WriteCoord(float) {}
static void WriteString(const char *s) {
    if (!s) return;
    if (strstr(s, ">>> RANDOM EVENT #")) ++eventBanners;
    if (strstr(s, "NEXT:")) ++countdowns;
    if (strstr(s, "STORY / VEHICLE SAFE MODE")) ++oldSafeMode;
    if (strstr(s, "HL1 CHAOS v1.1")) ++fixedVersionHud;
}
static int CmdArgc() { return 2; }
static const char *CmdArgv(int n) { return n == 0 ? "chaos_trigger" : "14"; }

int main(int argc, char **argv) {
    assert(argc == 3);
    bool expectOriginal = strcmp(argv[2], "present") == 0;
    void *proxy = OpenModule(argv[1]);
    if (!proxy) {
#ifdef _WIN32
        fprintf(stderr, "LoadLibrary proxy failed: %lu\n", (unsigned long)GetLastError());
#else
        fprintf(stderr, "dlopen proxy: %s\n", dlerror());
#endif
        return 1;
    }
    GiveFn give = (GiveFn)FindModuleSymbol(proxy, "GiveFnptrsToDll");
    GetApi2Fn api2 = (GetApi2Fn)FindModuleSymbol(proxy, "GetEntityAPI2");
    EntityFn worldspawn = (EntityFn)FindModuleSymbol(proxy, "worldspawn");
    assert(give && api2 && worldspawn);
    enginefuncs_t engine = {};
    engine.pfnPrecacheSound = PrecacheSound;
    engine.pfnPEntityOfEntIndex = EntityOfIndex;
    engine.pfnRegUserMsg = RegisterUserMsg;
    engine.pfnCVarGetFloat = CVarGetFloat;
    engine.pfnMessageBegin = MessageBegin;
    engine.pfnMessageEnd = MessageEnd;
    engine.pfnWriteByte = WriteInt;
    engine.pfnWriteShort = WriteInt;
    engine.pfnWriteCoord = WriteCoord;
    engine.pfnWriteString = WriteString;
    engine.pfnCmd_Argc = CmdArgc;
    engine.pfnCmd_Argv = CmdArgv;
    const char pool[] = "\0worldspawn\0c0a0\0c1a0\0";
    globalvars_t globals = {};
    globals.pStringBase = pool;
    give(&engine, &globals);
    DLL_FUNCTIONS dll = {};
    int version = INTERFACE_VERSION;
    if (!expectOriginal) {
        assert(api2(&dll, &version) == FALSE);
        puts("[OK] Missing original DLL fails cleanly");
        CloseModule(proxy);
        return 0;
    }
    assert(api2(&dll, &version) == TRUE); // GetEntityAPI2 -> original GetEntityAPI
    assert(dll.pfnGameInit && dll.pfnSpawn && dll.pfnSave && dll.pfnGetGameDescription);
    dll.pfnGameInit();
    edict_t world = {};
    world.v.classname = 1;
    assert(dll.pfnSpawn(&world) == 17 && soundCalls == 1);
    dll.pfnSave(&world, nullptr);
    assert(strcmp(dll.pfnGetGameDescription(), "HL1: Randomizer Chaos") == 0);
    worldspawn(&world.v);
    assert(world.v.health == 321.0f);

    // Drive the actual loaded proxy through the intro and c1a0 without HEV.
    // This catches a stale shipped binary that merely passes export checks.
    assert(dll.pfnServerActivate && dll.pfnServerDeactivate && dll.pfnClientPutInServer &&
           dll.pfnPlayerPreThink && dll.pfnStartFrame && dll.pfnPlayerPostThink && dll.pfnClientCommand);
    edict_t player = {};
    player.pvPrivateData = &player;
    player.v.pContainingEntity = &player;
    player.v.flags = FL_CLIENT | FL_ONTRAIN | FL_ONGROUND;
    player.v.movetype = MOVETYPE_WALK;
    player.v.health = 100;
    player.v.maxspeed = 320;
    player.v.deadflag = DEAD_NO;
    activePlayer = &player;
    globals.mapname = 12; // c0a0 in pool above
    globals.time = 1;
    globals.maxClients = 1;
    globals.maxEntities = 128;
    dll.pfnServerActivate(&world, 2, 1);
    dll.pfnClientPutInServer(&player);
    for (int i = 0; i < 130; ++i) {
        globals.time += 0.2f;
        dll.pfnPlayerPreThink(&player);
        dll.pfnStartFrame();
        dll.pfnPlayerPostThink(&player);
    }
    assert(eventBanners == 2 && countdowns > 0 && oldSafeMode == 0 && fixedVersionHud > 0);
    dll.pfnServerDeactivate();

    globals.mapname = 17; // c1a0
    globals.time = 1;
    player.v.flags = FL_CLIENT | FL_ONGROUND;
    player.v.movetype = MOVETYPE_WALK;
    player.v.health = 100;
    player.v.weapons = 0; // no HEV
    player.v.fov = 0;
    player.v.maxspeed = 320;
    eventBanners = countdowns = oldSafeMode = fixedVersionHud = 0;
    dll.pfnServerActivate(&world, 2, 1);
    dll.pfnClientPutInServer(&player);
    for (int i = 0; i < 60; ++i) {
        globals.time += 0.2f;
        dll.pfnPlayerPreThink(&player);
        dll.pfnStartFrame();
        dll.pfnPlayerPostThink(&player);
    }
    assert(eventBanners == 1 && countdowns > 0 && oldSafeMode == 0 && fixedVersionHud > 0);
    dll.pfnClientCommand(&player); // chaos_trigger 14: real speed effect before HEV
    assert(eventBanners == 2 && player.v.maxspeed == 720);
    dll.pfnServerDeactivate();
    activePlayer = nullptr;

    char originalPath[1024];
    snprintf(originalPath, sizeof(originalPath), "%s", argv[1]);
    char *slash = strrchr(originalPath, '/');
#ifdef _WIN32
    char *backslash = strrchr(originalPath, '\\');
    if (!slash || (backslash && backslash > slash)) slash = backslash;
    const char *originalName = "hl_orig.dll";
#else
    const char *originalName = "hl_orig.so";
#endif
    assert(slash);
    snprintf(slash + 1, sizeof(originalPath) - (size_t)(slash + 1 - originalPath), "%s", originalName);
    void *original = OpenModule(originalPath);
    assert(original);
    ProbeFn init = (ProbeFn)FindModuleSymbol(original, "fake_init_calls");
    ProbeFn spawn = (ProbeFn)FindModuleSymbol(original, "fake_spawn_calls");
    ProbeFn fallback = (ProbeFn)FindModuleSymbol(original, "fake_has_legacy_fallback");
    ProbeFn save = (ProbeFn)FindModuleSymbol(original, "fake_save_calls");
    ProbeFn roundtrip = (ProbeFn)FindModuleSymbol(original, "fake_function_roundtrip");
    assert(init && spawn && fallback && save && roundtrip);
    assert(init() == 1 && spawn() == 1 && fallback() == 1 && save() == 1);
    if (sizeof(void *) == 4) assert(roundtrip() == 1);
    CloseModule(original);
    CloseModule(proxy);
    puts(sizeof(void *) == 4 ? "[OK] Shipped x86 proxy forwards, saves, resolves names, and runs intro chaos" :
                              "[OK] Proxy forwards to the fake original and runs intro chaos");
    return 0;
}
