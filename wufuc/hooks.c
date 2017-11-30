#include "stdafx.h"
#include "hooks.h"
#include "patchwua.h"
#include "helpers.h"

#include <Psapi.h>

LPFN_REGQUERYVALUEEXW g_pfnRegQueryValueExW;
LPFN_LOADLIBRARYEXW g_pfnLoadLibraryExW;

LSTATUS WINAPI RegQueryValueExW_hook(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
        LSTATUS result;
        DWORD cbData;
        NTSTATUS Status;
        ULONG ResultLength;
        PKEY_NAME_INFORMATION pkni;
        size_t BufferCount;
        int current;
        int pos;
        wchar_t *tmp;
        size_t MaxCount;
        size_t cbLength;

        if ( (lpData && lpcbData)
                && (lpValueName && !_wcsicmp(lpValueName, L"ServiceDll")) ) {

                // store original lpData buffer size
                cbData = *lpcbData;

                // this way the dll path is guaranteed to be null-terminated
                result = RegGetValueW(hKey, NULL, lpValueName, RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND, lpType, lpData, lpcbData);
                if ( result != ERROR_SUCCESS )
                        goto L1;

                pkni = NULL;
                ResultLength = 0;
                do {
                        if ( ResultLength ) {
                                if ( pkni )
                                        free(pkni);
                                pkni = malloc(ResultLength);
                        }
                        Status = NtQueryKey((HANDLE)hKey, KeyNameInformation, pkni, ResultLength, &ResultLength);
                } while ( Status == STATUS_BUFFER_OVERFLOW || Status == STATUS_BUFFER_TOO_SMALL );
                if ( !NT_SUCCESS(Status) )
                        goto L2;

                BufferCount = pkni->NameLength / sizeof(wchar_t);
                // change key name to lower-case because there is no case-insensitive version of _snwscanf_s
                if ( !_wcslwr_s(pkni->Name, BufferCount) && _snwscanf_s(pkni->Name, BufferCount,
                        L"\\registry\\machine\\system\\controlset%03d\\services\\wuauserv\\parameters%n", &current, &pos) == 1
                        && pos == BufferCount ) {

                        const wchar_t *fname = path_find_fname((wchar_t *)lpData);

                        if ( !_wcsicmp(fname, L"wuaueng2.dll") // UpdatePack7R2
                                || !_wcsicmp(fname, L"WuaCpuFix64.dll") // WuaCpuFix
                                || !_wcsicmp(fname, L"WuaCpuFix.dll") ) {

                                MaxCount = cbData / sizeof(wchar_t);
                                tmp = malloc(cbData);
                                path_change_fname((wchar_t *)lpData, L"wuaueng.dll", tmp, MaxCount);
                                if ( path_expand_file_exists(tmp)
                                        && !wcscpy_s((wchar_t *)lpData, MaxCount, tmp)
                                        && SUCCEEDED(StringCbLengthW((PNZWCH)lpData, cbData, &cbLength)) ) {

                                        *lpcbData = (DWORD)(cbLength + (sizeof UNICODE_NULL));
                                }
                                free(tmp);
                        }
                }
L2:             free(pkni);
        } else {
                // handle normally
                result = g_pfnRegQueryValueExW(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
        }
L1:     return result;
}

HMODULE WINAPI LoadLibraryExW_hook(LPCWSTR lpFileName, HANDLE hFile, DWORD dwFlags)
{
        HMODULE result;
        DWORD dwLen;
        LPVOID pBlock;
        PLANGANDCODEPAGE ptl;
        UINT cb;
        wchar_t lpSubBlock[38];
        wchar_t *lpszInternalName;
        UINT uLen;
        VS_FIXEDFILEINFO *pffi;
        wchar_t path[MAX_PATH];
        const wchar_t *fname;
        MODULEINFO modinfo;

        result = g_pfnLoadLibraryExW(lpFileName, hFile, dwFlags);
        if ( !result ) {
                trace(L"Failed to load library: %ls (error code=%08X)", lpFileName, GetLastError());
                goto L1;
        }

        trace(L"Loaded library: %ls", lpFileName);
        dwLen = GetFileVersionInfoSizeW(lpFileName, NULL);
        if ( !dwLen )
                goto L1;

        pBlock = malloc(dwLen);

        if ( !GetFileVersionInfoW(lpFileName, 0, dwLen, pBlock)
                || !VerQueryValueW(pBlock, L"\\VarFileInfo\\Translation", (LPVOID *)&ptl, &cb) )
                goto L2;

        for ( size_t i = 0; i < (cb / sizeof(LANGANDCODEPAGE)); i++ ) {
                swprintf_s(lpSubBlock, _countof(lpSubBlock),
                        L"\\StringFileInfo\\%04x%04x\\InternalName",
                        ptl[i].wLanguage,
                        ptl[i].wCodePage);

                if ( VerQueryValueW(pBlock, lpSubBlock, (LPVOID *)&lpszInternalName, &uLen)
                        && !_wcsicmp(lpszInternalName, L"wuaueng.dll") ) {

                        VerQueryValueW(pBlock, L"\\", (LPVOID *)&pffi, &uLen);
                        GetModuleFileNameW(result, path, _countof(path));
                        fname = path_find_fname(path);

                        if ( (/*verify_win7() &&*/ ffi_ver_compare(pffi, 7, 6, 7601, 23714) != -1)
                                || (/*verify_win81() &&*/ ffi_ver_compare(pffi, 7, 9, 9600, 18621) != -1) ) {

                                if ( GetModuleInformation(GetCurrentProcess(), result, &modinfo, sizeof(MODULEINFO)) ) {
                                        if ( !patch_wua(modinfo.lpBaseOfDll, modinfo.SizeOfImage, fname) )
                                                trace(L"Failed to patch %ls!", fname);
                                }
                        }
                        break;
                }
        }
L2:     free(pBlock);
L1:     return result;
}

BOOL WINAPI IsDeviceServiceable_hook(void)
{
        return TRUE;
}
