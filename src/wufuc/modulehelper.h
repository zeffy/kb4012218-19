#pragma once

HMODULE mod_get_from_th32_snapshot(HANDLE hSnapshot, const wchar_t *pLibFileName);
bool mod_inject_and_begin_thread(
        HANDLE hProcess,
        HMODULE hModule,
        LPTHREAD_START_ROUTINE pStartAddress,
        const void *pParam,
        size_t cbParam);
bool mod_inject_by_hmodule(HANDLE hProcess, HMODULE hModule, HMODULE *phRemoteModule);
bool mod_inject(
        HANDLE hProcess,
        const wchar_t *pLibFilename,
        size_t cchLibFilename,
        HMODULE *phRemoteModule);
