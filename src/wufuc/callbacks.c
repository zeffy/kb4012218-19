#include "stdafx.h"
#include "callbacks.h"
#include "hlpmem.h"
#include "hlpmisc.h"
#include "hlpsvc.h"
#include "hooks.h"

VOID CALLBACK ServiceNotifyCallback(PSERVICE_NOTIFYW pNotifyBuffer)
{
        switch ( pNotifyBuffer->dwNotificationStatus ) {
        case ERROR_SUCCESS:
                if ( pNotifyBuffer->ServiceStatus.dwProcessId )
                        wufuc_InjectLibrary(
                                pNotifyBuffer->ServiceStatus.dwProcessId,
                                (ContextHandles *)pNotifyBuffer->pContext);
                break;
        case ERROR_SERVICE_MARKED_FOR_DELETE:
                SetEvent(((ContextHandles *)pNotifyBuffer->pContext)->hUnloadEvent);
                break;
        }
        if ( pNotifyBuffer->pszServiceNames )
                LocalFree((HLOCAL)pNotifyBuffer->pszServiceNames);
}

DWORD WINAPI PipeLoopThreadCallback(LPVOID pParam)
{
        HANDLE hPipe = (HANDLE)pParam;
        BOOL fSuccess;
        wchar_t  chBuf[512];
        while (true) {
                // Read from the pipe. 

                fSuccess = ReadFile(
                        hPipe,    // pipe handle 
                        chBuf,    // buffer to receive reply 
                        BUFSIZE * sizeof(wchar_t),  // size of buffer 
                        &cbRead,  // number of bytes read 
                        NULL);    // not overlapped 

                if ( !fSuccess && GetLastError() != ERROR_MORE_DATA )
                        break;;

                _tprintf(TEXT("\"%s\"\n"), chBuf);
        } 
}

DWORD WINAPI StartThreadCallback(LPVOID pParam)
{
        ContextHandles ctx;
        SC_HANDLE hSCM;
        SC_HANDLE hService;
        DWORD dwProcessId;
        LPQUERY_SERVICE_CONFIGW pServiceConfig;
        DWORD dwServiceType;
        wchar_t *str;
        HMODULE hModule;
        wchar_t Filename[MAX_PATH];
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
                trace(L"Failed to acquire child mutex within five seconds. (%p)", ctx.hChildMutex);
                goto close_handles;
        }

        hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
        if ( !hSCM ) {
                trace(L"Failed to open SCM. (GetLastError=%lu)", GetLastError());
                goto release;
        }

        hService = OpenServiceW(hSCM, L"wuauserv", SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
        dwProcessId = HeuristicServiceProcessId(hSCM, hService);
        pServiceConfig = QueryServiceConfigAlloc(hSCM, hService, NULL);
        dwServiceType = pServiceConfig->dwServiceType;
        free(pServiceConfig);
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);

        if ( dwProcessId != GetCurrentProcessId() ) {
                trace(L"Injected into wrong process!", GetCurrentProcessId(), dwProcessId);
                goto release;
        }

        trace(L"Installing hooks...");

        if ( dwServiceType == SERVICE_WIN32_SHARE_PROCESS ) {
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());

                // assume wuaueng.dll hasn't been loaded yet, apply
                // RegQueryValueExW hook to fix incompatibility with
                // UpdatePack7R2 and other patches that work by
                // modifying the Windows Update ServiceDll path in the
                // registry.
                g_pfnRegQueryValueExW = DetourFindFunction("kernel32.dll", "RegQueryValueExW");
                if ( g_pfnRegQueryValueExW )
                        DetourAttach(&(PVOID)g_pfnRegQueryValueExW, RegQueryValueExW_hook);
                DetourTransactionCommit();
        }

        // query the ServiceDll path after applying our compat hook so that it
        // is correct
        str = (wchar_t *)RegQueryValueExAlloc(HKEY_LOCAL_MACHINE,
                L"SYSTEM\\CurrentControlSet\\services\\wuauserv\\Parameters",
                L"ServiceDll", NULL, NULL);
        g_pszWUServiceDll = ExpandEnvironmentStringsAlloc(str, NULL);
        free(str);

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        g_pfnLoadLibraryExW = DetourFindFunction("kernel32.dll", "LoadLibraryExW");
        if ( g_pfnLoadLibraryExW )
                DetourAttach(&(PVOID)g_pfnLoadLibraryExW, LoadLibraryExW_hook);

        if ( g_pszWUServiceDll ) {
                if ( GetModuleHandleExW(0, g_pszWUServiceDll, &hModule) ) {
                        if ( FindIDSFunctionAddress(hModule, &(PVOID)g_pfnIsDeviceServiceable) ) {
                                trace(L"Matched pattern for %ls!IsDeviceServiceable. (%p)",
                                        PathFindFileNameW(g_pszWUServiceDll),
                                        g_pfnIsDeviceServiceable);
                                DetourAttach(&(PVOID)g_pfnIsDeviceServiceable, IsDeviceServiceable_hook);
                        } else {
                                trace(L"No pattern matched!");
                        }
                        FreeLibrary(hModule);
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
        if ( g_pfnLoadLibraryExW || g_pfnIsDeviceServiceable || g_pfnRegQueryValueExW ) {
                trace(L"Removing hooks...");
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());

                if ( g_pfnLoadLibraryExW )
                        DetourDetach(&(PVOID)g_pfnLoadLibraryExW, LoadLibraryExW_hook);

                // check to see if the last known address of IsDeviceServiceable
                // is still in the address space of wuaueng.dll before 
                // attempting to unhook the function.
                if ( g_pfnIsDeviceServiceable
                        && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                        (LPWSTR)g_pfnIsDeviceServiceableLastKnown, &hModule) ) {

                        if ( GetModuleFileNameW(hModule, Filename, _countof(Filename))
                                && (!_wcsicmp(Filename, g_pszWUServiceDll)
                                        || !_wcsicmp(Filename, PathFindFileNameW(g_pszWUServiceDll))) )
                                DetourDetach(&(PVOID)g_pfnIsDeviceServiceable, IsDeviceServiceable_hook);
                        FreeLibrary(hModule);
                }
                if ( g_pfnRegQueryValueExW )
                        DetourDetach(&(PVOID)g_pfnRegQueryValueExW, RegQueryValueExW_hook);

                DetourTransactionCommit();
        }
        free(g_pszWUServiceDll);

release:
        ReleaseMutex(ctx.hChildMutex);
close_handles:
        CloseHandle(ctx.hChildMutex);
        CloseHandle(ctx.hUnloadEvent);
        CloseHandle(ctx.hParentMutex);
        if ( g_hTracingMutex )
                CloseHandle(g_hTracingMutex);
unload:
        trace(L"Freeing library and exiting main thread.");
        FreeLibraryAndExitThread(PIMAGEBASE, 0);
}
