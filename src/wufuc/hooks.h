#pragma once


typedef LSTATUS(WINAPI *LPFN_REGQUERYVALUEEXW)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
typedef HMODULE(WINAPI *LPFN_LOADLIBRARYEXW)(LPCWSTR, HANDLE, DWORD);
typedef BOOL(WINAPI *LPFN_ISDEVICESERVICEABLE)(void);

extern wchar_t *g_pszWUServiceDll;

extern LPFN_REGQUERYVALUEEXW g_pfnRegQueryValueExW;
extern LPFN_LOADLIBRARYEXW g_pfnLoadLibraryExW;
extern LPFN_ISDEVICESERVICEABLE g_pfnIsDeviceServiceable;

extern PVOID g_ptRegQueryValueExW;
extern PVOID g_ptLoadLibraryExW;
extern PVOID g_ptIsDeviceServiceable;

LSTATUS WINAPI RegQueryValueExW_hook(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
HMODULE WINAPI LoadLibraryExW_hook(LPCWSTR lpFileName, HANDLE hFile, DWORD dwFlags);
BOOL WINAPI IsDeviceServiceable_hook(void);
