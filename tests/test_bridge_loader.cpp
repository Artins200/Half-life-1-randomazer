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
static int PrecacheSound(char *) { ++soundCalls; return 1; }
static edict_t *EntityOfIndex(int) { return nullptr; }

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
    const char pool[] = "\0worldspawn\0";
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
    puts(sizeof(void *) == 4 ? "[OK] Shipped x86 proxy forwards, saves, and resolves original function names" :
                              "[OK] Linux proxy loads and forwards to the fake original DLL");
    return 0;
}
