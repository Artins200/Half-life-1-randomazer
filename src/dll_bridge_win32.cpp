#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dll_bridge.h"

typedef void (__stdcall *PFN_GIVEFNPTRSTODLL)(enginefuncs_t *, globalvars_t *);
typedef int  (__cdecl   *PFN_GETENTITYAPI)(DLL_FUNCTIONS *, int);
typedef int  (__cdecl   *PFN_GETENTITYAPI2)(DLL_FUNCTIONS *, int *);
typedef int  (__cdecl   *PFN_GETNEWDLLFUNCTIONS)(NEW_DLL_FUNCTIONS *, int *);
typedef int  (__cdecl   *PFN_SERVER_GETBLENDING)(int, void **, void *, float *, float *);

static HMODULE g_hSelfModule = nullptr;
static HMODULE g_hGameModule = nullptr;
static bool    g_loadAttempted = false;

static PFN_GIVEFNPTRSTODLL    g_pfnOrigGiveFnptrs = nullptr;
static PFN_GETENTITYAPI       g_pfnOrigGetEntityAPI = nullptr;
static PFN_GETENTITYAPI2      g_pfnOrigGetEntityAPI2 = nullptr;
static PFN_GETNEWDLLFUNCTIONS g_pfnOrigGetNewDLLFunctions = nullptr;
static PFN_SERVER_GETBLENDING g_pfnOrigServerGetBlending = nullptr;

#define RVA_TO_VA(base, rva) ((uintptr_t)(base) + (uintptr_t)(rva))

static IMAGE_EXPORT_DIRECTORY *GetModuleExportDir(HMODULE mod) {
    if (!mod) return nullptr;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)mod;
    if (IsBadReadPtr(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return nullptr;
    }
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)RVA_TO_VA(mod, dos->e_lfanew);
    if (IsBadReadPtr(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE) {
        return nullptr;
    }
    DWORD expRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!expRva) return nullptr;
    IMAGE_EXPORT_DIRECTORY *expDir = (IMAGE_EXPORT_DIRECTORY *)RVA_TO_VA(mod, expRva);
    if (IsBadReadPtr(expDir, sizeof(*expDir))) return nullptr;
    return expDir;
}

static bool IsSameFilePath(const char *pathA, const char *pathB) {
    char fullA[MAX_PATH] = {0};
    char fullB[MAX_PATH] = {0};
    if (!GetFullPathNameA(pathA, MAX_PATH, fullA, nullptr)) return false;
    if (!GetFullPathNameA(pathB, MAX_PATH, fullB, nullptr)) return false;
    return _stricmp(fullA, fullB) == 0;
}

static void StripLastPathComponent(char *path) {
    size_t len = strlen(path);
    while (len > 0 && (path[len - 1] == '\\' || path[len - 1] == '/')) {
        path[--len] = '\0';
    }
    while (len > 0 && path[len - 1] != '\\' && path[len - 1] != '/') {
        path[--len] = '\0';
    }
    while (len > 0 && (path[len - 1] == '\\' || path[len - 1] == '/')) {
        path[--len] = '\0';
    }
}

