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
                *pcbData = cbData;
        } else {
                free(result);
                result = NULL;
        }
        return result;
}
