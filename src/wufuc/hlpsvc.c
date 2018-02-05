#include "stdafx.h"
#include "hlpmisc.h"
#include "hlpsvc.h"

LPQUERY_SERVICE_CONFIGW QueryServiceConfigByNameAlloc(
        SC_HANDLE hSCM,
        const wchar_t *pServiceName,
        LPDWORD pcbBufSize)
{
        SC_HANDLE hService;
        DWORD cbBytesNeeded;
        LPQUERY_SERVICE_CONFIGW result = NULL;

        hService = OpenServiceW(hSCM, pServiceName, SERVICE_QUERY_CONFIG);
        if ( !hService ) return result;

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
        CloseServiceHandle(hService);
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

bool QueryServiceGroupName(const LPQUERY_SERVICE_CONFIGW pServiceConfig, wchar_t *pGroupName, size_t nSize)
{
        bool result = false;
        int NumArgs;
        LPWSTR *argv;

        argv = CommandLineToArgvW(pServiceConfig->lpBinaryPathName, &NumArgs);
        if ( argv ) {
                if ( !_wcsicmp(PathFindFileNameW(argv[0]), L"svchost.exe") ) {

                        for ( int i = 1; (i + 1) < NumArgs; i++ ) {
                                if ( !_wcsicmp(argv[i], L"-k") )
                                        return !wcscpy_s(pGroupName, nSize, argv[++i]);
                        }
                }
                LocalFree((HLOCAL)argv);
        }
        return result;
}

DWORD QueryServiceProcessId(SC_HANDLE hSCM, const wchar_t *pServiceName)
{
        SERVICE_STATUS_PROCESS ServiceStatusProcess;

        if ( QueryServiceStatusProcessInfoByName(hSCM, pServiceName, &ServiceStatusProcess) )
                return ServiceStatusProcess.dwProcessId;
        return 0;
}

DWORD InferSvchostGroupProcessId(SC_HANDLE hSCM, const wchar_t *pGroupName)
{
        DWORD result = 0;
        DWORD cbData;
        wchar_t *pData;
        DWORD dwProcessId;
        DWORD cbBufSize;
        LPQUERY_SERVICE_CONFIGW pServiceConfig;
        bool success;
        WCHAR GroupName[256];

        pData = RegGetValueAlloc(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Svchost", pGroupName, RRF_RT_REG_MULTI_SZ, NULL, &cbData);
        if ( !pData ) return result;

        for ( wchar_t *pName = pData; *pName; pName += wcslen(pName) + 1 ) {
                dwProcessId = QueryServiceProcessId(hSCM, pName);
                trace(L"pName=%ls dwProcessId=%lu", pName, dwProcessId);
                if ( !dwProcessId ) continue;

                pServiceConfig = QueryServiceConfigByNameAlloc(hSCM, pName, &cbBufSize);
                if ( !pServiceConfig ) continue;
                success = QueryServiceGroupName(pServiceConfig, GroupName, _countof(GroupName));
                free(pServiceConfig);
                if ( success && !_wcsicmp(pGroupName, GroupName) ) {
                        trace(L"found PID for group %ls: %lu", pGroupName, dwProcessId);
                        result = dwProcessId;
                        break;
                }
        }
        free(pData);
        return result;
}
