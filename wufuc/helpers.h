#pragma once

bool create_exclusive_mutex(const wchar_t *name, HANDLE *pmutex);
bool create_event_with_security_descriptor(const wchar_t *descriptor, bool manualreset, bool initialstate, const wchar_t *name, HANDLE *pevent);

bool inject_self_into_process(DWORD dwProcessId, HMODULE *phModule);
bool inject_dll_into_process(DWORD dwProcessId, const wchar_t *pszFilename, size_t nLength, HMODULE *phModule);

const wchar_t *path_find_fname(const wchar_t *path);
bool path_change_fname(const wchar_t *srcpath, const wchar_t *fname, wchar_t *dstpath, size_t size);
bool path_file_exists(const wchar_t *path);
bool path_expand_file_exists(const wchar_t *path);
int ffi_ver_compare(VS_FIXEDFILEINFO *pffi, WORD wMajor, WORD wMinor, WORD wBuild, WORD wRev);