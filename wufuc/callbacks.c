#include "stdafx.h"
#include "callbacks.h"
#include "hooks.h"
#include "helpers.h"

VOID CALLBACK ServiceNotifyCallback(PSERVICE_NOTIFYW pNotifyBuffer)
{
        switch ( pNotifyBuffer->dwNotificationStatus ) {
        case ERROR_SUCCESS:
                if ( pNotifyBuffer->ServiceStatus.dwProcessId )
                        InjectSelfAndCreateRemoteThread(pNotifyBuffer->ServiceStatus.dwProcessId, &StartAddress, (HANDLE)pNotifyBuffer->pContext, SYNCHRONIZE);
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
        SC_HANDLE hSCM;
        DWORD cbBufSize;
        LPQUERY_SERVICE_CONFIGW pServiceConfig;
        DWORD dwServiceType;
        int result;
        DWORD cbData;
        LPWSTR pServiceDll;
        HMODULE hModule;

        hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
        if ( !hSCM ) goto cleanup;

        pServiceConfig = QueryServiceConfigByNameAlloc(hSCM, L"wuauserv", &cbBufSize);
        CloseServiceHandle(hSCM);
        if ( !pServiceConfig ) goto cleanup;

        //dwServiceType = pServiceConfig->dwServiceType;
        result = _wcsicmp(pServiceConfig->lpBinaryPathName, GetCommandLineW());
        free(pServiceConfig);
        if ( result ) goto cleanup;

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        // hook apis if configured as shared service
       /* if ( dwServiceType == SERVICE_WIN32_SHARE_PROCESS ) {
                g_pfnLoadLibraryExW = DetourFindFunction("kernel32.dll", "LoadLibraryExW");
                DetourAttach(&(PVOID)g_pfnLoadLibraryExW, LoadLibraryExW_hook);

                g_pfnRegQueryValueExW = DetourFindFunction("kernel32.dll", "RegQueryValueExW");
                DetourAttach(&(PVOID)g_pfnRegQueryValueExW, RegQueryValueExW_hook);
        }*/

        // hook IsDeviceServiceable if wuaueng.dll is already loaded
        pServiceDll = RegGetValueAlloc(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\services\\wuauserv\\Parameters", L"ServiceDll", RRF_RT_REG_SZ, NULL, &cbData);
        if ( pServiceDll ) {
                hModule = GetModuleHandleW(pServiceDll);
                if ( hModule && FindIsDeviceServiceablePtr(pServiceDll, hModule, &(PVOID)g_pfnIsDeviceServiceable) )
                        DetourAttach(&(PVOID)g_pfnIsDeviceServiceable, IsDeviceServiceable_hook);

                free(pServiceDll);
        }
        DetourTransactionCommit();

        // wait for unload event
        WaitForSingleObject((HANDLE)pParam, INFINITE);

        // unhook functions
        trace(L"DetourTransactionBegin result=%d", DetourTransactionBegin());
        trace(L"DetourUpdateThread result=%d", DetourUpdateThread(GetCurrentThread()));
        //if ( g_pfnLoadLibraryExW )
        //        trace(L"DetourDetach LoadLibraryExW_hook result=%d", DetourDetach(&(PVOID)g_pfnLoadLibraryExW, LoadLibraryExW_hook));
        //if ( g_pfnRegQueryValueExW )
        //        trace(L"DetourDetach RegQueryValueExW_hook result=%d", DetourDetach(&(PVOID)g_pfnRegQueryValueExW, RegQueryValueExW_hook));
        if ( g_pfnIsDeviceServiceable )
                trace(L"DetourDetach IsDeviceServiceable_hook result=%d", DetourDetach(&(PVOID)g_pfnIsDeviceServiceable, IsDeviceServiceable_hook));
        trace(L"DetourTransactionCommit result=%d", DetourTransactionCommit());

cleanup:
        CloseHandle((HANDLE)pParam);
        FreeLibraryAndExitThread(PIMAGEBASE, 0);
}