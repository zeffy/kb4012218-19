#pragma once

PVOID reg_get_value_alloc(
        HKEY hkey,
        const wchar_t *pSubKey,
        const wchar_t *pValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        LPDWORD pcbData);
LPBYTE reg_query_value_alloc(
        HKEY hKey,
        const wchar_t *pSubKey,
        const wchar_t *pValueName,
        LPDWORD pType,
        LPDWORD pcbData);
PVOID reg_query_key_alloc(
        HANDLE KeyHandle,
        KEY_INFORMATION_CLASS KeyInformationClass,
        PULONG pResultLength);
wchar_t *env_expand_strings_alloc(const wchar_t *src, LPDWORD pcchLength);
