#pragma once

typedef struct
{
        WORD wLanguage;
        WORD wCodePage;
} LANGANDCODEPAGE, *PLANGANDCODEPAGE;

bool InitializeMutex(bool InitialOwner, const wchar_t *pMutexName, HANDLE *phMutex);
bool CreateEventWithStringSecurityDescriptor(
        const wchar_t *pStringSecurityDescriptor,
        bool ManualReset,
        bool InitialState,
        const wchar_t *pName,
        HANDLE *phEvent);
int FileInfoVerCompare(VS_FIXEDFILEINFO *pffi, WORD wMajor, WORD wMinor, WORD wBuild, WORD wRev);
bool GetVersionInfoFromHModule(HMODULE hModule, LPCWSTR pszSubBlock, LPVOID pData, PUINT pcbData);
LPVOID GetVersionInfoFromHModuleAlloc(HMODULE hModule, LPCWSTR pszSubBlock, PUINT pcbData);
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
bool IsWindowsVersion(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor);
PVOID RegGetValueAlloc(
        HKEY hkey,
        const wchar_t *pSubKey,
        const wchar_t *pValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        LPDWORD pcbData);
LPQUERY_SERVICE_CONFIGW QueryServiceConfigByNameAlloc(
        SC_HANDLE hSCM,
        const wchar_t *pServiceName,
        LPDWORD pcbBufSize);
bool QueryServiceStatusProcessInfoByName(
        SC_HANDLE hSCM,
        const wchar_t *pServiceName,
        LPSERVICE_STATUS_PROCESS pServiceStatus);
DWORD QueryServiceProcessId(SC_HANDLE hSCM, const wchar_t *pServiceName);
