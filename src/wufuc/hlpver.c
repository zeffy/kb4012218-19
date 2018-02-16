#include "stdafx.h"
#include "hlpver.h"

int ProductVersionCompare(VS_FIXEDFILEINFO *pffi, WORD wMajor, WORD wMinor, WORD wBuild, WORD wRev)
{
        if ( HIWORD(pffi->dwProductVersionMS) < wMajor ) return -1;
        if ( HIWORD(pffi->dwProductVersionMS) > wMajor ) return 1;
        if ( LOWORD(pffi->dwProductVersionMS) < wMinor ) return -1;
        if ( LOWORD(pffi->dwProductVersionMS) > wMinor ) return 1;
        if ( HIWORD(pffi->dwProductVersionLS) < wBuild ) return -1;
        if ( HIWORD(pffi->dwProductVersionLS) > wBuild ) return 1;
        if ( LOWORD(pffi->dwProductVersionLS) < wRev ) return -1;
        if ( LOWORD(pffi->dwProductVersionLS) > wRev ) return 1;
        return 0;
}

bool GetVersionInfoFromHModule(HMODULE hModule, const wchar_t *pszSubBlock, LPVOID pData, PUINT pcbData)
{
        bool result = false;
        UINT cbData;
        HRSRC hResInfo;
        DWORD dwSize;
        HGLOBAL hResData;
        LPVOID pRes;
        LPVOID pCopy;
        LPVOID pBuffer;
        UINT uLen;

        if ( !pcbData ) return result;
        cbData = *pcbData;

        hResInfo = FindResourceW(hModule,
                MAKEINTRESOURCEW(VS_VERSION_INFO),
                RT_VERSION);
        if ( !hResInfo ) return result;

        dwSize = SizeofResource(hModule, hResInfo);
        if ( !dwSize ) return result;

        hResData = LoadResource(hModule, hResInfo);
        if ( !hResData ) return result;

        pRes = LockResource(hResData);
        if ( !pRes ) return result;

        pCopy = malloc(dwSize);
        if ( !pCopy
                || memcpy_s(pCopy, dwSize, pRes, dwSize)
                || !VerQueryValueW(pCopy, pszSubBlock, &pBuffer, &uLen) )
                goto cleanup;

        if ( !_wcsnicmp(pszSubBlock, L"\\StringFileInfo\\", 16) )
                *pcbData = uLen * sizeof(wchar_t);
        else
                *pcbData = uLen;

        if ( !pData ) {
                result = true;
                goto cleanup;
        }
        if ( cbData < *pcbData
                || memcpy_s(pData, cbData, pBuffer, *pcbData) )
                goto cleanup;

        result = true;
cleanup:
        free(pCopy);
        return result;
}

LPVOID GetVersionInfoFromHModuleAlloc(HMODULE hModule, const wchar_t *pszSubBlock, PUINT pcbData)
{
        UINT cbData = 0;
        LPVOID result = NULL;

        if ( !GetVersionInfoFromHModule(hModule, pszSubBlock, NULL, &cbData) )
                return result;

        result = malloc(cbData);
        if ( !result ) return result;

        if ( GetVersionInfoFromHModule(hModule, pszSubBlock, result, &cbData) ) {
                *pcbData = cbData;
        } else {
                free(result);
                result = NULL;
        }
        return result;
}

bool IsWindowsVersion(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor)
{
        OSVERSIONINFOEXW osvi = { sizeof osvi };

        DWORDLONG dwlConditionMask = 0;
        VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_EQUAL);
        VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, VER_EQUAL);
        VER_SET_CONDITION(dwlConditionMask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

        osvi.dwMajorVersion = wMajorVersion;
        osvi.dwMinorVersion = wMinorVersion;
        osvi.wServicePackMajor = wServicePackMajor;

        return VerifyVersionInfoW(&osvi,
                VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR,
                dwlConditionMask) != FALSE;
}
