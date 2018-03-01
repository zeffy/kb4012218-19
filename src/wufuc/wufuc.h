#pragma once

typedef struct
{
        WORD wLanguage;
        WORD wCodePage;
} LANGANDCODEPAGE, *PLANGANDCODEPAGE;

bool wufuc_inject(DWORD dwProcessId,
        LPTHREAD_START_ROUTINE pfnStart,
        context *pContext);
bool wufuc_hook(HMODULE hModule);