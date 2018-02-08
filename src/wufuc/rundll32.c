#include "stdafx.h"
#include "callbacks.h"
#include "hlpmisc.h"
#include "hlpmem.h"
#include "hlpsvc.h"

void CALLBACK RUNDLL32_StartW(HWND hwnd, HINSTANCE hinst, LPWSTR lpszCmdLine, int nCmdShow)
{
        ContextHandles ctx;
        DWORD dwDesiredAccess;
        DWORD dwProcessId;
        bool Unloading = false;
        bool Lagging;
        SC_HANDLE hSCM;
        SC_HANDLE hService;
        SERVICE_NOTIFYW NotifyBuffer;

        if ( !InitializeMutex(true,
                L"Global\\25020063-b5a7-4227-9fdf-25cb75e8c645",
                &ctx.hParentMutex) ) {

                trace(L"Failed to initialize main mutex. (GetLastError=%ul)", GetLastError());
                return;
        };
        if ( !CreateEventWithStringSecurityDescriptor(L"D:(A;;0x001F0003;;;BA)",
                true,
                false,
                L"Global\\wufuc_UnloadEvent",
                &ctx.hUnloadEvent) ) {

                trace(L"Failed to create unload event. (GetLastError=%ul)", GetLastError());
                goto close_mutex;
        }
        dwDesiredAccess = SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG;
        do {
                Lagging = false;
                hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
                if ( !hSCM ) {
                        trace(L"Failed to open SCM. (GetLastError=%ul)", GetLastError());
                        goto close_event;
                }

                hService = OpenServiceW(hSCM, L"wuauserv", dwDesiredAccess);
                if ( !hService ) {
                        trace(L"Failed to open service. (GetLastError=%ul)", GetLastError());
                        goto close_scm;
                }
                if ( (dwDesiredAccess & SERVICE_QUERY_CONFIG) == SERVICE_QUERY_CONFIG ) {
                        dwDesiredAccess &= ~SERVICE_QUERY_CONFIG;

                        dwProcessId = HeuristicServiceProcessId(hSCM, hService);
                        if ( dwProcessId )
                                wufuc_InjectLibrary(dwProcessId, &ctx);
                }
                ZeroMemory(&NotifyBuffer, sizeof NotifyBuffer);
                NotifyBuffer.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE;
                NotifyBuffer.pfnNotifyCallback = ServiceNotifyCallback;
                NotifyBuffer.pContext = (PVOID)&ctx;
                while ( !Unloading && !Lagging ) {
                        switch ( NotifyServiceStatusChangeW(hService,
                                SERVICE_NOTIFY_START_PENDING | SERVICE_NOTIFY_RUNNING,
                                &NotifyBuffer) ) {
                        case ERROR_SUCCESS:
                                Unloading = WaitForSingleObjectEx(ctx.hUnloadEvent, INFINITE, TRUE) == WAIT_OBJECT_0;
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
close_scm:      CloseServiceHandle(hSCM);
        } while ( Lagging );
close_event:
        CloseHandle(ctx.hUnloadEvent);
close_mutex:
        ReleaseMutex(ctx.hParentMutex);
        CloseHandle(ctx.hParentMutex);
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
        LPWSTR *argv;

        argv = CommandLineToArgvW(lpszCmdLine, &argc);
        if ( argv ) {
                if ( !DeleteFileW(argv[0]) && GetLastError() == ERROR_ACCESS_DENIED )
                        MoveFileExW(argv[0], NULL, MOVEFILE_DELAY_UNTIL_REBOOT);

                LocalFree((HLOCAL)argv);
        }
}
