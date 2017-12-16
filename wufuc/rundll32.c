#include "stdafx.h"
#include "callbacks.h"
#include "helpers.h"

void CALLBACK RUNDLL32_StartW(HWND hwnd, HINSTANCE hinst, LPWSTR lpszCmdLine, int nCmdShow)
{
        HANDLE hMutex;
        HANDLE hEvent;
        DWORD cbBufSize;
        LPQUERY_SERVICE_CONFIGW pServiceConfig;
        wchar_t GroupName[256];
        DWORD dwProcessId;
        bool Unloading = false;
        bool Lagging;
        SC_HANDLE hSCM;
        SC_HANDLE hService;
        SERVICE_NOTIFYW NotifyBuffer;

        if ( !CreateExclusiveMutex(L"Global\\{25020063-B5A7-4227-9FDF-25CB75E8C645}", &hMutex) )
                return;

        if ( !CreateEventWithStringSecurityDescriptor(L"D:(A;;0x001F0003;;;BA)", true, false, L"Global\\wufuc_UnloadEvent", &hEvent) )
                goto L1;

        //hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
        //if ( hSCM ) {
        //        trace(L"hSCM=%p", hSCM);
        //        pServiceConfig = QueryServiceConfigByNameAlloc(hSCM, L"wuauserv", &cbBufSize);
        //        trace(L"pServiceConfig=%p", pServiceConfig);
        //        if ( pServiceConfig ) {
        //                // inject into existing service host process if wuauserv is configured as shared process
        //                trace(L"pServiceConfig->dwServiceType=%lu", pServiceConfig->dwServiceType);
        //                if ( pServiceConfig->dwServiceType == SERVICE_WIN32_SHARE_PROCESS
        //                        && QueryServiceGroupName(pServiceConfig, GroupName, _countof(GroupName)) ) {
        //                        trace(L"GroupName=%ls", GroupName);

        //                        dwProcessId = InferSvchostGroupProcessId(hSCM, GroupName);
        //                        trace(L"dwProcessId=%lu", dwProcessId);
        //                        if ( dwProcessId )
        //                                InjectSelfAndCreateRemoteThread(dwProcessId, StartAddress, hEvent, SYNCHRONIZE);
        //                }
        //                free(pServiceConfig);
        //        }
        //        CloseServiceHandle(hSCM);
        //}

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
        HANDLE hEvent;

        hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Global\\wufuc_UnloadEvent");
        if ( hEvent ) {
                SetEvent(hEvent);
                CloseHandle(hEvent);
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
