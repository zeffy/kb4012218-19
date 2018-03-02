#include "stdafx.h"

#include <minhook.h>

BOOL APIENTRY DllMain(HMODULE hModule,
        DWORD  ul_reason_for_call,
        LPVOID lpReserved)
{
        switch ( ul_reason_for_call ) {
        case DLL_PROCESS_ATTACH:
                MH_Initialize();
                break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
                MH_Uninitialize();
                break;
        }
        return TRUE;
}
