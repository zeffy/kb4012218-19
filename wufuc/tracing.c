#include "stdafx.h"
#include "tracing.h"

void trace_(const wchar_t *const format, ...)
{
        va_list argptr;
        va_start(argptr, format);
        int count = _vscwprintf(format, argptr) + 1;
        wchar_t *buffer = calloc(count, sizeof(wchar_t));
        vswprintf_s(buffer, count, format, argptr);
        va_end(argptr);
        OutputDebugStringW(buffer);
        free(buffer);
}
