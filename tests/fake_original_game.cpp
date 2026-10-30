// Minimal original DLL for the real proxy-loader integration test (not shipped).
#include "hlsdk_mini.h"
#include <string.h>

#ifdef _WIN32
#define FAKE_EXPORT extern "C" __declspec(dllexport)
#define FAKE_GIVE_CALL __stdcall
#else
#define FAKE_EXPORT extern "C"
#define FAKE_GIVE_CALL
#endif

static enginefuncs_t *engine = nullptr;
static int initCalls = 0;
static int spawnCalls = 0;
static int saveCalls = 0;
static int restoreCalls = 0;
static entvars_t savedVars;
static int namedThinkCalls = 0;

FAKE_EXPORT void FAKE_GIVE_CALL GiveFnptrsToDll(enginefuncs_t *funcs, globalvars_t *) { engine = funcs; }
static void FakeGameInit() {
    ++initCalls;
    if (engine && engine->pfnRegUserMsg) {
        engine->pfnRegUserMsg("SetFOV", 1);
        engine->pfnRegUserMsg("ScreenFade", 10);
        engine->pfnRegUserMsg("ScreenShake", 6);
    }
}
static int FakeSpawn(edict_t *) {
    ++spawnCalls;
    if (engine && engine->pfnPrecacheSound) engine->pfnPrecacheSound((char *)"fvox/beep.wav");
    return 17;
}
static void FakeSave(edict_t *entity, SAVERESTOREDATA *) {
    ++saveCalls;
    if (entity) savedVars = entity->v;
}
static int FakeRestore(edict_t *entity, SAVERESTOREDATA *, int) {
    ++restoreCalls;
    if (!entity) return 0;
    entity->v = savedVars;
    return 1;
}
static const char *FakeDescription() { return "original"; }
// Intentionally export only GetEntityAPI; the proxy must adapt API2 requests.
FAKE_EXPORT int GetEntityAPI(DLL_FUNCTIONS *table, int version) {
    if (!table || version != INTERFACE_VERSION || !engine) return FALSE;
    memset(table, 0, sizeof(*table));
    table->pfnGameInit = FakeGameInit;
    table->pfnSpawn = FakeSpawn;
    table->pfnSave = FakeSave;
    table->pfnRestore = FakeRestore;
    table->pfnGetGameDescription = FakeDescription;
    return TRUE;
}
FAKE_EXPORT void worldspawn(entvars_t *v) { if (v) v->health = 321.0f; }
// Prove that OF-only weapon and projectile entity wrappers forward into the
// selected original DLL, rather than merely existing in the export table.
FAKE_EXPORT void weapon_eagle(entvars_t *v) { if (v) v->health = 444.0f; }
FAKE_EXPORT void spore(entvars_t *v) { if (v) v->health = 555.0f; }
FAKE_EXPORT int fake_init_calls() { return initCalls; }
FAKE_EXPORT int fake_spawn_calls() { return spawnCalls; }
FAKE_EXPORT int fake_has_legacy_fallback() {
    return engine && engine->pfnPEntityOfEntIndex &&
           engine->pfnPEntityOfEntIndexAllEntities == engine->pfnPEntityOfEntIndex;
}

// Save/restore resolves function names against the original library rather
// than the proxy's export table; exercise the true 32-bit pointer ABI.
FAKE_EXPORT void fake_named_think() { ++namedThinkCalls; }
FAKE_EXPORT int fake_function_roundtrip() {
    if (!engine || !engine->pfnFunctionFromName || !engine->pfnNameForFunction) return 0;
    uint32_t address = engine->pfnFunctionFromName("fake_named_think");
    if (!address) return 0;
    const char *name = engine->pfnNameForFunction(address);
    const char *cached = engine->pfnNameForFunction(address);
    return name && cached && strcmp(name, "fake_named_think") == 0 && strcmp(cached, name) == 0;
}
FAKE_EXPORT int fake_save_calls() { return saveCalls; }
FAKE_EXPORT int fake_restore_calls() { return restoreCalls; }
