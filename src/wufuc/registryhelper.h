#pragma once

PVOID reg_get_value_alloc(
        HKEY hkey,
        LPCWSTR pSubKey,
        LPCWSTR pValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        LPDWORD pcbData);
LPBYTE reg_query_value_alloc(
        HKEY hKey,
        LPCWSTR pSubKey,
        LPCWSTR pValueName,
        LPDWORD pType,
        LPDWORD pcbData);
PVOID reg_query_key_alloc(
        HANDLE KeyHandle,
        KEY_INFORMATION_CLASS KeyInformationClass,
        PULONG pResultLength);
LPWSTR env_expand_strings_alloc(LPCWSTR Src, LPDWORD pcchLength);
