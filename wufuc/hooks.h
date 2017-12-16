#pragma once

typedef struct
{
        WORD wLanguage;
        WORD wCodePage;
} LANGANDCODEPAGE, *PLANGANDCODEPAGE;

typedef LSTATUS(WINAPI *LPFN_REGQUERYVALUEEXW)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
typedef HMODULE(WINAPI *LPFN_LOADLIBRARYEXW)(LPCWSTR, HANDLE, DWORD);
typedef BOOL(WINAPI *LPFN_ISDEVICESERVICEABLE)(void);

extern LPFN_REGQUERYVALUEEXW g_pfnRegQueryValueExW;
extern LPFN_LOADLIBRARYEXW g_pfnLoadLibraryExW;
extern LPFN_ISDEVICESERVICEABLE g_pfnIsDeviceServiceable;

LSTATUS WINAPI RegQueryValueExW_hook(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
HMODULE WINAPI LoadLibraryExW_hook(LPCWSTR lpFileName, HANDLE hFile, DWORD dwFlags);
BOOL WINAPI IsDeviceServiceable_hook(void);
