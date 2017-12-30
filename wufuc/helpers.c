#include "stdafx.h"
#include "helpers.h"
#include "hooks.h"
#include <sddl.h>

bool InitializeMutex(bool InitialOwner, const wchar_t *pMutexName, HANDLE *phMutex)
{
        HANDLE hMutex;

        hMutex = CreateMutexW(NULL, InitialOwner, pMutexName);
        if ( hMutex ) {
                if ( GetLastError() == ERROR_ALREADY_EXISTS ) {
                        CloseHandle(hMutex);
                        return false;
                }
                *phMutex = hMutex;
                return true;
        }
        return false;
}

bool CreateEventWithStringSecurityDescriptor(
        const wchar_t *pStringSecurityDescriptor,
        bool ManualReset,
        bool InitialState,
        const wchar_t *pName,
        HANDLE *phEvent)
{
        SECURITY_ATTRIBUTES sa = { sizeof sa };
        HANDLE event;

        if ( ConvertStringSecurityDescriptorToSecurityDescriptorW(
                pStringSecurityDescriptor,
                SDDL_REVISION_1,
                &sa.lpSecurityDescriptor,
                NULL) ) {

                event = CreateEventW(&sa, ManualReset, InitialState, pName);
                if ( event ) {
                        *phEvent = event;
                        return true;
                }
        }
        return false;
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

bool GetVersionInfoFromHModule(HMODULE hModule, LPCWSTR pszSubBlock, LPVOID pData, PUINT pcbData)
{
        bool result = false;
        UINT cbData;
        HRSRC hResInfo;
        DWORD dwSize;
        HGLOBAL hResData;
        LPVOID pRes;
        LPVOID pCopy;
        LPVOID pBuffer;
        UINT uLen;

        if ( !pcbData ) return result;
        cbData = *pcbData;

        hResInfo = FindResourceW(hModule,
                MAKEINTRESOURCEW(VS_VERSION_INFO),
                RT_VERSION);
        if ( !hResInfo ) return result;

        dwSize = SizeofResource(hModule, hResInfo);
        if ( !dwSize ) return result;

        hResData = LoadResource(hModule, hResInfo);
        if ( !hResData ) return result;

        pRes = LockResource(hResData);
        if ( !pRes ) return result;

        pCopy = malloc(dwSize);
        if ( !pCopy
                || memcpy_s(pCopy, dwSize, pRes, dwSize)
                || !VerQueryValueW(pCopy, pszSubBlock, &pBuffer, &uLen) )
                goto cleanup;

        if ( !_wcsnicmp(pszSubBlock, L"\\StringFileInfo\\", 16) )
                *pcbData = uLen * sizeof(wchar_t);
        else
                *pcbData = uLen;

        if ( !pData ) {
                result = true;
                goto cleanup;
        }
        if ( cbData < *pcbData
                || memcpy_s(pData, cbData, pBuffer, *pcbData) )
                goto cleanup;

        result = true;
cleanup:
        free(pCopy);
        return result;
}

LPVOID GetVersionInfoFromHModuleAlloc(HMODULE hModule, LPCWSTR pszSubBlock, PUINT pcbData)
{
        UINT cbData = 0;
        LPVOID result = NULL;

        if ( !GetVersionInfoFromHModule(hModule, pszSubBlock, NULL, &cbData) )
                return result;

        result = malloc(cbData);
        if ( !result ) return result;

        if ( GetVersionInfoFromHModule(hModule, pszSubBlock, result, &cbData) ) {
                *pcbData = cbData;
        } else {
                free(result);
                result = NULL;
        }
        return result;
}

bool FindIsDeviceServiceablePtr(HMODULE hModule, PVOID *ppfnIsDeviceServiceable)
{
        bool result = false;
        bool is_win7 = false;
        bool is_win81 = false;
        PLANGANDCODEPAGE ptl;
        HANDLE hProcess;
        int tmp;
        UINT cbtl;
        wchar_t SubBlock[38];
        UINT cbInternalName;
        wchar_t *pInternalName;
        UINT cbffi;
        VS_FIXEDFILEINFO *pffi;
        MODULEINFO modinfo;
        size_t offset;

        is_win7 = IsWindowsVersion(6, 1, 1);
        if ( !is_win7 ) {
                is_win81 = IsWindowsVersion(6, 3, 0);
                if ( !is_win81 ) {
                        trace(L"Unsupported operating system. is_win7=%ls is_win81=%ls",
                                is_win7 ? L"true" : L"false",
                                is_win81 ? L"true" : L"false");
                        return result;
                }
        }

        ptl = GetVersionInfoFromHModuleAlloc(hModule, L"\\VarFileInfo\\Translation", &cbtl);
        if ( !ptl ) {
                trace(L"Failed to allocate version translation information from hmodule.");
                return result;
        }
        hProcess = GetCurrentProcess();

        for ( size_t i = 0, count = (cbtl / sizeof *ptl); i < count; i++ ) {
                if ( swprintf_s(SubBlock,
                        _countof(SubBlock),
                        L"\\StringFileInfo\\%04x%04x\\InternalName",
                        ptl[i].wLanguage,
                        ptl[i].wCodePage) == -1 )
                        continue;

                pInternalName = GetVersionInfoFromHModuleAlloc(hModule, SubBlock, &cbInternalName);
                if ( !pInternalName ) {
                        trace(L"Failed to allocate version internal name from hmodule.");
                        continue;
                }

                // identify wuaueng.dll by its resource data
                tmp = _wcsicmp(pInternalName, L"wuaueng.dll");
                if ( tmp )
                        trace(L"Module internal name does not match. pInternalName=%ls cbInternalName=%lu", pInternalName, cbInternalName);
                free(pInternalName);
                if ( tmp )
                        continue;

                pffi = GetVersionInfoFromHModuleAlloc(hModule, L"\\", &cbffi);
                if ( !pffi ) {
                        trace(L"Failed to allocate version information from hmodule.");
                        continue;
                }

                // assure wuaueng.dll is at least the minimum supported version
                tmp = ((is_win7 && FileInfoVerCompare(pffi, 7, 6, 7601, 23714) != -1)
                        || (is_win81 && FileInfoVerCompare(pffi, 7, 9, 9600, 18621) != -1));
                free(pffi);
                if ( !tmp ) {
                        trace(L"Module does not meet the minimum supported version.");
                        continue;
                }

                if ( !GetModuleInformation(hProcess, hModule, &modinfo, sizeof modinfo) )
                        break;

                offset = patternfind(modinfo.lpBaseOfDll,
                        modinfo.SizeOfImage,
#ifdef _WIN64
                        "FFF3 4883EC?? 33DB 391D???????? 7508 8B05????????"
#else
                        is_win7
                        ? "833D????????00 743E E8???????? A3????????"
                        : "8BFF 51 833D????????00 7507 A1????????"
#endif
                );
                if ( offset != -1 ) {
                        *ppfnIsDeviceServiceable = (PVOID)((uint8_t *)modinfo.lpBaseOfDll + offset);
                        trace(L"Found IsDeviceServiceable address: %p", *ppfnIsDeviceServiceable);
                        result = true;
                } else {
                        trace(L"IsDeviceServiceable function pattern not found.");
                }
                break;
        }
        free(ptl);
        return result;
}

HANDLE GetRemoteHModuleFromTh32ModuleSnapshot(HANDLE hSnapshot, const wchar_t *pLibFileName)
{
        MODULEENTRY32W me = { sizeof me };
        if ( !Module32FirstW(hSnapshot, &me) )
                return NULL;
        do {
                if ( !_wcsicmp(me.szExePath, pLibFileName) )
                        return me.hModule;
        } while ( Module32NextW(hSnapshot, &me) );
        return NULL;
}

bool InjectLibraryAndCreateRemoteThread(
        HANDLE hProcess,
        HMODULE hModule,
        LPTHREAD_START_ROUTINE pStartAddress,
        const void *pParam,
        size_t cbParam)
{
        bool result = false;
        NTSTATUS Status;
        LPVOID pBaseAddress = NULL;
        SIZE_T cb;
        HMODULE hRemoteModule = NULL;
        HANDLE hThread;

        Status = NtSuspendProcess(hProcess);
        if ( !NT_SUCCESS(Status) ) return result;
        trace(L"Suspended process. hProcess=%p", hProcess);

        if ( pParam ) {
                // this gets freed by pStartAddress
                pBaseAddress = VirtualAllocEx(hProcess,
                        NULL,
                        cbParam,
                        MEM_RESERVE | MEM_COMMIT,
                        PAGE_READWRITE);
                if ( !pBaseAddress ) goto resume;

                if ( !WriteProcessMemory(hProcess, pBaseAddress, pParam, cbParam, &cb) )
                        goto vfree;
        }
        if ( InjectLibrary(hProcess, hModule, &hRemoteModule) ) {
                trace(L"Injected library. hRemoteModule=%p", hRemoteModule);

                hThread = CreateRemoteThread(hProcess,
                        NULL,
                        0,
                        (LPTHREAD_START_ROUTINE)((uint8_t *)hRemoteModule + ((uint8_t *)pStartAddress - (uint8_t *)hModule)),
                        pBaseAddress,
                        0,
                        NULL);

                if ( hThread ) {
                        trace(L"Created remote thread. hThread=%p", hThread);
                        CloseHandle(hThread);
                        result = true;
                }
        }
vfree:
        if ( !result && pBaseAddress )
                VirtualFreeEx(hProcess, pBaseAddress, 0, MEM_RELEASE);
resume: NtResumeProcess(hProcess);
        return result;
}

bool InjectLibrary(HANDLE hProcess, HMODULE hModule, HMODULE *phRemoteModule)
{
        WCHAR Filename[MAX_PATH];
        DWORD nLength;

        nLength = GetModuleFileNameW(hModule, Filename, _countof(Filename));
        if ( nLength ) {
                trace(L"Got module filename for local module. "
                        L"hModule=%p Filename=%ls nLength=%lu",
                        hModule, Filename, nLength);
                return InjectLibraryByFilename(hProcess,
                        Filename,
                        nLength,
                        phRemoteModule);
        }
        return false;
}

bool InjectLibraryByFilename(
        HANDLE hProcess,
        const wchar_t *pLibFilename,
        size_t cchLibFilename,
        HMODULE *phRemoteModule)
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
        trace(L"Suspended process. hProcess=%p", hProcess);

        dwProcessId = GetProcessId(hProcess);

        hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessId);
        if ( !hSnapshot ) goto resume;
        trace(L"Created TH32 module snapshot. dwProcessId=%lu", dwProcessId);

        *phRemoteModule = GetRemoteHModuleFromTh32ModuleSnapshot(hSnapshot,
                pLibFilename);

        CloseHandle(hSnapshot);

        // already injected... still sets *phRemoteModule
        if ( *phRemoteModule ) goto resume;

        nSize = (cchLibFilename + 1) * sizeof *pLibFilename;
        pBaseAddress = VirtualAllocEx(hProcess,
                NULL,
                nSize,
                MEM_RESERVE | MEM_COMMIT,
                PAGE_READWRITE);

        if ( !pBaseAddress ) goto resume;
        trace(L"Allocated virtual memory in process. hProcess=%p pBaseAddress=%p nSize=%Iu",
                hProcess, pBaseAddress, nSize);

        if ( !WriteProcessMemory(hProcess, pBaseAddress, pLibFilename, nSize, NULL) )
                goto vfree;
        trace(L"Wrote to process memory. hProcess=%p pBaseAddress=%p pLibFileName=%.*ls nSize=%Iu",
                hProcess, pBaseAddress, cchLibFilename, pLibFilename, nSize);

        hThread = CreateRemoteThread(hProcess,
                NULL,
                0,
                (LPTHREAD_START_ROUTINE)LoadLibraryW,
                pBaseAddress,
                0,
                NULL);
        if ( !hThread ) goto vfree;
        trace(L"Created remote thread. hThread=%p", hThread);

        WaitForSingleObject(hThread, INFINITE);
        trace(L"Created thread finished running. hThread=%p", hThread);

        if ( sizeof *phRemoteModule > sizeof(DWORD) ) {
                hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessId);
                if ( hSnapshot ) {
                        *phRemoteModule = GetRemoteHModuleFromTh32ModuleSnapshot(
                                hSnapshot,
                                pLibFilename);

                        CloseHandle(hSnapshot);
                }
        } else {
                result = !!GetExitCodeThread(hThread, (LPDWORD)phRemoteModule);
        }
        CloseHandle(hThread);
