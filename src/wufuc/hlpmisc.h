#pragma once

bool InitializeMutex(bool InitialOwner, const wchar_t *pMutexName, HANDLE *phMutex);
bool CreateEventWithStringSecurityDescriptor(
        const wchar_t *pStringSecurityDescriptor,
        bool ManualReset,
        bool InitialState,
        const wchar_t *pName,
        HANDLE *phEvent);
PVOID RegGetValueAlloc(
        HKEY hkey,
        const wchar_t *pSubKey,
        const wchar_t *pValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        LPDWORD pcbData);
LPBYTE RegQueryValueExAlloc(
        HKEY hKey,
        const wchar_t *pSubKey,
        const wchar_t *pValueName,
        LPDWORD pType,
        LPDWORD pcbData);
PVOID NtQueryKeyAlloc(
        HANDLE KeyHandle,
        KEY_INFORMATION_CLASS KeyInformationClass,
        PULONG pResultLength);
wchar_t *ExpandEnvironmentStringsAlloc(const wchar_t *src, LPDWORD pcchLength);
