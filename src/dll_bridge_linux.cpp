#ifndef _WIN32

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dll_bridge.h"

typedef void (*PFN_GIVEFNPTRSTODLL)(enginefuncs_t *, globalvars_t *);
typedef int  (*PFN_GETENTITYAPI)(DLL_FUNCTIONS *, int);
typedef int  (*PFN_GETENTITYAPI2)(DLL_FUNCTIONS *, int *);
typedef int  (*PFN_GETNEWDLLFUNCTIONS)(NEW_DLL_FUNCTIONS *, int *);
typedef int  (*PFN_SERVER_GETBLENDING)(int, void **, void *, float *, float *);
typedef void (*PFN_ENTITY_FN)(entvars_t *);

static void *g_hGameSo = nullptr;
static bool g_loadAttempted = false;

static PFN_GIVEFNPTRSTODLL    g_pfnOrigGiveFnptrs = nullptr;
static PFN_GETENTITYAPI       g_pfnOrigGetEntityAPI = nullptr;
static PFN_GETENTITYAPI2      g_pfnOrigGetEntityAPI2 = nullptr;
static PFN_GETNEWDLLFUNCTIONS g_pfnOrigGetNewDLLFunctions = nullptr;
static PFN_SERVER_GETBLENDING g_pfnOrigServerGetBlending = nullptr;

static void StripComponent(char *path) {
    size_t len = strlen(path);
    while (len > 0 && path[len - 1] == '/') path[--len] = '\0';
    while (len > 0 && path[len - 1] != '/') path[--len] = '\0';
    while (len > 0 && path[len - 1] == '/') path[--len] = '\0';
}

bool Bridge_LoadOriginalGameDll() {
    if (g_hGameSo != nullptr) return true;
    if (g_loadAttempted) return false;
    g_loadAttempted = true;

    Dl_info selfInfo;
    memset(&selfInfo, 0, sizeof(selfInfo));
    char selfReal[PATH_MAX] = {0};

    if (dladdr((void *)&Bridge_LoadOriginalGameDll, &selfInfo) && selfInfo.dli_fname) {
        if (!realpath(selfInfo.dli_fname, selfReal)) {
            snprintf(selfReal, sizeof(selfReal), "%s", selfInfo.dli_fname);
        }
    }

    char dllDir[PATH_MAX];
    snprintf(dllDir, sizeof(dllDir), "%s", selfReal);
    StripComponent(dllDir); // .../<mod>/dlls

    char modDir[PATH_MAX];
    snprintf(modDir, sizeof(modDir), "%s", dllDir);
    StripComponent(modDir); // .../<mod>

    char hlRoot[PATH_MAX];
    snprintf(hlRoot, sizeof(hlRoot), "%s", modDir);
    StripComponent(hlRoot); // .../Half-Life

    char candidates[6][PATH_MAX];
    snprintf(candidates[0], PATH_MAX, "%s/hl_orig.so", dllDir);
    snprintf(candidates[1], PATH_MAX, "%s/valve/dlls/hl.so", hlRoot);
    snprintf(candidates[2], PATH_MAX, "%s/valve/dlls/hl_i386.so", hlRoot);
    snprintf(candidates[3], PATH_MAX, "valve/dlls/hl.so");
    snprintf(candidates[4], PATH_MAX, "valve/dlls/hl_i386.so");
    snprintf(candidates[5], PATH_MAX, "../valve/dlls/hl.so");

    for (int i = 0; i < 6; ++i) {
        char candReal[PATH_MAX] = {0};
        if (realpath(candidates[i], candReal) && selfReal[0] != '\0') {
            if (strcmp(candReal, selfReal) == 0) {
                continue;
            }
        }
        if (access(candidates[i], R_OK) != 0) {
            continue;
        }
        void *handle = dlopen(candidates[i], RTLD_NOW | RTLD_LOCAL);
        if (handle) {
            g_hGameSo = handle;
            break;
        }
    }

    if (!g_hGameSo) {
        fprintf(stderr, "[HL1 Chaos Mod] Could not find original valve/dlls/hl.so!\n");
        return false;
    }

    g_pfnOrigGiveFnptrs         = (PFN_GIVEFNPTRSTODLL)dlsym(g_hGameSo, "GiveFnptrsToDll");
    g_pfnOrigGetEntityAPI       = (PFN_GETENTITYAPI)dlsym(g_hGameSo, "GetEntityAPI");
    g_pfnOrigGetEntityAPI2      = (PFN_GETENTITYAPI2)dlsym(g_hGameSo, "GetEntityAPI2");
    g_pfnOrigGetNewDLLFunctions = (PFN_GETNEWDLLFUNCTIONS)dlsym(g_hGameSo, "GetNewDLLFunctions");
    g_pfnOrigServerGetBlending  = (PFN_SERVER_GETBLENDING)dlsym(g_hGameSo, "Server_GetBlendingInterface");

    if (!g_pfnOrigGiveFnptrs || (!g_pfnOrigGetEntityAPI && !g_pfnOrigGetEntityAPI2)) {
        fprintf(stderr, "[HL1 Chaos Mod] Original game library has incompatible exports.\n");
        dlclose(g_hGameSo);
        g_hGameSo = nullptr;
        return false;
    }
    return true;
}

