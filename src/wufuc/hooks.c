#include "stdafx.h"
#include "hooks.h"
#include "hlpmisc.h"
#include "hlpmem.h"

LPWSTR g_pszWUServiceDll;

LPFN_REGQUERYVALUEEXW g_pfnRegQueryValueExW;
LPFN_LOADLIBRARYEXW g_pfnLoadLibraryExW;
LPFN_ISDEVICESERVICEABLE g_pfnIsDeviceServiceable;

LSTATUS WINAPI RegQueryValueExW_hook(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
        PWCH pBuffer;
        DWORD MaximumLength = 0;
        LSTATUS result;
        ULONG ResultLength;
        PKEY_NAME_INFORMATION pkni;
        size_t NameCount;
        int current;
        int pos;
        LPWSTR fname;
        const WCHAR realpath[] = L"%systemroot%\\system32\\wuaueng.dll";

        // save original buffer size
        if ( lpData && lpcbData )
                MaximumLength = *lpcbData;
        result = g_pfnRegQueryValueExW(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);

        if ( result != ERROR_SUCCESS || !MaximumLength || !lpValueName || _wcsicmp(lpValueName, L"ServiceDll") )
                return result;

        pBuffer = (PWCH)lpData;

        // get name of registry key being queried
        pkni = NtQueryKeyAlloc((HANDLE)hKey, KeyNameInformation, &ResultLength);
        if ( !pkni )
                return result;

        NameCount = pkni->NameLength / sizeof *pkni->Name;
        // change key name to lower-case because there is no case-insensitive version of _snwscanf_s
        if ( !_wcslwr_s(pkni->Name, NameCount)
                && _snwscanf_s(pkni->Name, NameCount, L"\\registry\\machine\\system\\controlset%03d\\services\\wuauserv\\parameters%n", &current, &pos) == 1
                && pos == NameCount ) {

                fname = PathFindFileNameW(pBuffer);

                if ( (!_wcsicmp(fname, L"wuaueng2.dll") // UpdatePack7R2
                        || !_wcsicmp(fname, L"WuaCpuFix64.dll") // WuaCpuFix
                        || !_wcsicmp(fname, L"WuaCpuFix.dll"))
                        && FileExistsExpandEnvironmentStrings(realpath)
                        && SUCCEEDED(StringCbCopyW(pBuffer, MaximumLength, realpath)) ) {

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
        trace(L"Loaded library: %ls (%p)", lpFileName, result);

        if ( g_pszWUServiceDll
                && !_wcsicmp(lpFileName, g_pszWUServiceDll) ) {

                if ( FindIDSFunctionPointer(result, &(PVOID)g_pfnIsDeviceServiceable) ) {
                        trace(L"Matched pattern for %ls!IsDeviceServiceable. (%p)",
                                PathFindFileNameW(lpFileName),
                                g_pfnIsDeviceServiceable);

                        DetourTransactionBegin();
                        DetourUpdateThread(GetCurrentThread());
                        DetourAttach(&(PVOID)g_pfnIsDeviceServiceable, IsDeviceServiceable_hook);
                        DetourTransactionCommit();
                } else if ( !g_pfnIsDeviceServiceable ) {
                        trace(L"No pattern matched!");
                }
        }
        return result;
}

BOOL WINAPI IsDeviceServiceable_hook(void)
{
        trace(L"Entered stub function.");
        return TRUE;
}
