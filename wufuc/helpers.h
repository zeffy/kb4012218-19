#pragma once

bool CreateExclusiveMutex(const wchar_t *name, HANDLE *pmutex);
bool CreateEventWithStringSecurityDescriptor(const wchar_t *descriptor, bool manualreset, bool initialstate, const wchar_t *name, HANDLE *pevent);

bool FileExists(const wchar_t *path);
bool FileExistsExpandEnvironmentStrings(const wchar_t *path);
int FileInfoVerCompare(VS_FIXEDFILEINFO *pffi, WORD wMajor, WORD wMinor, WORD wBuild, WORD wRev);
size_t FindArgument(const wchar_t **argv, size_t argc, const wchar_t *val);

bool FindIsDeviceServiceablePtr(const wchar_t *pFilename, HMODULE hModule, PVOID *ppfnIsDeviceServiceable);
bool GetRemoteHModuleFromTh32ModuleSnapshot(HANDLE hSnapshot, const wchar_t *pLibFileName, HMODULE *phRemoteModule);
bool InjectSelfAndCreateRemoteThread(DWORD dwProcessId, LPTHREAD_START_ROUTINE pStartAddress, HANDLE SourceHandle, DWORD dwDesiredAccess);
bool InjectLibraryByFileName(HANDLE hProcess, const wchar_t *pLibFileName, size_t nLength, HMODULE *phRemoteModule);
bool InjectLibraryByLocalHModule(HANDLE hProcess, HMODULE hModule, HMODULE *phRemoteModule);
bool IsWindowsVersion(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor);

PVOID NtQueryKeyAlloc(HANDLE KeyHandle, KEY_INFORMATION_CLASS KeyInformationClass, PULONG pResultLength);
PVOID RegGetValueAlloc(HKEY hkey, const wchar_t *pSubKey, const wchar_t *pValue, DWORD dwFlags, LPDWORD pdwType, LPDWORD pcbData);
LPQUERY_SERVICE_CONFIGW QueryServiceConfigByNameAlloc(SC_HANDLE hSCM, const wchar_t *pServiceName, LPDWORD pcbBufSize);
bool QueryServiceStatusProcessInfoByName(SC_HANDLE hSCM, const wchar_t *pServiceName, LPSERVICE_STATUS_PROCESS pServiceStatus);

bool QueryServiceGroupName(const LPQUERY_SERVICE_CONFIGW pServiceConfig, wchar_t *pGroupName, size_t nSize);
DWORD QueryServiceProcessId(SC_HANDLE hSCM, const wchar_t *pServiceName);
DWORD InferSvchostGroupProcessId(SC_HANDLE hSCM, const wchar_t *pGroupName);
