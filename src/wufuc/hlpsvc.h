#pragma once

LPQUERY_SERVICE_CONFIGW QueryServiceConfigByNameAlloc(
        SC_HANDLE hSCM,
        const wchar_t *pServiceName,
        LPDWORD pcbBufSize);
LPQUERY_SERVICE_CONFIGW QueryServiceConfigAlloc(
        SC_HANDLE hSCM,
        SC_HANDLE hService,
        LPDWORD pcbBufSize);
bool QueryServiceStatusProcessInfoByName(
        SC_HANDLE hSCM,
        const wchar_t *pServiceName,
        LPSERVICE_STATUS_PROCESS pServiceStatus);
bool QueryServiceGroupName(
        const LPQUERY_SERVICE_CONFIGW pServiceConfig,
        wchar_t **pGroupName,
        HLOCAL *hMem);
DWORD QueryServiceProcessId(SC_HANDLE hSCM, SC_HANDLE hService);
DWORD QueryServiceProcessIdByName(SC_HANDLE hSCM, const wchar_t *pServiceName);
DWORD HeuristicServiceGroupProcessId(SC_HANDLE hSCM, const wchar_t *pGroupName);
DWORD HeuristicServiceProcessId(SC_HANDLE hSCM, SC_HANDLE hService);
DWORD HeuristicServiceProcessIdByName(SC_HANDLE hSCM, const wchar_t *pServiceName);
