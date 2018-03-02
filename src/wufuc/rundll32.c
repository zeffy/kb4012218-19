#include "stdafx.h"
#include "context.h"
#include "callbacks.h"
#include "log.h"
#include "modulehelper.h"
#include "registryhelper.h"
#include "servicehelper.h"
#include "wufuc.h"

void CALLBACK RUNDLL32_StartW(HWND hwnd, HINSTANCE hinst, LPWSTR lpszCmdLine, int nCmdShow)
{
        context ctx;
        DWORD dwDesiredAccess;
        DWORD dwProcessId;
        bool Unloading = false;
        bool Lagging;
        SC_HANDLE hSCM;
        SC_HANDLE hService;
        SERVICE_NOTIFYW NotifyBuffer;

        if ( !ctx_create(&ctx,
                L"Global\\25020063-b5a7-4227-9fdf-25cb75e8c645", 0,
                L"Global\\wufuc_UnloadEvent", L"D:(A;;0x001F0003;;;BA)", 0) )
                return;

        dwDesiredAccess = SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG;
        do {
                Lagging = false;
                hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
                if ( !hSCM ) goto delete_ctx;

                hService = OpenServiceW(hSCM, L"wuauserv", dwDesiredAccess);
                if ( !hService ) goto close_scm;

                if ( (dwDesiredAccess & SERVICE_QUERY_CONFIG) == SERVICE_QUERY_CONFIG ) {
                        dwDesiredAccess &= ~SERVICE_QUERY_CONFIG;

                        dwProcessId = svc_heuristic_process_id(hSCM, hService);
                        if ( dwProcessId )
                                wufuc_inject(dwProcessId, (LPTHREAD_START_ROUTINE)cb_start, &ctx);
                }
                ZeroMemory(&NotifyBuffer, sizeof NotifyBuffer);
                NotifyBuffer.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE;
                NotifyBuffer.pfnNotifyCallback = cb_service_notify;
                NotifyBuffer.pContext = (PVOID)&ctx;
                while ( !Unloading && !Lagging ) {
                        switch ( NotifyServiceStatusChangeW(hService,
                                SERVICE_NOTIFY_START_PENDING | SERVICE_NOTIFY_RUNNING,
                                &NotifyBuffer) ) {
                        case ERROR_SUCCESS:
                                Unloading = WaitForSingleObjectEx(ctx.uevent, INFINITE, TRUE) == WAIT_OBJECT_0;
                                break;
                        case ERROR_SERVICE_NOTIFY_CLIENT_LAGGING:
                                trace(L"Client lagging!");
                                Lagging = true;
                                break;
                        default:
                                Unloading = true;
                                break;
                        }
                }
                CloseServiceHandle(hService);
close_scm:      
                CloseServiceHandle(hSCM);
        } while ( Lagging );
delete_ctx:
        ctx_delete(&ctx);
}

void CALLBACK RUNDLL32_UnloadW(HWND hwnd, HINSTANCE hinst, LPWSTR lpszCmdLine, int nCmdShow)
{
        HANDLE hEvent;

        hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Global\\wufuc_UnloadEvent");
        if ( hEvent ) {
                SetEvent(hEvent);
                CloseHandle(hEvent);
        }
}

void CALLBACK RUNDLL32_DeleteFileW(
        HWND hwnd,
        HINSTANCE hinst,
        LPWSTR lpszCmdLine,
        int nCmdShow)
{
        int argc;
        wchar_t **argv;

        argv = CommandLineToArgvW(lpszCmdLine, &argc);
        if ( argv ) {
                if ( !DeleteFileW(argv[0]) && GetLastError() == ERROR_ACCESS_DENIED )
                        MoveFileExW(argv[0], NULL, MOVEFILE_DELAY_UNTIL_REBOOT);

                LocalFree((HLOCAL)argv);
        }
}
