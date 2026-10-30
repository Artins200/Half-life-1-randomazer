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
static edict_t *worldEdict = nullptr;
static edict_t pickupEdict = {};
static globalvars_t *engineGlobals = nullptr;
static const char stringPool[] = "\0worldspawn\0c0a0\0c1a0\0weapon_eagle\0";
static int sphereSearchCalls = 0;
static const char *triggerId = "14";
static int eventBanners = 0, countdowns = 0, oldSafeMode = 0, fixedVersionHud = 0;
static bool clientBufferInitialized = true;
static int PrecacheSound(char *) { ++soundCalls; return 1; }
static edict_t *EntityOfIndex(int index) {
    return index == 1 ? activePlayer : (index == 2 && !pickupEdict.free ? &pickupEdict : nullptr);
}
static int IndexOfEdict(const edict_t *edict) { return edict == activePlayer ? 1 : (edict == &pickupEdict ? 2 : 0); }
static int EntOffsetOfPEntity(const edict_t *edict) { return IndexOfEdict(edict) * (int)sizeof(edict_t); }
static int AllocString(const char *name) { return name && strcmp(name, "weapon_eagle") == 0 ? 22 : 0; }
static edict_t *CreateNamedEntity(int classname) {
    assert(engineGlobals && engineGlobals->pStringBase);
    const char *name = reinterpret_cast<const char *>(
        reinterpret_cast<uintptr_t>(engineGlobals->pStringBase) + (uint32_t)classname);
    assert(strcmp(name, "weapon_eagle") == 0);
    memset(&pickupEdict, 0, sizeof(pickupEdict));
    pickupEdict.v.classname = (string_t)classname;
    pickupEdict.v.pContainingEntity = &pickupEdict;
    pickupEdict.v.solid = SOLID_TRIGGER;
    pickupEdict.pvPrivateData = &pickupEdict;
    return &pickupEdict;
}
static void RemoveEntity(edict_t *entity) { if (entity) entity->free = TRUE; }
static int NumberOfEntities() { return pickupEdict.free ? 2 : 3; }
static edict_t *FindEntityInSphere(edict_t *start, const float *, float) {
    assert(start != worldEdict && "searched past the GoldSrc worldspawn sentinel");
    ++sphereSearchCalls;
    return start ? worldEdict : activePlayer;
}
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
static void MessageBegin(int dest, int, const float *, edict_t *) {
    if (dest == MSG_ONE || dest == MSG_ONE_UNRELIABLE)
        assert(clientBufferInitialized && "SZ_GetSpace: uninitialized client sizebuf_t");
}
static void MessageEnd() {}
static void WriteInt(int) {}
static void WriteCoord(float) {}
static void WriteString(const char *s) {
    if (!s) return;
    if (strstr(s, ">>> RANDOM EVENT #")) ++eventBanners;
    if (strstr(s, "NEXT:")) ++countdowns;
    if (strstr(s, "STORY / VEHICLE SAFE MODE")) ++oldSafeMode;
    if (strstr(s, "HL1 CHAOS v1.3.1")) ++fixedVersionHud;
}
static int CmdArgc() { return 2; }
static const char *CmdArgv(int n) { return n == 0 ? "chaos_trigger" : triggerId; }

