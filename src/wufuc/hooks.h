#pragma once


typedef HMODULE(WINAPI *LPFN_LOADLIBRARYEXW)(LPCWSTR, HANDLE, DWORD);
typedef BOOL(WINAPI *LPFN_ISDEVICESERVICEABLE)(void);

extern LPWSTR g_pszWUServiceDll;

extern LPFN_LOADLIBRARYEXW g_pfnLoadLibraryExW;
extern LPFN_ISDEVICESERVICEABLE g_pfnIsDeviceServiceable;

HMODULE WINAPI LoadLibraryExW_hook(LPCWSTR lpFileName, HANDLE hFile, DWORD dwFlags);
BOOL WINAPI IsDeviceServiceable_hook(void);
