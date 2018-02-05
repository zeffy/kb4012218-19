#include "stdafx.h"
#include "hooks.h"
#include "hlpmem.h"
#include "hlpmisc.h"
#include "hlpsvc.h"

LPWSTR g_pszWUServiceDll;

LPFN_ISDEVICESERVICEABLE g_pfnIsDeviceServiceable;
LPFN_LOADLIBRARYEXW g_pfnLoadLibraryExW;

HMODULE WINAPI LoadLibraryExW_hook(LPCWSTR lpFileName, HANDLE hFile, DWORD dwFlags)
{
        HMODULE result;

        result = g_pfnLoadLibraryExW(lpFileName, hFile, dwFlags);
        if ( !result ) return result;
        trace(L"Loaded library. lpFileName=%ls result=%p", lpFileName, result);

        if ( g_pszWUServiceDll
                && !_wcsicmp(lpFileName, g_pszWUServiceDll)
                && FindIsDeviceServiceablePtr(result, &(PVOID)g_pfnIsDeviceServiceable) ) {

                trace(L"DetourTransactionBegin=%lu", DetourTransactionBegin());
                trace(L"DetourUpdateThread=%lu", DetourUpdateThread(GetCurrentThread()));
                trace(L"DetourAttach=%lu", DetourAttach(&(PVOID)g_pfnIsDeviceServiceable, IsDeviceServiceable_hook));
                trace(L"DetourTransactionCommit=%lu", DetourTransactionCommit());
        }
        return result;
}

BOOL WINAPI IsDeviceServiceable_hook(void)
{
        return TRUE;
}
