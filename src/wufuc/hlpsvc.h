#pragma once

LPQUERY_SERVICE_CONFIGW QueryServiceConfigByNameAlloc(
        SC_HANDLE hSCM,
        const wchar_t *pServiceName,
        LPDWORD pcbBufSize);
bool QueryServiceStatusProcessInfoByName(
        SC_HANDLE hSCM,
        const wchar_t *pServiceName,
        LPSERVICE_STATUS_PROCESS pServiceStatus);
bool QueryServiceGroupName(const LPQUERY_SERVICE_CONFIGW pServiceConfig, wchar_t *pGroupName, size_t nSize);
DWORD QueryServiceProcessId(SC_HANDLE hSCM, const wchar_t *pServiceName);
DWORD InferSvchostGroupProcessId(SC_HANDLE hSCM, const wchar_t *pGroupName);
