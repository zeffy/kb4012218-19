#pragma once

int ProductVersionCompare(VS_FIXEDFILEINFO *pffi, WORD wMajor, WORD wMinor, WORD wBuild, WORD wRev);
bool GetVersionInfoFromHModule(HMODULE hModule, const wchar_t *pszSubBlock, LPVOID pData, PUINT pcbData);
LPVOID GetVersionInfoFromHModuleAlloc(HMODULE hModule, const wchar_t *pszSubBlock, PUINT pcbData);
bool IsWindowsVersion(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor);