int main(int argc, char **argv) {
    assert(argc == 3);
    const bool opforI386 = strcmp(argv[2], "opfor_i386") == 0;
    const bool opforProfile = strcmp(argv[2], "opfor") == 0 || strcmp(argv[2], "opfor_missing") == 0 || opforI386;
    const bool expectOriginal = strcmp(argv[2], "present") == 0 || strcmp(argv[2], "opfor") == 0 || opforI386;
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
    engine.pfnEntOffsetOfPEntity = EntOffsetOfPEntity;
    engine.pfnIndexOfEdict = IndexOfEdict;
    engine.pfnAllocString = AllocString;
    engine.pfnCreateNamedEntity = CreateNamedEntity;
    engine.pfnRemoveEntity = RemoveEntity;
    engine.pfnNumberOfEntities = NumberOfEntities;
    engine.pfnFindEntityInSphere = FindEntityInSphere;
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
    globalvars_t globals = {};
    globals.pStringBase = stringPool;
    engineGlobals = &globals;
    pickupEdict.free = TRUE;
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
    worldEdict = &world;
    world.v.classname = 1;
    assert(dll.pfnSpawn(&world) == 17 && soundCalls == 1);
    dll.pfnSave(&world, nullptr);
    assert(strcmp(dll.pfnGetGameDescription(), "HL1: Randomizer Chaos") == 0);
    worldspawn(&world.v);
    assert(world.v.health == 321.0f);
    if (opforProfile) {
        EntityFn eagle = (EntityFn)FindModuleSymbol(proxy, "weapon_eagle");
        EntityFn spore = (EntityFn)FindModuleSymbol(proxy, "spore");
        assert(eagle && spore);
        eagle(&world.v); assert(world.v.health == 444.0f);
        spore(&world.v); assert(world.v.health == 555.0f);
    }

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

    // Exercise the actual loaded binary with an uninitialized client buffer at
    // deactivation and a restored FL_CLIENT edict before client signon.
    globals.time += 0.4f;
    triggerId = "33";
    dll.pfnClientCommand(&player);
    assert(player.v.fov == 130);
    assert(dll.pfnRestore);
    dll.pfnSave(&player, nullptr);
    assert(player.v.fov == 130 && player.v.maxspeed == 720); // live effects restored after serialization
    clientBufferInitialized = false;
    dll.pfnServerDeactivate();
    globals.time = 1;
    // Restore through the original game DLL, just as the engine does. The
    // saved player contains the values from before the temporary effects.
    player.v.fov = 77;
    player.v.maxspeed = 77;
    assert(dll.pfnRestore(&player, nullptr, 0) == 1);
    assert(player.v.fov == 0 && player.v.maxspeed == 320 && (player.v.flags & FL_CLIENT));
    dll.pfnServerActivate(&world, 2, 1);
    // Save loads skip ClientPutInServer; merely seeing FL_CLIENT isn't enough.
    eventBanners = countdowns = 0;
    for (int i = 0; i < 70; ++i) {
        globals.time += 0.2f;
        dll.pfnStartFrame();
    }
    assert(eventBanners == 0 && countdowns == 0);
    clientBufferInitialized = true;
    dll.pfnPlayerPreThink(&player);
    for (int i = 0; i < 55; ++i) {
        globals.time += 0.2f;
        dll.pfnPlayerPreThink(&player);
        dll.pfnStartFrame();
        dll.pfnPlayerPostThink(&player);
    }
    assert(eventBanners == 1 && countdowns > 0);

    // Run the bomb in a fresh session of the actual loaded binary. The fake
    // engine returns worldspawn at end-of-search, exactly as GoldSrc does;
    // the old DLL would resume from it and trip the assertion in the mock.
    dll.pfnServerDeactivate();
    globals.time = 1;
    player.v.health = 100;
    player.v.flags = FL_CLIENT | FL_ONGROUND;
    player.v.velocity = Vec3(0, 0, 0);
    dll.pfnServerActivate(&world, 2, 1);
    dll.pfnClientPutInServer(&player);
    dll.pfnPlayerPreThink(&player); // first spawned-client user command
    triggerId = "27";
    eventBanners = sphereSearchCalls = 0;
    dll.pfnClientCommand(&player);
    assert(eventBanners == 1);
    for (int i = 0; i < 18; ++i) {
        globals.time += 0.2f;
        dll.pfnPlayerPreThink(&player);
        dll.pfnStartFrame();
        dll.pfnPlayerPostThink(&player);
    }
    if (sphereSearchCalls != 2 || player.v.health != 85 || player.v.velocity.z <= 0)
        fprintf(stderr, "Bomb smoke test: sphere calls=%d, hp=%g, velocity.z=%g\n",
                sphereSearchCalls, (double)player.v.health, (double)player.v.velocity.z);
    assert(sphereSearchCalls == 2 && player.v.health == 85 && player.v.velocity.z > 0);

    if (opforProfile) {
        // The loaded DLL must recognize the high-bit class created by the
        // fake Gearbox worldspawn, then deliver it via the original Touch.
        player.v.weapons |= (1u << WEAPON_SUIT);
        triggerId = "494";
        int previousBanners = eventBanners;
        globals.time += 0.4f;
        dll.pfnClientCommand(&player);
        assert(eventBanners == previousBanners + 1);
        globals.time += 0.3f;
        dll.pfnPlayerPreThink(&player);
        dll.pfnStartFrame();
        assert((player.v.weapons & (1 << 5)) != 0);
    }
    dll.pfnServerDeactivate();
    activePlayer = nullptr;
    worldEdict = nullptr;

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
    if (opforProfile) {
        // .../<Half-Life>/hl_randomizer_opfor/dlls/hl.* -> gearbox/dlls/opfor.*
        *slash = '\0';
        char *second = strrchr(originalPath, '/');
#ifdef _WIN32
        backslash = strrchr(originalPath, '\\');
        if (!second || (backslash && backslash > second)) second = backslash;
#endif
        assert(second); *second = '\0';
        char *third = strrchr(originalPath, '/');
#ifdef _WIN32
        backslash = strrchr(originalPath, '\\');
        if (!third || (backslash && backslash > third)) third = backslash;
#endif
        assert(third);
#ifdef _WIN32
        snprintf(third + 1, sizeof(originalPath) - (size_t)(third + 1 - originalPath), "gearbox\\dlls\\opfor.dll");
#else
        snprintf(third + 1, sizeof(originalPath) - (size_t)(third + 1 - originalPath), "%s",
                 opforI386 ? "gearbox/dlls/opfor_i386.so" : "gearbox/dlls/opfor.so");
#endif
    } else {
        snprintf(slash + 1, sizeof(originalPath) - (size_t)(slash + 1 - originalPath), "%s", originalName);
    }
    void *original = OpenModule(originalPath);
    assert(original);
    ProbeFn init = (ProbeFn)FindModuleSymbol(original, "fake_init_calls");
    ProbeFn spawn = (ProbeFn)FindModuleSymbol(original, "fake_spawn_calls");
    ProbeFn pickup = (ProbeFn)FindModuleSymbol(original, "fake_pickup_calls");
    ProbeFn fallback = (ProbeFn)FindModuleSymbol(original, "fake_has_legacy_fallback");
    ProbeFn save = (ProbeFn)FindModuleSymbol(original, "fake_save_calls");
    ProbeFn restore = (ProbeFn)FindModuleSymbol(original, "fake_restore_calls");
    ProbeFn roundtrip = (ProbeFn)FindModuleSymbol(original, "fake_function_roundtrip");
    assert(init && spawn && pickup && fallback && save && restore && roundtrip);
    assert(init() == 1 && spawn() == (opforProfile ? 2 : 1) && pickup() == (opforProfile ? 1 : 0) &&
           fallback() == 1 && save() == 2 && restore() == 1);
    if (sizeof(void *) == 4) assert(roundtrip() == 1);
    CloseModule(original);
    CloseModule(proxy);
    if (opforProfile) {
        puts(sizeof(void *) == 4 ? "[OK] Shipped x86 OF proxy forwards and delivers a high-bit precached weapon" :
                                  "[OK] OF proxy forwards and delivers a high-bit precached weapon");
    } else {
        puts(sizeof(void *) == 4 ? "[OK] Shipped x86 proxy forwards, runs intro chaos and detonates bomb" :
                                  "[OK] Proxy forwards, runs intro chaos and detonates bomb");
    }
    return 0;
}
