#pragma once

typedef struct
{
        WORD wLanguage;
        WORD wCodePage;
} LANGANDCODEPAGE, *PLANGANDCODEPAGE;

#define WUFUC_CRASH_THRESHOLD 3

extern size_t g_ServiceHostCrashCount;

bool wufuc_inject(DWORD dwProcessId,
        LPTHREAD_START_ROUTINE pfnStart,
        context *pContext);
bool wufuc_hook(HMODULE hModule);