#include "stdafx.h"
#include "callbacks.h"
#include "hooks.h"
#include "helpers.h"

bool DuplicateContextHandles(HANDLE hSrcProcess, ContextHandles *pSrcContext, HANDLE hAuxiliaryMutex, HANDLE hTargetProcess, ContextHandles *pTargetContext)
{
        if ( DuplicateHandle(hSrcProcess, pSrcContext->hMainMutex,
                hTargetProcess, &pTargetContext->hMainMutex, SYNCHRONIZE, FALSE, 0)

                && DuplicateHandle(hSrcProcess, pSrcContext->hUnloadEvent,
                        hTargetProcess, &pTargetContext->hUnloadEvent, SYNCHRONIZE, FALSE, 0)

                && DuplicateHandle(hSrcProcess, hAuxiliaryMutex,
                        hTargetProcess, &pTargetContext->hAuxiliaryMutex, 0, FALSE, DUPLICATE_SAME_ACCESS) ) {

                return true;
        }
        return false;
}

VOID CALLBACK ServiceNotifyCallback(PSERVICE_NOTIFYW pNotifyBuffer)
{
        HANDLE hProcess;
        wchar_t MutexName[44];
        HANDLE hAuxiliaryMutex;
        ContextHandles TargetContext;

        switch ( pNotifyBuffer->dwNotificationStatus ) {
        case ERROR_SUCCESS:
                if ( !pNotifyBuffer->ServiceStatus.dwProcessId
                        || swprintf_s(MutexName, _countof(MutexName),
                                L"Global\\%08x-7132-44a8-be15-56698979d2f3",
                                pNotifyBuffer->ServiceStatus.dwProcessId) == -1
                        || !InitializeMutex(false, MutexName, &hAuxiliaryMutex) )
                        break;

                hProcess = OpenProcess(PROCESS_ALL_ACCESS,
                        FALSE,
                        pNotifyBuffer->ServiceStatus.dwProcessId);
                if ( !hProcess ) {
                        trace(L"Failed to open target process! (GetLastError=%lu)", GetLastError());
                        break;
                };

                if ( !DuplicateContextHandles(GetCurrentProcess(), pNotifyBuffer->pContext, hAuxiliaryMutex, hProcess, &TargetContext) ) {
                        trace(L"Failed to duplicate handles into target process."
                                L"%p %p %p (GetLastError=%lu)",
                                TargetContext.hMainMutex,
                                TargetContext.hUnloadEvent,
                                TargetContext.hAuxiliaryMutex,
                                GetLastError());
                        break;
                };
                InjectLibraryAndCreateRemoteThread(
                        hProcess,
                        PIMAGEBASE,
                        StartAddress,
                        &TargetContext,
                        sizeof TargetContext);
                break;
        case ERROR_SERVICE_MARKED_FOR_DELETE:
                SetEvent((HANDLE)pNotifyBuffer->pContext);
                break;
        }
        if ( pNotifyBuffer->pszServiceNames )
                LocalFree((HLOCAL)pNotifyBuffer->pszServiceNames);
}

DWORD WINAPI StartAddress(LPVOID pParam)
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
                trace(L"Failed to free context parameter. pParam=%p GetLastError=%lu",
                        pParam, GetLastError());

        // acquire child mutex, should be immediate.
        if ( WaitForSingleObject(ctx.hAuxiliaryMutex, 5000) != WAIT_OBJECT_0 ) {
                trace(L"Failed to acquire aux mutex within five seconds. hAuxiliaryMutex=%p", ctx.hAuxiliaryMutex);
                goto close_handles;
        }

        hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
        if ( !hSCM ) {
                trace(L"Failed to open SCM. (GetLastError=%ul)", GetLastError());
                goto release;
        }

        dwProcessId = QueryServiceProcessId(hSCM, L"wuauserv");
        CloseServiceHandle(hSCM);
        if ( dwProcessId != GetCurrentProcessId() ) {
                trace(L"Injected into wrong process! CurrentProcessId=%lu wuauserv ProcessId=%lu",
                        GetCurrentProcessId, dwProcessId);
                goto release;
        }

        // hook IsDeviceServiceable
        g_pszWUServiceDll = RegGetValueAlloc(HKEY_LOCAL_MACHINE,
                L"SYSTEM\\CurrentControlSet\\services\\wuauserv\\Parameters",
                L"ServiceDll",
                RRF_RT_REG_SZ,
                NULL,
                &cbData);

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        // hook LoadLibraryExW
        g_pfnLoadLibraryExW = DetourFindFunction("kernel32.dll", "LoadLibraryExW");
        if ( g_pfnLoadLibraryExW )
                DetourAttach(&(PVOID)g_pfnLoadLibraryExW, LoadLibraryExW_hook);

        if ( g_pszWUServiceDll ) {
                hModule = GetModuleHandleW(g_pszWUServiceDll);
                if ( hModule && FindIsDeviceServiceablePtr(hModule,
                        &(PVOID)g_pfnIsDeviceServiceable) ) {

                        DetourAttach(&(PVOID)g_pfnIsDeviceServiceable, IsDeviceServiceable_hook);
                }
        }
        DetourTransactionCommit();


        // wait for unload event or parent mutex to be abandoned.
        // for example if the user killed rundll32.exe with task manager.
        // intentionally leave parent mutex open until this thread ends, at
        // which point it becomes abandoned again.
        result = WaitForMultipleObjects(_countof(ctx.handles), ctx.handles, FALSE, INFINITE);

        trace(L"Unloading!");

        // unhook
        if ( g_pfnLoadLibraryExW || g_pfnIsDeviceServiceable ) {
                trace(L"Removing hooks...");
                trace(L"DetourTransactionBegin %lu", DetourTransactionBegin());
                trace(L"DetourUpdateThread %lu", DetourUpdateThread(GetCurrentThread()));

                if ( g_pfnLoadLibraryExW )
                        trace(L"DetourDetach LoadLibraryExW %lu", DetourDetach(&(PVOID)g_pfnLoadLibraryExW, LoadLibraryExW_hook));

                if ( g_pfnIsDeviceServiceable )
                        trace(L"DetourDetach IsDeviceServiceable %lu", DetourDetach(&(PVOID)g_pfnIsDeviceServiceable, IsDeviceServiceable_hook));

                trace(L"DetourTransactionCommit %lu", DetourTransactionCommit());
        }
        free(g_pszWUServiceDll);

release:
        ReleaseMutex(ctx.hAuxiliaryMutex);
close_handles:
        CloseHandle(ctx.hAuxiliaryMutex);
        CloseHandle(ctx.hMainMutex);
        CloseHandle(ctx.hUnloadEvent);
unload:
        FreeLibraryAndExitThread(PIMAGEBASE, 0);
}