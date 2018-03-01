#pragma once

int ver_compare_product_version(VS_FIXEDFILEINFO *pffi, WORD wMajor, WORD wMinor, WORD wBuild, WORD wRev);
bool ver_get_version_info_from_hmodule(HMODULE hModule, const wchar_t *pszSubBlock, LPVOID pData, PUINT pcbData);
LPVOID ver_get_version_info_from_hmodule_alloc(HMODULE hModule, const wchar_t *pszSubBlock, PUINT pcbData);
bool ver_verify_version_info(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor);
bool ver_verify_windows_7_sp1(void);
bool ver_verify_windows_8_1(void);