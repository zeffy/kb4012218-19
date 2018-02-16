#pragma once

#pragma pack(push, 1)
typedef struct
{
        HANDLE hChildMutex;
        union
        {
                struct
                {
                        HANDLE hParentMutex;
                        HANDLE hUnloadEvent;
                } DUMMYSTRUCTNAME;
                struct
                {
                        HANDLE hMainMutex;
                        HANDLE hUnloadEvent;
                } u;
                HANDLE handles[2];
        };
} ContextHandles;
#pragma pack(pop)

typedef struct
{
        WORD wLanguage;
        WORD wCodePage;
} LANGANDCODEPAGE, *PLANGANDCODEPAGE;

bool FindIDSFunctionAddress(HMODULE hModule, PVOID *ppfnIsDeviceServiceable);
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
bool wufuc_InjectLibrary(DWORD dwProcessId, ContextHandles *pContext);
