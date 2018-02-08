#include "stdafx.h"
#include "hlpmisc.h"
#include "hlpsvc.h"

LPQUERY_SERVICE_CONFIGW QueryServiceConfigByNameAlloc(
        SC_HANDLE hSCM,
        const wchar_t *pServiceName,
        LPDWORD pcbBufSize)
{
        SC_HANDLE hService;
        LPQUERY_SERVICE_CONFIGW result = NULL;

        hService = OpenServiceW(hSCM, pServiceName, SERVICE_QUERY_CONFIG);
        if ( !hService ) return result;

        result = QueryServiceConfigAlloc(hSCM, hService, pcbBufSize);

        CloseServiceHandle(hService);
        return result;
}

LPQUERY_SERVICE_CONFIGW QueryServiceConfigAlloc(
        SC_HANDLE hSCM,
        SC_HANDLE hService,
        LPDWORD pcbBufSize)
{
        DWORD cbBytesNeeded;
        LPQUERY_SERVICE_CONFIGW result = NULL;

        if ( !QueryServiceConfigW(hService, NULL, 0, &cbBytesNeeded)
                && GetLastError() == ERROR_INSUFFICIENT_BUFFER ) {

                result = malloc(cbBytesNeeded);
                if ( result ) {
                        if ( QueryServiceConfigW(hService, result, cbBytesNeeded, &cbBytesNeeded) ) {
                                *pcbBufSize = cbBytesNeeded;
                        } else {
                                free(result);
                                result = NULL;
                        }
                }
        }
        return result;
}

bool QueryServiceStatusProcessInfoByName(
        SC_HANDLE hSCM,
        const wchar_t *pServiceName,
        LPSERVICE_STATUS_PROCESS pServiceStatus)
{
        bool result = false;
        SC_HANDLE hService;
        DWORD cbBytesNeeded;

        hService = OpenServiceW(hSCM, pServiceName, SERVICE_QUERY_STATUS);
        if ( !hService ) {
                trace(L"Failed to open service %ls! (GetLastError=%ul)", pServiceName, GetLastError());
                return result;
        }

        result = !!QueryServiceStatusEx(hService,
                SC_STATUS_PROCESS_INFO,
                (LPBYTE)pServiceStatus,
                sizeof *pServiceStatus,
                &cbBytesNeeded);
        CloseServiceHandle(hService);
        return result;
}

bool QueryServiceGroupName(const LPQUERY_SERVICE_CONFIGW pServiceConfig, LPWSTR *pGroupName, HLOCAL *hMem)
{
        bool result = false;
        int NumArgs;
        LPWSTR *argv;

        argv = CommandLineToArgvW(pServiceConfig->lpBinaryPathName, &NumArgs);
        if ( argv ) {
                if ( !_wcsicmp(PathFindFileNameW(argv[0]), L"svchost.exe") ) {

                        for ( int i = 1; (i + 1) < NumArgs; i++ ) {
                                if ( !_wcsicmp(argv[i], L"-k") ) {
                                        *pGroupName = argv[++i];
                                        *hMem = (HLOCAL)argv;
                                        return true;
                                }
                        }
                }
                LocalFree((HLOCAL)argv);
        }
        return false;
}

DWORD QueryServiceProcessId(SC_HANDLE hSCM, SC_HANDLE hService)
{
        DWORD result = 0;
        SERVICE_STATUS_PROCESS ServiceStatus;
        DWORD cbBytesNeeded;

        if ( QueryServiceStatusEx(hService,
                SC_STATUS_PROCESS_INFO,
                (LPBYTE)&ServiceStatus,
                sizeof ServiceStatus,
                &cbBytesNeeded) ) {

                result = ServiceStatus.dwProcessId;
        }
        return result;
}

DWORD QueryServiceProcessIdByName(SC_HANDLE hSCM, const wchar_t *pServiceName)
{
        SERVICE_STATUS_PROCESS ServiceStatusProcess;

        if ( QueryServiceStatusProcessInfoByName(hSCM, pServiceName, &ServiceStatusProcess) )
                return ServiceStatusProcess.dwProcessId;
        return 0;
}

DWORD HeuristicServiceGroupProcessId(SC_HANDLE hSCM, const wchar_t *pGroupNameSearch)
{
        wchar_t *pData;
        DWORD cbData;
        DWORD result = 0;
        DWORD dwProcessId;
        DWORD cbBufSize;
        LPQUERY_SERVICE_CONFIGW pServiceConfig;
        bool success;
        LPWSTR pGroupName;
        HLOCAL hMem;

        pData = RegGetValueAlloc(HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Svchost",
                pGroupNameSearch,
                RRF_RT_REG_MULTI_SZ,
                NULL,
                &cbData);

        if ( !pData ) return result;

        for ( wchar_t *pName = pData; *pName; pName += wcslen(pName) + 1 ) {
                dwProcessId = QueryServiceProcessIdByName(hSCM, pName);
                trace(L"pName=%ls dwProcessId=%lu", pName, dwProcessId);
                if ( !dwProcessId ) continue;

                pServiceConfig = QueryServiceConfigByNameAlloc(hSCM, pName, &cbBufSize);
                if ( !pServiceConfig ) continue;

                success = QueryServiceGroupName(pServiceConfig, &pGroupName, &hMem);
                free(pServiceConfig);
                if ( !success )
                        continue;

                success = !_wcsicmp(pGroupNameSearch, pGroupName);
                LocalFree(hMem);
                if ( success ) {
                        trace(L"Found PID: %lu", dwProcessId);
                        result = dwProcessId;
                        break;
                }
        }
        free(pData);
        return result;
}

DWORD HeuristicServiceProcessId(SC_HANDLE hSCM, SC_HANDLE hService)
{
        DWORD result = 0;
        LPQUERY_SERVICE_CONFIGW pServiceConfig;
        LPWSTR pGroupName;
        HLOCAL hMem;
        DWORD cbBufSize;

        result = QueryServiceProcessId(hSCM, hService);
        if ( result )
                return result;

        pServiceConfig = QueryServiceConfigAlloc(hSCM, hService, &cbBufSize);
        if ( pServiceConfig ) {
                switch ( pServiceConfig->dwServiceType ) {
                case SERVICE_WIN32_OWN_PROCESS:
                        // if the service isn't already running there's no
                        // way to accurately guess the PID when it is set to
                        // run in its own process. returns 0
                        break;
                case SERVICE_WIN32_SHARE_PROCESS:
                        // when the service is configured to run in a shared
                        // process, it is possible to "guess" which svchost.exe
                        // it will eventually be loaded into by finding other
                        // services in the same group that are already running.
                        if ( QueryServiceGroupName(pServiceConfig, &pGroupName, &hMem) ) {
                                result = HeuristicServiceGroupProcessId(hSCM, pGroupName);
                                LocalFree(hMem);
                                return result;
                        }
                        break;
                }
                free(pServiceConfig);
        }
}

DWORD HeuristicServiceProcessIdByName(SC_HANDLE hSCM, const wchar_t *pServiceName)
{
        DWORD result = 0;
        SC_HANDLE hService;

        hService = OpenServiceW(hSCM, pServiceName, SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
        result = HeuristicServiceProcessId(hSCM, hService);
        CloseServiceHandle(hService);
        return result;

}
