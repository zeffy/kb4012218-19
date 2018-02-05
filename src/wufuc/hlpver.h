#pragma once

int FileInfoVerCompare(VS_FIXEDFILEINFO *pffi, WORD wMajor, WORD wMinor, WORD wBuild, WORD wRev);
bool GetVersionInfoFromHModule(HMODULE hModule, LPCWSTR pszSubBlock, LPVOID pData, PUINT pcbData);
LPVOID GetVersionInfoFromHModuleAlloc(HMODULE hModule, LPCWSTR pszSubBlock, PUINT pcbData);
bool IsWindowsVersion(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor);
