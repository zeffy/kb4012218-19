#include "stdafx.h"
#include "context.h"
#include "wufuc.h"
#include "hooks.h"
#include "log.h"
#include "modulehelper.h"
#include "patternfind.h"
#include "versionhelper.h"

#include <minhook.h>

bool wufuc_inject(DWORD dwProcessId,
        LPTHREAD_START_ROUTINE pfnStart,
        context *pContext)
{
        bool result = false;
        HANDLE hProcess;
        HANDLE hMutex;
        context ctx;

        if ( !ctx_add_new_mutex_fmt(pContext,
                false,
                dwProcessId,
                &hMutex,
                L"Global\\%08x-7132-44a8-be15-56698979d2f3", dwProcessId) )
                return false;

        hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);
        if ( !hProcess ) return false;

        result = ctx_duplicate_context(pContext, hProcess, &ctx, hMutex, DUPLICATE_SAME_ACCESS, dwProcessId)
                && mod_inject_and_begin_thread(hProcess, PIMAGEBASE, pfnStart, &ctx, sizeof ctx);
        CloseHandle(hProcess);
        return result;
}

bool wufuc_hook(HMODULE hModule)
{
        bool result = false;
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

        if ( !ver_verify_windows_7_sp1() && !ver_verify_windows_8_1() )
                return false;

        ptl = ver_get_version_info_from_hmodule_alloc(hModule, L"\\VarFileInfo\\Translation", &cbtl);
        if ( !ptl ) {
                trace(L"Failed to allocate version translation information from hmodule.");
                return false;
        }
        hProcess = GetCurrentProcess();

        for ( size_t i = 0, count = (cbtl / sizeof *ptl); i < count; i++ ) {
                if ( swprintf_s(SubBlock,
                        _countof(SubBlock),
                        L"\\StringFileInfo\\%04x%04x\\InternalName",
                        ptl[i].wLanguage,
                        ptl[i].wCodePage) == -1 )
                        continue;

                pInternalName = ver_get_version_info_from_hmodule_alloc(hModule, SubBlock, &cbInternalName);
                if ( !pInternalName ) {
                        trace(L"Failed to allocate version internal name from hmodule.");
                        continue;
                }

                // identify wuaueng.dll by its resource data
                if ( !_wcsicmp(pInternalName, L"wuaueng.dll") ) {
                        pffi = ver_get_version_info_from_hmodule_alloc(hModule, L"\\", &cbffi);
                        if ( !pffi ) {
                                trace(L"Failed to allocate version information from hmodule.");
                                break;
                        }
                        trace(L"Windows Update Agent version: %hu.%hu.%hu.%hu"),
                                HIWORD(pffi->dwProductVersionMS),
                                LOWORD(pffi->dwProductVersionMS),
                                HIWORD(pffi->dwProductVersionLS),
                                LOWORD(pffi->dwProductVersionLS);

                        // assure wuaueng.dll is at least the minimum supported version
                        tmp = ((ver_verify_windows_7_sp1() && ver_compare_product_version(pffi, 7, 6, 7601, 23714) != -1)
                                || (ver_verify_windows_8_1() && ver_compare_product_version(pffi, 7, 9, 9600, 18621) != -1));
                        free(pffi);
                        if ( !tmp ) {
                                trace(L"Windows Update Agent does not meet the minimum supported version.");
                                break;
                        }
                        if ( !GetModuleInformation(hProcess, hModule, &modinfo, sizeof modinfo) ) {
                                trace(L"Failed to get module information (%p)", hModule);
                                break;
                        }
                        offset = patternfind(modinfo.lpBaseOfDll, modinfo.SizeOfImage,
#ifdef _WIN64
                                "FFF3 4883EC?? 33DB 391D???????? 7508 8B05????????"
#else
                                ver_verify_windows_7_sp1()
                                ? "833D????????00 743E E8???????? A3????????"
                                : "8BFF 51 833D????????00 7507 A1????????"
#endif
                        );

                        if ( offset == -1 ) {
                                trace(L"Could not locate pattern offset!");
                                break;
                        } else {
                                result = MH_CreateHook((PVOID)((uint8_t *)modinfo.lpBaseOfDll + offset),
                                        IsDeviceServiceable_hook,
                                        NULL) == MH_OK;
                        }
                        break;
                        } else trace(L"Module internal name does not match. (%ls)", pInternalName);
                        free(pInternalName);
                }
        free(ptl);
        return result;
        }
