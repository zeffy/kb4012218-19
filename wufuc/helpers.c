#include "stdafx.h"
#include "helpers.h"
#include <sddl.h>

bool create_exclusive_mutex(const wchar_t *name, HANDLE *pmutex)
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

bool create_event_with_security_descriptor(const wchar_t *descriptor, bool manualreset, bool initialstate, const wchar_t *name, HANDLE *pevent)
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

bool inject_self_into_process(DWORD dwProcessId, HMODULE *phModule)
{
        wchar_t szFilename[MAX_PATH];
        DWORD nLength;

        nLength = GetModuleFileNameW(PIMAGEBASE, szFilename, _countof(szFilename));

        if ( nLength )
                return inject_dll_into_process(dwProcessId, szFilename, nLength, phModule);

        return false;
}

bool inject_dll_into_process(DWORD dwProcessId, const wchar_t *pszFilename, size_t nLength, HMODULE *phModule)
{
        bool result = false;
        HANDLE hProcess;
        NTSTATUS Status;
        LPVOID pBaseAddress;
        HANDLE hThread;
        HANDLE hModule = NULL;

        hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);
        if ( !hProcess ) return result;

        Status = NtSuspendProcess(hProcess);
        if ( !NT_SUCCESS(Status) ) goto L1;

        pBaseAddress = VirtualAllocEx(hProcess, NULL, nLength + (sizeof *pszFilename), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if ( !pBaseAddress ) goto L2;

        if ( WriteProcessMemory(hProcess, pBaseAddress, pszFilename, nLength, NULL)
                && (hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryW, pBaseAddress, 0, NULL)) ) {

                WaitForSingleObject(hThread, INFINITE);
                if ( sizeof(DWORD) < sizeof hModule ) {

                } else {
                        GetExitCodeThread(hThread, (LPDWORD)&hModule);
                }
                if ( hModule ) {
                        result = true;
                        *phModule = hModule;
                }
                CloseHandle(hThread);
        }
        VirtualFreeEx(hProcess, pBaseAddress, 0, MEM_RELEASE);
L2:     NtResumeProcess(hProcess);
L1:     CloseHandle(hProcess);
        return result;
}

const wchar_t *path_find_fname(const wchar_t *path)
{
        const wchar_t *pwc = NULL;
        if ( path )
                pwc = (const wchar_t *)wcsrchr(path, L'\\');

        return (pwc && *(++pwc)) ? pwc : path;
}

bool path_change_fname(const wchar_t *srcpath, const wchar_t *fname, wchar_t *dstpath, size_t size)
{
        bool result;
        wchar_t drive[_MAX_DRIVE];
        wchar_t dir[_MAX_DIR];

        result = !_wsplitpath_s(srcpath, drive, _countof(drive), dir, _countof(dir), NULL, 0, NULL, 0);
        if ( result )
                result = !_wmakepath_s(dstpath, size, drive, dir, fname, NULL);
        return result;
}

bool path_file_exists(const wchar_t *path)
{
        return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

bool path_expand_file_exists(const wchar_t *path)
{
        bool result;
        wchar_t *dst;
        DWORD buffersize;
        DWORD size;

        dst = NULL;
        size = 0;
        do {
                if ( size ) {
                        if ( dst )
                                free(dst);
                        dst = calloc(size, sizeof *dst);
                }
                buffersize = size;
                size = ExpandEnvironmentStringsW(path, dst, buffersize);
        } while ( buffersize < size );

        result = path_file_exists(dst);
        free(dst);
        return result;
}

int ffi_ver_compare(VS_FIXEDFILEINFO *pffi, WORD wMajor, WORD wMinor, WORD wBuild, WORD wRev)
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

size_t find_argv_option(const wchar_t **argv, size_t argc, const wchar_t *option)
{
        size_t index = -1;

        for ( size_t i = 1; i < argc; i++ ) {
                if ( !_wcsicmp(argv[i], option) )
                        index = i;
        }
        if ( index == -1 )
                return index;

        return ++index < argc ? index : -1;
}
