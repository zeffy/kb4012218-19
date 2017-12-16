#include "stdafx.h"
#include "hooks.h"
#include "helpers.h"

LPFN_REGQUERYVALUEEXW g_pfnRegQueryValueExW;
LPFN_LOADLIBRARYEXW g_pfnLoadLibraryExW;
LPFN_ISDEVICESERVICEABLE g_pfnIsDeviceServiceable;

LSTATUS WINAPI RegQueryValueExW_hook(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
        PWCH pBuffer = NULL;
        DWORD MaximumLength = 0;
        LSTATUS result;
        ULONG ResultLength;
        PKEY_NAME_INFORMATION pkni;
        size_t NameCount;
        int current;
        int pos;
        LPWSTR fname;
        const WCHAR realpath[] = L"%systemroot%\\system32\\wuaueng.dll";

        trace(L"");

        // save original buffer size
        if ( lpData && lpcbData )
                MaximumLength = *lpcbData;
        trace(L"");

        result = g_pfnRegQueryValueExW(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);

        if ( result != ERROR_SUCCESS || !MaximumLength || !lpValueName || _wcsicmp(lpValueName, L"ServiceDll") )
                return result;
        trace(L"");

        pBuffer = (wchar_t *)lpData;
        trace(L"");

        // get name of registry key being queried
        pkni = NtQueryKeyAlloc((HANDLE)hKey, KeyNameInformation, &ResultLength);
        trace(L"");
        if ( !pkni )
                return result;
        trace(L"");

        NameCount = pkni->NameLength / sizeof *pkni->Name;
        // change key name to lower-case because there is no case-insensitive version of _snwscanf_s
        if ( !_wcslwr_s(pkni->Name, NameCount)
                && _snwscanf_s(pkni->Name, NameCount, L"\\registry\\machine\\system\\controlset%03d\\services\\wuauserv\\parameters%n", &current, &pos) == 1
                && pos == NameCount ) {

                trace(L"key=%.*ls", NameCount, pkni->Name);

                fname = PathFindFileNameW(pBuffer);

                trace(L"fname=%ls", fname);

                if ( (!_wcsicmp(fname, L"wuaueng2.dll") // UpdatePack7R2
                        || !_wcsicmp(fname, L"WuaCpuFix64.dll") // WuaCpuFix 64-bit
                        || !_wcsicmp(fname, L"WuaCpuFix.dll"))
                        && FileExistsExpandEnvironmentStrings(realpath)
                        && SUCCEEDED(StringCbCopyW(pBuffer, MaximumLength, realpath)) ) {
                        trace(L"");

                        *lpcbData = sizeof realpath;
                }
        }
        free(pkni);
        return result;
}

HMODULE WINAPI LoadLibraryExW_hook(LPCWSTR lpFileName, HANDLE hFile, DWORD dwFlags)
{
        HMODULE result;

        result = g_pfnLoadLibraryExW(lpFileName, hFile, dwFlags);
        if ( !result ) return result;

        trace(L"Loaded library: %ls", lpFileName);
        if ( FindIsDeviceServiceablePtr(lpFileName, result, (PVOID *)&g_pfnIsDeviceServiceable) ) {
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());
                DetourAttach((PVOID *)&g_pfnIsDeviceServiceable, IsDeviceServiceable_hook);
                DetourTransactionCommit();
        }
        return result;
}

BOOL WINAPI IsDeviceServiceable_hook(void)
{
        return TRUE;
}
