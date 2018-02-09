#pragma once

bool InitializeMutex(bool InitialOwner, LPCWSTR pMutexName, HANDLE *phMutex);
bool CreateEventWithStringSecurityDescriptor(
        LPCWSTR pStringSecurityDescriptor,
        bool ManualReset,
        bool InitialState,
        LPCWSTR pName,
        HANDLE *phEvent);
PVOID RegGetValueAlloc(
        HKEY hkey,
        LPCWSTR pSubKey,
        LPCWSTR pValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        LPDWORD pcbData);
LPBYTE RegQueryValueExAlloc(
        HKEY hKey,
        LPCWSTR pSubKey,
        LPCWSTR pValueName,
        LPDWORD pType,
        LPDWORD pcbData);
PVOID NtQueryKeyAlloc(HANDLE KeyHandle, KEY_INFORMATION_CLASS KeyInformationClass, PULONG pResultLength);
LPWSTR ExpandEnvironmentStringsAlloc(LPCWSTR src, LPDWORD pcchLength);
