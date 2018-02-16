#include "stdafx.h"
#include "hlpmisc.h"
#include <sddl.h>

bool InitializeMutex(bool InitialOwner, const wchar_t *pMutexName, HANDLE *phMutex)
{
        HANDLE hMutex;

        hMutex = CreateMutexW(NULL, InitialOwner, pMutexName);
        if ( hMutex ) {
                if ( GetLastError() == ERROR_ALREADY_EXISTS ) {
                        CloseHandle(hMutex);
                        return false;
                }
                *phMutex = hMutex;
                return true;
        } else {
                trace(L"Failed to create mutex: %ls (GetLastError=%ld)",
                        pMutexName, GetLastError());
        }
        return false;
}

bool CreateEventWithStringSecurityDescriptor(
        const wchar_t *pStringSecurityDescriptor,
        bool ManualReset,
        bool InitialState,
        const wchar_t *pName,
        HANDLE *phEvent)
{
        SECURITY_ATTRIBUTES sa = { sizeof sa };
        HANDLE event;

        if ( ConvertStringSecurityDescriptorToSecurityDescriptorW(
                pStringSecurityDescriptor,
                SDDL_REVISION_1,
                &sa.lpSecurityDescriptor,
                NULL) ) {

                event = CreateEventW(&sa, ManualReset, InitialState, pName);
                if ( event ) {
                        *phEvent = event;
                        return true;
                }
        }
        return false;
}

PVOID RegGetValueAlloc(
        HKEY hkey,
        const wchar_t *pSubKey,
        const wchar_t *pValue,
        DWORD dwFlags,
        LPDWORD pdwType,
        LPDWORD pcbData)
{
        DWORD cbData = 0;
        PVOID result = NULL;

        if ( RegGetValueW(hkey, pSubKey, pValue, dwFlags, pdwType, NULL, &cbData) != ERROR_SUCCESS )
                return result;

        result = malloc(cbData);
        if ( !result ) return result;

        if ( RegGetValueW(hkey, pSubKey, pValue, dwFlags, pdwType, result, &cbData) == ERROR_SUCCESS ) {
                if ( pcbData )
                        *pcbData = cbData;
        } else {
                free(result);
                result = NULL;
        }
        return result;
}

LPBYTE RegQueryValueExAlloc(
        HKEY hKey,
        const wchar_t *pSubKey,
        const wchar_t *pValueName,
        LPDWORD pType,
        LPDWORD pcbData)
{
        HKEY hSubKey;
        DWORD cbData = 0;
        size_t length;
        LPBYTE result = NULL;

        if ( pSubKey && *pSubKey ) {
                if ( RegOpenKeyW(hKey, pSubKey, &hSubKey) != ERROR_SUCCESS )
                        return result;
        } else {
                hSubKey = hKey;
        }
        if ( RegQueryValueExW(hSubKey, pValueName, NULL, pType, result, &cbData) != ERROR_SUCCESS )
                return result;

        length = cbData + (sizeof UNICODE_NULL * 2);
        result = malloc(length);

        if ( !result ) return result;
        ZeroMemory(result, length);

        if ( RegQueryValueExW(hSubKey, pValueName, NULL, pType, result, &cbData) == ERROR_SUCCESS ) {
                if ( pcbData )
                        *pcbData = cbData;
        } else {
                free(result);
                result = NULL;
        }
        return result;
}

PVOID NtQueryKeyAlloc(
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

wchar_t *ExpandEnvironmentStringsAlloc(const wchar_t *src, LPDWORD pcchLength)
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
