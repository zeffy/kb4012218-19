#include "stdafx.h"
#include "hlpmisc.h"
#include <sddl.h>

bool InitializeMutex(bool InitialOwner, LPCWSTR pMutexName, HANDLE *phMutex)
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
                trace(L"Failed to create mutex: %ls (GetLastError=%ld)", pMutexName, GetLastError());
        }
        return false;
}

bool CreateEventWithStringSecurityDescriptor(
        LPCWSTR pStringSecurityDescriptor,
        bool ManualReset,
        bool InitialState,
        LPCWSTR pName,
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
        LPCWSTR pSubKey,
        LPCWSTR pValue,
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
        LPCWSTR pSubKey,
        LPCWSTR pValueName,
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

        length = cbData + sizeof(WCHAR); // make sure it is null-terminated
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

PVOID NtQueryKeyAlloc(HANDLE KeyHandle, KEY_INFORMATION_CLASS KeyInformationClass, PULONG pResultLength)
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

LPWSTR ExpandEnvironmentStringsAlloc(LPCWSTR src)
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
        }
        return result;
}
