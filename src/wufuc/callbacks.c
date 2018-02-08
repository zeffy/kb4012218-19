#include "stdafx.h"
#include "callbacks.h"
#include "hooks.h"
#include "hlpmisc.h"
#include "hlpmem.h"
#include "hlpsvc.h"

VOID CALLBACK ServiceNotifyCallback(PSERVICE_NOTIFYW pNotifyBuffer)
{
        trace(L"Enter service notify callback. (NotifyStatus=%ld ServiceStatus=%ld)", pNotifyBuffer->dwNotificationStatus, pNotifyBuffer->ServiceStatus);
        switch ( pNotifyBuffer->dwNotificationStatus ) {
        case ERROR_SUCCESS:
                if ( pNotifyBuffer->ServiceStatus.dwProcessId )
                        wufuc_InjectLibrary(pNotifyBuffer->ServiceStatus.dwProcessId, (ContextHandles *)pNotifyBuffer->pContext);
                break;
        case ERROR_SERVICE_MARKED_FOR_DELETE:
                SetEvent(((ContextHandles *)pNotifyBuffer->pContext)->hUnloadEvent);
                break;
        }
        if ( pNotifyBuffer->pszServiceNames )
                LocalFree((HLOCAL)pNotifyBuffer->pszServiceNames);
}

DWORD WINAPI ThreadStartCallback(LPVOID pParam)
{
        ContextHandles ctx;
        SC_HANDLE hSCM;
        DWORD dwProcessId;
        DWORD cbData;
        HMODULE hModule;
        DWORD result;

        // get mutex and unload event handles from virtual memory
        if ( !pParam ) {
                trace(L"Context parameter is null!");
                goto unload;
        }
        ctx = *(ContextHandles *)pParam;
        if ( !VirtualFree(pParam, 0, MEM_RELEASE) )
                trace(L"Failed to free context parameter. (%p, GetLastError=%lu)",
                        pParam, GetLastError());

        // acquire child mutex, should be immediate.
        if ( WaitForSingleObject(ctx.hChildMutex, 5000) != WAIT_OBJECT_0 ) {
                trace(L"Failed to acquire aux mutex within five seconds. (%p)", ctx.hChildMutex);
                goto close_handles;
        }

        hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
        if ( !hSCM ) {
                trace(L"Failed to open SCM. (GetLastError=%ul)", GetLastError());
                goto release;
        }

        dwProcessId = HeuristicServiceProcessIdByName(hSCM, L"wuauserv");
        CloseServiceHandle(hSCM);
        if ( dwProcessId != GetCurrentProcessId() ) {
                trace(L"Injected into wrong process!", GetCurrentProcessId(), dwProcessId);
                goto release;
        }

        g_pszWUServiceDll = RegGetValueAlloc(HKEY_LOCAL_MACHINE,
                L"SYSTEM\\CurrentControlSet\\services\\wuauserv\\Parameters",
                L"ServiceDll",
                RRF_RT_REG_SZ,
                NULL,
                &cbData);

        trace(L"Installing hooks...");

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        // hook LoadLibraryExW
        g_pfnLoadLibraryExW = DetourFindFunction("kernel32.dll", "LoadLibraryExW");
        if ( g_pfnLoadLibraryExW )
                DetourAttach(&(PVOID)g_pfnLoadLibraryExW, LoadLibraryExW_hook);

        // hook IsDeviceServiceable
        if ( g_pszWUServiceDll ) {
                hModule = GetModuleHandleW(g_pszWUServiceDll);
                if ( hModule ) {
                        if ( FindIDSFunctionPointer(hModule, &(PVOID)g_pfnIsDeviceServiceable) ) {
                                trace(L"Matched pattern for %ls!IsDeviceServiceable. (%p)",
                                        PathFindFileNameW(g_pszWUServiceDll),
                                        g_pfnIsDeviceServiceable);
                                DetourAttach(&(PVOID)g_pfnIsDeviceServiceable, IsDeviceServiceable_hook);
                        } else {
                                trace(L"No pattern matched!");
                        }
                } else {
                        // assume wuaueng.dll hasn't been loaded yet, apply
                        // RegQueryValueExW hook to fix incompatibility with
                        // UpdatePack7R2 and other patches that work by
                        // modifying the Windows Update ServiceDll path in the
                        // registry.
                        g_pfnRegQueryValueExW = DetourFindFunction("advapi.dll", "RegQueryValueExW");
                        if ( g_pfnRegQueryValueExW )
                                DetourAttach(&(PVOID)g_pfnRegQueryValueExW, RegQueryValueExW_hook);
                }

        }

        DetourTransactionCommit();

        // wait for unload event or parent mutex to be abandoned.
        // for example if the user killed rundll32.exe with task manager.
        // intentionally leave parent mutex open until this thread ends, at
        // which point it becomes abandoned again.
        result = WaitForMultipleObjects(_countof(ctx.handles), ctx.handles, FALSE, INFINITE);

        trace(L"Unload condition has been met.");

        // unhook
        if ( g_pfnLoadLibraryExW || g_pfnIsDeviceServiceable || g_pfnRegQueryValueExW) {
                trace(L"Removing hooks...");
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());

                if ( g_pfnLoadLibraryExW )
                        DetourDetach(&(PVOID)g_pfnLoadLibraryExW, LoadLibraryExW_hook);

                if ( g_pfnIsDeviceServiceable )
                        DetourDetach(&(PVOID)g_pfnIsDeviceServiceable, IsDeviceServiceable_hook);

                if ( g_pfnRegQueryValueExW )
                        DetourDetach(&(PVOID)g_pfnRegQueryValueExW, RegQueryValueExW_hook);

                DetourTransactionCommit();
        }
        free(g_pszWUServiceDll);

release:
        ReleaseMutex(ctx.hChildMutex);
close_handles:
        CloseHandle(ctx.hChildMutex);
        CloseHandle(ctx.hParentMutex);
        CloseHandle(ctx.hUnloadEvent);
unload:
        trace(L"Freeing library and exiting main thread.");
        FreeLibraryAndExitThread(PIMAGEBASE, 0);
}
