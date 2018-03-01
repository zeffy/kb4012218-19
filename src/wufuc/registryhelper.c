#include "stdafx.h"
#include "registryhelper.h"
#include <sddl.h>

PVOID reg_get_value_alloc(
        HKEY hKey,
        const wchar_t *SubKey,
        const wchar_t *Value,
        DWORD dwFlags,
        LPDWORD pdwType,
        LPDWORD pcbData)
{
        DWORD cbData = 0;
        PVOID result = NULL;

        if ( RegGetValueW(hKey, SubKey, Value, dwFlags, pdwType, NULL, &cbData) != ERROR_SUCCESS )
                return result;

        result = malloc(cbData);
        if ( !result ) return result;

        if ( RegGetValueW(hKey, SubKey, Value, dwFlags, pdwType, result, &cbData) == ERROR_SUCCESS ) {
                if ( pcbData )
                        *pcbData = cbData;
        } else {
                free(result);
                result = NULL;
        }
        return result;
}

LPBYTE reg_query_value_alloc(
        HKEY hKey,
        const wchar_t *SubKey,
        const wchar_t *Value,
        LPDWORD pdwType,
        LPDWORD pcbData)
{
        HKEY hSubKey;
        DWORD cbData = 0;
        DWORD dwType;
        LPBYTE result = NULL;

        if ( SubKey && *SubKey ) {
                if ( RegOpenKeyW(hKey, SubKey, &hSubKey) != ERROR_SUCCESS )
                        return result;
        } else {
                hSubKey = hKey;
        }
        if ( RegQueryValueExW(hSubKey, Value, NULL, &dwType, result, &cbData) != ERROR_SUCCESS )
                return result;

        switch ( dwType ) {
        case REG_SZ:
        case REG_EXPAND_SZ:
                cbData += sizeof UNICODE_NULL;
                break;
        case REG_MULTI_SZ:
                cbData += (sizeof UNICODE_NULL) * 2;
                break;
        }
        result = malloc(cbData);

        if ( !result ) return result;
        ZeroMemory(result, cbData);

        if ( RegQueryValueExW(hSubKey, Value, NULL, pdwType, result, &cbData) == ERROR_SUCCESS ) {
                if ( pcbData )
                        *pcbData = cbData;
        } else {
                free(result);
                result = NULL;
        }
        return result;
}

PVOID reg_query_key_alloc(
        HANDLE KeyHandle,
        KEY_INFORMATION_CLASS KeyInformationClass,
        PULONG pResultLength)
{
        NTSTATUS Status;
        ULONG ResultLength;
        PVOID result = NULL;

        Status = NtQueryKey(KeyHandle, KeyInformationClass, NULL, 0, &ResultLength);
        if ( Status != STATUS_BUFFER_OVERFLOW && Status != STATUS_BUFFER_TOO_SMALL )
                return result;

        result = malloc(ResultLength);
        if ( !result ) return result;

        Status = NtQueryKey(KeyHandle, KeyInformationClass, result, ResultLength, &ResultLength);
        if ( NT_SUCCESS(Status) ) {
                *pResultLength = ResultLength;
        } else {
                free(result);
                result = NULL;
        }
        return result;
}

wchar_t *env_expand_strings_alloc(const wchar_t *src, LPDWORD pcchLength)
{
        wchar_t *result;
        DWORD buffersize;
        DWORD size;

        buffersize = ExpandEnvironmentStringsW(src, NULL, 0);
        result = calloc(buffersize, sizeof *result);
        size = ExpandEnvironmentStringsW(src, result, buffersize);
        if ( !size || size > buffersize ) {
                free(result);
                result = NULL;
        } else if ( pcchLength ) {
                *pcchLength = buffersize;
        }
        return result;
}
