#pragma once

typedef struct
{
        WORD wLanguage;
        WORD wCodePage;
} LANGANDCODEPAGE, *PLANGANDCODEPAGE;

bool FindIsDeviceServiceablePtr(HMODULE hModule, PVOID *ppfnIsDeviceServiceable);
HANDLE GetRemoteHModuleFromTh32ModuleSnapshot(HANDLE hSnapshot, const wchar_t *pLibFileName);
bool InjectLibraryAndCreateRemoteThread(
        HANDLE hProcess,
        HMODULE hModule,
        LPTHREAD_START_ROUTINE pStartAddress,
        const void *pParam,
        size_t cbParam);
bool InjectLibrary(HANDLE hProcess, HMODULE hModule, HMODULE *phRemoteModule);
bool InjectLibraryByFilename(
        HANDLE hProcess,
        const wchar_t *pLibFilename,
        size_t cchLibFilename,
        HMODULE *phRemoteModule);
