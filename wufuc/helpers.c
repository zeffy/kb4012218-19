#include "stdafx.h"
#include "helpers.h"
#include "hooks.h"
#include <sddl.h>

bool CreateExclusiveMutex(const wchar_t *name, HANDLE *pmutex)
{
        HANDLE mutex;

        mutex = CreateMutexW(NULL, TRUE, name);
        if ( mutex ) {
                if ( GetLastError() == ERROR_ALREADY_EXISTS ) {
                        CloseHandle(mutex);
                        return false;
                }
                *pmutex = mutex;
                return true;
        }
        return false;
}

bool CreateEventWithStringSecurityDescriptor(const wchar_t *descriptor, bool manualreset, bool initialstate, const wchar_t *name, HANDLE *pevent)
{
        SECURITY_ATTRIBUTES sa = { sizeof sa };
        HANDLE event;

        if ( ConvertStringSecurityDescriptorToSecurityDescriptorW(descriptor, SDDL_REVISION_1, &sa.lpSecurityDescriptor, NULL) ) {
                event = CreateEventW(&sa, manualreset, initialstate, name);
                if ( event ) {
                        *pevent = event;
                        return true;
                }
        }
        return false;
}

bool FileExists(const wchar_t *path)
{
        return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

bool FileExistsExpandEnvironmentStrings(const wchar_t *path)
{
        bool result;
        LPWSTR dst;
        DWORD buffersize;
        DWORD size;

        buffersize = ExpandEnvironmentStringsW(path, NULL, 0);
        dst = calloc(buffersize, sizeof *dst);
        size = ExpandEnvironmentStringsW(path, dst, buffersize);
        if ( !size || size > buffersize )
                return false;
        result = FileExists(dst);
        free(dst);
        return result;
}

int FileInfoVerCompare(VS_FIXEDFILEINFO *pffi, WORD wMajor, WORD wMinor, WORD wBuild, WORD wRev)
{
        if ( HIWORD(pffi->dwProductVersionMS) < wMajor ) return -1;
        if ( HIWORD(pffi->dwProductVersionMS) > wMajor ) return 1;
        if ( LOWORD(pffi->dwProductVersionMS) < wMinor ) return -1;
        if ( LOWORD(pffi->dwProductVersionMS) > wMinor ) return 1;
        if ( HIWORD(pffi->dwProductVersionLS) < wBuild ) return -1;
        if ( HIWORD(pffi->dwProductVersionLS) > wBuild ) return 1;
        if ( LOWORD(pffi->dwProductVersionLS) < wRev ) return -1;
        if ( LOWORD(pffi->dwProductVersionLS) > wRev ) return 1;
        return 0;
}

bool FindIsDeviceServiceablePtr(const wchar_t *pFilename, HMODULE hModule, PVOID *ppfnIsDeviceServiceable)
{
        bool result = false;
        DWORD dwLen;
        LPVOID pBlock;
        PLANGANDCODEPAGE ptl;
        UINT cb;
        wchar_t SubBlock[38];
        wchar_t *pInternalName;
        UINT uLen;
        VS_FIXEDFILEINFO *pffi;
        bool is_win7 = false;
        bool is_win81 = false;
        size_t n;
        char szModule[MAX_PATH];
        LPFN_ISDEVICESERVICEABLE pfnIsDeviceServiceable = NULL;
        MODULEINFO modinfo;
        const char *pattern;
        size_t offset;

        dwLen = GetFileVersionInfoSizeW(pFilename, NULL);
        if ( !dwLen ) return result;

        pBlock = malloc(dwLen);
        if ( !pBlock ) return result;

        if ( !GetFileVersionInfoW(pFilename, 0, dwLen, pBlock)
                || !VerQueryValueW(pBlock, L"\\VarFileInfo\\Translation", (LPVOID *)&ptl, &cb) )
                goto cleanup;

        is_win7 = IsWindowsVersion(6, 1, 1);

        if ( !is_win7 )
                is_win81 = IsWindowsVersion(6, 3, 0);

        for ( size_t i = 0; i < (cb / sizeof *ptl); i++ ) {
                swprintf_s(SubBlock, _countof(SubBlock), L"\\StringFileInfo\\%04x%04x\\InternalName",
                        ptl[i].wLanguage,
                        ptl[i].wCodePage);

                // identify wuaueng.dll by its resource data
                if ( !VerQueryValueW(pBlock, SubBlock, (LPVOID *)&pInternalName, &uLen)
                        || _wcsicmp(pInternalName, L"wuaueng.dll")
                        || !VerQueryValueW(pBlock, L"\\", (LPVOID *)&pffi, &uLen) )
                        continue;

                // assure wuaueng.dll is at least the minimum supported version
                if ( (is_win7 && FileInfoVerCompare(pffi, 7, 6, 7601, 23714) != -1)
                        || (is_win81 && FileInfoVerCompare(pffi, 7, 9, 9600, 18621) != -1) ) {

                        // convert pFilename to multibyte string because DetourFindFunction doesn't take wide string
                        // try resolving with detours helper function first (needs dbghelp.dll and an internet connection)
                        //if ( !wcstombs_s(&n, szModule, _countof(szModule), pFilename, _TRUNCATE) )
                        //        pfnIsDeviceServiceable = DetourFindFunction(PathFindFileNameA(szModule), "IsDeviceServiceable");

                        // fall back to byte pattern search if the above method fails
                        if ( !pfnIsDeviceServiceable
                                && K32GetModuleInformation(GetCurrentProcess(), hModule, &modinfo, sizeof modinfo) ) {
#ifdef _WIN64
                                pattern = "FFF3 4883EC?? 33DB 391D???????? 7508 8B05????????";
#else
                                if ( is_win7 )
                                        pattern = "833D????????00 743E E8???????? A3????????";
                                else // if we've reached this point we can assume win 8.1 without checking is_win81 again
                                        pattern = "8BFF 51 833D????????00 7507 A1????????";
#endif
                                offset = patternfind(modinfo.lpBaseOfDll, modinfo.SizeOfImage, pattern);
                                if ( offset != -1 )
                                        pfnIsDeviceServiceable = (LPFN_ISDEVICESERVICEABLE)((uint8_t *)modinfo.lpBaseOfDll + offset);
                        }

                        if ( pfnIsDeviceServiceable ) {
                                trace(L"Found IsDeviceServiceable address: %p", pfnIsDeviceServiceable);
                                *ppfnIsDeviceServiceable = pfnIsDeviceServiceable;
                                result = true;
                        }
                        break;
                }
        }
cleanup:
        free(pBlock);
        return result;
}

bool GetRemoteHModuleFromTh32ModuleSnapshot(HANDLE hSnapshot, const wchar_t *pLibFileName, HMODULE *phRemoteModule)
{
        MODULEENTRY32W me = { sizeof me };
        if ( !Module32FirstW(hSnapshot, &me) )
                return false;
        do {
                if ( !_wcsicmp(me.szExePath, pLibFileName) ) {
                        *phRemoteModule = me.hModule;
                        return true;
                }
        } while ( Module32NextW(hSnapshot, &me) );
        return false;
}

bool InjectSelfAndCreateRemoteThread(DWORD dwProcessId, LPTHREAD_START_ROUTINE pStartAddress, HANDLE SourceHandle, DWORD dwDesiredAccess)
{
        bool result = false;
        HANDLE hProcess;
        NTSTATUS Status;
        HMODULE hRemoteModule;
        LPTHREAD_START_ROUTINE pfnTargetStartAddress;
        HANDLE TargetHandle = NULL;
        HANDLE hThread;

        hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);
        if ( !hProcess ) return result;

        Status = NtSuspendProcess(hProcess);

        if ( !NT_SUCCESS(Status) ) goto cleanup;

        if ( InjectLibraryByLocalHModule(hProcess, PIMAGEBASE, &hRemoteModule)
                && (!SourceHandle || DuplicateHandle(GetCurrentProcess(), SourceHandle, hProcess, &TargetHandle, dwDesiredAccess, FALSE, 0)) ) {

                // get pointer to StartAddress in remote process
                pfnTargetStartAddress = (LPTHREAD_START_ROUTINE)((uint8_t *)hRemoteModule + ((uint8_t *)pStartAddress - (uint8_t *)PIMAGEBASE));

                hThread = CreateRemoteThread(hProcess, NULL, 0, pfnTargetStartAddress, (LPVOID)TargetHandle, 0, NULL);
                if ( hThread ) {
                        CloseHandle(hThread);
                        result = true;
                }
        }
        NtResumeProcess(hProcess);
cleanup:
        CloseHandle(hProcess);
        return result;
}

