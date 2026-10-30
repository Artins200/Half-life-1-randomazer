#ifndef DLL_BRIDGE_H
#define DLL_BRIDGE_H

#include "hlsdk_mini.h"

bool Bridge_LoadOriginalGameDll();
bool Bridge_IsLoaded();

void Bridge_ForwardGiveFnptrsToDll(enginefuncs_t *pHookedEngfuncs, globalvars_t *pGlobals);
int  Bridge_ForwardGetEntityAPI(DLL_FUNCTIONS *pFunctionTable, int interfaceVersion);
int  Bridge_ForwardGetEntityAPI2(DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion);
int  Bridge_ForwardGetNewDLLFunctions(NEW_DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion);
int  Bridge_ForwardServerGetBlendingInterface(int version, void **ppinterface, void *pstudio, float *rotationmatrix, float *bonetransform);

uint32_t    Bridge_ResolveFunctionFromName(const char *pName);
const char *Bridge_ResolveNameForFunction(uint32_t functionAddr);

#endif // DLL_BRIDGE_H
