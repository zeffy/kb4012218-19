#include "stdafx.h"
#include "context.h"
#include "callbacks.h"
#include "hooks.h"
#include "log.h"
#include "modulehelper.h"
#include "registryhelper.h"
#include "servicehelper.h"
#include "wufuc.h"

#include <minhook.h>

VOID CALLBACK cb_service_notify(PSERVICE_NOTIFYW pNotifyBuffer)
{
        switch ( pNotifyBuffer->dwNotificationStatus ) {
        case ERROR_SUCCESS:
                if ( pNotifyBuffer->ServiceStatus.dwProcessId )
                        wufuc_inject(
                                pNotifyBuffer->ServiceStatus.dwProcessId,
                                (LPTHREAD_START_ROUTINE)cb_start,
                                (context *)pNotifyBuffer->pContext);
                break;
        case ERROR_SERVICE_MARKED_FOR_DELETE:
                SetEvent(((context *)pNotifyBuffer->pContext)->uevent);
                break;
        }
        if ( pNotifyBuffer->pszServiceNames )
                LocalFree((HLOCAL)pNotifyBuffer->pszServiceNames);
}

DWORD WINAPI cb_start(context *ctx)
{
        SC_HANDLE hSCM;
        SC_HANDLE hService;
        DWORD dwProcessId;
        LPQUERY_SERVICE_CONFIGW pServiceConfig;
        DWORD dwServiceType;
        wchar_t *str;
        HMODULE hModule;
        DWORD result;

        // get mutex and unload event handles from virtual memory
        if ( !ctx ) {
                trace(L"Context parameter is null!");
                goto unload;
        }

        // acquire child mutex, this should be immediate.
        if ( WaitForSingleObject(ctx->handles[ctx->mutex_tag], 5000) != WAIT_OBJECT_0 ) {
                trace(L"Failed to acquire child mutex within five seconds. (%p)", ctx->handles[ctx->mutex_tag]);
                goto close_handles;
        }

        hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
        if ( !hSCM ) {
                trace(L"Failed to open SCM. (GetLastError=%lu)", GetLastError());
                goto release;
        }

        hService = OpenServiceW(hSCM, L"wuauserv",
                SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
        dwProcessId = svc_heuristic_process_id(hSCM, hService);
        pServiceConfig = svc_query_config_alloc(hSCM, hService, NULL);
        dwServiceType = pServiceConfig->dwServiceType;
        free(pServiceConfig);
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);

        if ( dwProcessId != GetCurrentProcessId() ) {
                trace(L"Injected into wrong process!");
                goto release;
        }

        if ( dwServiceType == SERVICE_WIN32_SHARE_PROCESS ) {
                // assume wuaueng.dll hasn't been loaded yet, apply
                // RegQueryValueExW hook to fix incompatibility with
                // UpdatePack7R2 and other patches that modify the
                // Windows Update ServiceDll path in the registry.
                MH_CreateHookApi(L"kernel32.dll",
                        "RegQueryValueExW",
                        RegQueryValueExW_hook,
                        &(PVOID)g_pfnRegQueryValueExW);
                MH_EnableHook(g_pfnRegQueryValueExW);
        }

        // query the ServiceDll path after applying our compat hook so that it
        // is correct
        str = (wchar_t *)reg_query_value_alloc(HKEY_LOCAL_MACHINE,
                L"SYSTEM\\CurrentControlSet\\services\\wuauserv\\Parameters",
                L"ServiceDll", NULL, NULL);
        if ( !str ) {
abort_hook:
                MH_RemoveHook(g_pfnRegQueryValueExW);
                goto release;
        }
        g_pszWUServiceDll = env_expand_strings_alloc(str, NULL);
        if ( !g_pszWUServiceDll ) goto abort_hook;
        free(str);

        MH_CreateHookApi(L"kernel32.dll",
                "LoadLibraryExW",
                LoadLibraryExW_hook,
                &(PVOID)g_pfnLoadLibraryExW);

        if ( GetModuleHandleExW(0, g_pszWUServiceDll, &hModule)
                || GetModuleHandleExW(0, PathFindFileNameW(g_pszWUServiceDll), &hModule) ) {

                // hook IsDeviceServiceable if wuaueng.dll is already loaded
                wufuc_hook(hModule);
                FreeLibrary(hModule);

        }
        MH_EnableHook(MH_ALL_HOOKS);

        // wait for unload event or the main mutex to be released or abandoned,
        // for example if the user killed rundll32.exe with task manager.

        // we use ctx_wait_any_unsafe here because contexts created by
        // ctx_duplicate_context are not initialized by ctx_create,
        // and have no critical section to lock, so they are only used to
        // hold static values to send to another process.
        ctx_wait_any_unsafe(ctx, false);
        trace(L"Unload condition has been met.");

        MH_DisableHook(MH_ALL_HOOKS);
        free(g_pszWUServiceDll);
release:
        ReleaseMutex(ctx->handles[ctx->mutex_tag]);
close_handles:
        CloseHandle(ctx->handles[ctx->mutex_tag]);
        CloseHandle(ctx->mutex);
        CloseHandle(ctx->uevent);
        VirtualFree(ctx, 0, MEM_RELEASE);
unload:
        trace(L"Unloading wufuc and exiting thread.");
        FreeLibraryAndExitThread(PIMAGEBASE, 0);
}