bool InjectLibraryByFileName(HANDLE hProcess, const wchar_t *pLibFileName, size_t cchLibFileName, HMODULE *phRemoteModule)
{
        bool result = false;
        DWORD dwProcessId;
        NTSTATUS Status;
        HANDLE hSnapshot;
        SIZE_T nSize;
        LPVOID pBaseAddress;
        HANDLE hThread;

        Status = NtSuspendProcess(hProcess);
        if ( !NT_SUCCESS(Status) ) return result;

        dwProcessId = GetProcessId(hProcess);

        hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessId);
        if ( !hSnapshot ) goto L1;

        result = GetRemoteHModuleFromTh32ModuleSnapshot(hSnapshot, pLibFileName, phRemoteModule);
        CloseHandle(hSnapshot);

        // already injected... returns false but sets *phRemoteModule
        if ( result ) goto L1;

        nSize = (cchLibFileName + 1) * sizeof *pLibFileName;
        pBaseAddress = VirtualAllocEx(hProcess, NULL, nSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

        if ( !pBaseAddress ) goto L1;

        if ( !WriteProcessMemory(hProcess, pBaseAddress, pLibFileName, nSize, NULL) )
                goto L2;

        hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryW, pBaseAddress, 0, NULL);
        if ( !hThread ) goto L2;

        WaitForSingleObject(hThread, INFINITE);

        if ( sizeof *phRemoteModule > sizeof(DWORD) ) {
                hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessId);
                if ( hSnapshot ) {
                        result = GetRemoteHModuleFromTh32ModuleSnapshot(hSnapshot, pLibFileName, phRemoteModule);
                        CloseHandle(hSnapshot);
                }
        } else {
                result = !!GetExitCodeThread(hThread, (LPDWORD)phRemoteModule);
        }
        CloseHandle(hThread);
