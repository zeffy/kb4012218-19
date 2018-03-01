#include "stdafx.h"
#include "modulehelper.h"

HMODULE mod_get_from_th32_snapshot(HANDLE hSnapshot, const wchar_t *pLibFileName)
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

bool mod_inject_and_begin_thread(
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
        if ( mod_inject_by_hmodule(hProcess, hModule, &hRemoteModule) ) {
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

bool mod_inject_by_hmodule(HANDLE hProcess, HMODULE hModule, HMODULE *phRemoteModule)
{
        WCHAR Filename[MAX_PATH];
        DWORD nLength;

        nLength = GetModuleFileNameW(hModule, Filename, _countof(Filename));
        if ( nLength ) {
                return mod_inject(hProcess,
                        Filename,
                        nLength,
                        phRemoteModule);
        }
        return false;
}

bool mod_inject(
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

        *phRemoteModule = mod_get_from_th32_snapshot(hSnapshot,
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
                        *phRemoteModule = mod_get_from_th32_snapshot(
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
