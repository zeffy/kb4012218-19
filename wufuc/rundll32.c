#include "stdafx.h"
#include "callbacks.h"
#include "helpers.h"

void CALLBACK RUNDLL32_StartW(HWND hwnd, HINSTANCE hinst, LPWSTR lpszCmdLine, int nCmdShow)
{
        HANDLE hMutex;
        HANDLE hEvent;
        bool Unloading;
        bool Lagging;
        SC_HANDLE hSCM;
        SC_HANDLE hService;
        SERVICE_NOTIFYW NotifyBuffer;

        if ( !create_exclusive_mutex(L"Global\\{25020063-B5A7-4227-9FDF-25CB75E8C645}", &hMutex) )
                return;

        if ( !create_event_with_security_descriptor(L"D:(A;;0x001F0003;;;BA)", true, false, L"Global\\wufuc_UnloadEvent", &hEvent) )
                goto L1;

        Unloading = false;
        do {
                Lagging = false;
                hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
                if ( !hSCM ) goto L2;

                hService = OpenServiceW(hSCM, L"wuauserv", SERVICE_QUERY_STATUS);
                if ( !hService ) goto L3;

                ZeroMemory(&NotifyBuffer, sizeof NotifyBuffer);
                NotifyBuffer.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE;
                NotifyBuffer.pfnNotifyCallback = ServiceNotifyCallback;
                NotifyBuffer.pContext = (PVOID)hEvent;
                while ( !Unloading && !Lagging ) {
                        switch ( NotifyServiceStatusChangeW(hService,
                                SERVICE_NOTIFY_START_PENDING | SERVICE_NOTIFY_RUNNING,
                                &NotifyBuffer) ) {
                        case ERROR_SUCCESS:
                                Unloading = WaitForSingleObjectEx(hEvent, INFINITE, TRUE) == WAIT_OBJECT_0;
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
L3:             CloseServiceHandle(hSCM);
        } while ( Lagging );
L2:     CloseHandle(hEvent);
L1:     ReleaseMutex(hMutex);
        CloseHandle(hMutex);
}

void CALLBACK RUNDLL32_UnloadW(HWND hwnd, HINSTANCE hinst, LPWSTR lpszCmdLine, int nCmdShow)
{
        HANDLE event;

        event = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Global\\wufuc_UnloadEvent");
        if ( event ) {
                SetEvent(event);
                CloseHandle(event);
        }
}

void CALLBACK RUNDLL32_DeleteFileW(HWND hwnd, HINSTANCE hinst, LPWSTR lpszCmdLine, int nCmdShow)
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