L2:     VirtualFreeEx(hProcess, pBaseAddress, 0, MEM_RELEASE);
L1:     NtResumeProcess(hProcess);
        return result;
}

bool InjectLibraryByLocalHModule(HANDLE hProcess, HMODULE hModule, HMODULE *phRemoteModule)
{
        WCHAR Filename[MAX_PATH];
        DWORD nLength;

        nLength = GetModuleFileNameW(hModule, Filename, _countof(Filename));

        if ( nLength )
                return InjectLibraryByFileName(hProcess, Filename, nLength, phRemoteModule);

        return false;
}

bool IsWindowsVersion(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor)
{
        OSVERSIONINFOEXW osvi = { sizeof osvi };

        DWORDLONG dwlConditionMask = 0;
        VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_EQUAL);
        VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, VER_EQUAL);
        VER_SET_CONDITION(dwlConditionMask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

        osvi.dwMajorVersion = wMajorVersion;
        osvi.dwMinorVersion = wMinorVersion;
        osvi.wServicePackMajor = wServicePackMajor;

        return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask) != FALSE;
}

PVOID NtQueryKeyAlloc(HANDLE KeyHandle, KEY_INFORMATION_CLASS KeyInformationClass, PULONG pResultLength)
{
        NTSTATUS Status;
        ULONG ResultLength;
        PVOID result = NULL;

        Status = NtQueryKey(KeyHandle, KeyInformationClass, NULL, 0, &ResultLength);
        if ( Status != STATUS_BUFFER_OVERFLOW && Status != STATUS_BUFFER_TOO_SMALL )
                return result;

        result = malloc(ResultLength);
        if ( !result ) return result;

        Status = NtQueryKey(KeyHandle, KeyInformationClass, result, ResultLength, &ResultLength);
        if ( NT_SUCCESS(Status) ) {
                *pResultLength = ResultLength;
        } else {
                free(result);
                result = NULL;
        }
        return result;
}

PVOID RegGetValueAlloc(HKEY hkey, const wchar_t *pSubKey, const wchar_t *pValue, DWORD dwFlags, LPDWORD pdwType, LPDWORD pcbData)
{
        DWORD cbData = 0;
        PVOID result = NULL;

        if ( RegGetValueW(hkey, pSubKey, pValue, dwFlags, pdwType, NULL, &cbData) != ERROR_SUCCESS )
                return result;

        result = malloc(cbData);
        if ( !result ) return result;

        if ( RegGetValueW(hkey, pSubKey, pValue, dwFlags, pdwType, result, &cbData) == ERROR_SUCCESS ) {
                *pcbData = cbData;
        } else {
                free(result);
                result = NULL;
        }
        return result;
}

