#include "stdafx.h"
#include "hlpmisc.h"
#include "hlpmem.h"
#include "hlpver.h"
#include "hooks.h"
#include "callbacks.h"

bool FindIDSFunctionPointer(HMODULE hModule, PVOID *ppfnIsDeviceServiceable)
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

        if ( !((is_win7 = IsWindowsVersion(6, 1, 1))
                || (is_win81 = IsWindowsVersion(6, 3, 0))) ) {

                trace(L"Unsupported operating system.");
                return result;
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
                        trace(L"Module internal name does not match. (%ls)", pInternalName);
                free(pInternalName);
                if ( tmp )
                        continue;

                pffi = GetVersionInfoFromHModuleAlloc(hModule, L"\\", &cbffi);
                if ( !pffi ) {
                        trace(L"Failed to allocate version information from hmodule.");
                        continue;
                }

                // assure wuaueng.dll is at least the minimum supported version
                tmp = ((is_win7 && ProductVersionCompare(pffi, 7, 6, 7601, 23714) != -1)
                        || (is_win81 && ProductVersionCompare(pffi, 7, 9, 9600, 18621) != -1));
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
                        result = true;
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

        if ( pParam ) {
                // this will be VirtualFree()'d by the function at pStartAddress
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
                hThread = CreateRemoteThread(hProcess,
                        NULL,
                        0,
                        (LPTHREAD_START_ROUTINE)((uint8_t *)hRemoteModule + ((uint8_t *)pStartAddress - (uint8_t *)hModule)),
                        pBaseAddress,
                        0,
                        NULL);

                if ( hThread ) {
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

        dwProcessId = GetProcessId(hProcess);

        hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessId);
        if ( !hSnapshot ) goto resume;

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

        if ( !WriteProcessMemory(hProcess, pBaseAddress, pLibFilename, nSize, NULL) )
                goto vfree;

        hThread = CreateRemoteThread(hProcess,
                NULL,
                0,
                (LPTHREAD_START_ROUTINE)LoadLibraryW,
                pBaseAddress,
                0,
                NULL);
        if ( !hThread ) goto vfree;

        WaitForSingleObject(hThread, INFINITE);

        if ( sizeof *phRemoteModule > sizeof(DWORD) ) {
                hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessId);
                if ( hSnapshot ) {
                        *phRemoteModule = GetRemoteHModuleFromTh32ModuleSnapshot(
                                hSnapshot,
                                pLibFilename);

                        CloseHandle(hSnapshot);
                        result = *phRemoteModule != NULL;
                }
        } else {
                result = GetExitCodeThread(hThread, (LPDWORD)phRemoteModule) != FALSE;
        }
        CloseHandle(hThread);
vfree:  VirtualFreeEx(hProcess, pBaseAddress, 0, MEM_RELEASE);
resume: NtResumeProcess(hProcess);
        return result;
}

bool wufuc_InjectLibrary(DWORD dwProcessId, ContextHandles *pContext)
{
        bool result = false;
        HANDLE hProcess;
        wchar_t MutexName[44];
        HANDLE hChildMutex;
        HANDLE hSrcProcess;
        ContextHandles param = { 0 };

        if ( swprintf_s(MutexName, _countof(MutexName), L"Global\\%08x-7132-44a8-be15-56698979d2f3", dwProcessId) == -1 ) {
                trace(L"Failed to print mutex name to string! (%lu)", dwProcessId);
                return result;
        }
        if ( !InitializeMutex(false, MutexName, &hChildMutex) ) {
                return result;
        }
        hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);

        if ( !hProcess ) {
                trace(L"Failed to open target process! (GetLastError=%lu)", GetLastError());
                goto close_mutex;
        };
        hSrcProcess = GetCurrentProcess();

        if ( DuplicateHandle(hSrcProcess, pContext->hParentMutex, hProcess, &param.hParentMutex, SYNCHRONIZE, FALSE, 0)
                && DuplicateHandle(hSrcProcess, pContext->hUnloadEvent, hProcess, &param.hUnloadEvent, SYNCHRONIZE, FALSE, 0)
                && DuplicateHandle(hSrcProcess, hChildMutex, hProcess, &param.hChildMutex, 0, FALSE, DUPLICATE_SAME_ACCESS) ) {

                if ( InjectLibraryAndCreateRemoteThread(hProcess, PIMAGEBASE, ThreadStartCallback, &param, sizeof param) )
                        trace(L"Injected into process. (%lu)", dwProcessId);
        } else {
                trace(L"Failed to duplicate context handles! (GetLastError=%lu", GetLastError());
        }
        CloseHandle(hProcess);
close_mutex:
        CloseHandle(hChildMutex);
        return result;
}
