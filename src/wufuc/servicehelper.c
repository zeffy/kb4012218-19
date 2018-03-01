#include "stdafx.h"
#include "servicehelper.h"
#include "registryhelper.h"

LPQUERY_SERVICE_CONFIGW svc_query_config_by_name_alloc(
        SC_HANDLE hSCM,
        const wchar_t *pServiceName,
        LPDWORD pcbBufSize)
{
        SC_HANDLE hService;
        LPQUERY_SERVICE_CONFIGW result = NULL;

        hService = OpenServiceW(hSCM, pServiceName, SERVICE_QUERY_CONFIG);
        if ( !hService ) return result;

        result = svc_query_config_alloc(hSCM, hService, pcbBufSize);

        CloseServiceHandle(hService);
        return result;
}

LPQUERY_SERVICE_CONFIGW svc_query_config_alloc(
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
                                if ( pcbBufSize )
                                        *pcbBufSize = cbBytesNeeded;
                        } else {
                                free(result);
                                result = NULL;
                        }
                }
        }
        return result;
}

bool svc_query_process_info_by_name(
        SC_HANDLE hSCM,
        const wchar_t *pServiceName,
        LPSERVICE_STATUS_PROCESS pServiceStatus)
{
        bool result = false;
        SC_HANDLE hService;
        DWORD cbBytesNeeded;

        hService = OpenServiceW(hSCM, pServiceName, SERVICE_QUERY_STATUS);
        if ( !hService )
                return result;

        result = !!QueryServiceStatusEx(hService,
                SC_STATUS_PROCESS_INFO,
                (LPBYTE)pServiceStatus,
                sizeof *pServiceStatus,
                &cbBytesNeeded);
        CloseServiceHandle(hService);
        return result;
}

bool svc_query_group_name(
        const LPQUERY_SERVICE_CONFIGW pServiceConfig,
        wchar_t **pGroupName,
        HLOCAL *hMem)
{
        bool result = false;
        int NumArgs;
        wchar_t **argv;

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

DWORD svc_query_process_id(SC_HANDLE hSCM, SC_HANDLE hService)
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

DWORD svc_query_process_id_by_name(SC_HANDLE hSCM, const wchar_t *pServiceName)
{
        SERVICE_STATUS_PROCESS ServiceStatusProcess;

        if ( svc_query_process_info_by_name(hSCM, pServiceName, &ServiceStatusProcess) )
                return ServiceStatusProcess.dwProcessId;
        return 0;
}

DWORD svc_heuristic_group_process_id(SC_HANDLE hSCM, const wchar_t *pGroupNameSearch)
{
        wchar_t *pData;
        DWORD result = 0;
        DWORD dwProcessId;
        DWORD cbBufSize;
        LPQUERY_SERVICE_CONFIGW pServiceConfig;
        bool success = false;
        wchar_t *pGroupName;
        HLOCAL hMem;

        pData = reg_get_value_alloc(HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Svchost",
                pGroupNameSearch,
                RRF_RT_REG_MULTI_SZ,
                NULL,
                NULL);

        if ( !pData ) return result;

        for ( wchar_t *pName = pData; *pName; pName += wcslen(pName) + 1 ) {
                dwProcessId = svc_query_process_id_by_name(hSCM, pName);
                if ( !dwProcessId ) continue;

                pServiceConfig = svc_query_config_by_name_alloc(hSCM, pName, &cbBufSize);
                if ( !pServiceConfig ) continue;

                if ( pServiceConfig->dwServiceType == SERVICE_WIN32_SHARE_PROCESS
                        && svc_query_group_name(pServiceConfig, &pGroupName, &hMem) ) {

                        success = !_wcsicmp(pGroupNameSearch, pGroupName);
                        LocalFree(hMem);
                }
                free(pServiceConfig);
                if ( success ) {
                        result = dwProcessId;
                        break;
                }
        }
        free(pData);
        return result;
}

DWORD svc_heuristic_process_id(SC_HANDLE hSCM, SC_HANDLE hService)
{
        DWORD result = 0;
        LPQUERY_SERVICE_CONFIGW pServiceConfig;
        wchar_t *pGroupName;
        HLOCAL hMem;

        result = svc_query_process_id(hSCM, hService);
        if ( result )
                return result;

        pServiceConfig = svc_query_config_alloc(hSCM, hService, NULL);
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
                        if ( svc_query_group_name(pServiceConfig, &pGroupName, &hMem) ) {
                                result = svc_heuristic_group_process_id(hSCM, pGroupName);
                                LocalFree(hMem);
                        }
                        break;
                }
                free(pServiceConfig);
        }
        return result;
}

DWORD svc_heuristic_process_id_by_name(SC_HANDLE hSCM, const wchar_t *pServiceName)
{
        DWORD result = 0;
        SC_HANDLE hService;

        hService = OpenServiceW(hSCM, pServiceName, SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
        result = svc_heuristic_process_id(hSCM, hService);
        CloseServiceHandle(hService);
        return result;

}