LPQUERY_SERVICE_CONFIGW QueryServiceConfigByNameAlloc(SC_HANDLE hSCM, const wchar_t *pServiceName, LPDWORD pcbBufSize)
{
        SC_HANDLE hService;
        DWORD cbBytesNeeded;
        LPQUERY_SERVICE_CONFIGW result = NULL;

        hService = OpenServiceW(hSCM, pServiceName, SERVICE_QUERY_CONFIG);
        if ( !hService ) return result;

        if ( !QueryServiceConfigW(hService, NULL, 0, &cbBytesNeeded)
                && GetLastError() == ERROR_INSUFFICIENT_BUFFER ) {

                result = malloc(cbBytesNeeded);
                if ( result ) {
                        if ( QueryServiceConfigW(hService, result, cbBytesNeeded, &cbBytesNeeded) ) {
                                *pcbBufSize = cbBytesNeeded;
                        } else {
                                free(result);
                                result = NULL;
                        }
                }
        }
        CloseServiceHandle(hService);
        return result;
}

bool QueryServiceStatusProcessInfoByName(SC_HANDLE hSCM, const wchar_t *pServiceName, LPSERVICE_STATUS_PROCESS pServiceStatus)
{
        bool result = false;
        SC_HANDLE hService;
        DWORD cbBytesNeeded;

        hService = OpenServiceW(hSCM, pServiceName, SERVICE_QUERY_STATUS);
        if ( !hService ) return result;

        result = !!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)pServiceStatus, sizeof *pServiceStatus, &cbBytesNeeded);
        CloseServiceHandle(hService);
        return result;
}

bool QueryServiceGroupName(const LPQUERY_SERVICE_CONFIGW pServiceConfig, wchar_t *pGroupName, size_t nSize)
{
        bool result = false;
        int NumArgs;
        LPWSTR *argv;

        argv = CommandLineToArgvW(pServiceConfig->lpBinaryPathName, &NumArgs);
        if ( argv ) {
                if ( !_wcsicmp(PathFindFileNameW(argv[0]), L"svchost.exe") ) {

                        for ( int i = 1; (i + 1) < NumArgs; i++ ) {
                                if ( !_wcsicmp(argv[i], L"-k") )
                                        return !wcscpy_s(pGroupName, nSize, argv[++i]);
                        }
                }
                LocalFree((HLOCAL)argv);
        }
        return result;
}

DWORD QueryServiceProcessId(SC_HANDLE hSCM, const wchar_t *pServiceName)
{
        SERVICE_STATUS_PROCESS ServiceStatusProcess;

        if ( QueryServiceStatusProcessInfoByName(hSCM, pServiceName, &ServiceStatusProcess) )
                return ServiceStatusProcess.dwProcessId;
        return 0;
}

DWORD InferSvchostGroupProcessId(SC_HANDLE hSCM, const wchar_t *pGroupName)
{
        DWORD result = 0;
        DWORD cbData;
        wchar_t *pData;
        DWORD dwProcessId;
        DWORD cbBufSize;
        LPQUERY_SERVICE_CONFIGW pServiceConfig;
        bool success;
        WCHAR GroupName[256];

        pData = RegGetValueAlloc(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Svchost", pGroupName, RRF_RT_REG_MULTI_SZ, NULL, &cbData);
        if ( !pData ) return result;

        for ( wchar_t *pName = pData; *pName; pName += wcslen(pName) + 1 ) {
                dwProcessId = QueryServiceProcessId(hSCM, pName);
                trace(L"pName=%ls dwProcessId=%lu", pName, dwProcessId);
                if ( !dwProcessId ) continue;

                pServiceConfig = QueryServiceConfigByNameAlloc(hSCM, pName, &cbBufSize);
                if ( !pServiceConfig ) continue;
                success = QueryServiceGroupName(pServiceConfig, GroupName, _countof(GroupName));
                free(pServiceConfig);
                if ( success && !_wcsicmp(pGroupName, GroupName) ) {
                        trace(L"found PID for group %ls: %lu", pGroupName, dwProcessId);
                        result = dwProcessId;
                        break;
                }
        }
        free(pData);
        return result;
}