bool Bridge_LoadOriginalGameDll() {
    if (g_hGameModule != nullptr) return true;
    if (g_loadAttempted) return false;
    g_loadAttempted = true;

    char selfPath[MAX_PATH] = {0};
    if (!GetModuleFileNameA(g_hSelfModule, selfPath, MAX_PATH)) {
        return false;
    }

    char dllDir[MAX_PATH];
    snprintf(dllDir, sizeof(dllDir), "%s", selfPath);
    StripLastPathComponent(dllDir); // ...\<mod>\dlls

    char modDir[MAX_PATH];
    snprintf(modDir, sizeof(modDir), "%s", dllDir);
    StripLastPathComponent(modDir); // ...\<mod>

    char hlRoot[MAX_PATH];
    snprintf(hlRoot, sizeof(hlRoot), "%s", modDir);
    StripLastPathComponent(hlRoot); // ...\Half-Life

    char candidates[4][MAX_PATH];
    snprintf(candidates[0], MAX_PATH, "%s\\hl_orig.dll", dllDir);
    snprintf(candidates[1], MAX_PATH, "%s\\valve\\dlls\\hl.dll", hlRoot);
    snprintf(candidates[2], MAX_PATH, "valve\\dlls\\hl.dll");
    snprintf(candidates[3], MAX_PATH, "..\\valve\\dlls\\hl.dll");

    for (int i = 0; i < 4; ++i) {
        if (IsSameFilePath(selfPath, candidates[i])) {
            continue;
        }
        DWORD attrs = GetFileAttributesA(candidates[i]);
        if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            continue;
        }
        HMODULE hLoaded = LoadLibraryA(candidates[i]);
        if (hLoaded && hLoaded != g_hSelfModule) {
            g_hGameModule = hLoaded;
            break;
        }
    }

    if (!g_hGameModule) {
        MessageBoxA(nullptr,
                    "Half-Life Randomizer Chaos could not find original 'valve\\dlls\\hl.dll'.\n"
                    "Make sure the 'hl_randomizer' folder is placed inside your 'Half-Life' directory!",
                    "HL1 Randomizer Chaos Error",
                    MB_OK | MB_ICONERROR);
        return false;
    }

    // Resolve original entry points; our export table remains untouched.
    g_pfnOrigGiveFnptrs        = (PFN_GIVEFNPTRSTODLL)GetProcAddress(g_hGameModule, "GiveFnptrsToDll");
    g_pfnOrigGetEntityAPI      = (PFN_GETENTITYAPI)GetProcAddress(g_hGameModule, "GetEntityAPI");
    g_pfnOrigGetEntityAPI2     = (PFN_GETENTITYAPI2)GetProcAddress(g_hGameModule, "GetEntityAPI2");
    g_pfnOrigGetNewDLLFunctions = (PFN_GETNEWDLLFUNCTIONS)GetProcAddress(g_hGameModule, "GetNewDLLFunctions");
    g_pfnOrigServerGetBlending = (PFN_SERVER_GETBLENDING)GetProcAddress(g_hGameModule, "Server_GetBlendingInterface");

    if (!g_pfnOrigGiveFnptrs || (!g_pfnOrigGetEntityAPI && !g_pfnOrigGetEntityAPI2)) {
        MessageBoxA(nullptr, "The original Half-Life game DLL has incompatible exports.",
                    "HL1 Randomizer Chaos Error", MB_OK | MB_ICONERROR);
        FreeLibrary(g_hGameModule);
        g_hGameModule = nullptr;
        return false;
    }
    return true;
}

bool Bridge_IsLoaded() {
    return (g_hGameModule != nullptr);
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
    if (!g_hGameModule || !pName || !pName[0]) {
        return 0;
    }
    FARPROC fp = GetProcAddress(g_hGameModule, pName);
    return (uint32_t)(uintptr_t)fp;
}

const char *Bridge_ResolveNameForFunction(uint32_t functionAddr) {
    if (!g_hGameModule || functionAddr == 0) {
        return nullptr;
    }
    // Save/restore and SetThink repeatedly ask for the same functions.
    // A bounded direct-mapped cache avoids scanning the PE export list each time.
    struct NameCacheEntry { uint32_t address; const char *name; };
    static NameCacheEntry cache[256] = {};
    NameCacheEntry &slot = cache[(functionAddr >> 4) % 256];
    if (slot.address == functionAddr) return slot.name;
    slot.address = functionAddr;
    slot.name = nullptr;
    IMAGE_EXPORT_DIRECTORY *expGame = GetModuleExportDir(g_hGameModule);
    if (!expGame) {
        return nullptr;
    }

    const DWORD *gameFuncs = (const DWORD *)RVA_TO_VA(g_hGameModule, expGame->AddressOfFunctions);
    const DWORD *gameNames = (const DWORD *)RVA_TO_VA(g_hGameModule, expGame->AddressOfNames);
    const WORD  *gameOrds  = (const WORD  *)RVA_TO_VA(g_hGameModule, expGame->AddressOfNameOrdinals);

    for (DWORD i = 0; i < expGame->NumberOfNames; ++i) {
        WORD ord = gameOrds[i];
        if (ord < expGame->NumberOfFunctions) {
            uint32_t va = (uint32_t)RVA_TO_VA(g_hGameModule, gameFuncs[ord]);
            if (va == functionAddr) {
                slot.name = (const char *)RVA_TO_VA(g_hGameModule, gameNames[i]);
                return slot.name;
            }
        }
    }
    return nullptr;
}

// Static, real PE exports: never patch the Windows loader's export table or
// fabricate RVAs outside the image. GoldSrc can resolve these normally.
typedef void (__cdecl *PFN_ENTITY_FN)(entvars_t *);
#define HL_ENTITY_LINK(entityName) \
    extern "C" void __cdecl entityName(entvars_t *pev) { \
        static PFN_ENTITY_FN fn = nullptr; \
        if (!fn && Bridge_LoadOriginalGameDll()) \
            fn = (PFN_ENTITY_FN)GetProcAddress(g_hGameModule, #entityName); \
        if (fn) fn(pev); \
    }
#include "hl_entity_list.inc"
#undef HL_ENTITY_LINK

extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_hSelfModule = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
    }
    // Do not call FreeLibrary from DllMain: the loader lock is held here and
    // unloading another game DLL can deadlock or crash during shutdown. The
    // OS releases the original module on process exit.
    return TRUE;
}

#endif // _WIN32