bool Bridge_IsLoaded() {
    return (g_hGameSo != nullptr);
}

void Bridge_ForwardGiveFnptrsToDll(enginefuncs_t *pHookedEngfuncs, globalvars_t *pGlobals) {
    if (g_pfnOrigGiveFnptrs) {
        g_pfnOrigGiveFnptrs(pHookedEngfuncs, pGlobals);
    }
}

int Bridge_ForwardGetEntityAPI(DLL_FUNCTIONS *pFunctionTable, int interfaceVersion) {
    if (g_pfnOrigGetEntityAPI) {
        return g_pfnOrigGetEntityAPI(pFunctionTable, interfaceVersion);
    }
    if (g_pfnOrigGetEntityAPI2) {
        int version = interfaceVersion;
        return g_pfnOrigGetEntityAPI2(pFunctionTable, &version);
    }
    return FALSE;
}

int Bridge_ForwardGetEntityAPI2(DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion) {
    if (g_pfnOrigGetEntityAPI2) {
        return g_pfnOrigGetEntityAPI2(pFunctionTable, interfaceVersion);
    }
    if (g_pfnOrigGetEntityAPI && interfaceVersion) {
        return g_pfnOrigGetEntityAPI(pFunctionTable, *interfaceVersion);
    }
    return FALSE;
}

int Bridge_ForwardGetNewDLLFunctions(NEW_DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion) {
    if (g_pfnOrigGetNewDLLFunctions) {
        return g_pfnOrigGetNewDLLFunctions(pFunctionTable, interfaceVersion);
    }
    return FALSE;
}

int Bridge_ForwardServerGetBlendingInterface(int version, void **ppinterface, void *pstudio, float *rotationmatrix, float *bonetransform) {
    if (g_pfnOrigServerGetBlending) {
        return g_pfnOrigServerGetBlending(version, ppinterface, pstudio, rotationmatrix, bonetransform);
    }
    return FALSE;
}

uint32_t Bridge_ResolveFunctionFromName(const char *pName) {
    if (!g_hGameSo || !pName || !pName[0]) {
        return 0;
    }
    void *sym = dlsym(g_hGameSo, pName);
    return (uint32_t)(uintptr_t)sym;
}

const char *Bridge_ResolveNameForFunction(uint32_t functionAddr) {
    if (functionAddr == 0) {
        return nullptr;
    }
    struct NameCacheEntry { uint32_t address; const char *name; };
    static NameCacheEntry cache[256] = {};
    NameCacheEntry &slot = cache[(functionAddr >> 4) % 256];
    if (slot.address == functionAddr) return slot.name;
    slot.address = functionAddr;
    slot.name = nullptr;
    Dl_info info;
    memset(&info, 0, sizeof(info));
    Dl_info original;
    memset(&original, 0, sizeof(original));
    void *address = (void *)(uintptr_t)functionAddr;
    if (g_pfnOrigGiveFnptrs && dladdr((void *)g_pfnOrigGiveFnptrs, &original) &&
        dladdr(address, &info) && info.dli_sname && info.dli_saddr == address &&
        info.dli_fbase == original.dli_fbase) {
        slot.name = info.dli_sname;
        return slot.name;
    }
    return nullptr;
}

// Forward all 250 canonical Half-Life 1 entity spawn symbols on Linux
#define HL_ENTITY_LINK(entityName) \
    extern "C" __attribute__((visibility("default"))) void entityName(entvars_t *pev) { \
        static PFN_ENTITY_FN s_fn = nullptr; \
        if (!s_fn && g_hGameSo) { \
            s_fn = (PFN_ENTITY_FN)dlsym(g_hGameSo, #entityName); \
        } \
        if (s_fn) { \
            s_fn(pev); \
        } \
    }

#include "hl_entity_list.inc"

#undef HL_ENTITY_LINK

#endif // !_WIN32
