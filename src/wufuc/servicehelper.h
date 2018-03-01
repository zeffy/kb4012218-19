#pragma once

LPQUERY_SERVICE_CONFIGW svc_query_config_by_name_alloc(
        SC_HANDLE hSCM,
        const wchar_t *pServiceName,
        LPDWORD pcbBufSize);
LPQUERY_SERVICE_CONFIGW svc_query_config_alloc(
        SC_HANDLE hSCM,
        SC_HANDLE hService,
        LPDWORD pcbBufSize);
bool svc_query_process_info_by_name(
        SC_HANDLE hSCM,
        const wchar_t *pServiceName,
        LPSERVICE_STATUS_PROCESS pServiceStatus);
bool svc_query_group_name(
        const LPQUERY_SERVICE_CONFIGW pServiceConfig,
        wchar_t **pGroupName,
        HLOCAL *hMem);
DWORD svc_query_process_id(SC_HANDLE hSCM, SC_HANDLE hService);
DWORD svc_query_process_id_by_name(SC_HANDLE hSCM, const wchar_t *pServiceName);
DWORD svc_heuristic_group_process_id(SC_HANDLE hSCM, const wchar_t *pGroupName);
DWORD svc_heuristic_process_id(SC_HANDLE hSCM, SC_HANDLE hService);
DWORD svc_heuristic_process_id_by_name(SC_HANDLE hSCM, const wchar_t *pServiceName);