vfree:  VirtualFreeEx(hProcess, pBaseAddress, 0, MEM_RELEASE);
resume: NtResumeProcess(hProcess);
        return result;
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

        return VerifyVersionInfoW(&osvi,
                VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR,
                dwlConditionMask) != FALSE;
}

PVOID RegGetValueAlloc(
        HKEY hkey,
        const wchar_t *pSubKey,
        const wchar_t *pValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        LPDWORD pcbData)
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

LPQUERY_SERVICE_CONFIGW QueryServiceConfigByNameAlloc(
        SC_HANDLE hSCM,
        const wchar_t *pServiceName,
        LPDWORD pcbBufSize)
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

bool QueryServiceStatusProcessInfoByName(
        SC_HANDLE hSCM,
        const wchar_t *pServiceName,
        LPSERVICE_STATUS_PROCESS pServiceStatus)
{
        bool result = false;
        SC_HANDLE hService;
        DWORD cbBytesNeeded;

        hService = OpenServiceW(hSCM, pServiceName, SERVICE_QUERY_STATUS);
        if ( !hService ) {
                trace(L"Failed to open service %ls! (GetLastError=%ul)", pServiceName, GetLastError());
                return result;
        }

        result = !!QueryServiceStatusEx(hService,
                SC_STATUS_PROCESS_INFO,
                (LPBYTE)pServiceStatus,
                sizeof *pServiceStatus,
                &cbBytesNeeded);
        CloseServiceHandle(hService);
        return result;
}

DWORD QueryServiceProcessId(SC_HANDLE hSCM, const wchar_t *pServiceName)
{
        SERVICE_STATUS_PROCESS ServiceStatusProcess;

        if ( QueryServiceStatusProcessInfoByName(hSCM, pServiceName, &ServiceStatusProcess) )
                return ServiceStatusProcess.dwProcessId;
        return 0;
}
